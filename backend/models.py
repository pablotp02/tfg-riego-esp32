from sqlalchemy import Column, Integer, Float, Boolean, String, Text, DateTime, SmallInteger, ForeignKey, Index, CheckConstraint, UniqueConstraint
from sqlalchemy.orm import relationship
from sqlalchemy.sql import func
from database import Base

class SensorReading(Base):
    __tablename__ = "sensor_readings"

    id          = Column(Integer, primary_key=True, index=True)
    recorded_at = Column(DateTime, server_default=func.now())

    soil_moisture = Column(Float)
    soil_temp     = Column(Float)
    ph            = Column(Float)
    ec            = Column(Float)

    read_ok   = Column(Boolean, nullable=False)
    validated = Column(Boolean, nullable=False)

    # Relaciones
    errors = relationship("SystemError", back_populates="reading")
    cycle  = relationship("SystemCycle", back_populates="reading")


class IrrigationEvent(Base):
    __tablename__ = "irrigation_events"
    __table_args__ = (
        CheckConstraint(
            "(irrigated = true) = (duration_ms IS NOT NULL)",
            name="irrigated_matches_duration"
        ),
    )

    id          = Column(Integer, primary_key=True, index=True)
    recorded_at = Column(DateTime, server_default=func.now())

    irrigated   = Column(Boolean, nullable=False)
    duration_ms = Column(Integer)
    reason      = Column(Text)

    # Relaciones
    cycle = relationship("SystemCycle", back_populates="irrigation")


class PowerState(Base):
    __tablename__ = "power_states"
    __table_args__ = (
        CheckConstraint("power_mode IN (0, 1, 2)", name="power_mode_valid_values"),
        CheckConstraint(
            "battery_pct_simulated >= 0 AND battery_pct_simulated <= 100",
            name="battery_pct_simulated_range"
        ),
        CheckConstraint(
            "battery_pct_real IS NULL OR (battery_pct_real >= 0 AND battery_pct_real <= 100)",
            name="battery_pct_real_range"
        ),
    )

    id          = Column(Integer, primary_key=True, index=True)
    recorded_at = Column(DateTime, server_default=func.now())

    battery_pct_simulated = Column(Float, nullable=False)
    battery_pct_real      = Column(Float)  # NULL hasta integrar hardware real

    power_mode   = Column(SmallInteger, nullable=False)  # 0: NORMAL | 1: LOW | 2: CRITICAL
    sleep_ms     = Column(Integer)
    wakeup_cause = Column(String(32))

    # Relaciones
    cycle = relationship("SystemCycle", back_populates="power")


class SystemError(Base):
    __tablename__ = "system_errors"

    id          = Column(Integer, primary_key=True, index=True)
    recorded_at = Column(DateTime, server_default=func.now())

    error_type = Column(String(32), nullable=False)
    reason     = Column(Text)

    reading_id = Column(Integer, ForeignKey("sensor_readings.id"), unique=True)

    # Relaciones
    reading = relationship("SensorReading", back_populates="errors")


class SystemCycle(Base):
    __tablename__ = "system_cycles"

    id          = Column(Integer, primary_key=True, index=True)
    recorded_at = Column(DateTime, server_default=func.now())

    cycle_number = Column(Integer, nullable=False)

    reading_id    = Column(Integer, ForeignKey("sensor_readings.id"), nullable=False, unique=True)
    irrigation_id = Column(Integer, ForeignKey("irrigation_events.id"), nullable=False, unique=True)
    power_id      = Column(Integer, ForeignKey("power_states.id"), nullable=False, unique=True)

    cooldown_current  = Column(SmallInteger, CheckConstraint("cooldown_current >= 0"))
    cooldown_required = Column(SmallInteger, CheckConstraint("cooldown_required >= 0"))

    # Relaciones
    reading    = relationship("SensorReading", back_populates="cycle")
    irrigation = relationship("IrrigationEvent", back_populates="cycle")
    power      = relationship("PowerState", back_populates="cycle")
    alerts     = relationship("Alert", back_populates="cycle")


class Alert(Base):
    __tablename__ = "alerts"
    __table_args__ = (
        CheckConstraint(
            "alert_type IN ('BATTERY_LOW', 'BATTERY_CRITICAL', 'SENSOR_ERROR_REPEATED', "
            "'SOIL_TEMP_LOW', 'SOIL_FREEZE_RISK', 'DEVICE_DISCONNECTED')",
            name="alert_type_valid_values"
        ),
    )

    id          = Column(Integer, primary_key=True, index=True)
    recorded_at = Column(DateTime, server_default=func.now())

    alert_type = Column(String(32), nullable=False)
    message    = Column(Text)
    sent       = Column(Boolean, nullable=False, default=False)

    cycle_id = Column(Integer, ForeignKey("system_cycles.id"))

    # Relaciones
    cycle      = relationship("SystemCycle", back_populates="alerts")
    deliveries = relationship("NotificationDelivery", back_populates="alert")

class DeviceConfig(Base):
    __tablename__ = "device_config"
    __table_args__ = (
        CheckConstraint(
            "soil_start_irrigation_pct < soil_stop_irrigation_pct",
            name="device_config_start_before_stop"
        ),
        CheckConstraint(
            "soil_start_irrigation_pct >= 0 AND soil_start_irrigation_pct <= 100",
            name="device_config_start_range"
        ),
        CheckConstraint(
            "soil_stop_irrigation_pct >= 0 AND soil_stop_irrigation_pct <= 100",
            name="device_config_stop_range"
        ),
        CheckConstraint(
            "irrigation_cooldown_cycles >= 0",
            name="device_config_cooldown_non_negative"
        ),
        CheckConstraint(
            "measure_period_ms > 0",
            name="device_config_measure_period_positive"
        ),
        CheckConstraint(
            "irrigate_time_ms > 0",
            name="device_config_irrigate_time_positive"
        ),
        CheckConstraint(
            "soil_freeze_risk_temp_c < soil_min_temp_c",
            name="device_config_freeze_before_min_temp"
        ),
    )

    id          = Column(Integer, primary_key=True, index=True)
    updated_at  = Column(DateTime, server_default=func.now(), onupdate=func.now())

    soil_start_irrigation_pct = Column(Float, nullable=False, default=40.0)
    soil_stop_irrigation_pct  = Column(Float, nullable=False, default=50.0)
    soil_min_temp_c           = Column(Float, nullable=False, default=5.0)
    soil_freeze_risk_temp_c   = Column(Float, nullable=False, default=2.0)
    irrigation_cooldown_cycles = Column(Integer, nullable=False, default=3)
    measure_period_ms         = Column(Integer, nullable=False, default=5000)
    irrigate_time_ms          = Column(Integer, nullable=False, default=2000)

    # Relaciones
    sync_logs = relationship("DeviceSyncLog", back_populates="config")


class DeviceSyncLog(Base):
    __tablename__ = "device_sync_log"

    id          = Column(Integer, primary_key=True, index=True)
    synced_at   = Column(DateTime, server_default=func.now())
    config_id   = Column(Integer, ForeignKey("device_config.id"), nullable=False)

    # Relaciones
    config = relationship("DeviceConfig", back_populates="sync_logs")

class Plant(Base):
    __tablename__ = "plants"
    __table_args__ = (
        Index(
            "only_one_active_plant",    # nombre del índice
            "is_active",                # columna sobre la que aplica
            unique=True,                # debe ser única...
            postgresql_where=(Column("is_active") == True)  # ...pero solo entre las filas donde is_active = True, es decir
        ),                                                  # solo puede haber una True, pero si permite varias False
        CheckConstraint(
            "soil_start_irrigation_pct < soil_stop_irrigation_pct",
            name="plant_start_before_stop"
        ),
        CheckConstraint(
            "soil_start_irrigation_pct >= 0 AND soil_start_irrigation_pct <= 100",
            name="plant_start_range"
        ),
        CheckConstraint(
            "soil_stop_irrigation_pct >= 0 AND soil_stop_irrigation_pct <= 100",
            name="plant_stop_range"
        ),
        CheckConstraint(
            "irrigation_cooldown_cycles >= 0",
            name="plant_cooldown_non_negative"
        ),
        CheckConstraint(
            "measure_period_ms > 0",
            name="plant_measure_period_positive"
        ),
        CheckConstraint(
            "irrigate_time_ms > 0",
            name="plant_irrigate_time_positive"
        ),
        CheckConstraint(
            "soil_freeze_risk_temp_c < soil_min_temp_c",
            name="plant_freeze_before_min_temp"
        ),
    )

    id          = Column(Integer, primary_key=True, index=True)
    created_at  = Column(DateTime, server_default=func.now())

    name        = Column(String(64), nullable=False, unique=True)
    is_active   = Column(Boolean, nullable=False, default=False)

    soil_start_irrigation_pct = Column(Float, nullable=False)
    soil_stop_irrigation_pct  = Column(Float, nullable=False)
    soil_min_temp_c           = Column(Float, nullable=False)
    soil_freeze_risk_temp_c   = Column(Float, nullable=False)
    irrigation_cooldown_cycles = Column(Integer, nullable=False)
    measure_period_ms         = Column(Integer, nullable=False)
    irrigate_time_ms          = Column(Integer, nullable=False)

class NotificationRecipient(Base):
    __tablename__ = "notification_recipients"
    __table_args__ = (
        CheckConstraint(
            "telegram_chat_id IS NOT NULL OR email IS NOT NULL",
            name="recipient_has_contact_method"
        ),
    )

    id      = Column(Integer, primary_key=True, index=True)
    name    = Column(String(64), nullable=False)

    telegram_chat_id = Column(String(64), nullable=True, unique=True)
    email             = Column(String(128), nullable=True, unique=True)

    enabled = Column(Boolean, nullable=False, default=True)


class NotificationDelivery(Base):
    __tablename__ = "notification_deliveries"
    __table_args__ = (
        CheckConstraint("channel IN ('telegram', 'email')", name="delivery_channel_valid_values"),
        UniqueConstraint("alert_id", "channel", name="uq_notification_delivery_alert_channel"),
        CheckConstraint("attempted_count >= 0", name="delivery_attempted_non_negative"),
        CheckConstraint("success_count >= 0", name="delivery_success_non_negative"),
        CheckConstraint("success_count <= attempted_count", name="delivery_success_not_greater_than_attempted"),
        CheckConstraint("sent = (success_count > 0)", name="delivery_sent_matches_success"),
    )

    id              = Column(Integer, primary_key=True, index=True)
    alert_id        = Column(Integer, ForeignKey("alerts.id"), nullable=False)
    channel         = Column(String(16), nullable=False)
    sent            = Column(Boolean, nullable=False, default=False)
    attempted_count = Column(Integer, nullable=False, default=0)
    success_count   = Column(Integer, nullable=False, default=0)
    attempted_at    = Column(DateTime, server_default=func.now())

    # Relaciones
    alert = relationship("Alert", back_populates="deliveries")


class NotificationSettings(Base):
    __tablename__ = "notification_settings"
    __table_args__ = (
        CheckConstraint("id = 1", name="notification_settings_singleton"),
    )

    id = Column(Integer, primary_key=True, default=1)

    channel_telegram_enabled = Column(Boolean, nullable=False, default=False)
    channel_email_enabled    = Column(Boolean, nullable=False, default=False)

