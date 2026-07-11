from fastapi import APIRouter, Depends
from sqlalchemy.orm import Session
from typing import List
from database import get_db
import models, schemas

router = APIRouter()

@router.get("/", response_model=List[schemas.SensorReadingOut])
def get_sensor_readings(skip: int = 0, limit: int = 50, db: Session = Depends(get_db)):
    """Devuelve las últimas lecturas del sensor para el dashboard."""
    return db.query(models.SensorReading)\
             .order_by(models.SensorReading.recorded_at.desc())\
             .offset(skip).limit(limit).all()

@router.get("/latest", response_model=schemas.SensorReadingOut)
def get_latest_reading(db: Session = Depends(get_db)):
    """Devuelve la última lectura del sensor — para el estado actual del sistema."""
    reading = db.query(models.SensorReading)\
                .order_by(models.SensorReading.recorded_at.desc())\
                .first()
    if not reading:
        from fastapi import HTTPException
        raise HTTPException(status_code=404, detail="No hay lecturas disponibles")
    return reading