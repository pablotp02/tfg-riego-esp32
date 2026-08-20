from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy.orm import Session
from sqlalchemy.exc import IntegrityError
from telegram_utils import send_telegram_message
from email_utils import send_email
from typing import List
from database import get_db
import models, schemas

router = APIRouter()


def get_or_create_settings(db: Session) -> models.NotificationSettings:
    """
    Implementa el patrón Singleton a nivel de base de datos: garantiza
    que siempre exista exactamente una fila de configuración de
    notificaciones. Si no existe ninguna todavía (primer arranque del
    sistema), la crea con los valores por defecto del modelo.
    """
    settings = db.query(models.NotificationSettings).filter(models.NotificationSettings.id == 1).first()
    if not settings:
        settings = models.NotificationSettings(id=1)
        db.add(settings)
        db.commit()
        db.refresh(settings)
    return settings

def notify_alert(db: Session, alert_id: int, message: str, subject: str = "Alerta - Sistema de Riego Automático") -> bool:
    """
    Envía una notificación a todos los destinatarios activos, según
    los canales habilitados en los ajustes globales. Centraliza la
    lógica de envío para no repetirla en cada punto donde se genera
    una alerta (batería baja, desconexión, error de sensor, etc.).

    Por cada canal activo, registra en NotificationDelivery el
    resultado del envío: si tuvo éxito con al menos un destinatario,
    y cuántos destinatarios se intentaron frente a cuántos tuvieron
    éxito, sin identificar a ningún destinatario en concreto.

    Devuelve True si el mensaje se envió correctamente a al menos un
    destinatario, por cualquiera de los canales; False si no había
    ningún destinatario disponible o todos los envíos fallaron.
    """
    settings = get_or_create_settings(db)

    any_success = False

    if settings.channel_telegram_enabled:
        telegram_recipients = db.query(models.NotificationRecipient)\
            .filter(
                models.NotificationRecipient.enabled == True,
                models.NotificationRecipient.telegram_chat_id.isnot(None)
            ).all()

        attempted = len(telegram_recipients)
        successes = 0
        for recipient in telegram_recipients:
            success = send_telegram_message(message, chat_id=recipient.telegram_chat_id)
            if success:
                successes += 1
                any_success = True

        if attempted > 0:
            delivery = models.NotificationDelivery(
                alert_id=alert_id,
                channel="telegram",
                sent=successes > 0,
                attempted_count=attempted,
                success_count=successes
            )
            db.add(delivery)

    if settings.channel_email_enabled:
        email_recipients = db.query(models.NotificationRecipient)\
            .filter(
                models.NotificationRecipient.enabled == True,
                models.NotificationRecipient.email.isnot(None)
            ).all()

        attempted = len(email_recipients)
        successes = 0
        for recipient in email_recipients:
            success = send_email(subject=subject, body=message, to_address=recipient.email)
            if success:
                successes += 1
                any_success = True

        if attempted > 0:
            delivery = models.NotificationDelivery(
                alert_id=alert_id,
                channel="email",
                sent=successes > 0,
                attempted_count=attempted,
                success_count=successes
            )
            db.add(delivery)

    db.flush()

    return any_success


# ─── Ajustes globales de notificación ──────────────────────────────────────

@router.get("/settings", response_model=schemas.NotificationSettingsOut)
def get_notification_settings(db: Session = Depends(get_db)):
    """Devuelve los ajustes globales de notificación (patrón singleton)."""
    return get_or_create_settings(db)


@router.put("/settings", response_model=schemas.NotificationSettingsOut)
def update_notification_settings(payload: schemas.NotificationSettingsUpdate, db: Session = Depends(get_db)):
    """Actualiza los ajustes globales de notificación."""
    settings = get_or_create_settings(db)
    settings.channel_telegram_enabled = payload.channel_telegram_enabled
    settings.channel_email_enabled = payload.channel_email_enabled
    db.commit()
    db.refresh(settings)
    return settings


# ─── Destinatarios ──────────────────────────────────────────────────────────

@router.get("/recipients", response_model=List[schemas.NotificationRecipientOut])
def get_recipients(db: Session = Depends(get_db)):
    """Devuelve todos los destinatarios de notificaciones registrados."""
    return db.query(models.NotificationRecipient).order_by(models.NotificationRecipient.name).all()


@router.post("/recipients", response_model=schemas.NotificationRecipientOut)
def create_recipient(payload: schemas.NotificationRecipientCreate, db: Session = Depends(get_db)):
    """
    Crea un nuevo destinatario. Debe indicarse al menos un canal
    (Telegram o email); no tiene sentido un destinatario sin ninguna
    forma de contacto.
    """
    if not payload.telegram_chat_id and not payload.email:
        raise HTTPException(status_code=400, detail="Debe indicarse al menos un chat_id de Telegram o un email")

    new_recipient = models.NotificationRecipient(**payload.model_dump())
    db.add(new_recipient)
    try:
        db.commit()
    except IntegrityError:
        db.rollback()
        raise HTTPException(
            status_code=400,
            detail="Ya existe un destinatario registrado con ese chat ID de Telegram o ese email"
        )
    db.refresh(new_recipient)
    return new_recipient


@router.put("/recipients/{recipient_id}", response_model=schemas.NotificationRecipientOut)
def update_recipient(recipient_id: int, payload: schemas.NotificationRecipientCreate, db: Session = Depends(get_db)):
    """Actualiza los datos de un destinatario existente."""
    recipient = db.query(models.NotificationRecipient).filter(models.NotificationRecipient.id == recipient_id).first()
    if not recipient:
        raise HTTPException(status_code=404, detail="Destinatario no encontrado")

    if not payload.telegram_chat_id and not payload.email:
        raise HTTPException(status_code=400, detail="Debe indicarse al menos un chat_id de Telegram o un email")

    for field, value in payload.model_dump().items():
        setattr(recipient, field, value)

    try:
        db.commit()
    except IntegrityError:
        db.rollback()
        raise HTTPException(
            status_code=400,
            detail="Ya existe un destinatario registrado con ese chat ID de Telegram o ese email"
        )
    db.refresh(recipient)
    return recipient


@router.delete("/recipients/{recipient_id}")
def delete_recipient(recipient_id: int, db: Session = Depends(get_db)):
    """Elimina un destinatario."""
    recipient = db.query(models.NotificationRecipient).filter(models.NotificationRecipient.id == recipient_id).first()
    if not recipient:
        raise HTTPException(status_code=404, detail="Destinatario no encontrado")

    db.delete(recipient)
    db.commit()
    return {"status": "ok", "message": f"Destinatario '{recipient.name}' eliminado"}