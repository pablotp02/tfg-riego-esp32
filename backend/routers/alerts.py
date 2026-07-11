from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy.orm import Session
from typing import List
from database import get_db
import models, schemas

router = APIRouter()

@router.get("/", response_model=List[schemas.AlertOut])
def get_alerts(skip: int = 0, limit: int = 50, db: Session = Depends(get_db)):
    """Devuelve el historial de alertas."""
    return db.query(models.Alert)\
             .order_by(models.Alert.recorded_at.desc())\
             .offset(skip).limit(limit).all()

@router.get("/pending", response_model=List[schemas.AlertOut])
def get_pending_alerts(db: Session = Depends(get_db)):
    """Devuelve las alertas pendientes de envío."""
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