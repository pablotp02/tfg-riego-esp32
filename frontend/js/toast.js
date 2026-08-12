// Sistema de notificaciones tipo "toast": avisos flotantes no
// bloqueantes que aparecen en la parte inferior de la pantalla,
// se cierran solos tras unos segundos, o el usuario puede cerrarlos
// manualmente con la "x".

const TOAST_DURATION_MS = 5000;

function getToastContainer() {
    let container = document.getElementById('toast-container');
    if (!container) {
        container = document.createElement('div');
        container.id = 'toast-container';
        container.className = 'toast-container';
        document.body.appendChild(container);
    }
    return container;
}

function showToast(message, type = 'default') {
    const container = getToastContainer();

    const toast = document.createElement('div');
    toast.className = `toast toast-${type}`;
    toast.innerHTML = `
        <span>${message}</span>
        <button class="toast-close" aria-label="Cerrar">✕</button>
    `;

    container.appendChild(toast);

    const removeToast = () => {
        toast.classList.add('toast-hiding');
        // Esperamos a que termine la animación de salida antes de
        // quitar el elemento del DOM
        setTimeout(() => toast.remove(), 250);
    };

    const timer = setTimeout(removeToast, TOAST_DURATION_MS);

    toast.querySelector('.toast-close').addEventListener('click', () => {
        clearTimeout(timer);
        removeToast();
    });
}