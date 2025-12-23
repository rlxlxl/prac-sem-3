// Dashboard JavaScript

let charts = {};

// Инициализация при загрузке страницы
document.addEventListener('DOMContentLoaded', function() {
    refreshAll();
    // Автообновление каждые 5 секунд для реального времени
    setInterval(refreshAll, 5000);
});

function refreshAll() {
    // Обновляем индикатор времени последнего обновления
    const now = new Date();
    document.getElementById('lastUpdate').innerHTML = 
        `<i class="fas fa-circle"></i> Обновлено: ${now.toLocaleTimeString()}`;
    
    loadActiveAgents();
    loadRecentLogins();
    loadHosts();
    loadEventsByType();
    loadEventsBySeverity();
    loadTopUsers();
    loadTopProcesses();
    loadEventsTimeline();
    loadStatistics();
}

function getAuthHeaders() {
    // Для API запросов используем сессию (cookies отправляются автоматически)
    // Если нужен Basic Auth для API, можно добавить здесь
    return {
        'Content-Type': 'application/json'
    };
}

async function apiCall(url) {
    try {
        // Добавляем параметр realtime=true для обновления в реальном времени
        const separator = url.includes('?') ? '&' : '?';
        const realtimeUrl = url + separator + 'realtime=true';
        
        const response = await fetch(realtimeUrl, {
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

function loadActiveAgents() {
    const container = document.getElementById('activeAgents');
    apiCall('/api/dashboard/active-agents?hours=24')
        .then(data => {
            if (!data) {
                container.innerHTML = '<div class="text-danger">Ошибка загрузки данных</div>';
                return;
            }
            
            if (data.error) {
                container.innerHTML = `<div class="text-danger">${data.error}</div>`;
                return;
            }
            
            if (data.length === 0) {
                container.innerHTML = '<div class="text-muted">Нет активных агентов</div>';
                return;
            }
            
            let html = '<table class="table table-hover"><thead><tr><th>Хост</th><th>Последняя активность</th><th>Событий (24ч)</th></tr></thead><tbody>';
            data.forEach(agent => {
                html += `<tr>
                    <td><i class="fas fa-server"></i> ${agent.hostname}</td>
                    <td>${formatTimestamp(agent.last_activity)}</td>
                    <td><span class="badge bg-primary">${agent.event_count}</span></td>
                </tr>`;
            });
            html += '</tbody></table>';
            container.innerHTML = html;
        });
}

function loadRecentLogins() {
    const container = document.getElementById('recentLogins');
    apiCall('/api/dashboard/recent-logins?limit=10')
        .then(data => {
            if (!data) {
                container.innerHTML = '<div class="text-danger">Ошибка загрузки данных</div>';
                return;
            }
            
            if (data.error) {
                container.innerHTML = `<div class="text-danger">${data.error}</div>`;
                return;
            }
            
            if (data.length === 0) {
                container.innerHTML = '<div class="text-muted">Нет данных о входах</div>';
                return;
            }
            
            let html = '<table class="table table-hover"><thead><tr><th>Время</th><th>Пользователь</th><th>Хост</th><th>Статус</th><th>Тип</th></tr></thead><tbody>';
            data.forEach(login => {
                const statusClass = login.status === 'success' ? 'status-success' : 'status-failure';
                const statusIcon = login.status === 'success' ? 'fa-check-circle' : 'fa-times-circle';
                html += `<tr>
                    <td>${formatTimestamp(login.timestamp)}</td>
                    <td>${login.user || 'unknown'}</td>
                    <td>${login.hostname}</td>
                    <td><i class="fas ${statusIcon} ${statusClass}"></i> ${login.status === 'success' ? 'Успешно' : 'Неудачно'}</td>
                    <td><span class="badge bg-secondary">${login.event_type}</span></td>
                </tr>`;
            });
            html += '</tbody></table>';
            container.innerHTML = html;
        });
}

function loadHosts() {
    const container = document.getElementById('hostsList');
    apiCall('/api/dashboard/hosts?hours=24')
        .then(data => {
            if (!data) {
                container.innerHTML = '<div class="text-danger">Ошибка загрузки данных</div>';
                return;
            }
            
            if (data.error) {
                container.innerHTML = `<div class="text-danger">${data.error}</div>`;
                return;
            }
            
            if (data.length === 0) {
                container.innerHTML = '<div class="text-muted">Нет данных о хостах</div>';
                return;
            }
            
            let html = '<div class="row">';
            data.forEach(host => {
                html += `<div class="col-md-3 mb-3">
                    <div class="card">
                        <div class="card-body">
                            <h5 class="card-title"><i class="fas fa-server"></i> ${host.hostname}</h5>
                            <p class="card-text"><span class="badge bg-primary">${host.event_count} событий</span></p>
                        </div>
                    </div>
                </div>`;
            });
            html += '</div>';
            container.innerHTML = html;
        });
}

function loadEventsByType() {
    apiCall('/api/dashboard/events-by-type?hours=24')
        .then(data => {
            if (!data || data.error) return;
            
            const ctx = document.getElementById('eventsByTypeChart').getContext('2d');
            
            if (charts.eventsByType) {
                charts.eventsByType.destroy();
            }
            
            charts.eventsByType = new Chart(ctx, {
                type: 'pie',
                data: {
                    labels: data.map(item => item.type),
                    datasets: [{
                        data: data.map(item => item.count),
                        backgroundColor: [
                            '#FF6384', '#36A2EB', '#FFCE56', '#4BC0C0',
                            '#9966FF', '#FF9F40', '#FF6384', '#C9CBCF'
                        ]
                    }]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: true,
                    plugins: {
                        legend: {
                            position: 'bottom'
                        }
                    }
                }
            });
        });
}

function loadEventsBySeverity() {
    apiCall('/api/dashboard/events-by-severity?hours=24')
        .then(data => {
            if (!data || data.error) return;
            
            const ctx = document.getElementById('eventsBySeverityChart').getContext('2d');
            
            if (charts.eventsBySeverity) {
                charts.eventsBySeverity.destroy();
            }
            
            const colors = {
                'high': '#dc3545',
                'medium': '#ffc107',
                'low': '#28a745'
            };
            
            charts.eventsBySeverity = new Chart(ctx, {
                type: 'bar',
                data: {
                    labels: data.map(item => item.severity),
                    datasets: [{
                        label: 'Количество событий',
                        data: data.map(item => item.count),
                        backgroundColor: data.map(item => colors[item.severity] || '#6c757d')
                    }]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: true,
                    scales: {
                        y: {
                            beginAtZero: true
                        }
                    }
                }
            });
        });
}

function loadTopUsers() {
    const container = document.getElementById('topUsers');
    apiCall('/api/dashboard/top-users?hours=24&limit=10')
        .then(data => {
            if (!data) {
                container.innerHTML = '<div class="text-danger">Ошибка загрузки данных</div>';
                return;
            }
            
            if (data.error) {
                container.innerHTML = `<div class="text-danger">${data.error}</div>`;
                return;
            }
            
            if (data.length === 0) {
                container.innerHTML = '<div class="text-muted">Нет данных</div>';
                return;
            }
            
            let html = '<table class="table table-sm"><thead><tr><th>Пользователь</th><th>Событий</th></tr></thead><tbody>';
            data.forEach(user => {
                html += `<tr>
                    <td><i class="fas fa-user"></i> ${user.user}</td>
                    <td><span class="badge bg-primary">${user.count}</span></td>
                </tr>`;
            });
            html += '</tbody></table>';
            container.innerHTML = html;
        });
}

function loadTopProcesses() {
    const container = document.getElementById('topProcesses');
    apiCall('/api/dashboard/top-processes?hours=24&limit=10')
        .then(data => {
            if (!data) {
                container.innerHTML = '<div class="text-danger">Ошибка загрузки данных</div>';
                return;
            }
            
            if (data.error) {
                container.innerHTML = `<div class="text-danger">${data.error}</div>`;
                return;
            }
            
            if (data.length === 0) {
                container.innerHTML = '<div class="text-muted">Нет данных</div>';
                return;
            }
            
            let html = '<table class="table table-sm"><thead><tr><th>Процесс</th><th>Событий</th></tr></thead><tbody>';
            data.forEach(proc => {
                html += `<tr>
                    <td><i class="fas fa-cog"></i> ${proc.process}</td>
                    <td><span class="badge bg-primary">${proc.count}</span></td>
                </tr>`;
            });
            html += '</tbody></table>';
            container.innerHTML = html;
        });
}

function loadEventsTimeline() {
    apiCall('/api/dashboard/events-timeline?hours=24')
        .then(data => {
            if (!data || data.error) return;
            
            const ctx = document.getElementById('eventsTimelineChart').getContext('2d');
            
            if (charts.eventsTimeline) {
                charts.eventsTimeline.destroy();
            }
            
            charts.eventsTimeline = new Chart(ctx, {
                type: 'bar',
                data: {
                    labels: data.map(item => item.hour),
                    datasets: [{
                        label: 'Количество событий',
                        data: data.map(item => item.count),
                        backgroundColor: '#007bff'
                    }]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: true,
                    scales: {
                        y: {
                            beginAtZero: true
                        }
                    }
                }
            });
        });
}

function loadStatistics() {
    // Загружаем общую статистику (увеличиваем период до недели для теста)
    apiCall('/api/events?per_page=1&hours=168')
        .then(data => {
            if (data && !data.error) {
                document.getElementById('totalEvents').textContent = data.total || 0;
            } else {
                document.getElementById('totalEvents').textContent = '0';
                if (data && data.error) {
                    console.error('Error loading total events:', data.error);
                }
            }
        });
    
    apiCall('/api/dashboard/events-by-severity?hours=168')
        .then(data => {
            if (data && !data.error && Array.isArray(data)) {
                const high = data.find(item => item.severity === 'high');
                document.getElementById('highSeverity').textContent = high ? high.count : 0;
            } else {
                document.getElementById('highSeverity').textContent = '0';
                if (data && data.error) {
                    console.error('Error loading severity:', data.error);
                }
            }
        });
    
    apiCall('/api/dashboard/hosts?hours=168')
        .then(data => {
            if (data && !data.error && Array.isArray(data)) {
                document.getElementById('activeHosts').textContent = data.length || 0;
            } else {
                document.getElementById('activeHosts').textContent = '0';
                if (data && data.error) {
                    console.error('Error loading hosts:', data.error);
                }
            }
        });
    
    apiCall('/api/dashboard/top-users?hours=168&limit=100')
        .then(data => {
            if (data && !data.error && Array.isArray(data)) {
                document.getElementById('activeUsers').textContent = data.length || 0;
            } else {
                document.getElementById('activeUsers').textContent = '0';
                if (data && data.error) {
                    console.error('Error loading users:', data.error);
                }
            }
        });
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

