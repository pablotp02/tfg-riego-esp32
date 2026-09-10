# Sistema de Riego Automático con ESP32

Trabajo Fin de Grado — Ingeniería Informática, Universidad de Córdoba
**Autor:** Pablo Tovar Pareja
**Tutor:** Dr. D. Joaquín Olivares Bueno

Sistema de riego automático e inteligente, basado en un dispositivo ESP32 alimentado por batería, que decide de forma autónoma cuándo regar una planta a partir de la monitorización real de las condiciones del suelo (humedad, temperatura, pH y conductividad eléctrica). Incluye una aplicación web para la supervisión, configuración y gestión de alertas del sistema.

## Características principales

- Lectura de humedad, temperatura, pH y conductividad eléctrica del suelo mediante el sensor SEN0604, vía Modbus RTU sobre RS485
- Decisión de riego automática por histéresis, condicionada por la temperatura mínima del suelo y un periodo de *cooldown* configurable
- Gestión energética basada en batería, con estimación real de nivel mediante el módulo INA219 y modos de funcionamiento (normal, bajo, crítico) que ajustan el consumo del sistema
- Sincronización remota de configuración con reintento y persistencia en memoria RTC entre ciclos de *deep sleep*
- Gestión de varias plantas con configuraciones de riego independientes
- Aplicación web con panel de estado en tiempo real, histórico gráfico, gestión de plantas, alertas automáticas y notificaciones por Telegram y correo electrónico
- Exportación del histórico de datos en CSV y PDF

## Arquitectura

El sistema se compone de tres partes:

- **Firmware** (`src/`): dispositivo ESP32, en C sobre ESP-IDF
- **Backend** (`backend/`): API REST en FastAPI, con base de datos PostgreSQL
- **Frontend** (`frontend/`): aplicación web en HTML, CSS y JavaScript

```
ESP32 (firmware) ──HTTP──▶ Backend (FastAPI + PostgreSQL) ◀──HTTP── Frontend (web)
```

## Tecnologías

- **Firmware:** C, ESP-IDF 5.1.2, PlatformIO
- **Backend:** Python 3.12, FastAPI, SQLAlchemy, PostgreSQL
- **Frontend:** HTML5, CSS3, JavaScript, Chart.js
- **Despliegue:** Docker y Docker Compose

## Puesta en marcha

El despliegue completo del sistema (backend, frontend, configuración de notificaciones, montaje del circuito y flasheo del firmware) se describe en detalle en el Manual técnico de este Trabajo Fin de Grado, no incluido en este repositorio.

Como resumen rápido del despliegue software:

```bash
# En la raíz del proyecto
docker compose up -d --build
```

El backend queda disponible en `localhost:8000` (documentación interactiva en `/docs`) y el frontend en `localhost:8080`.

El firmware se compila y se carga sobre el ESP32 mediante PlatformIO, copiando `src/credentials_example.h` como `src/credentials.h` y completando las credenciales de WiFi y la dirección del backend.

## Estructura del repositorio

```
tfg-riego-esp32/
├── src/              # Firmware del ESP32
├── backend/          # API REST (FastAPI + PostgreSQL)
├── frontend/         # Aplicación web
├── docker-compose.yml
└── platformio.ini
```

Para una descripción completa de la organización interna de cada componente, véase el Manual de código fuente de este Trabajo Fin de Grado, no incluido en este repositorio.

## Financiación

Este proyecto se desarrolla en el marco de la Cátedra Internacional ENIA en Inteligencia Artificial y Agricultura — Universidad de Córdoba, financiada por la Secretaría de Estado de Digitalización e Inteligencia Artificial y por la Unión Europea — Next Generation EU. Plan de Recuperación, Transformación y Resiliencia — Financiado por la Unión Europea — NextGenerationEU.