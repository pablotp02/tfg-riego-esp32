from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy.orm import Session
from typing import List, Optional
from datetime import datetime, date
from database import get_db
import models, schemas
import csv
import io
from fastapi.responses import StreamingResponse
from routers.notifications import notify_alert

from reportlab.lib import colors
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib.units import cm
from reportlab.platypus import SimpleDocTemplate, Paragraph, Spacer, Table, TableStyle

from datetime import timedelta

SENSOR_ERROR_THRESHOLD = 5  # lecturas consecutivas inválidas para disparar alerta
SENSOR_ERROR_ALERT_COOLDOWN_MINUTES = 15

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

    # Comprobar patrón de errores repetidos de sensor tras guardar
    # esta lectura (incluye la actual en el recuento)
    check_repeated_sensor_error(db)

    # Comprobar si la temperatura del suelo requiere generar alguna
    # alerta (frío perjudicial para la planta, o riesgo de congelación)
    check_soil_temperature_alerts(db, payload.soil_temp)

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
        alert_type = "BATTERY_CRITICAL" if payload.battery_pct <= 10.0 else "BATTERY_LOW"
        alert_message = f"Nivel de batería: {payload.battery_pct}%"

        alert = models.Alert(
            alert_type = alert_type,
            message    = alert_message,
            sent       = False,
            channel    = "telegram",
            power_id   = power.id
        )
        db.add(alert)
        db.flush()  # para poder actualizar alert.sent tras el envío

        emoji = "🔴" if alert_type == "BATTERY_CRITICAL" else "🟠"
        sent_ok = notify_alert(db, f"{emoji} {alert_type}\n{alert_message}")
        alert.sent = sent_ok

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

def check_repeated_sensor_error(db: Session):
    """
    Comprueba si las últimas SENSOR_ERROR_THRESHOLD lecturas de sensor
    son todas inválidas. Si es así, y no hay ya una alerta reciente del
    mismo tipo, genera una nueva alerta SENSOR_ERROR_REPEATED.
    """
    recent_readings = db.query(models.SensorReading)\
                         .order_by(models.SensorReading.recorded_at.desc())\
                         .limit(SENSOR_ERROR_THRESHOLD)\
                         .all()

    if len(recent_readings) < SENSOR_ERROR_THRESHOLD:
        # Todavía no hay histórico suficiente para evaluar el patrón
        return

    if not all(not r.validated for r in recent_readings):
        # Al menos una de las últimas lecturas es válida, no hay problema
        return

    recent_alert = db.query(models.Alert)\
                      .filter(models.Alert.alert_type == "SENSOR_ERROR_REPEATED")\
                      .order_by(models.Alert.recorded_at.desc())\
                      .first()

    if recent_alert:
        time_since_last_alert = datetime.utcnow() - recent_alert.recorded_at
        if time_since_last_alert < timedelta(minutes=SENSOR_ERROR_ALERT_COOLDOWN_MINUTES):
            return

    alert_message = f"Las últimas {SENSOR_ERROR_THRESHOLD} lecturas del sensor no son válidas. Revisa la conexión física del sensor SEN0604."

    new_alert = models.Alert(
        alert_type="SENSOR_ERROR_REPEATED",
        message=alert_message,
        sent=False,
        channel="telegram",
        power_id=None
    )
    db.add(new_alert)
    db.flush()

    sent_ok = notify_alert(db, f"⚠️ SENSOR_ERROR_REPEATED\n{alert_message}")
    new_alert.sent = sent_ok

def check_soil_temperature_alerts(db: Session, soil_temp: float | None):
    """
    Comprueba si la temperatura del suelo del ciclo actual está por
    debajo del umbral mínimo de riego o del umbral de riesgo de
    congelación, generando la alerta correspondiente en cada caso.
    """
    if soil_temp is None:
        return

    last_config = db.query(models.DeviceConfig)\
                     .order_by(models.DeviceConfig.updated_at.desc())\
                     .first()

    if not last_config:
        return

    if soil_temp < last_config.soil_min_temp_c:
        alert_message = f"Temperatura del suelo: {soil_temp}°C. El riego se pausa para no perjudicar a la planta."
        alert = models.Alert(
            alert_type="SOIL_TEMP_LOW",
            message=alert_message,
            sent=False,
            channel="telegram",
            power_id=None
        )
        db.add(alert)
        db.flush()
        sent_ok = notify_alert(db, f"🥶 SOIL_TEMP_LOW\n{alert_message}")
        alert.sent = sent_ok

    if soil_temp < last_config.soil_freeze_risk_temp_c:
        alert_message = f"Temperatura del suelo: {soil_temp}°C. Riesgo de congelación del agua en el sistema."
        alert = models.Alert(
            alert_type="SOIL_FREEZE_RISK",
            message=alert_message,
            sent=False,
            channel="telegram",
            power_id=None
        )
        db.add(alert)
        db.flush()
        sent_ok = notify_alert(db, f"❄️ SOIL_FREEZE_RISK\n{alert_message}")
        alert.sent = sent_ok

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


@router.get("/export/pdf")
def export_cycles_pdf(
    desde: Optional[date] = None,
    hasta: Optional[date] = None,
    db: Session = Depends(get_db)
):
    """
    Genera un informe PDF legible del histórico de ciclos, pensado
    para lectura humana (a diferencia del CSV, orientado a análisis
    de datos). Incluye un resumen ejecutivo y una tabla resumida con
    las columnas más relevantes.
    """
    query = db.query(models.SystemCycle)

    if desde:
        query = query.filter(models.SystemCycle.recorded_at >= datetime.combine(desde, datetime.min.time()))
    if hasta:
        query = query.filter(models.SystemCycle.recorded_at <= datetime.combine(hasta, datetime.max.time()))

    cycles = query.order_by(models.SystemCycle.recorded_at.asc()).all()

    buffer = io.BytesIO()
    doc = SimpleDocTemplate(buffer, pagesize=A4,
                             topMargin=2*cm, bottomMargin=2*cm,
                             leftMargin=2*cm, rightMargin=2*cm)

    styles = getSampleStyleSheet()
    title_style = ParagraphStyle('TituloInforme', parent=styles['Heading1'],
                                  textColor=colors.HexColor('#2d6a4f'))
    subtitle_style = ParagraphStyle('Subtitulo', parent=styles['Normal'],
                                     textColor=colors.HexColor('#718096'), fontSize=10)
    section_style = ParagraphStyle('Seccion', parent=styles['Heading2'],
                                    textColor=colors.HexColor('#2d6a4f'), spaceBefore=16)
    cell_style = ParagraphStyle('Celda', parent=styles['Normal'], fontSize=8)

    story = []

    # ─── Cabecera ────────────────────────────────────────────
    story.append(Paragraph("Informe de Riego Automático", title_style))

    rango_txt = "Histórico completo"
    if desde or hasta:
        rango_txt = f"Del {desde.strftime('%d/%m/%Y') if desde else 'inicio'} al {hasta.strftime('%d/%m/%Y') if hasta else 'hoy'}"
    story.append(Paragraph(rango_txt, subtitle_style))
    story.append(Paragraph(f"Generado el {datetime.now().strftime('%d/%m/%Y %H:%M')}", subtitle_style))
    story.append(Spacer(1, 0.5*cm))

    # ─── Resumen ejecutivo ───────────────────────────────────
    story.append(Paragraph("Resumen", section_style))

    total_ciclos = len(cycles)
    riegos_realizados = sum(1 for c in cycles if c.irrigation and c.irrigation.irrigated)

    lecturas_validas = [c.reading for c in cycles if c.reading and c.reading.validated]
    humedad_media = (sum(r.soil_moisture for r in lecturas_validas) / len(lecturas_validas)) if lecturas_validas else None
    temp_media = (sum(r.soil_temp for r in lecturas_validas) / len(lecturas_validas)) if lecturas_validas else None

    ultima_bateria = None
    if cycles and cycles[-1].power:
        p = cycles[-1].power
        ultima_bateria = p.battery_pct_real if p.battery_pct_real is not None else p.battery_pct_simulated

    resumen_data = [
        ["Total de ciclos registrados", str(total_ciclos)],
        ["Riegos realizados", str(riegos_realizados)],
        ["Humedad media del suelo", f"{humedad_media:.1f}%" if humedad_media is not None else "N/D"],
        ["Temperatura media del suelo", f"{temp_media:.1f}°C" if temp_media is not None else "N/D"],
        ["Última batería registrada", f"{ultima_bateria:.1f}%" if ultima_bateria is not None else "N/D"],
    ]

    resumen_table = Table(resumen_data, colWidths=[8*cm, 6*cm])
    resumen_table.setStyle(TableStyle([
        ('FONTSIZE', (0, 0), (-1, -1), 9),
        ('TEXTCOLOR', (0, 0), (0, -1), colors.HexColor('#718096')),
        ('FONTNAME', (1, 0), (1, -1), 'Helvetica-Bold'),
        ('BOTTOMPADDING', (0, 0), (-1, -1), 6),
        ('TOPPADDING', (0, 0), (-1, -1), 6),
        ('LINEBELOW', (0, 0), (-1, -1), 0.5, colors.HexColor('#e2e8f0')),
    ]))
    story.append(resumen_table)
    story.append(Spacer(1, 0.7*cm))

    # ─── Tabla de ciclos ─────────────────────────────────────
    story.append(Paragraph("Detalle de ciclos", section_style))

    table_data = [["Fecha", "Humedad %", "Temp. °C", "¿Regó?", "Motivo", "Batería %"]]

    for cycle in cycles:
        reading = cycle.reading
        irrigation = cycle.irrigation
        power = cycle.power

        battery = None
        if power:
            battery = power.battery_pct_real if power.battery_pct_real is not None else power.battery_pct_simulated

        table_data.append([
            cycle.recorded_at.strftime('%d/%m %H:%M') if cycle.recorded_at else "",
            Paragraph(f"{reading.soil_moisture:.1f}" if reading and reading.soil_moisture is not None else "N/D", cell_style),
            Paragraph(f"{reading.soil_temp:.1f}" if reading and reading.soil_temp is not None else "N/D", cell_style),
            "Sí" if irrigation and irrigation.irrigated else "No",
            Paragraph(irrigation.reason if irrigation and irrigation.reason else "-", cell_style),
            Paragraph(f"{battery:.1f}" if battery is not None else "N/D", cell_style),
        ])

    cycles_table = Table(table_data, colWidths=[2.8*cm, 2.3*cm, 2.3*cm, 1.8*cm, 5.3*cm, 2.3*cm], repeatRows=1)

    table_style_cmds = [
        ('BACKGROUND', (0, 0), (-1, 0), colors.HexColor('#2d6a4f')),
        ('TEXTCOLOR', (0, 0), (-1, 0), colors.white),
        ('FONTNAME', (0, 0), (-1, 0), 'Helvetica-Bold'),
        ('FONTSIZE', (0, 0), (-1, -1), 8),
        ('GRID', (0, 0), (-1, -1), 0.5, colors.HexColor('#e2e8f0')),
        ('VALIGN', (0, 0), (-1, -1), 'MIDDLE'),
        ('ROWBACKGROUNDS', (0, 1), (-1, -1), [colors.white, colors.HexColor('#f7fafc')]),
    ]

    # Colorear la columna "¿Regó?" según su valor
    for i, cycle in enumerate(cycles, start=1):
        if cycle.irrigation and cycle.irrigation.irrigated:
            table_style_cmds.append(('TEXTCOLOR', (3, i), (3, i), colors.HexColor('#2d6a4f')))
            table_style_cmds.append(('FONTNAME', (3, i), (3, i), 'Helvetica-Bold'))
        # Resaltar batería crítica
        power = cycle.power
        battery = None
        if power:
            battery = power.battery_pct_real if power.battery_pct_real is not None else power.battery_pct_simulated
        if battery is not None and battery <= 20.0:
            table_style_cmds.append(('TEXTCOLOR', (5, i), (5, i), colors.HexColor('#c0392b')))

    cycles_table.setStyle(TableStyle(table_style_cmds))
    story.append(cycles_table)

    doc.build(story)
    buffer.seek(0)

    return StreamingResponse(
        buffer,
        media_type="application/pdf",
        headers={"Content-Disposition": "attachment; filename=informe_riego.pdf"}
    )

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