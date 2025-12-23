// Events page JavaScript

let currentPage = 1;
let totalPages = 1;
let currentFilters = {};

document.addEventListener('DOMContentLoaded', function() {
    loadEvents();
    setupEventListeners();
});

function setupEventListeners() {
    // Поиск по Enter
    document.getElementById('searchInput').addEventListener('keypress', function(e) {
        if (e.key === 'Enter') {
            loadEvents();
        }
    });
    
    // Фильтры
    ['eventTypeFilter', 'severityFilter', 'hostnameFilter', 'userFilter'].forEach(id => {
        document.getElementById(id).addEventListener('change', function() {
            loadEvents();
        });
    });
}

function getAuthHeaders() {
    // Для API запросов используем сессию (cookies отправляются автоматически)
    return {
        'Content-Type': 'application/json'
    };
}

async function apiCall(url) {
    try {
        const response = await fetch(url, {
            headers: getAuthHeaders()
        });
        if (!response.ok) {
            if (response.status === 401) {
                window.location.href = '/login';
                return null;
            }
            const errorText = await response.text();
            try {
                const errorJson = JSON.parse(errorText);
                return { error: errorJson.error || `HTTP ${response.status}` };
            } catch {
                return { error: `HTTP ${response.status}: ${errorText}` };
            }
        }
        return await response.json();
    } catch (error) {
        console.error('API call error:', error);
        return { error: error.message || 'Network error' };
    }
}

function loadEvents(page = 1) {
    currentPage = page;
    const container = document.getElementById('eventsTable');
    container.innerHTML = '<div class="loading"><i class="fas fa-spinner fa-spin"></i> Загрузка событий...</div>';
    
    // Собираем фильтры
    const params = new URLSearchParams({
        page: page,
        per_page: 50,
        hours: 24
    });
    
    const search = document.getElementById('searchInput').value.trim();
    if (search) {
        params.append('search', search);
    }
    
    const eventType = document.getElementById('eventTypeFilter').value;
    if (eventType) {
        params.append('event_type', eventType);
    }
    
    const severity = document.getElementById('severityFilter').value;
    if (severity) {
        params.append('severity', severity);
    }
    
    const hostname = document.getElementById('hostnameFilter').value.trim();
    if (hostname) {
        params.append('hostname', hostname);
    }
    
    const user = document.getElementById('userFilter').value.trim();
    if (user) {
        params.append('user', user);
    }
    
    apiCall(`/api/events?${params.toString()}`)
        .then(data => {
            if (!data) {
                container.innerHTML = '<div class="text-danger">Ошибка загрузки данных</div>';
                return;
            }
            
            if (data.error) {
                container.innerHTML = `<div class="text-danger">${data.error}</div>`;
                return;
            }
            
            totalPages = data.pages || 1;
            updatePagination(data.page, data.pages, data.total);
            updateShowingInfo(data.page, data.per_page, data.total);
            
            if (data.events.length === 0) {
                container.innerHTML = '<div class="text-muted text-center p-4">События не найдены</div>';
                return;
            }
            
            // Загружаем уникальные типы событий для фильтра
            loadEventTypes(data.events);
            
            let html = '<table class="table table-hover"><thead><tr><th>Время</th><th>Тип</th><th>Критичность</th><th>Хост</th><th>Пользователь</th><th>Процесс</th><th>Действие</th></tr></thead><tbody>';
            
            data.events.forEach(event => {
                const severityClass = `severity-${event.severity || 'low'}`;
                const severityBadge = `<span class="badge badge-severity-${event.severity || 'low'}">${event.severity || 'low'}</span>`;
                
                html += `<tr class="event-card ${severityClass}" onclick="toggleEventDetails('${event._id}')">
                    <td>${formatTimestamp(event.timestamp)}</td>
                    <td><span class="badge bg-secondary">${event.event_type || 'unknown'}</span></td>
                    <td>${severityBadge}</td>
                    <td>${event.hostname || 'unknown'}</td>
                    <td>${event.user || 'N/A'}</td>
                    <td>${event.process || 'N/A'}</td>
                    <td><button class="btn btn-sm btn-outline-primary" onclick="event.stopPropagation(); showEventDetails('${event._id}')">
                        <i class="fas fa-info-circle"></i> Детали
                    </button></td>
                </tr>
                <tr id="details-${event._id}" class="event-details">
                    <td colspan="7">
                        <div class="card">
                            <div class="card-body">
                                <h6>Детали события</h6>
                                <pre class="bg-light p-3">${JSON.stringify(event, null, 2)}</pre>
                            </div>
                        </div>
                    </td>
                </tr>`;
            });
            
            html += '</tbody></table>';
            container.innerHTML = html;
        });
}

function toggleEventDetails(eventId) {
    const details = document.getElementById(`details-${eventId}`);
    if (details) {
        details.classList.toggle('show');
    }
}

function showEventDetails(eventId) {
    const details = document.getElementById(`details-${eventId}`);
    if (details) {
        details.classList.add('show');
        details.scrollIntoView({ behavior: 'smooth', block: 'nearest' });
    }
}

function loadEventTypes(events) {
    const types = new Set();
    events.forEach(event => {
        if (event.event_type) {
            types.add(event.event_type);
        }
    });
    
    const select = document.getElementById('eventTypeFilter');
    const currentValue = select.value;
    
    // Очищаем опции кроме "Все"
    while (select.options.length > 1) {
        select.remove(1);
    }
    
    // Добавляем уникальные типы
    Array.from(types).sort().forEach(type => {
        const option = document.createElement('option');
        option.value = type;
        option.textContent = type;
        select.appendChild(option);
    });
    
    // Восстанавливаем выбранное значение
    select.value = currentValue;
}

function updatePagination(page, pages, total) {
    const container = document.getElementById('pagination');
    container.innerHTML = '';
    
    if (pages <= 1) return;
    
    // Предыдущая страница
    const prevLi = document.createElement('li');
    prevLi.className = `page-item ${page === 1 ? 'disabled' : ''}`;
    prevLi.innerHTML = `<a class="page-link" href="#" onclick="loadEvents(${page - 1}); return false;">Предыдущая</a>`;
    container.appendChild(prevLi);
    
    // Номера страниц
    const startPage = Math.max(1, page - 2);
    const endPage = Math.min(pages, page + 2);
    
    if (startPage > 1) {
        const li = document.createElement('li');
        li.className = 'page-item';
        li.innerHTML = `<a class="page-link" href="#" onclick="loadEvents(1); return false;">1</a>`;
        container.appendChild(li);
        if (startPage > 2) {
            const li = document.createElement('li');
            li.className = 'page-item disabled';
            li.innerHTML = '<span class="page-link">...</span>';
            container.appendChild(li);
        }
    }
    
    for (let i = startPage; i <= endPage; i++) {
        const li = document.createElement('li');
        li.className = `page-item ${i === page ? 'active' : ''}`;
        li.innerHTML = `<a class="page-link" href="#" onclick="loadEvents(${i}); return false;">${i}</a>`;
        container.appendChild(li);
    }
    
    if (endPage < pages) {
        if (endPage < pages - 1) {
            const li = document.createElement('li');
            li.className = 'page-item disabled';
            li.innerHTML = '<span class="page-link">...</span>';
            container.appendChild(li);
        }
        const li = document.createElement('li');
        li.className = 'page-item';
        li.innerHTML = `<a class="page-link" href="#" onclick="loadEvents(${pages}); return false;">${pages}</a>`;
        container.appendChild(li);
    }
    
    // Следующая страница
    const nextLi = document.createElement('li');
    nextLi.className = `page-item ${page === pages ? 'disabled' : ''}`;
    nextLi.innerHTML = `<a class="page-link" href="#" onclick="loadEvents(${page + 1}); return false;">Следующая</a>`;
    container.appendChild(nextLi);
}

function updateShowingInfo(page, perPage, total) {
    const from = (page - 1) * perPage + 1;
    const to = Math.min(page * perPage, total);
    
    document.getElementById('showingFrom').textContent = total > 0 ? from : 0;
    document.getElementById('showingTo').textContent = to;
    document.getElementById('totalEvents').textContent = total;
}

function formatTimestamp(timestamp) {
    if (!timestamp) return 'N/A';
    try {
        const date = new Date(timestamp);
        return date.toLocaleString('ru-RU');
    } catch {
        return timestamp;
    }
}

function exportEvents(format) {
    const params = new URLSearchParams({
        format: format,
        hours: 24
    });
    
    // Добавляем фильтры
    const search = document.getElementById('searchInput').value.trim();
    if (search) {
        params.append('search', search);
    }
    
    const eventType = document.getElementById('eventTypeFilter').value;
    if (eventType) {
        params.append('event_type', eventType);
    }
    
    const severity = document.getElementById('severityFilter').value;
    if (severity) {
        params.append('severity', severity);
    }
    
    const hostname = document.getElementById('hostnameFilter').value.trim();
    if (hostname) {
        params.append('hostname', hostname);
    }
    
    const user = document.getElementById('userFilter').value.trim();
    if (user) {
        params.append('user', user);
    }
    
    const url = `/api/export/events?${params.toString()}`;
    const link = document.createElement('a');
    link.href = url;
    link.download = `events.${format}`;
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
}

