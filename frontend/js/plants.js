const API_URL = 'http://localhost:8000';

const activeContainer = document.getElementById('active-plant-container');
const listContainer   = document.getElementById('plants-list-container');
const modalOverlay    = document.getElementById('modal-overlay');
const modalTitle      = document.getElementById('modal-title');
const modalWarning    = document.getElementById('modal-warning');
const plantForm       = document.getElementById('plant-form');
const statusBadge = document.getElementById('status-badge');

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
updateOnlineStatus();
setInterval(updateOnlineStatus, 10000);

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

async function loadPlantsList() {
    try {
        const res = await fetch(`${API_URL}/api/plants/`);
        const plants = await res.json();
        if (plants.length === 0) {
            listContainer.innerHTML = '<p class="no-alerts">No hay plantas guardadas</p>';
            return;
        }
        listContainer.innerHTML = plants.map(p => renderPlantCard(p)).join('');
    } catch (err) {
        listContainer.innerHTML = '<p class="no-alerts">Error al cargar las plantas</p>';
    }
}

function renderPlantCard(plant) {
    const badge = plant.is_active
        ? '<span class="badge-si">Activa</span>'
        : '<span class="badge-no">Inactiva</span>';

    const activateBtn = plant.is_active
        ? `<button class="btn-secondary btn-small" onclick="activatePlant(${plant.id})">Reaplicar config</button>`
        : `<button class="btn-secondary btn-small" onclick="activatePlant(${plant.id})">Activar</button>`;

    return `
        <div class="plant-card ${plant.is_active ? 'is-active' : ''}">
            <div class="plant-card-top">
                <span class="plant-card-name">${plant.name}</span>
                ${badge}
            </div>
            <div class="plant-card-params">
                Inicio riego: ${plant.soil_start_irrigation_pct}% &middot;
                Parada riego: ${plant.soil_stop_irrigation_pct}%<br>
                Temp. mínima: ${plant.soil_min_temp_c}ºC &middot;
                Cooldown: ${plant.irrigation_cooldown_cycles} ciclos<br>
                Medida: ${plant.measure_period_ms} ms &middot;
                Riego: ${plant.irrigate_time_ms} ms
            </div>
            <div class="plant-card-actions">
                ${activateBtn}
                <button class="btn-edit btn-small" onclick="editPlant(${plant.id})">Editar</button>
                <button class="btn-delete btn-small" onclick="deletePlant(${plant.id}, '${plant.name}')">Eliminar</button>
            </div>
        </div>
    `;
}

async function activatePlant(id) {
    try {
        await fetch(`${API_URL}/api/plants/${id}/activate`, { method: 'POST' });
        loadActivePlant();
        loadPlantsList();
    } catch (err) {
        alert('Error al activar la planta');
    }
}

async function deletePlant(id, name) {
    if (!confirm(`¿Eliminar la planta "${name}"?`)) return;
    try {
        await fetch(`${API_URL}/api/plants/${id}`, { method: 'DELETE' });
        loadActivePlant();
        loadPlantsList();
    } catch (err) {
        alert('Error al eliminar la planta');
    }
}

async function editPlant(id) {
    try {
        const res = await fetch(`${API_URL}/api/plants/`);
        const plants = await res.json();
        const plant = plants.find(p => p.id === id);
        if (!plant) return;
        openModal('edit', plant);
    } catch (err) {
        alert('Error al cargar los datos de la planta');
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
        document.getElementById('f-mintemp').value  = plant.soil_min_temp_c;
        document.getElementById('f-cooldown').value = plant.irrigation_cooldown_cycles;
        document.getElementById('f-measure').value  = plant.measure_period_ms;
        document.getElementById('f-irrigate').value = plant.irrigate_time_ms;

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

    const payload = {
        name: document.getElementById('f-name').value,
        soil_start_irrigation_pct: parseFloat(document.getElementById('f-start').value),
        soil_stop_irrigation_pct:  parseFloat(document.getElementById('f-stop').value),
        soil_min_temp_c:           parseFloat(document.getElementById('f-mintemp').value),
        irrigation_cooldown_cycles: parseInt(document.getElementById('f-cooldown').value),
        measure_period_ms:         parseInt(document.getElementById('f-measure').value),
        irrigate_time_ms:          parseInt(document.getElementById('f-irrigate').value),
    };

    try {
        if (id) {
            await fetch(`${API_URL}/api/plants/${id}`, {
                method: 'PUT',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(payload)
            });
        } else {
            await fetch(`${API_URL}/api/plants/`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(payload)
            });
        }
        closeModal();
        loadActivePlant();
        loadPlantsList();
    } catch (err) {
        alert('Error al guardar la planta');
    }
});