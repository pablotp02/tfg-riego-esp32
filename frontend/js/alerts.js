const API_URL = 'http://localhost:8000';
const PAGE_SIZE = 10;

const alertsContainer = document.getElementById('alerts-container');
const btnLoadMore      = document.getElementById('btn-load-more');
const noMoreAlerts     = document.getElementById('no-more-alerts');
const statusBadge      = document.getElementById('status-badge');

const infoModalOverlay = document.getElementById('info-modal-overlay');
const infoModalMessage = document.getElementById('info-modal-message');
const infoModalOk      = document.getElementById('info-modal-ok');

let currentSkip = 0;
let allAlerts = []; // acumulamos lo cargado para no perderlo al marcar como enviada

function showInfoModal(message) {
    infoModalMessage.textContent = message;
    infoModalOverlay.classList.add('visible');
}
infoModalOk.addEventListener('click', () => infoModalOverlay.classList.remove('visible'));
infoModalOverlay.addEventListener('click', (e) => {
    if (e.target === infoModalOverlay) infoModalOverlay.classList.remove('visible');
});

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

function toLocalDate(isoString) {
    // El backend guarda las fechas en UTC pero sin indicarlo en el texto.
    // Añadimos la "Z" para que JavaScript lo interprete como UTC y lo
    // convierta correctamente a la hora local del usuario.
    return new Date(isoString.endsWith('Z') ? isoString : isoString + 'Z');
}

function formatDate(isoString) {
    const d = toLocalDate(isoString);
    return d.toLocaleString('es-ES', {
        day: '2-digit', month: '2-digit', year: 'numeric',
        hour: '2-digit', minute: '2-digit'
    });
}

function renderAlerts() {
    if (allAlerts.length === 0) {
        alertsContainer.innerHTML = '<p class="no-alerts">No hay alertas registradas</p>';
        return;
    }

    alertsContainer.innerHTML = allAlerts.map(alert => `
        <div class="alert-item ${alert.alert_type.includes('CRITICAL') ? 'critical' : ''}">
            <div>
                <div class="alert-type">
                    ${alert.sent ? '✅' : '⚠️'} ${alert.alert_type}
                    ${alert.sent ? '<span class="badge-si" style="margin-left:8px;">Enviada</span>' : '<span class="badge-no" style="margin-left:8px;">Pendiente</span>'}
                </div>
                <div class="alert-message">${alert.message || ''}</div>
            </div>
            <div class="alert-item-right">
                <div class="alert-date">${formatDate(alert.recorded_at)}</div>
                ${alert.sent
                    ? `<button class="btn-neutral btn-small" onclick="markAsPending(${alert.id})">Marcar como pendiente</button>`
                    : `<button class="btn-activate btn-small" onclick="markAsSent(${alert.id})">Marcar como enviada</button>`
                }
            </div>
        </div>
    `).join('');
}

async function loadAlerts() {
    try {
        const res = await fetch(`${API_URL}/api/alerts/?skip=${currentSkip}&limit=${PAGE_SIZE}`);
        if (!res.ok) throw new Error('Error al cargar alertas');
        const data = await res.json();

        allAlerts = allAlerts.concat(data);
        currentSkip += PAGE_SIZE;

        renderAlerts();

        // Si el backend devolvió menos de PAGE_SIZE, ya no hay más páginas
        if (data.length < PAGE_SIZE) {
            btnLoadMore.style.display = 'none';
            noMoreAlerts.style.display = 'block';
        }
    } catch (err) {
        alertsContainer.innerHTML = '<p class="no-alerts">Error al cargar las alertas</p>';
    }
}

async function markAsSent(id) {
    try {
        await fetch(`${API_URL}/api/alerts/${id}/sent`, { method: 'PATCH' });
        // Actualizamos localmente en vez de recargar toda la lista desde el principio
        const alert = allAlerts.find(a => a.id === id);
        if (alert) alert.sent = true;
        renderAlerts();
    } catch (err) {
        showInfoModal('Error al marcar la alerta como enviada');
    }
}

async function markAsPending(id) {
    try {
        await fetch(`${API_URL}/api/alerts/${id}/pending`, { method: 'PATCH' });
        const alert = allAlerts.find(a => a.id === id);
        if (alert) alert.sent = false;
        renderAlerts();
    } catch (err) {
        showInfoModal('Error al marcar la alerta como pendiente');
    }
}

btnLoadMore.addEventListener('click', loadAlerts);

// Carga inicial
loadAlerts();
updateOnlineStatus();
setInterval(updateOnlineStatus, 10000);