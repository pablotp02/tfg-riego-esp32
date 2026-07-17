from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy.orm import Session
from typing import List
from database import get_db
import models, schemas

router = APIRouter()

@router.get("/latest", response_model=schemas.PowerStateOut)
def get_latest_power_state(db: Session = Depends(get_db)):
    """Devuelve el último estado energético registrado."""
    power = db.query(models.PowerState)\
              .order_by(models.PowerState.recorded_at.desc())\
              .first()
    if not power:
        raise HTTPException(status_code=404, detail="No hay datos energéticos disponibles")
    return power

@router.get("/", response_model=List[schemas.PowerStateOut])
def get_power_states(skip: int = 0, limit: int = 50, db: Session = Depends(get_db)):
    """Devuelve el historial de estados energéticos."""
    return db.query(models.PowerState)\
             .order_by(models.PowerState.recorded_at.desc())\
             .offset(skip).limit(limit).all()