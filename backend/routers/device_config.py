from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy.orm import Session
from database import get_db
import models, schemas

router = APIRouter()

@router.get("/latest", response_model=schemas.DeviceConfigOut)
def get_latest_config(db: Session = Depends(get_db)):
    """
    Devuelve la configuración activa más reciente.
    Cada llamada registra un log de sincronización, ya que este endpoint
    lo consulta la ESP32 al arrancar cada ciclo.
    """
    config = db.query(models.DeviceConfig)\
               .order_by(models.DeviceConfig.updated_at.desc())\
               .first()

    if not config:
        raise HTTPException(status_code=404, detail="No hay configuración disponible")

    # Registrar que la ESP32 (o quien sea) ha pedido la configuración
    sync_log = models.DeviceSyncLog()
    db.add(sync_log)
    db.commit()

    return config


@router.put("/", response_model=schemas.DeviceConfigOut)
def update_config(payload: schemas.DeviceConfigUpdate, db: Session = Depends(get_db)):
    """
    Crea una nueva versión de la configuración (no se sobreescribe la anterior,
    se inserta una nueva fila con el timestamp actualizado). Esto permite
    llevar un histórico de cambios de configuración.
    """
    new_config = models.DeviceConfig(**payload.model_dump())
    db.add(new_config)
    db.commit()
    db.refresh(new_config)
    return new_config


@router.get("/sync-status")
def get_sync_status(db: Session = Depends(get_db)):
    """
    Devuelve la fecha de la última vez que la ESP32 pidió configuración,
    y si la configuración activa ya fue sincronizada (es decir, si la
    última sincronización es posterior a la última modificación de config).
    """
    last_sync = db.query(models.DeviceSyncLog)\
                   .order_by(models.DeviceSyncLog.synced_at.desc())\
                   .first()

    last_config = db.query(models.DeviceConfig)\
                     .order_by(models.DeviceConfig.updated_at.desc())\
                     .first()

    if not last_config:
        raise HTTPException(status_code=404, detail="No hay configuración disponible")

    synced = last_sync is not None and last_sync.synced_at >= last_config.updated_at

    return {
        "last_sync_at": last_sync.synced_at if last_sync else None,
        "last_config_updated_at": last_config.updated_at,
        "is_synced": synced
    }