from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy.orm import Session
from typing import List
from database import get_db
import models, schemas

router = APIRouter()


@router.get("/", response_model=List[schemas.PlantOut])
def get_plants(db: Session = Depends(get_db)):
    """Devuelve todas las plantas guardadas."""
    return db.query(models.Plant).order_by(models.Plant.name).all()


@router.get("/active", response_model=schemas.PlantOut)
def get_active_plant(db: Session = Depends(get_db)):
    """Devuelve la planta actualmente activa, si hay alguna."""
    plant = db.query(models.Plant).filter(models.Plant.is_active == True).first()
    if not plant:
        raise HTTPException(status_code=404, detail="No hay ninguna planta activa")
    return plant


@router.post("/", response_model=schemas.PlantOut)
def create_plant(payload: schemas.PlantCreate, db: Session = Depends(get_db)):
    """Crea una nueva planta con su configuración de riego."""
    existing = db.query(models.Plant).filter(models.Plant.name == payload.name).first()
    if existing:
        raise HTTPException(status_code=400, detail="Ya existe una planta con ese nombre")

    new_plant = models.Plant(**payload.model_dump(), is_active=False)
    db.add(new_plant)
    db.commit()
    db.refresh(new_plant)
    return new_plant


@router.post("/{plant_id}/activate", response_model=schemas.PlantOut)
def activate_plant(plant_id: int, db: Session = Depends(get_db)):
    """
    Marca una planta como activa (desmarcando cualquier otra) y crea
    una nueva versión de device_config con sus parámetros, para que
    la ESP32 la reciba en su próxima sincronización.
    """
    plant = db.query(models.Plant).filter(models.Plant.id == plant_id).first()
    if not plant:
        raise HTTPException(status_code=404, detail="Planta no encontrada")

    # Desmarcar cualquier otra planta activa
    db.query(models.Plant).filter(models.Plant.is_active == True)\
        .update({"is_active": False})

    plant.is_active = True

    # Crear nueva configuración activa a partir de los parámetros de la planta
    new_config = models.DeviceConfig(
        soil_start_irrigation_pct=plant.soil_start_irrigation_pct,
        soil_stop_irrigation_pct=plant.soil_stop_irrigation_pct,
        soil_min_temp_c=plant.soil_min_temp_c,
        irrigation_cooldown_cycles=plant.irrigation_cooldown_cycles,
        measure_period_ms=plant.measure_period_ms,
        irrigate_time_ms=plant.irrigate_time_ms,
    )
    db.add(new_config)

    db.commit()
    db.refresh(plant)
    return plant


@router.delete("/{plant_id}")
def delete_plant(plant_id: int, db: Session = Depends(get_db)):
    """Elimina una planta guardada."""
    plant = db.query(models.Plant).filter(models.Plant.id == plant_id).first()
    if not plant:
        raise HTTPException(status_code=404, detail="Planta no encontrada")

    db.delete(plant)
    db.commit()
    return {"status": "ok", "message": f"Planta '{plant.name}' eliminada"}