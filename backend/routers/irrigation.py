from fastapi import APIRouter, Depends
from sqlalchemy.orm import Session
from typing import List
from database import get_db
import models, schemas

router = APIRouter()

@router.get("/", response_model=List[schemas.IrrigationEventOut])
def get_irrigation_events(skip: int = 0, limit: int = 50, db: Session = Depends(get_db)):
    """Devuelve el historial de eventos de riego, incluyendo el nivel
    de batería registrado en el ciclo correspondiente a cada evento."""
    events = db.query(models.IrrigationEvent)\
               .order_by(models.IrrigationEvent.recorded_at.desc())\
               .offset(skip).limit(limit).all()

    for event in events:
        cycle = db.query(models.SystemCycle)\
                   .filter(models.SystemCycle.irrigation_id == event.id)\
                   .first()
        battery = None
        if cycle and cycle.power:
            p = cycle.power
            battery = p.battery_pct_real if p.battery_pct_real is not None else p.battery_pct_simulated
        event.battery_pct = battery

    return events

@router.get("/active", response_model=List[schemas.IrrigationEventOut])
def get_active_irrigations(db: Session = Depends(get_db)):
    """Devuelve solo los eventos en los que realmente se regó."""
    return db.query(models.IrrigationEvent)\
             .filter(models.IrrigationEvent.irrigated == True)\
             .order_by(models.IrrigationEvent.recorded_at.desc())\
             .all()