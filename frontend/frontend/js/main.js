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
function formatDate(isoString) {
    const d = new Date(isoString);
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
        setOnline(true);
    } catch {
        setOnline(false);
    }
}

// ─── Actualizar batería y modo desde cycles ───────────────────
async function updatePowerState() {
    try {
        const res = await fetch(`${API_URL}/api/cycles/?limit=1`);
        if (!res.ok) return;
        const data = await res.json();
        if (!data.length) return;

        // El power_id del último ciclo nos da el estado energético
        // Por simplicidad mostramos el modo del último ciclo
        const cycle = data[0];
        const modeEl = document.getElementById('val-mode');

        // Buscamos el estado de batería desde el último ciclo
        // (el backend no expone power directamente, usamos cycles)
        document.getElementById('val-battery').textContent = '--';
        modeEl.textContent = '--';

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
            container.innerHTML = '<p class="no-alerts">Sin alertas pendientes</p>';
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
        updateCharts(),
        updateIrrigationTable(),
        updateAlerts()
    ]);
}

// Arrancar
refresh();
setInterval(refresh, REFRESH_INTERVAL);