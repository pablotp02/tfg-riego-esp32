from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy.orm import Session
from typing import List, Optional
from datetime import datetime, date
from database import get_db
import models, schemas
import csv
import io
from fastapi.responses import StreamingResponse

router = APIRouter()

@router.post("/", response_model=schemas.SystemCycleOut)
def create_cycle(payload: schemas.CyclePayload, db: Session = Depends(get_db)):
    """
    Endpoint principal — recibe el payload completo del firmware ESP32
    y lo desglosa en las tablas correspondientes de la base de datos.
    """

    # 1) Guardar lectura del sensor
    sensor = models.SensorReading(
        soil_moisture = payload.soil_moisture,
        soil_temp     = payload.soil_temp,
        ph            = payload.ph,
        ec            = payload.ec,
        read_ok       = payload.read_ok,
        validated     = payload.validated
    )
    db.add(sensor)
    db.flush()  # para obtener el id sin hacer commit todavía

    # 2) Guardar error de sensor si lo hay
    if payload.sensor_error:
        error = models.SystemError(
            error_type = "READ_ERROR" if not payload.read_ok else "VALIDATION_ERROR",
            reason     = payload.sensor_error,
            reading_id = sensor.id
        )
        db.add(error)

    # 3) Guardar evento de riego
    irrigation = models.IrrigationEvent(
        irrigated   = payload.irrigated,
        duration_ms = payload.duration_ms if payload.irrigated else None,
        reason      = payload.irrigation_reason,
        reading_id  = sensor.id
    )
    db.add(irrigation)
    db.flush()

    # 4) Guardar estado energético
    power = models.PowerState(
        battery_pct_simulated = payload.battery_pct,
        battery_pct_real      = None,  # pendiente de hardware real
        power_mode            = payload.power_mode,
        sleep_ms              = payload.sleep_ms,
        wakeup_cause          = payload.wakeup_cause
    )
    db.add(power)
    db.flush()

    # 5) Comprobar si hay que generar alerta de batería
    if payload.battery_pct <= 20.0:
        alert = models.Alert(
            alert_type = "BATTERY_CRITICAL" if payload.battery_pct <= 10.0 else "BATTERY_LOW",
            message    = f"Nivel de batería: {payload.battery_pct}%",
            sent       = False,
            channel    = "telegram",
            power_id   = power.id
        )
        db.add(alert)

    # 6) Guardar resumen del ciclo
    cycle = models.SystemCycle(
        cycle_number      = payload.cycle_number,
        reading_id        = sensor.id,
        irrigation_id     = irrigation.id,
        power_id          = power.id,
        cooldown_current  = payload.cooldown_current,
        cooldown_required = payload.cooldown_required
    )
    db.add(cycle)
    db.commit()
    db.refresh(cycle)

    return cycle

@router.get("/", response_model=List[schemas.SystemCycleOut])
def get_cycles(skip: int = 0, limit: int = 50, db: Session = Depends(get_db)):
    """Devuelve los últimos ciclos para el dashboard."""
    return db.query(models.SystemCycle)\
             .order_by(models.SystemCycle.recorded_at.desc())\
             .offset(skip).limit(limit).all()


def power_mode_to_str(mode: int) -> str:
    return {0: "NORMAL", 1: "LOW", 2: "CRITICAL"}.get(mode, "DESCONOCIDO")


@router.get("/export/range")
def get_export_date_range(db: Session = Depends(get_db)):
    """
    Devuelve la fecha del primer y último ciclo registrado, para que
    el frontend pueda limitar el selector de fechas de exportación a
    un rango que realmente contenga datos.
    """
    first_cycle = db.query(models.SystemCycle).order_by(models.SystemCycle.recorded_at.asc()).first()
    last_cycle = db.query(models.SystemCycle).order_by(models.SystemCycle.recorded_at.desc()).first()

    return {
        "first_date": first_cycle.recorded_at.isoformat() if first_cycle else None,
        "last_date": last_cycle.recorded_at.isoformat() if last_cycle else None,
    }


@router.get("/export/csv")
def export_cycles_csv(
    desde: Optional[date] = None,
    hasta: Optional[date] = None,
    db: Session = Depends(get_db)
):
    """
    Exporta el histórico de ciclos en formato CSV, uniendo en cada
    fila los datos de lectura de sensores, evento de riego y estado
    energético asociados. Admite un rango de fechas opcional; si no
    se especifica, exporta el histórico completo.
    """
    query = db.query(models.SystemCycle)

    if desde:
        query = query.filter(models.SystemCycle.recorded_at >= datetime.combine(desde, datetime.min.time()))
    if hasta:
        query = query.filter(models.SystemCycle.recorded_at <= datetime.combine(hasta, datetime.max.time()))

    cycles = query.order_by(models.SystemCycle.recorded_at.asc()).all()

    output = io.StringIO()
    writer = csv.writer(output)

    writer.writerow([
        "cycle_number", "fecha",
        "humedad_suelo_%", "temperatura_suelo_C", "ph", "ec_uS_cm", "lectura_valida",
        "rego", "duracion_riego_ms", "motivo",
        "cooldown_actual", "cooldown_requerido",
        "bateria_%", "modo_energetico", "tiempo_sueno_ms", "causa_despertar"
    ])

    for cycle in cycles:
        reading = cycle.reading
        irrigation = cycle.irrigation
        power = cycle.power

        battery = None
        if power:
            battery = power.battery_pct_real if power.battery_pct_real is not None else power.battery_pct_simulated

        writer.writerow([
            cycle.cycle_number,
            cycle.recorded_at.isoformat() if cycle.recorded_at else "",
            reading.soil_moisture if reading else "",
            reading.soil_temp if reading else "",
            reading.ph if reading else "",
            reading.ec if reading else "",
            reading.validated if reading else "",
            irrigation.irrigated if irrigation else "",
            irrigation.duration_ms if irrigation else "",
            irrigation.reason if irrigation else "",
            cycle.cooldown_current,
            cycle.cooldown_required,
            battery,
            power_mode_to_str(power.power_mode) if power else "",
            power.sleep_ms if power else "",
            power.wakeup_cause if power else "",
        ])

    output.seek(0)
    return StreamingResponse(
        iter([output.getvalue()]),
        media_type="text/csv",
        headers={"Content-Disposition": "attachment; filename=historial_riego.csv"}
    )


@router.get("/{cycle_id}", response_model=schemas.SystemCycleOut)
def get_cycle(cycle_id: int, db: Session = Depends(get_db)):
    """Devuelve un ciclo concreto por su ID."""
    cycle = db.query(models.SystemCycle).filter(models.SystemCycle.id == cycle_id).first()
    if not cycle:
        raise HTTPException(status_code=404, detail="Ciclo no encontrado")
    return cycle