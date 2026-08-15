from pydantic import BaseModel
from datetime import datetime
from typing import Optional

# ─── Sensor Readings ───────────────────────────────────────────────────────────

class SensorReadingCreate(BaseModel):
    soil_moisture: Optional[float] = None
    soil_temp:     Optional[float] = None
    ph:            Optional[float] = None
    ec:            Optional[float] = None
    read_ok:       bool
    validated:     bool

class SensorReadingOut(SensorReadingCreate):
    id:          int
    recorded_at: datetime

    class Config:
        from_attributes = True

# ─── Irrigation Events ─────────────────────────────────────────────────────────

class IrrigationEventCreate(BaseModel):
    irrigated:   bool
    duration_ms: Optional[int]   = None
    reason:      Optional[str]   = None
    reading_id:  Optional[int]   = None

class IrrigationEventOut(IrrigationEventCreate):
    id:          int
    recorded_at: datetime
    battery_pct: Optional[float] = None

    class Config:
        from_attributes = True

# ─── Power States ──────────────────────────────────────────────────────────────

class PowerStateCreate(BaseModel):
    battery_pct_simulated: float
    battery_pct_real:      Optional[float] = None
    power_mode:            int
    sleep_ms:              Optional[int]   = None
    wakeup_cause:          Optional[str]   = None

class PowerStateOut(PowerStateCreate):
    id:          int
    recorded_at: datetime

    class Config:
        from_attributes = True

# ─── System Errors ─────────────────────────────────────────────────────────────

class SystemErrorCreate(BaseModel):
    error_type: str
    reason:     Optional[str] = None
    reading_id: Optional[int] = None

class SystemErrorOut(SystemErrorCreate):
    id:          int
    recorded_at: datetime

    class Config:
        from_attributes = True

# ─── System Cycles ─────────────────────────────────────────────────────────────

class SystemCycleCreate(BaseModel):
    cycle_number:      int
    reading_id:        Optional[int] = None
    irrigation_id:     Optional[int] = None
    power_id:          Optional[int] = None
    cooldown_current:  Optional[int] = None
    cooldown_required: Optional[int] = None

class SystemCycleOut(SystemCycleCreate):
    id:          int
    recorded_at: datetime

    class Config:
        from_attributes = True

# ─── Alerts ────────────────────────────────────────────────────────────────────

class AlertCreate(BaseModel):
    alert_type: str
    message:    Optional[str] = None
    sent:       bool          = False
    channel:    Optional[str] = None
    power_id:   Optional[int] = None

class AlertOut(AlertCreate):
    id:          int
    recorded_at: datetime

    class Config:
        from_attributes = True

# ─── Payload completo del firmware ─────────────────────────────────────────────
# Este es el JSON que enviará la ESP32 en cada ciclo

class CyclePayload(BaseModel):
    cycle_number:      int
    soil_moisture:     Optional[float] = None
    soil_temp:         Optional[float] = None
    ph:                Optional[float] = None
    ec:                Optional[float] = None
    read_ok:           bool
    validated:         bool
    irrigated:         bool
    duration_ms:       Optional[int]   = None
    irrigation_reason: Optional[str]   = None
    battery_pct:       float
    power_mode:        int
    sleep_ms:          Optional[int]   = None
    wakeup_cause:      Optional[str]   = None
    cooldown_current:  Optional[int]   = None
    cooldown_required: Optional[int]   = None
    sensor_error:      Optional[str]   = None

# ─── Device Config ─────────────────────────────────────────────────────────

class DeviceConfigUpdate(BaseModel):
    soil_start_irrigation_pct: float
    soil_stop_irrigation_pct:  float
    soil_min_temp_c:           float
    soil_freeze_risk_temp_c:   float
    irrigation_cooldown_cycles: int
    measure_period_ms:         int
    irrigate_time_ms:          int

class DeviceConfigOut(DeviceConfigUpdate):
    id:         int
    updated_at: datetime

    class Config:
        from_attributes = True

# ─── Device Sync Log ────────────────────────────────────────────────────────

class DeviceSyncLogOut(BaseModel):
    id:        int
    synced_at: datetime

    class Config:
        from_attributes = True

# ─── Plants ─────────────────────────────────────────────────────────────────

class PlantCreate(BaseModel):
    name: str
    soil_start_irrigation_pct: float
    soil_stop_irrigation_pct:  float
    soil_min_temp_c:           float
    soil_freeze_risk_temp_c:   float
    irrigation_cooldown_cycles: int
    measure_period_ms:         int
    irrigate_time_ms:          int

class PlantOut(PlantCreate):
    id:             int
    is_active:      bool
    created_at:     datetime
    config_synced:  Optional[bool] = None

    class Config:
        from_attributes = True

# ─── Notification Recipients ───────────────────────────────────────────────

class NotificationRecipientCreate(BaseModel):
    name: str
    telegram_chat_id: Optional[str] = None
    email: Optional[str] = None
    enabled: bool = True

class NotificationRecipientOut(NotificationRecipientCreate):
    id: int

    class Config:
        from_attributes = True

# ─── Notification Settings ─────────────────────────────────────────────────

class NotificationSettingsUpdate(BaseModel):
    channel_telegram_enabled: bool
    channel_email_enabled: bool

class NotificationSettingsOut(NotificationSettingsUpdate):
    id: int

    class Config:
        from_attributes = True