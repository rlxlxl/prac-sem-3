import json
import re
import csv
from io import StringIO
from flask import request, jsonify, Response
from auth import requires_auth
from event_utils import (
    get_events_from_source,
    filter_events_by_time,
    parse_timestamp,
    sync_events_from_json_to_db
)


def register_api_routes(app):
    
    @app.route('/api/events', methods=['GET'])
    @requires_auth
    def api_get_events():
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
        try:
            format_type = request.args.get('format', 'json')
            hours = int(request.args.get('hours', 24))
            
            all_events = get_events_from_source()
            recent_events = filter_events_by_time(all_events, hours)
            recent_events.sort(key=lambda x: parse_timestamp(x.get('timestamp', '')), reverse=True)
            
            if format_type == 'csv':
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
            else:
                return Response(
                    json.dumps(recent_events, indent=2),
                    mimetype='application/json',
                    headers={'Content-Disposition': 'attachment; filename=events.json'}
                )
        except Exception as e:
            return jsonify({'error': str(e)}), 500

