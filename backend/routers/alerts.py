from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy.orm import Session
from sqlalchemy import desc
from datetime import datetime, timedelta
from typing import List
from database import get_db
import models, schemas

router = APIRouter()

# Factor multiplicador sobre measure_period_ms para considerar el
# dispositivo desconectado. Por ejemplo con factor 3, si el periodo
# configurado es de 5000ms, se generará la alerta si pasan más de 15s
# sin recibir un nuevo ciclo.
DISCONNECT_THRESHOLD_FACTOR = 3

# Tiempo mínimo entre alertas de desconexión repetidas, para no
# generar una alerta nueva cada vez que se consulta este endpoint
DISCONNECT_ALERT_COOLDOWN_MINUTES = 15


def check_device_disconnected(db: Session):
    """
    Comprueba si ha pasado demasiado tiempo desde el último ciclo
    recibido de la ESP32, según el measure_period_ms configurado.
    Si es así y no hay ya una alerta reciente del mismo tipo, genera
    una nueva alerta de desconexión.
    """
    last_cycle = db.query(models.SystemCycle)\
                    .order_by(desc(models.SystemCycle.recorded_at))\
                    .first()

    if not last_cycle:
        # Nunca ha llegado ningún ciclo, no hay nada que comparar todavía
        return

    last_config = db.query(models.DeviceConfig)\
                     .order_by(desc(models.DeviceConfig.updated_at))\
                     .first()

    # Si no hay configuración, usamos un valor por defecto conservador
    expected_period_ms = last_config.measure_period_ms if last_config else 5000
    threshold = timedelta(milliseconds=expected_period_ms * DISCONNECT_THRESHOLD_FACTOR)

    time_since_last_cycle = datetime.utcnow() - last_cycle.recorded_at

    if time_since_last_cycle <= threshold:
        # Todo normal, el dispositivo sigue comunicando a tiempo
        return

    # Comprobar si ya existe una alerta de desconexión reciente para
    # no generar una nueva en cada consulta
    recent_alert = db.query(models.Alert)\
                      .filter(models.Alert.alert_type == "DEVICE_DISCONNECTED")\
                      .order_by(desc(models.Alert.recorded_at))\
                      .first()

    if recent_alert:
        time_since_last_alert = datetime.utcnow() - recent_alert.recorded_at
        if time_since_last_alert < timedelta(minutes=DISCONNECT_ALERT_COOLDOWN_MINUTES):
            return

    new_alert = models.Alert(
        alert_type="DEVICE_DISCONNECTED",
        message=f"Sin comunicación con la ESP32 desde hace {int(time_since_last_cycle.total_seconds() // 60)} minutos",
        sent=False,
        channel=None,
        power_id=None
    )
    db.add(new_alert)
    db.commit()


@router.get("/", response_model=List[schemas.AlertOut])
def get_alerts(skip: int = 0, limit: int = 50, db: Session = Depends(get_db)):
    """Devuelve el historial de alertas."""
    return db.query(models.Alert)\
             .order_by(models.Alert.recorded_at.desc())\
             .offset(skip).limit(limit).all()


@router.get("/pending", response_model=List[schemas.AlertOut])
def get_pending_alerts(db: Session = Depends(get_db)):
    """
    Devuelve las alertas pendientes de envío. Antes de consultarlas,
    comprueba si el dispositivo lleva demasiado tiempo sin comunicar
    y genera una alerta de desconexión si corresponde.
    """
    check_device_disconnected(db)

    return db.query(models.Alert)\
             .filter(models.Alert.sent == False)\
             .order_by(models.Alert.recorded_at.desc())\
             .all()


@router.patch("/{alert_id}/sent", response_model=schemas.AlertOut)
def mark_alert_sent(alert_id: int, db: Session = Depends(get_db)):
    """Marca una alerta como enviada."""
    alert = db.query(models.Alert).filter(models.Alert.id == alert_id).first()
    if not alert:
        raise HTTPException(status_code=404, detail="Alerta no encontrada")
    alert.sent = True
    db.commit()
    db.refresh(alert)
    return alert