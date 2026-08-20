const API_URL = `http://${window.location.hostname}:8000`;

const statusBadge = document.getElementById('status-badge');
const settingsContainer = document.getElementById('settings-container');
const recipientsContainer = document.getElementById('recipients-container');

const modalOverlay = document.getElementById('modal-overlay');
const modalTitle = document.getElementById('modal-title');
const recipientForm = document.getElementById('recipient-form');

const infoModalOverlay = document.getElementById('info-modal-overlay');
const infoModalMessage = document.getElementById('info-modal-message');
const infoModalOk = document.getElementById('info-modal-ok');

const confirmModalOverlay = document.getElementById('confirm-modal-overlay');
const confirmModalMessage = document.getElementById('confirm-modal-message');
const confirmModalOk = document.getElementById('confirm-modal-ok');
const confirmModalCancel = document.getElementById('confirm-modal-cancel');

let confirmModalCallback = null;
let allRecipients = [];

function showInfoModal(message, isError = false) {
    infoModalMessage.innerHTML = message;
    document.getElementById('info-modal-title').textContent = isError ? 'Error' : 'Aviso';
    infoModalOverlay.classList.add('visible');
}
infoModalOk.addEventListener('click', () => infoModalOverlay.classList.remove('visible'));
infoModalOverlay.addEventListener('click', (e) => {
    if (e.target === infoModalOverlay) infoModalOverlay.classList.remove('visible');
});

function showConfirmModal(message, onConfirm) {
    confirmModalMessage.innerHTML = message;
    confirmModalCallback = onConfirm;
    confirmModalOverlay.classList.add('visible');
}
confirmModalOk.addEventListener('click', () => {
    confirmModalOverlay.classList.remove('visible');
    if (confirmModalCallback) confirmModalCallback();
    confirmModalCallback = null;
});
confirmModalCancel.addEventListener('click', () => {
    confirmModalOverlay.classList.remove('visible');
    confirmModalCallback = null;
});
confirmModalOverlay.addEventListener('click', (e) => {
    if (e.target === confirmModalOverlay) {
        confirmModalOverlay.classList.remove('visible');
        confirmModalCallback = null;
    }
});

function escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
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

// ─── Ajustes globales ─────────────────────────────────────────

async function loadSettings() {
    try {
        const res = await fetch(`${API_URL}/api/notifications/settings`);
        if (!res.ok) throw new Error('Error al cargar ajustes');
        const settings = await res.json();
        renderSettings(settings);
    } catch (err) {
        settingsContainer.innerHTML = '<p class="no-alerts">Error al cargar los ajustes</p>';
    }
}

function renderSettings(settings) {
    settingsContainer.innerHTML = `
        <div class="settings-card">
            <div class="settings-row">
                <div>
                    <div class="settings-row-label">Notificaciones por Telegram</div>
                    <div class="settings-row-desc">Enviar alertas a los destinatarios con chat de Telegram configurado</div>
                </div>
                <label class="toggle-switch">
                    <input type="checkbox" id="toggle-telegram" ${settings.channel_telegram_enabled ? 'checked' : ''}>
                    <span class="toggle-slider"></span>
                </label>
            </div>
            <div class="settings-row">
                <div>
                    <div class="settings-row-label">Notificaciones por Email</div>
                    <div class="settings-row-desc">Enviar alertas a los destinatarios con email configurado</div>
                </div>
                <label class="toggle-switch">
                    <input type="checkbox" id="toggle-email" ${settings.channel_email_enabled ? 'checked' : ''}>
                    <span class="toggle-slider"></span>
                </label>
            </div>
        </div>
    `;

    document.getElementById('toggle-telegram').addEventListener('change', updateSettings);
    document.getElementById('toggle-email').addEventListener('change', updateSettings);
}

async function updateSettings() {
    const payload = {
        channel_telegram_enabled: document.getElementById('toggle-telegram').checked,
        channel_email_enabled: document.getElementById('toggle-email').checked
    };

    try {
        const res = await fetch(`${API_URL}/api/notifications/settings`, {
            method: 'PUT',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(payload)
        });
        if (!res.ok) throw new Error('Error al guardar');
        showToast('Ajustes de notificación actualizados', 'success');
    } catch (err) {
        showInfoModal('Error al actualizar los ajustes de notificación', true);
        loadSettings(); // revertir el toggle visualmente si falló
    }
}

// ─── Destinatarios ─────────────────────────────────────────────

async function loadRecipients() {
    try {
        const res = await fetch(`${API_URL}/api/notifications/recipients`);
        if (!res.ok) throw new Error('Error al cargar destinatarios');
        allRecipients = await res.json();

        if (allRecipients.length === 0) {
            recipientsContainer.innerHTML = '<p class="no-alerts">No hay destinatarios registrados</p>';
            return;
        }

        recipientsContainer.innerHTML = allRecipients.map(renderRecipientCard).join('');
    } catch (err) {
        recipientsContainer.innerHTML = '<p class="no-alerts">Error al cargar los destinatarios</p>';
    }
}

function renderRecipientCard(recipient) {
    const channels = [];
    if (recipient.telegram_chat_id) channels.push('<span class="channel-tag">Telegram</span>');
    if (recipient.email) channels.push('<span class="channel-tag">Email</span>');

    const statusBadgeHtml = recipient.enabled
        ? '<span class="badge-si">Activo</span>'
        : '<span class="badge-no">Inactivo</span>';

    const toggleBtn = recipient.enabled
        ? `<button class="btn-neutral btn-small" data-action="toggle" data-id="${recipient.id}">Desactivar</button>`
        : `<button class="btn-activar btn-small" data-action="toggle" data-id="${recipient.id}">Activar</button>`;

    return `
        <div class="recipient-card ${recipient.enabled ? '' : 'is-disabled'}">
            <div class="recipient-card-top">
                <span class="recipient-card-name">${escapeHtml(recipient.name)}</span>
                ${statusBadgeHtml}
            </div>
            <div class="recipient-card-channels">
                ${channels.join('') || 'Sin canal configurado'}
            </div>
            <div class="recipient-card-actions">
                ${toggleBtn}
                <button class="btn-edit btn-small" data-action="edit" data-id="${recipient.id}">Editar</button>
                <button class="btn-delete btn-small" data-action="delete" data-id="${recipient.id}">Eliminar</button>
            </div>
        </div>
    `;
}

document.addEventListener('click', (e) => {
    const btn = e.target.closest('[data-action]');
    if (!btn) return;

    const action = btn.dataset.action;
    const id = parseInt(btn.dataset.id);
    const recipient = allRecipients.find(r => r.id === id);
    if (!recipient) return;

    if (action === 'edit') {
        openModal('edit', recipient);
    } else if (action === 'toggle') {
        const verb = recipient.enabled ? 'desactivar' : 'activar';
        showConfirmModal(`¿Seguro que quieres ${verb} a <strong>${escapeHtml(recipient.name)}</strong>?`, () => toggleRecipient(recipient));
    } else if (action === 'delete') {
        showConfirmModal(`¿Eliminar a <strong>${escapeHtml(recipient.name)}</strong> de los destinatarios?`, () => deleteRecipient(recipient));
    }
});

async function toggleRecipient(recipient) {
    try {
        const payload = {
            name: recipient.name,
            telegram_chat_id: recipient.telegram_chat_id,
            email: recipient.email,
            enabled: !recipient.enabled
        };
        const res = await fetch(`${API_URL}/api/notifications/recipients/${recipient.id}`, {
            method: 'PUT',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(payload)
        });
        if (!res.ok) throw new Error('Error al actualizar');
        loadRecipients();
        showToast(`${escapeHtml(recipient.name)} ${payload.enabled ? 'activado' : 'desactivado'} correctamente`, 'success');
    } catch (err) {
        showInfoModal('Error al actualizar el destinatario', true);
    }
}

async function deleteRecipient(recipient) {
    try {
        const res = await fetch(`${API_URL}/api/notifications/recipients/${recipient.id}`, { method: 'DELETE' });
        if (!res.ok) throw new Error('Error al eliminar');
        loadRecipients();
        showToast(`Destinatario <strong>${escapeHtml(recipient.name)}</strong> eliminado correctamente`, 'success');
    } catch (err) {
        showInfoModal('Error al eliminar el destinatario', true);
    }
}

// ─── Modal crear/editar ─────────────────────────────────────────

document.getElementById('btn-new-recipient').addEventListener('click', () => openModal('create'));
document.getElementById('modal-close').addEventListener('click', closeModal);
document.getElementById('modal-cancel').addEventListener('click', closeModal);
modalOverlay.addEventListener('click', (e) => {
    if (e.target === modalOverlay) closeModal();
});

function openModal(mode, recipient = null) {
    recipientForm.reset();
    document.getElementById('recipient-id').value = '';

    if (mode === 'create') {
        modalTitle.textContent = 'Nuevo destinatario';
    } else {
        modalTitle.textContent = 'Editar destinatario';
        document.getElementById('recipient-id').value = recipient.id;
        document.getElementById('f-name').value = recipient.name;
        document.getElementById('f-chat-id').value = recipient.telegram_chat_id || '';
        document.getElementById('f-email').value = recipient.email || '';
    }

    modalOverlay.classList.add('visible');
}

function closeModal() {
    modalOverlay.classList.remove('visible');
}

recipientForm.addEventListener('submit', async (e) => {
    e.preventDefault();

    const id = document.getElementById('recipient-id').value;
    const name = document.getElementById('f-name').value.trim();
    const chatId = document.getElementById('f-chat-id').value.trim();
    const email = document.getElementById('f-email').value.trim();

    if (!chatId && !email) {
        showInfoModal('Debe indicarse al menos un canal de contacto (Telegram o email).', true);
        return;
    }

    // Validación de formato del chat_id de Telegram: los identificadores
    // de conversaciones privadas son siempre números enteros positivos,
    // sin una longitud fija de dígitos garantizada por Telegram
    if (chatId && !/^\d+$/.test(chatId)) {
        showInfoModal('El chat ID de Telegram solo puede contener números positivos.', true);
        return;
    }

    // Validación de formato del email
    if (email && !/^\S+@\S+\.\S+$/.test(email)) {
        showInfoModal('Introduce una dirección de correo válida.', true);
        return;
    }

    // Comprobación de caracteres que podrían causar problemas de
    // renderizado o inyección si se insertaran sin escapar
    if (/['"<>]/.test(name) || /['"<>]/.test(chatId) || /['"<>]/.test(email)) {
        showInfoModal('Los campos no deben contener comillas ni los símbolos < >.', true);
        return;
    }

    const payload = {
        name: name,
        telegram_chat_id: chatId || null,
        email: email || null,
        enabled: true
    };

    try {
        let res;
        if (id) {
            // Al editar, mantenemos el estado enabled actual en vez de forzar true
            const current = allRecipients.find(r => r.id === parseInt(id));
            payload.enabled = current ? current.enabled : true;

            res = await fetch(`${API_URL}/api/notifications/recipients/${id}`, {
                method: 'PUT',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(payload)
            });
        } else {
            res = await fetch(`${API_URL}/api/notifications/recipients`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(payload)
            });
        }

        if (!res.ok) {
            const errorData = await res.json();
            showInfoModal(errorData.detail || 'Error al guardar el destinatario.', true);
            return;
        }

        closeModal();
        loadRecipients();
        showToast(`Destinatario <strong>${escapeHtml(name)}</strong> ${id ? 'actualizado' : 'creado'} correctamente`, 'success');
    } catch (err) {
        showInfoModal('Error al guardar el destinatario. Comprueba tu conexión con el servidor.', true);
    }
});

// ─── Carga inicial ───────────────────────────────────────────

loadSettings();
loadRecipients();
updateOnlineStatus();
setInterval(updateOnlineStatus, 10000);