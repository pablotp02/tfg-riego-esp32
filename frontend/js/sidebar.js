// ─── Lógica del menú hamburguesa / sidebar ────────────────────
const menuToggle    = document.getElementById('menu-toggle');
const sidebar       = document.getElementById('sidebar');
const sidebarClose  = document.getElementById('sidebar-close');
const sidebarOverlay = document.getElementById('sidebar-overlay');

function openSidebar() {
    sidebar.classList.add('open');
    sidebarOverlay.classList.add('visible');
}

function closeSidebar() {
    sidebar.classList.remove('open');
    sidebarOverlay.classList.remove('visible');
}

menuToggle.addEventListener('click', openSidebar);
sidebarClose.addEventListener('click', closeSidebar);
sidebarOverlay.addEventListener('click', closeSidebar);