from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy.orm import Session
from sqlalchemy import func
from typing import List
from database import get_db
import models, schemas

router = APIRouter()

def is_plant_synced(plant: models.Plant, db: Session) -> bool | None:
    """
    Compara los parámetros actuales de la planta con la última versión
    de device_config, para determinar si el dispositivo ya tiene
    aplicados estos valores. Solo tiene sentido para la planta activa;
    para el resto devuelve None (no aplica).
    """
    if not plant.is_active:
        return None

    last_config = db.query(models.DeviceConfig)\
                     .order_by(models.DeviceConfig.updated_at.desc())\
                     .first()

    if not last_config:
        return False

    return (
        plant.soil_start_irrigation_pct == last_config.soil_start_irrigation_pct and
        plant.soil_stop_irrigation_pct == last_config.soil_stop_irrigation_pct and
        plant.soil_min_temp_c == last_config.soil_min_temp_c and
        plant.irrigation_cooldown_cycles == last_config.irrigation_cooldown_cycles and
        plant.measure_period_ms == last_config.measure_period_ms and
        plant.irrigate_time_ms == last_config.irrigate_time_ms
    )


@router.get("/", response_model=List[schemas.PlantOut])
def get_plants(db: Session = Depends(get_db)):
    """Devuelve todas las plantas guardadas."""
    plants = db.query(models.Plant).order_by(models.Plant.name).all()
    for plant in plants:
        plant.config_synced = is_plant_synced(plant, db)
    return plants


@router.get("/active", response_model=schemas.PlantOut)
def get_active_plant(db: Session = Depends(get_db)):
    """Devuelve la planta actualmente activa, si hay alguna."""
    plant = db.query(models.Plant).filter(models.Plant.is_active == True).first()
    if not plant:
        raise HTTPException(status_code=404, detail="No hay ninguna planta activa")
    plant.config_synced = is_plant_synced(plant, db)
    return plant


@router.post("/", response_model=schemas.PlantOut)
def create_plant(payload: schemas.PlantCreate, db: Session = Depends(get_db)):
    """Crea una nueva planta con su configuración de riego."""
    existing = db.query(models.Plant)\
                 .filter(func.lower(models.Plant.name) == payload.name.lower())\
                 .first()
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
    plant.config_synced = is_plant_synced(plant, db)
    return plant


@router.delete("/{plant_id}")
def delete_plant(plant_id: int, db: Session = Depends(get_db)):
    """Elimina una planta guardada."""
    plant = db.query(models.Plant).filter(models.Plant.id == plant_id).first()
    if not plant:
        raise HTTPException(status_code=404, detail="Planta no encontrada")

    was_active = plant.is_active

    db.delete(plant)
    db.commit()
    return {
        "status": "ok",
        "message": f"Planta '{plant.name}' eliminada",
        "was_active": was_active
    }


@router.put("/{plant_id}", response_model=schemas.PlantOut)
def update_plant(plant_id: int, payload: schemas.PlantCreate, db: Session = Depends(get_db)):
    """Actualiza los parámetros de una planta existente."""
    plant = db.query(models.Plant).filter(models.Plant.id == plant_id).first()
    if not plant:
        raise HTTPException(status_code=404, detail="Planta no encontrada")

    # Si se cambia el nombre, comprobar que no choque con otra planta
    # (comparación insensible a mayúsculas)
    if payload.name.lower() != plant.name.lower():
        existing = db.query(models.Plant)\
                     .filter(func.lower(models.Plant.name) == payload.name.lower())\
                     .first()
        if existing:
            raise HTTPException(status_code=400, detail="Ya existe una planta con ese nombre")

    for field, value in payload.model_dump().items():
        setattr(plant, field, value)

    db.commit()
    db.refresh(plant)
    plant.config_synced = is_plant_synced(plant, db)
    return plant