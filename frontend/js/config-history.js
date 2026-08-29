const API_URL = `http://${window.location.hostname}:8000`;
const PAGE_SIZE = 10;

const tableBody   = document.getElementById('config-history-table');
const btnLoadMore = document.getElementById('btn-load-more');
const noMoreHistory = document.getElementById('no-more-history');
const statusBadge = document.getElementById('status-badge');

let currentSkip = 0;
let isFirstPage = true;

function toLocalDate(isoString) {
    return new Date(isoString.endsWith('Z') ? isoString : isoString + 'Z');
}

function formatDate(isoString) {
    return toLocalDate(isoString).toLocaleString('es-ES', {
        day: '2-digit', month: '2-digit', year: 'numeric',
        hour: '2-digit', minute: '2-digit'
    });
}

// Convierte una duración en milisegundos a un texto legible en
// horas y minutos (ej. "2h 30min", "45min", "3h")
function formatDurationHM(ms) {
    const totalMinutes = Math.round(ms / 60000);
    const hours = Math.floor(totalMinutes / 60);
    const minutes = totalMinutes % 60;

    if (hours === 0) return `${minutes}min`;
    if (minutes === 0) return `${hours}h`;
    return `${hours}h ${minutes}min`;
}

// Convierte una duración en milisegundos a un texto legible en
// minutos y segundos (ej. "2min 30s", "45s", "3min")
function formatDurationMS(ms) {
    const totalSeconds = Math.round(ms / 1000);
    const minutes = Math.floor(totalSeconds / 60);
    const seconds = totalSeconds % 60;

    if (minutes === 0) return `${seconds}s`;
    if (seconds === 0) return `${minutes}min`;
    return `${minutes}min ${seconds}s`;
}

function setOnline(online) {
    statusBadge.textContent = online ? 'Online' : 'Sin datos';
    statusBadge.className = 'status-badge ' + (online ? 'online' : 'offline');
}

async function updateOnlineStatus() {
    try {
        const res = await fetch(`${API_URL}/api/alerts/device-status`);
        if (!res.ok) throw new Error('Sin datos');
        const data = await res.json();
        setOnline(data.online);
    } catch {
        setOnline(false);
    }
}

async function loadConfigHistory() {
    try {
        const res = await fetch(`${API_URL}/api/config/history?skip=${currentSkip}&limit=${PAGE_SIZE}`);
        if (!res.ok) throw new Error('Error al cargar el historial');
        const data = await res.json();

        if (isFirstPage && data.length === 0) {
            tableBody.innerHTML = '<tr><td colspan="9">No hay ninguna configuración registrada todavía</td></tr>';
            btnLoadMore.style.display = 'none';
            return;
        }

        if (isFirstPage) {
            tableBody.innerHTML = '';
        }

        const rows = data.map((cfg, index) => {
            // Solo la primera fila de la primera página es la versión "actual"
            const isCurrent = isFirstPage && index === 0;
            const lastSyncText = cfg.last_synced_at
                ? formatDate(cfg.last_synced_at)
                : 'Nunca sincronizada';
            return `
                <tr ${isCurrent ? 'style="background-color: #e8f5ee;"' : ''}>
                    <td style="white-space: nowrap;">${formatDate(cfg.updated_at)} ${isCurrent ? '<span class="badge-si" style="margin-left:8px;">Actual</span>' : ''}</td>
                    <td>${cfg.soil_start_irrigation_pct}%</td>
                    <td>${cfg.soil_stop_irrigation_pct}%</td>
                    <td>${cfg.soil_min_temp_c}ºC</td>
                    <td>${cfg.soil_freeze_risk_temp_c}ºC</td>
                    <td>${cfg.irrigation_cooldown_cycles} ciclos</td>
                    <td>${formatDurationHM(cfg.measure_period_ms)}</td>
                    <td>${formatDurationMS(cfg.irrigate_time_ms)}</td>
                    <td>${lastSyncText}</td>
                </tr>
            `;
        }).join('');

        tableBody.innerHTML += rows;
        isFirstPage = false;
        currentSkip += PAGE_SIZE;

        if (data.length < PAGE_SIZE) {
            btnLoadMore.style.display = 'none';
            noMoreHistory.style.display = 'block';
        }
    } catch (err) {
        tableBody.innerHTML = '<tr><td colspan="9">Error al cargar el historial</td></tr>';
    }
}

btnLoadMore.addEventListener('click', loadConfigHistory);

loadConfigHistory();
updateOnlineStatus();
setInterval(updateOnlineStatus, 10000);