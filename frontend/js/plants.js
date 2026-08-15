const API_URL = 'http://localhost:8000';

const activeContainer = document.getElementById('active-plant-container');
const listContainer   = document.getElementById('plants-list-container');
const modalOverlay    = document.getElementById('modal-overlay');
const modalTitle      = document.getElementById('modal-title');
const modalWarning    = document.getElementById('modal-warning');
const plantForm       = document.getElementById('plant-form');
const statusBadge = document.getElementById('status-badge');

const infoModalOverlay = document.getElementById('info-modal-overlay');
const infoModalMessage = document.getElementById('info-modal-message');
const infoModalOk      = document.getElementById('info-modal-ok');

function showInfoModal(message, isError = false) {
    infoModalMessage.innerHTML = message;
    document.getElementById('info-modal-title').textContent = isError ? 'Error' : 'Aviso';
    infoModalOverlay.classList.add('visible');
}
infoModalOk.addEventListener('click', () => infoModalOverlay.classList.remove('visible'));
infoModalOverlay.addEventListener('click', (e) => {
    if (e.target === infoModalOverlay) infoModalOverlay.classList.remove('visible');
});

const confirmModalOverlay = document.getElementById('confirm-modal-overlay');
const confirmModalMessage = document.getElementById('confirm-modal-message');
const confirmModalOk      = document.getElementById('confirm-modal-ok');
const confirmModalCancel  = document.getElementById('confirm-modal-cancel');

let confirmModalCallback = null;

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

let currentActiveId = null;

loadActivePlant();
loadPlantsList();
loadSyncStatus();
updateOnlineStatus();
setInterval(updateOnlineStatus, 10000);
setInterval(loadSyncStatus, 10000);

async function loadActivePlant() {
    try {
        const res = await fetch(`${API_URL}/api/plants/active`);
        if (res.status === 404) {
            currentActiveId = null;
            activeContainer.innerHTML = '<p class="no-alerts">No hay ninguna planta activa</p>';
            return;
        }
        const plant = await res.json();
        currentActiveId = plant.id;
        activeContainer.innerHTML = renderPlantCard(plant);
    } catch (err) {
        activeContainer.innerHTML = '<p class="no-alerts">Error al cargar la planta activa</p>';
    }
}

let allLoadedPlants = [];

async function loadPlantsList() {
    try {
        const res = await fetch(`${API_URL}/api/plants/`);
        const plants = await res.json();
        allLoadedPlants = plants;
        if (plants.length === 0) {
            listContainer.innerHTML = '<p class="no-alerts">No hay plantas guardadas</p>';
            return;
        }
        listContainer.innerHTML = plants.map(p => renderPlantCard(p)).join('');
    } catch (err) {
        listContainer.innerHTML = '<p class="no-alerts">Error al cargar las plantas</p>';
    }
}

function toLocalDate(isoString) {
    // El backend guarda las fechas en UTC pero sin indicarlo en el texto
    // (ej. "2026-08-11T21:24:01"). Añadimos la "Z" para decirle a
    // JavaScript que interprete el texto como UTC; a partir de ahí, el
    // propio navegador se encarga de convertirlo a la hora local del
    // usuario, incluyendo el ajuste automático de horario de verano/invierno.
    return new Date(isoString.endsWith('Z') ? isoString : isoString + 'Z');
}

function formatRelativeTime(isoString) {
    const diffMs = Date.now() - toLocalDate(isoString).getTime();
    const diffMin = Math.round(diffMs / 60000);

    if (diffMin < 1) return 'hace unos segundos';
    if (diffMin === 1) return 'hace 1 minuto';
    if (diffMin < 60) return `hace ${diffMin} minutos`;

    const diffHours = Math.round(diffMin / 60);
    if (diffHours === 1) return 'hace 1 hora';
    if (diffHours < 24) return `hace ${diffHours} horas`;

    const diffDays = Math.round(diffHours / 24);
    return diffDays === 1 ? 'hace 1 día' : `hace ${diffDays} días`;
}

async function loadSyncStatus() {
    const container = document.getElementById('sync-status-container');
    try {
        const res = await fetch(`${API_URL}/api/config/sync-status`);

        if (res.status === 404) {
            container.innerHTML = '<p class="no-alerts">Aún no hay ninguna configuración enviada al dispositivo</p>';
            return;
        }
        if (!res.ok) throw new Error('Error al consultar el estado');

        const data = await res.json();

        const syncedBadge = data.is_synced
            ? '<span class="badge-si">Sincronizado</span>'
            : '<span class="badge-pendiente">Pendiente de sincronizar</span>';

        const lastSyncText = data.last_sync_at
            ? `${formatRelativeTime(data.last_sync_at)}`
            : 'nunca solicitada';

        container.innerHTML = `
            <div class="sync-card">
                <div class="sync-card-info">
                    <div class="sync-card-row"><strong>Última sincronización solicitada por el dispositivo:</strong> ${lastSyncText}</div>
                    <div class="sync-card-row"><strong>Última configuración aplicada desde la app:</strong> ${formatRelativeTime(data.last_config_updated_at)}</div>
                </div>
                ${syncedBadge}
            </div>
        `;
    } catch (err) {
        container.innerHTML = '<p class="no-alerts">Error al cargar el estado de sincronización</p>';
    }
}

// Escapa caracteres HTML especiales antes de insertar el nombre de una
// planta dentro de un mensaje con innerHTML, evitando que un nombre con
// caracteres como < o & rompa el renderizado del modal
function escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

function renderPlantCard(plant) {
    let badge;
    if (!plant.is_active) {
        badge = '<span class="badge-no">Inactiva</span>';
    } else if (plant.config_synced) {
        badge = '<span class="badge-si">Activa</span>';
    } else {
        badge = '<span class="badge-pendiente">Pendiente</span>';
    }

    const activateBtn = plant.is_active
        ? `<button class="btn-activate btn-small" data-action="activate" data-id="${plant.id}" data-already-active="true">Aplicar cambios</button>`
        : `<button class="btn-activar btn-small" data-action="activate" data-id="${plant.id}" data-already-active="false">Activar</button>`;

    return `
        <div class="plant-card ${plant.is_active ? 'is-active' : ''}">
            <div class="plant-card-top">
                <span class="plant-card-name">${escapeHtml(plant.name)}</span>
                ${badge}
            </div>
            <div class="plant-card-params">
                Inicio riego: ${plant.soil_start_irrigation_pct}% &middot;
                Parada riego: ${plant.soil_stop_irrigation_pct}%<br>
                Temp. mínima: ${plant.soil_min_temp_c}ºC &middot;
                Riesgo congelación: ${plant.soil_freeze_risk_temp_c}ºC<br>
                Cooldown: ${plant.irrigation_cooldown_cycles} ciclos<br>
                Medida: ${plant.measure_period_ms / 1000}s &middot;
                Riego: ${plant.irrigate_time_ms / 1000}s
            </div>
            <div class="plant-card-actions">
                ${activateBtn}
                <button class="btn-edit btn-small" data-action="edit" data-id="${plant.id}">Editar</button>
                <button class="btn-delete btn-small" data-action="delete" data-id="${plant.id}">Eliminar</button>
            </div>
        </div>
    `;
}

async function activatePlant(id, wasAlreadyActive) {
    try {
        const res = await fetch(`${API_URL}/api/plants/${id}/activate`, { method: 'POST' });
        if (!res.ok) throw new Error('Fallo en la petición');
        const plant = await res.json();

        loadActivePlant();
        loadPlantsList();
        loadSyncStatus();

        // Verificamos con el propio dato que devuelve el backend, en vez
        // de asumir éxito solo porque la petición HTTP no dio error
        if (plant.config_synced === true) {
            const message = wasAlreadyActive
                ? `Cambios aplicados correctamente. La configuración de <strong>${escapeHtml(plant.name)}</strong> ya está lista para ser recogida por el dispositivo.`
                : `<strong>${escapeHtml(plant.name)}</strong> es ahora la planta activa. El dispositivo aplicará esta configuración en su próxima sincronización.`;
            showInfoModal(message);
        } else {
            showInfoModal('La activación se completó, pero no se pudo confirmar que la configuración quedara sincronizada. Vuelve a intentarlo.', true);
        }
    } catch (err) {
        showInfoModal('Error al activar la planta', true);
    }
}

async function deletePlant(id, name) {
    const plant = allLoadedPlants.find(p => p.id === id);
    const isActivePlant = plant ? plant.is_active : false;

    const message = isActivePlant
        ? `<strong>${escapeHtml(name)}</strong> es la planta activa. Si la eliminas, el dispositivo seguirá funcionando con la última configuración conocida hasta que actives otra planta. ¿Deseas continuar?`
        : `¿Eliminar la planta <strong>${escapeHtml(name)}</strong>?`;

    showConfirmModal(message, async () => {
        try {
            await fetch(`${API_URL}/api/plants/${id}`, { method: 'DELETE' });
            loadActivePlant();
            loadPlantsList();
            showToast(`Planta <strong>${escapeHtml(name)}</strong> eliminada correctamente`, 'success');
        } catch (err) {
            showInfoModal('Error al eliminar la planta', true);
        }
    });
}

async function editPlant(id) {
    try {
        const res = await fetch(`${API_URL}/api/plants/`);
        const plants = await res.json();
        const plant = plants.find(p => p.id === id);
        if (!plant) return;
        openModal('edit', plant);
    } catch (err) {
        showInfoModal('Error al cargar los datos de la planta', true);
    }
}

document.getElementById('btn-new-plant').addEventListener('click', () => openModal('create'));
document.getElementById('modal-close').addEventListener('click', closeModal);
document.getElementById('modal-cancel').addEventListener('click', closeModal);
modalOverlay.addEventListener('click', (e) => {
    if (e.target === modalOverlay) closeModal();
});

function openModal(mode, plant = null) {
    plantForm.reset();
    document.getElementById('plant-id').value = '';

    if (mode === 'create') {
        modalTitle.textContent = 'Nueva planta';
        modalWarning.classList.add('hidden');
    } else {
        modalTitle.textContent = 'Editar planta';
        document.getElementById('plant-id').value = plant.id;
        document.getElementById('f-name').value     = plant.name;
        document.getElementById('f-start').value    = plant.soil_start_irrigation_pct;
        document.getElementById('f-stop').value     = plant.soil_stop_irrigation_pct;
        document.getElementById('f-mintemp').value    = plant.soil_min_temp_c;
        document.getElementById('f-freezetemp').value = plant.soil_freeze_risk_temp_c;
        document.getElementById('f-cooldown').value   = plant.irrigation_cooldown_cycles;
        document.getElementById('f-measure').value  = plant.measure_period_ms / 1000;
        document.getElementById('f-irrigate').value = plant.irrigate_time_ms / 1000;

        if (plant.id === currentActiveId) {
            modalWarning.classList.remove('hidden');
        } else {
            modalWarning.classList.add('hidden');
        }
    }

    modalOverlay.classList.add('visible');
}

function closeModal() {
    modalOverlay.classList.remove('visible');
}

plantForm.addEventListener('submit', async (e) => {
    e.preventDefault();

    const id = document.getElementById('plant-id').value;
    const wasEditingActivePlant = id && parseInt(id) === currentActiveId;

    // Validación de rangos antes de construir el payload, como segunda
    // capa de seguridad además de los atributos min/max del HTML
    const start    = parseFloat(document.getElementById('f-start').value);
    const stop     = parseFloat(document.getElementById('f-stop').value);
    const cooldown = parseInt(document.getElementById('f-cooldown').value);
    const measure  = parseInt(document.getElementById('f-measure').value);
    const irrigate = parseInt(document.getElementById('f-irrigate').value);

    const name = document.getElementById('f-name').value.trim();
    const nameRegex = /^[\p{L}\p{N} '_-]+$/u;
    if (!nameRegex.test(name)) {
        showInfoModal('El nombre de la planta solo puede contener letras, números, espacios, guiones, guiones bajos y apóstrofos.', true);
        return;
    }

    if (start < 0 || start > 100 || stop < 0 || stop > 100) {
        showInfoModal('Los umbrales de riego deben estar entre 0% y 100%.', true);
        return;
    }
    if (start >= stop) {
        showInfoModal('El umbral de inicio de riego debe ser menor que el umbral de parada de riego.', true);
        return;
    }
    const minTemp = parseFloat(document.getElementById('f-mintemp').value);
    const freezeTemp = parseFloat(document.getElementById('f-freezetemp').value);
    if (freezeTemp >= minTemp) {
        showInfoModal('La temperatura de riesgo de congelación debe ser menor que la temperatura mínima de riego.', true);
        return;
    }
    if (cooldown < 0 || cooldown > 5) {
        showInfoModal('El cooldown debe estar entre 0 y 5 ciclos, ya que cada ciclo puede representar varias horas de espera entre riegos.', true);
        return;
    }
    if (measure < 1 || irrigate < 1) {
        showInfoModal('El periodo de medida y la duración de riego deben ser de al menos 1 segundo.', true);
        return;
    }

    const payload = {
        name: name,
        soil_start_irrigation_pct:  start,
        soil_stop_irrigation_pct:   stop,
        soil_min_temp_c:            parseFloat(document.getElementById('f-mintemp').value),
        soil_freeze_risk_temp_c:    parseFloat(document.getElementById('f-freezetemp').value),
        irrigation_cooldown_cycles: cooldown,
        measure_period_ms:          measure * 1000,
        irrigate_time_ms:           irrigate * 1000,
    };

    try {
        let res;
        if (id) {
            res = await fetch(`${API_URL}/api/plants/${id}`, {
                method: 'PUT',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(payload)
            });
        } else {
            res = await fetch(`${API_URL}/api/plants/`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(payload)
            });
        }

        if (!res.ok) {
            const errorData = await res.json();
            showInfoModal(errorData.detail || 'Error al guardar la planta.', true);
            return;
        }

        closeModal();
        loadActivePlant();
        loadPlantsList();

        if (wasEditingActivePlant) {
            // Si se editó la planta activa, los cambios NO llegan solos al
            // dispositivo. Avisamos con un modal para que no pase
            // desapercibido, ya que es información importante.
            showInfoModal('Cambios guardados.\n\nLa planta ha pasado a estado <strong>Pendiente</strong>, ya que estos cambios aún no están disponibles para el dispositivo. Pulsa <strong>Aplicar cambios</strong> en su tarjeta para dejarlos listos, y el dispositivo los recogerá en su próxima sincronización.');
        } else if (id) {
            // Edición de una planta que no era la activa: confirmación simple
            showToast(`Planta <strong>${escapeHtml(name)}</strong> actualizada correctamente`, 'success');
        } else {
            // Creación de una planta nueva
            showToast(`Planta <strong>${escapeHtml(name)}</strong> creada correctamente`, 'success');
        }
    } catch (err) {
        showInfoModal('Error al guardar la planta. Comprueba tu conexión con el servidor.', true);
    }
});

// Listener delegado para los botones de acción de las tarjetas de planta.
// En vez de usar onclick inline (frágil ante caracteres como comillas en
// el nombre de la planta), usamos data-attributes y delegación de eventos.
document.addEventListener('click', (e) => {
    const btn = e.target.closest('[data-action]');
    if (!btn) return;

    const action = btn.dataset.action;
    const id = parseInt(btn.dataset.id);

    if (action === 'activate') {
        const wasAlreadyActive = btn.dataset.alreadyActive === 'true';
        activatePlant(id, wasAlreadyActive);
    } else if (action === 'edit') {
        editPlant(id);
    } else if (action === 'delete') {
        // Buscamos el nombre real de la planta en los datos ya cargados,
        // en vez de pasarlo por el HTML
        const plant = allLoadedPlants.find(p => p.id === id);
        deletePlant(id, plant ? plant.name : '');
    }
});