from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from database import engine, Base

# Importar routers
from routers import cycles, sensors, irrigation, alerts, power

# Crear tablas en la base de datos
Base.metadata.create_all(bind=engine)

app = FastAPI(
    title="TFG Riego API",
    description="API REST para el sistema de riego automático con ESP32",
    version="1.0.0"
)

# Configurar CORS para que el frontend pueda comunicarse con el backend
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Registrar routers
app.include_router(cycles.router, prefix="/api/cycles", tags=["cycles"])
app.include_router(sensors.router, prefix="/api/sensors", tags=["sensors"])
app.include_router(irrigation.router, prefix="/api/irrigation", tags=["irrigation"])
app.include_router(alerts.router, prefix="/api/alerts", tags=["alerts"])
app.include_router(power.router, prefix="/api/power", tags=["power"])

@app.get("/")
def root():
    return {"status": "ok", "message": "TFG Riego API funcionando"}