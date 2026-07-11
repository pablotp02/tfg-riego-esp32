from sqlalchemy import Column, Integer, Float, Boolean, String, Text, DateTime, SmallInteger, ForeignKey
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
    irrigation_event = relationship("IrrigationEvent", back_populates="reading")
    errors           = relationship("SystemError", back_populates="reading")
    cycle            = relationship("SystemCycle", back_populates="reading")


class IrrigationEvent(Base):
    __tablename__ = "irrigation_events"

    id          = Column(Integer, primary_key=True, index=True)
    recorded_at = Column(DateTime, server_default=func.now())

    irrigated   = Column(Boolean, nullable=False)
    duration_ms = Column(Integer)
    reason      = Column(Text)

    reading_id = Column(Integer, ForeignKey("sensor_readings.id"))

    # Relaciones
    reading = relationship("SensorReading", back_populates="irrigation_event")
    cycle   = relationship("SystemCycle", back_populates="irrigation")


class PowerState(Base):
    __tablename__ = "power_states"

    id          = Column(Integer, primary_key=True, index=True)
    recorded_at = Column(DateTime, server_default=func.now())

    battery_pct_simulated = Column(Float, nullable=False)
    battery_pct_real      = Column(Float)  # NULL hasta integrar hardware real

    power_mode   = Column(SmallInteger, nullable=False)  # 0: NORMAL | 1: LOW | 2: CRITICAL
    sleep_ms     = Column(Integer)
    wakeup_cause = Column(String(32))

    # Relaciones
    cycle  = relationship("SystemCycle", back_populates="power")
    alerts = relationship("Alert", back_populates="power")


class SystemError(Base):
    __tablename__ = "system_errors"

    id          = Column(Integer, primary_key=True, index=True)
    recorded_at = Column(DateTime, server_default=func.now())

    error_type = Column(String(32), nullable=False)
    reason     = Column(Text)

    reading_id = Column(Integer, ForeignKey("sensor_readings.id"))

    # Relaciones
    reading = relationship("SensorReading", back_populates="errors")


class SystemCycle(Base):
    __tablename__ = "system_cycles"

    id          = Column(Integer, primary_key=True, index=True)
    recorded_at = Column(DateTime, server_default=func.now())

    cycle_number = Column(Integer, nullable=False)

    reading_id    = Column(Integer, ForeignKey("sensor_readings.id"))
    irrigation_id = Column(Integer, ForeignKey("irrigation_events.id"))
    power_id      = Column(Integer, ForeignKey("power_states.id"))

    cooldown_current  = Column(SmallInteger)
    cooldown_required = Column(SmallInteger)

    # Relaciones
    reading    = relationship("SensorReading", back_populates="cycle")
    irrigation = relationship("IrrigationEvent", back_populates="cycle")
    power      = relationship("PowerState", back_populates="cycle")


class Alert(Base):
    __tablename__ = "alerts"

    id          = Column(Integer, primary_key=True, index=True)
    recorded_at = Column(DateTime, server_default=func.now())

    alert_type = Column(String(32), nullable=False)
    message    = Column(Text)
    sent       = Column(Boolean, nullable=False, default=False)
    channel    = Column(String(16))

    power_id = Column(Integer, ForeignKey("power_states.id"))

    # Relaciones
    power = relationship("PowerState", back_populates="alerts")