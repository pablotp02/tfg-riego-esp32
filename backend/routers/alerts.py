from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy.orm import Session
from sqlalchemy import desc
from datetime import datetime, timedelta
from typing import List
from database import get_db
import models, schemas
from telegram_utils import send_telegram_message
from routers.notifications import notify_alert

router = APIRouter()

# Factor multiplicador sobre measure_period_ms para considerar el
# dispositivo desconectado. Por ejemplo con factor 3, si el periodo
# configurado es de 5000ms, se generará la alerta si pasan más de 15s
# sin recibir un nuevo ciclo.
DISCONNECT_THRESHOLD_FACTOR = 3

# Tiempo mínimo entre alertas de desconexión repetidas, para no
# generar una alerta nueva cada vez que se consulta este endpoint
DISCONNECT_ALERT_COOLDOWN_MINUTES = 15


def get_connection_state(db: Session):
    """
    Calcula si el dispositivo se considera 'conectado' según el tiempo
    transcurrido desde el último ciclo recibido, comparado con
    measure_period_ms configurado. No genera ninguna alerta, solo
    devuelve el estado calculado para que lo reutilicen otras funciones.
    """
    last_cycle = db.query(models.SystemCycle)\
                    .order_by(desc(models.SystemCycle.recorded_at))\
                    .first()

    if not last_cycle:
        # Nunca ha llegado ningún ciclo todavía
        return {
            "online": False,
            "last_cycle_at": None,
            "seconds_since_last_cycle": None
        }

    last_config = db.query(models.DeviceConfig)\
                     .order_by(desc(models.DeviceConfig.updated_at))\
                     .first()

    expected_period_ms = last_config.measure_period_ms if last_config else 5000
    threshold = timedelta(milliseconds=expected_period_ms * DISCONNECT_THRESHOLD_FACTOR)

    time_since_last_cycle = datetime.utcnow() - last_cycle.recorded_at
    online = time_since_last_cycle <= threshold

    return {
        "online": online,
        "last_cycle_at": last_cycle.recorded_at,
        "seconds_since_last_cycle": int(time_since_last_cycle.total_seconds())
    }


def check_device_disconnected(db: Session):
    """
    Si el dispositivo está desconectado (según get_connection_state) y no
    hay ya una alerta reciente del mismo tipo, genera una nueva alerta
    de desconexión.
    """
    state = get_connection_state(db)

    if state["online"] or state["last_cycle_at"] is None:
        return

    recent_alert = db.query(models.Alert)\
                      .filter(models.Alert.alert_type == "DEVICE_DISCONNECTED")\
                      .order_by(desc(models.Alert.recorded_at))\
                      .first()

    if recent_alert:
        time_since_last_alert = datetime.utcnow() - recent_alert.recorded_at
        if time_since_last_alert < timedelta(minutes=DISCONNECT_ALERT_COOLDOWN_MINUTES):
            return

    alert_message = f"Sin comunicación con la ESP32 desde hace {state['seconds_since_last_cycle'] // 60} minutos"

    new_alert = models.Alert(
        alert_type="DEVICE_DISCONNECTED",
        message=alert_message,
        sent=False,
        cycle_id=None
    )
    db.add(new_alert)
    db.flush()

    sent_ok = notify_alert(db, new_alert.id, f"🔌 DEVICE_DISCONNECTED\n{alert_message}")
    new_alert.sent = sent_ok

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

@router.get("/device-status")
def get_device_status(db: Session = Depends(get_db)):
    """
    Devuelve si el dispositivo está actualmente 'online', calculado
    a partir del tiempo transcurrido desde el último ciclo recibido.
    Pensado para que el frontend lo consulte y refleje el estado real
    de conectividad de la ESP32, no solo si el backend responde.
    """
    return get_connection_state(db)

@router.patch("/{alert_id}/sent", response_model=schemas.AlertOut)
def mark_alert_sent(alert_id: int, db: Session = Depends(get_db)):
    """Marca una alerta como gestionada."""
    alert = db.query(models.Alert).filter(models.Alert.id == alert_id).first()
    if not alert:
        raise HTTPException(status_code=404, detail="Alerta no encontrada")
    alert.sent = True
    db.commit()
    db.refresh(alert)
    return alert

@router.patch("/{alert_id}/pending", response_model=schemas.AlertOut)
def mark_alert_pending(alert_id: int, db: Session = Depends(get_db)):
    """Revierte una alerta ya gestionada, devolviéndola a sin gestionar."""
    alert = db.query(models.Alert).filter(models.Alert.id == alert_id).first()
    if not alert:
        raise HTTPException(status_code=404, detail="Alerta no encontrada")
    alert.sent = False
    db.commit()
    db.refresh(alert)
    return alert