from fastapi import APIRouter, Depends
from sqlalchemy.orm import Session
from typing import List
from database import get_db
import models, schemas

router = APIRouter()

@router.get("/", response_model=List[schemas.IrrigationEventOut])
def get_irrigation_events(skip: int = 0, limit: int = 50, db: Session = Depends(get_db)):
    """Devuelve el historial de eventos de riego."""
    return db.query(models.IrrigationEvent)\
             .order_by(models.IrrigationEvent.recorded_at.desc())\
             .offset(skip).limit(limit).all()

@router.get("/active", response_model=List[schemas.IrrigationEventOut])
def get_active_irrigations(db: Session = Depends(get_db)):
    """Devuelve solo los eventos en los que realmente se regó."""
    return db.query(models.IrrigationEvent)\
             .filter(models.IrrigationEvent.irrigated == True)\
             .order_by(models.IrrigationEvent.recorded_at.desc())\
             .all()