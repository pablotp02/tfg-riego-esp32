from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy.orm import Session
from typing import List
from database import get_db
import models, schemas

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

@router.get("/{cycle_id}", response_model=schemas.SystemCycleOut)
def get_cycle(cycle_id: int, db: Session = Depends(get_db)):
    """Devuelve un ciclo concreto por su ID."""
    cycle = db.query(models.SystemCycle).filter(models.SystemCycle.id == cycle_id).first()
    if not cycle:
        raise HTTPException(status_code=404, detail="Ciclo no encontrado")
    return cycle