
import os
import json
import re
from datetime import datetime, timedelta
from functools import wraps
from typing import Dict, List, Optional
from flask import Flask, request, jsonify, render_template, session, redirect, url_for, Response
from werkzeug.security import check_password_hash
import base64
from db_client import DatabaseClient

app = Flask(__name__)
app.secret_key = os.environ.get('SECRET_KEY', 'siem-secret-key-change-in-production')

# Конфигурация БД
DB_HOST = os.environ.get('DB_HOST', 'localhost')
DB_PORT = int(os.environ.get('DB_PORT', '8080'))
DB_NAME = 'security_db'
COLLECTION_NAME = 'security_events'

# Путь к JSON файлу с событиями
JSON_EVENTS_FILE = os.environ.get('JSON_EVENTS_FILE', '/tmp/security_events.json')

# Basic Authentication credentials (из переменных окружения)
AUTH_USERNAME = os.environ.get('SIEM_USERNAME', 'admin')
AUTH_PASSWORD = os.environ.get('SIEM_PASSWORD', 'admin123')


def check_auth(username: str, password: str) -> bool:
    """Проверка учетных данных"""
    return username == AUTH_USERNAME and password == AUTH_PASSWORD


def authenticate():
    """Отправка 401 ответа с требованием аутентификации"""
    return Response(
        'Could not verify your access level for that URL.\n'
        'You have to login with proper credentials', 401,
        {'WWW-Authenticate': 'Basic realm="Login Required"'})


def requires_auth(f):
    """Декоратор для защиты маршрутов Basic Authentication или сессией"""
    @wraps(f)
    def decorated(*args, **kwargs):
        # Сначала проверяем сессию
        if 'username' in session:
            return f(*args, **kwargs)
        
        # Если сессии нет, проверяем Basic Auth
        auth = request.authorization
        if auth and check_auth(auth.username, auth.password):
            session['username'] = auth.username
            return f(*args, **kwargs)
        
        # Если это API запрос, возвращаем 401
        if request.path.startswith('/api/'):
            return authenticate()
        
        # Для обычных страниц редиректим на login
        return redirect(url_for('login'))
    return decorated


def get_db_client() -> DatabaseClient:
    """Получение клиента БД"""
    client = DatabaseClient(host=DB_HOST, port=DB_PORT)
    if not client.connect():
        raise ConnectionError(f"Cannot connect to database server at {DB_HOST}:{DB_PORT}. "
                           f"Make sure db_server is running.")
    return client


def parse_timestamp(ts: str) -> datetime:
    """Парсинг timestamp из ISO формата"""
    if not ts:
        return datetime.utcnow()
    try:
        # Формат: 2024-01-15T10:30:00Z
        ts_clean = ts.replace('Z', '').replace('+00:00', '')
        return datetime.strptime(ts_clean, '%Y-%m-%dT%H:%M:%S')
    except:
        try:
            # Пробуем другие форматы
            if 'T' in ts:
                return datetime.fromisoformat(ts.replace('Z', '+00:00'))
            else:
                # Если формат не распознан, возвращаем текущее время
                return datetime.utcnow()
        except:
            return datetime.utcnow()


def filter_events_by_time(events: List[Dict], hours: int = 24) -> List[Dict]:
    """Фильтрация событий за последние N часов"""
    if not events:
        return []
    
    # Если hours = 0 или отрицательное, возвращаем все события
    if hours <= 0:
        return events
    
    cutoff = datetime.utcnow() - timedelta(hours=hours)
    filtered = []
    for event in events:
        try:
            ts = parse_timestamp(event.get('timestamp', ''))
            # Если timestamp в будущем или очень старый (больше года), включаем его
            if ts >= cutoff or ts > datetime.utcnow():
                filtered.append(event)
        except Exception as e:
            # Если не удалось распарсить timestamp, включаем событие (на случай проблем с форматом)
            filtered.append(event)
    return filtered


def load_events_from_json_file(file_path: str = None) -> List[Dict]:
    """Загрузка событий из JSON файла (JSONL формат)"""
    if file_path is None:
        file_path = JSON_EVENTS_FILE
    
    events = []
    if not os.path.exists(file_path):
        return events
    
    try:
        # Пробуем читать с обработкой ошибок кодировки
        with open(file_path, 'r', encoding='utf-8', errors='replace') as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue
                try:
                    event = json.loads(line)
                    events.append(event)
                except json.JSONDecodeError:
                    continue
    except Exception as e:
        print(f"Error reading JSON file {file_path}: {e}")
    
    return events


def sync_events_from_json_to_db():
    """Синхронизация событий из JSON файла в БД"""
    try:
        json_events = load_events_from_json_file()
        if not json_events:
            return 0
        
        with get_db_client() as db:
            # Проверяем, какие события уже есть в БД
            existing_events = db.find_events({})
            existing_ids = {e.get('_id') for e in existing_events if '_id' in e}
            
            # Добавляем только новые события
            added = 0
            for event in json_events:
                # Используем комбинацию полей как уникальный идентификатор
                event_key = f"{event.get('timestamp', '')}_{event.get('command', '')}_{event.get('user', '')}"
                
                # Проверяем, есть ли уже такое событие
                query = {
                    'timestamp': event.get('timestamp', ''),
                    'command': event.get('command', ''),
                    'user': event.get('user', '')
                }
                existing = db.find_events(query)
                
                if not existing:
                    try:
                        db.insert_event(event)
                        added += 1
                    except Exception as e:
                        print(f"Error inserting event: {e}")
                        continue
            
            return added
    except Exception as e:
        print(f"Error syncing events to DB: {e}")
        return 0


def get_events_from_source(force_json: bool = False) -> List[Dict]:
    """Получение событий из JSON файла или БД (fallback)
    
    Args:
        force_json: Если True, всегда читать только из JSON файла (для реального времени)
    """
    # Если принудительно читать из JSON (для реального времени)
    if force_json:
        # Всегда читаем только из JSON файла, без fallback на БД
        return load_events_from_json_file()
    
    # Сначала пробуем JSON файл (приоритет)
    events = load_events_from_json_file()
    if events:
        return events
    
    # Если JSON файл пуст или недоступен, пробуем БД
    try:
        with get_db_client() as db:
            events = db.find_events({})
            if events:
                return events
    except Exception as e:
        pass
    
    return []


@app.route('/')
def index():
    """Главная страница - редирект на login"""
    if 'username' in session:
        return redirect(url_for('dashboard'))
    return redirect(url_for('login'))


@app.route('/login', methods=['GET', 'POST'])
def login():
    """Страница авторизации"""
    if 'username' in session:
        return redirect(url_for('dashboard'))
    
    if request.method == 'POST':
        username = request.form.get('username', '')
        password = request.form.get('password', '')
        
        if check_auth(username, password):
            session['username'] = username
            return redirect(url_for('dashboard'))
        else:
            return render_template('login.html', error='Неверное имя пользователя или пароль')
    
    return render_template('login.html')


@app.route('/logout')
def logout():
    """Выход из системы"""
    session.pop('username', None)
    return redirect(url_for('login'))


@app.route('/dashboard')
@requires_auth
def dashboard():
    """Дашборд с виджетами"""
    return render_template('dashboard.html')


@app.route('/events')
@requires_auth
def events():
    """Реестр событий"""
    return render_template('events.html')


# ========== API Endpoints ==========

@app.route('/api/events', methods=['GET'])
@requires_auth
def api_get_events():
    """Получение списка событий с фильтрацией и пагинацией"""
    try:
        page = int(request.args.get('page', 1))
        per_page = int(request.args.get('per_page', 50))
        search = request.args.get('search', '')
        event_type = request.args.get('event_type', '')
        severity = request.args.get('severity', '')
        hostname = request.args.get('hostname', '')
        user = request.args.get('user', '')
        hours = int(request.args.get('hours', 24))
        realtime = request.args.get('realtime', 'false').lower() == 'true'
        

        all_events = get_events_from_source(force_json=realtime)
        
        if event_type:
            all_events = [e for e in all_events if e.get('event_type') == event_type]
        if severity:
            all_events = [e for e in all_events if e.get('severity') == severity]
        if hostname:
            all_events = [e for e in all_events if e.get('hostname') == hostname]
        if user:
            all_events = [e for e in all_events if e.get('user') == user]
        
        if hours > 0:
            all_events = filter_events_by_time(all_events, hours)
        
        if search:
            try:
                pattern = re.compile(search, re.IGNORECASE)
                filtered_events = []
                for event in all_events:
                    event_str = json.dumps(event).lower()
                    if pattern.search(event_str):
                        filtered_events.append(event)
                all_events = filtered_events
            except re.error:
                search_lower = search.lower()
                all_events = [
                    e for e in all_events 
                    if search_lower in json.dumps(e).lower()
                ]
        
        all_events.sort(key=lambda x: parse_timestamp(x.get('timestamp', '')), reverse=True)
        
        # Пагинация
        total = len(all_events)
        start = (page - 1) * per_page
        end = start + per_page
        events_page = all_events[start:end]
        
        return jsonify({
            'events': events_page,
            'total': total,
            'page': page,
            'per_page': per_page,
            'pages': (total + per_page - 1) // per_page
        })
    
    except Exception as e:
        return jsonify({'error': str(e)}), 500


@app.route('/api/events/<event_id>', methods=['GET'])
@requires_auth
def api_get_event(event_id):
    """Получение конкретного события по ID"""
    try:
        all_events = get_events_from_source()
        for event in all_events:
            if event.get('_id') == event_id:
                return jsonify(event)
        return jsonify({'error': 'Event not found'}), 404
    except Exception as e:
        return jsonify({'error': str(e)}), 500


@app.route('/api/sync', methods=['POST'])
@requires_auth
def api_sync_events():
    """Синхронизация событий из JSON файла в БД"""
    try:
        added = sync_events_from_json_to_db()
        return jsonify({
            'status': 'success',
            'message': f'Synced {added} new events to database',
            'added': added
        })
    except Exception as e:
        return jsonify({'error': str(e)}), 500


@app.route('/api/dashboard/active-agents', methods=['GET'])
@requires_auth
def api_active_agents():
    """Активные агенты с временем последней активности"""
    try:
        hours = int(request.args.get('hours', 24))
        realtime = request.args.get('realtime', 'true').lower() == 'true'
        all_events = get_events_from_source(force_json=realtime)
        if hours > 0:
            recent_events = filter_events_by_time(all_events, hours)
        else:
            recent_events = all_events
        
        agents = {}
        for event in recent_events:
            hostname = event.get('hostname', 'unknown')
            if hostname not in agents:
                agents[hostname] = {
                    'hostname': hostname,
                    'last_activity': event.get('timestamp', ''),
                    'event_count': 0
                }
            agents[hostname]['event_count'] += 1
            event_ts = parse_timestamp(event.get('timestamp', ''))
            agent_ts = parse_timestamp(agents[hostname]['last_activity'])
            if event_ts > agent_ts:
                agents[hostname]['last_activity'] = event.get('timestamp', '')
        
        return jsonify(list(agents.values()))
    except Exception as e:
        return jsonify({'error': str(e)}), 500


@app.route('/api/dashboard/recent-logins', methods=['GET'])
@requires_auth
def api_recent_logins():
    """Последние входы в систему (успешные/неуспешные)"""
    try:
        limit = int(request.args.get('limit', 10))
        realtime = request.args.get('realtime', 'true').lower() == 'true'
        all_events = get_events_from_source(force_json=realtime)
        login_events = [
            e for e in all_events 
            if e.get('event_type') in ['user_login', 'auth_failure', 'authentication']
        ]
        
        login_events.sort(key=lambda x: parse_timestamp(x.get('timestamp', '')), reverse=True)
        
        result = []
        for event in login_events[:limit]:
            result.append({
                'timestamp': event.get('timestamp', ''),
                'user': event.get('user', 'unknown'),
                'hostname': event.get('hostname', 'unknown'),
                'status': 'success' if event.get('event_type') == 'user_login' else 'failure',
                'event_type': event.get('event_type', ''),
                'severity': event.get('severity', 'low')
            })
        
        return jsonify(result)
    except Exception as e:
        return jsonify({'error': str(e)}), 500


@app.route('/api/dashboard/hosts', methods=['GET'])
@requires_auth
def api_hosts():
    """Список хостов с количеством событий за сутки"""
    try:
        hours = int(request.args.get('hours', 24))
        realtime = request.args.get('realtime', 'true').lower() == 'true'
        all_events = get_events_from_source(force_json=realtime)
        if hours > 0:
            recent_events = filter_events_by_time(all_events, hours)
        else:
            recent_events = all_events
        
        hosts = {}
        for event in recent_events:
            hostname = event.get('hostname', 'unknown')
            if hostname not in hosts:
                hosts[hostname] = {
                    'hostname': hostname,
                    'event_count': 0
                }
            hosts[hostname]['event_count'] += 1
        
        return jsonify(list(hosts.values()))
    except Exception as e:
        return jsonify({'error': str(e)}), 500


@app.route('/api/dashboard/events-by-type', methods=['GET'])
@requires_auth
def api_events_by_type():
    """Топ событий по типу за сутки"""
    try:
        hours = int(request.args.get('hours', 24))
        realtime = request.args.get('realtime', 'true').lower() == 'true'
        all_events = get_events_from_source(force_json=realtime)
        if hours > 0:
            recent_events = filter_events_by_time(all_events, hours)
        else:
            recent_events = all_events
        
        type_counts = {}
        for event in recent_events:
            event_type = event.get('event_type', 'unknown')
            type_counts[event_type] = type_counts.get(event_type, 0) + 1
        
        result = [{'type': k, 'count': v} for k, v in type_counts.items()]
        result.sort(key=lambda x: x['count'], reverse=True)
        
        return jsonify(result)
    except Exception as e:
        return jsonify({'error': str(e)}), 500


@app.route('/api/dashboard/events-by-severity', methods=['GET'])
@requires_auth
def api_events_by_severity():
    """Распределение событий по severity за сутки"""
    try:
        hours = int(request.args.get('hours', 24))
        realtime = request.args.get('realtime', 'true').lower() == 'true'
        all_events = get_events_from_source(force_json=realtime)
        if hours > 0:
            recent_events = filter_events_by_time(all_events, hours)
        else:
            recent_events = all_events
        
        severity_counts = {}
        for event in recent_events:
            severity = event.get('severity', 'low')
            severity_counts[severity] = severity_counts.get(severity, 0) + 1
        
        result = [{'severity': k, 'count': v} for k, v in severity_counts.items()]
        
        return jsonify(result)
    except Exception as e:
        return jsonify({'error': str(e)}), 500


@app.route('/api/dashboard/top-users', methods=['GET'])
@requires_auth
def api_top_users():
    """Топ пользователей по активности за сутки"""
    try:
        hours = int(request.args.get('hours', 24))
        limit = int(request.args.get('limit', 10))
        realtime = request.args.get('realtime', 'true').lower() == 'true'
        all_events = get_events_from_source(force_json=realtime)
        if hours > 0:
            recent_events = filter_events_by_time(all_events, hours)
        else:
            recent_events = all_events
        
        user_counts = {}
        for event in recent_events:
            user = event.get('user', 'unknown')
            if user and user != 'unknown':
                user_counts[user] = user_counts.get(user, 0) + 1
        
        result = [{'user': k, 'count': v} for k, v in user_counts.items()]
        result.sort(key=lambda x: x['count'], reverse=True)
        
        return jsonify(result[:limit])
    except Exception as e:
        return jsonify({'error': str(e)}), 500


@app.route('/api/dashboard/top-processes', methods=['GET'])
@requires_auth
def api_top_processes():
    """Топ процессов по количеству событий за сутки"""
    try:
        hours = int(request.args.get('hours', 24))
        limit = int(request.args.get('limit', 10))
        realtime = request.args.get('realtime', 'true').lower() == 'true'
        all_events = get_events_from_source(force_json=realtime)
        if hours > 0:
            recent_events = filter_events_by_time(all_events, hours)
        else:
            recent_events = all_events
        
        process_counts = {}
        for event in recent_events:
            process = event.get('process', 'unknown')
            if process and process != 'unknown':
                process_counts[process] = process_counts.get(process, 0) + 1
        
        result = [{'process': k, 'count': v} for k, v in process_counts.items()]
        result.sort(key=lambda x: x['count'], reverse=True)
        
        return jsonify(result[:limit])
    except Exception as e:
        return jsonify({'error': str(e)}), 500


@app.route('/api/dashboard/events-timeline', methods=['GET'])
@requires_auth
def api_events_timeline():
    """График событий во времени (по часам за сутки)"""
    try:
        hours = int(request.args.get('hours', 24))
        realtime = request.args.get('realtime', 'true').lower() == 'true'
        all_events = get_events_from_source(force_json=realtime)
        if hours > 0:
            recent_events = filter_events_by_time(all_events, hours)
        else:
            recent_events = all_events
        
        hour_counts = {}
        for event in recent_events:
            ts = parse_timestamp(event.get('timestamp', ''))
            hour_key = ts.strftime('%Y-%m-%d %H:00')
            hour_counts[hour_key] = hour_counts.get(hour_key, 0) + 1
        
        result = [{'hour': k, 'count': v} for k, v in hour_counts.items()]
        result.sort(key=lambda x: x['hour'])
        
        return jsonify(result)
    except Exception as e:
        return jsonify({'error': str(e)}), 500


@app.route('/api/export/events', methods=['GET'])
@requires_auth
def api_export_events():
    """Экспорт событий в JSON или CSV"""
    try:
        format_type = request.args.get('format', 'json')
        hours = int(request.args.get('hours', 24))
        
        all_events = get_events_from_source()
        recent_events = filter_events_by_time(all_events, hours)
        recent_events.sort(key=lambda x: parse_timestamp(x.get('timestamp', '')), reverse=True)
        
        if format_type == 'csv':
            import csv
            from io import StringIO
            
            output = StringIO()
            if recent_events:
                writer = csv.DictWriter(output, fieldnames=recent_events[0].keys())
                writer.writeheader()
                writer.writerows(recent_events)
            
            return Response(
                output.getvalue(),
                mimetype='text/csv',
                headers={'Content-Disposition': 'attachment; filename=events.csv'}
            )
        else:  # JSON
            return Response(
                json.dumps(recent_events, indent=2),
                mimetype='application/json',
                headers={'Content-Disposition': 'attachment; filename=events.json'}
            )
    except Exception as e:
        return jsonify({'error': str(e)}), 500


if __name__ == '__main__':
    web_port = int(os.environ.get('WEB_PORT', '5001'))
    print(f"Connecting to database at {DB_HOST}:{DB_PORT}")
    print(f"Using credentials: username={AUTH_USERNAME}")
    print(f"JSON events file: {JSON_EVENTS_FILE}")
    
    try:
        print("Syncing events from JSON file to database...")
        added = sync_events_from_json_to_db()
        if added > 0:
            print(f"Synced {added} new events to database")
        else:
            print("No new events to sync")
    except Exception as e:
        print(f"Warning: Could not sync events to DB: {e}")
        print("Will read events from JSON file directly")
    
    print(f"Starting SIEM web server on http://localhost:{web_port}")
    app.run(debug=True, host='0.0.0.0', port=web_port)

