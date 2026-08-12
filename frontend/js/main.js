// ─── Configuración ───────────────────────────────────────────
const API_URL = 'http://localhost:8000';
const REFRESH_INTERVAL = 10000; // actualizar cada 10 segundos

// ─── Referencias DOM ─────────────────────────────────────────
const statusBadge = document.getElementById('status-badge');
const lastUpdate  = document.getElementById('last-update');

// ─── Gráficas ────────────────────────────────────────────────
const chartConfig = (label, color) => ({
    type: 'line',
    data: {
        labels: [],
        datasets: [{
            label,
            data: [],
            borderColor: color,
            backgroundColor: color + '22',
            borderWidth: 2,
            pointRadius: 3,
            tension: 0.3,
            fill: true
        }]
    },
    options: {
        responsive: true,
        plugins: { legend: { display: false } },
        scales: {
            x: { ticks: { maxTicksLimit: 8, font: { size: 10 } } },
            y: { ticks: { font: { size: 10 } } }
        }
    }
});

const charts = {
    moisture: new Chart(document.getElementById('chart-moisture'), chartConfig('Humedad (%)', '#40916c')),
    temp:     new Chart(document.getElementById('chart-temp'),     chartConfig('Temperatura (ºC)', '#f4a261')),
    ph:       new Chart(document.getElementById('chart-ph'),       chartConfig('pH', '#4361ee')),
    ec:       new Chart(document.getElementById('chart-ec'),       chartConfig('EC (uS/cm)', '#9b5de5'))
};

// ─── Utilidades ──────────────────────────────────────────────
function toLocalDate(isoString) {
    // El backend guarda las fechas en UTC pero sin indicarlo en el texto.
    // Añadimos la "Z" para que JavaScript lo interprete como UTC y lo
    // convierta correctamente a la hora local del usuario.
    return new Date(isoString.endsWith('Z') ? isoString : isoString + 'Z');
}

function formatDate(isoString) {
    const d = toLocalDate(isoString);
    return d.toLocaleString('es-ES', {
        day: '2-digit', month: '2-digit',
        hour: '2-digit', minute: '2-digit'
    });
}

function modeToString(mode) {
    switch (mode) {
        case 0: return 'NORMAL';
        case 1: return 'LOW';
        case 2: return 'CRITICAL';
        default: return '--';
    }
}

function setOnline(online) {
    statusBadge.textContent = online ? 'Online' : 'Sin datos';
    statusBadge.className = 'status-badge ' + (online ? 'online' : 'offline');
}

function updateChart(chart, labels, data) {
    chart.data.labels = labels;
    chart.data.datasets[0].data = data;
    chart.update();
}

// ─── Actualizar tarjetas de estado actual ────────────────────
async function updateCurrentState() {
    try {
        const res = await fetch(`${API_URL}/api/sensors/latest`);
        if (!res.ok) throw new Error('Sin datos');
        const data = await res.json();

        document.getElementById('val-moisture').textContent =
            data.soil_moisture !== null ? data.soil_moisture.toFixed(1) : '--';
        document.getElementById('val-temp').textContent =
            data.soil_temp !== null ? data.soil_temp.toFixed(1) : '--';
        document.getElementById('val-ph').textContent =
            data.ph !== null ? data.ph.toFixed(1) : '--';
        document.getElementById('val-ec').textContent =
            data.ec !== null ? Math.round(data.ec) : '--';

        lastUpdate.textContent = 'Última lectura: ' + formatDate(data.recorded_at);
    } catch (e) {
        console.error(e);
    }
}

// Comprueba la conectividad real de la ESP32,
// usando el endpoint que calcula el estado según el tiempo
// transcurrido desde el último ciclo recibido y measure_period_ms
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

// ─── Actualizar batería y modo desde cycles ───────────────────
async function updatePowerState() {
    try {
        // Consultamos el último estado energético registrado
        // usando el nuevo endpoint /api/power/latest
        const res = await fetch(`${API_URL}/api/power/latest`);
        if (!res.ok) return;
        const data = await res.json();

        // Usamos la batería real si está disponible (INA219),
        // si no usamos la simulada como fallback
        const battery = data.battery_pct_real !== null
            ? data.battery_pct_real
            : data.battery_pct_simulated;
        document.getElementById('val-battery').textContent =
            battery !== null ? battery.toFixed(1) : '--';

        // Mostramos el modo energético con su clase CSS correspondiente
        // 0: NORMAL (verde), 1: LOW (naranja), 2: CRITICAL (rojo)
        const modeEl = document.getElementById('val-mode');
        modeEl.textContent = modeToString(data.power_mode);
        modeEl.className = 'card-value card-mode ' +
            (['normal', 'low', 'critical'][data.power_mode] || 'normal');

    } catch (e) {
        console.error(e);
    }
}

// ─── Actualizar gráficas ─────────────────────────────────────
async function updateCharts() {
    try {
        const res = await fetch(`${API_URL}/api/sensors/?limit=20`);
        if (!res.ok) return;
        const data = await res.json();

        // Los datos vienen en orden descendente, los invertimos para la gráfica
        const reversed = [...data].reverse();

        const labels   = reversed.map(d => formatDate(d.recorded_at));
        const moisture = reversed.map(d => d.soil_moisture);
        const temp     = reversed.map(d => d.soil_temp);
        const ph       = reversed.map(d => d.ph);
        const ec       = reversed.map(d => d.ec);

        updateChart(charts.moisture, labels, moisture);
        updateChart(charts.temp,     labels, temp);
        updateChart(charts.ph,       labels, ph);
        updateChart(charts.ec,       labels, ec);
    } catch (e) {
        console.error(e);
    }
}

// ─── Actualizar historial de riegos ──────────────────────────
async function updateIrrigationTable() {
    try {
        const res = await fetch(`${API_URL}/api/irrigation/?limit=10`);
        if (!res.ok) return;
        const data = await res.json();

        const tbody = document.getElementById('irrigation-table');

        if (!data.length) {
            tbody.innerHTML = '<tr><td colspan="4" style="text-align:center;color:#718096">Sin eventos de riego</td></tr>';
            return;
        }

        tbody.innerHTML = data.map(event => `
            <tr>
                <td>${formatDate(event.recorded_at)}</td>
                <td><span class="badge-${event.irrigated ? 'si' : 'no'}">${event.irrigated ? 'Sí' : 'No'}</span></td>
                <td>${event.duration_ms ? (event.duration_ms / 1000).toFixed(1) + ' s' : '--'}</td>
                <td>${event.reason || '--'}</td>
            </tr>
        `).join('');
    } catch (e) {
        console.error(e);
    }
}

// ─── Actualizar alertas ───────────────────────────────────────
async function updateAlerts() {
    try {
        const res = await fetch(`${API_URL}/api/alerts/pending`);
        if (!res.ok) return;
        const data = await res.json();

        const container = document.getElementById('alerts-container');

        if (!data.length) {
            container.innerHTML = '<p class="no-alerts">No hay más alertas pendientes de ser enviadas</p>';
            return;
        }

        container.innerHTML = data.map(alert => `
            <div class="alert-item ${alert.alert_type.includes('CRITICAL') ? 'critical' : ''}">
                <div>
                    <div class="alert-type">⚠️ ${alert.alert_type}</div>
                    <div class="alert-message">${alert.message || ''}</div>
                </div>
                <div class="alert-date">${formatDate(alert.recorded_at)}</div>
            </div>
        `).join('');
    } catch (e) {
        console.error(e);
    }
}

// ─── Ciclo principal de actualización ────────────────────────
async function refresh() {
    await Promise.all([
        updateCurrentState(),
        updateOnlineStatus(),
        updatePowerState(),
        updateCharts(),
        updateIrrigationTable(),
        updateAlerts()
    ]);
}

// Arrancar
refresh();
setInterval(refresh, REFRESH_INTERVAL);

const exportModalOverlay = document.getElementById('export-modal-overlay');
const exportDesdeInput   = document.getElementById('export-desde');
const exportHastaInput   = document.getElementById('export-hasta');
const exportForm         = document.getElementById('export-form');

document.getElementById('btn-export-csv').addEventListener('click', async () => {
    // Consultamos el rango disponible cada vez que se abre el modal,
    // para que los límites de fecha reflejen siempre los datos reales
    try {
        const res = await fetch(`${API_URL}/api/cycles/export/range`);
        const data = await res.json();

        const todayStr = new Date().toISOString().split('T')[0];

        if (data.first_date) {
            const minDate = data.first_date.split('T')[0];
            exportDesdeInput.min = minDate;
            exportHastaInput.min = minDate;
        }
        // La fecha "hasta" nunca puede ser futura respecto a hoy
        exportDesdeInput.max = todayStr;
        exportHastaInput.max = todayStr;

        exportDesdeInput.value = '';
        exportHastaInput.value = '';
    } catch (err) {
        // Si falla la consulta del rango, dejamos los campos sin
        // restricciones adicionales; el backend seguirá validando
        // igualmente que "hasta" no sea futura si se introduce a mano
    }

    exportModalOverlay.classList.add('visible');
});

document.getElementById('export-modal-close').addEventListener('click', () => {
    exportModalOverlay.classList.remove('visible');
});
document.getElementById('export-modal-cancel').addEventListener('click', () => {
    exportModalOverlay.classList.remove('visible');
});
exportModalOverlay.addEventListener('click', (e) => {
    if (e.target === exportModalOverlay) exportModalOverlay.classList.remove('visible');
});

exportForm.addEventListener('submit', (e) => {
    e.preventDefault();

    const desde = exportDesdeInput.value;
    const hasta = exportHastaInput.value;

    if (desde && hasta && desde > hasta) {
        alert('La fecha "Desde" no puede ser posterior a la fecha "Hasta".');
        return;
    }

    const format = document.querySelector('input[name="export-format"]:checked').value;

    const params = new URLSearchParams();
    if (desde) params.append('desde', desde);
    if (hasta) params.append('hasta', hasta);

    const queryString = params.toString();
    const url = `${API_URL}/api/cycles/export/${format}${queryString ? '?' + queryString : ''}`;

    window.location.href = url;
    exportModalOverlay.classList.remove('visible');
});