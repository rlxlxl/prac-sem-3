import os
import json
from datetime import datetime, timedelta
from typing import Dict, List
from config import JSON_EVENTS_FILE
from database import get_db_client


def parse_timestamp(ts: str) -> datetime:
    if not ts:
        return datetime.utcnow()
    try:
        ts_clean = ts.replace('Z', '').replace('+00:00', '')
        return datetime.strptime(ts_clean, '%Y-%m-%dT%H:%M:%S')
    except:
        try:
            if 'T' in ts:
                return datetime.fromisoformat(ts.replace('Z', '+00:00'))
            else:
                return datetime.utcnow()
        except:
            return datetime.utcnow()


def filter_events_by_time(events: List[Dict], hours: int = 24) -> List[Dict]:
    if not events:
        return []
    
    if hours <= 0:
        return events
    
    cutoff = datetime.utcnow() - timedelta(hours=hours)
    filtered = []
    for event in events:
        try:
            ts = parse_timestamp(event.get('timestamp', ''))
            if ts >= cutoff or ts > datetime.utcnow():
                filtered.append(event)
        except Exception as e:
            filtered.append(event)
    return filtered


def load_events_from_json_file(file_path: str = None) -> List[Dict]:
    if file_path is None:
        file_path = JSON_EVENTS_FILE
    
    events = []
    if not os.path.exists(file_path):
        return events
    
    try:
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
    try:
        json_events = load_events_from_json_file()
        if not json_events:
            return 0
        
        with get_db_client() as db:
            existing_events = db.find_events({})
            existing_ids = {e.get('_id') for e in existing_events if '_id' in e}
            
            added = 0
            for event in json_events:
                event_key = f"{event.get('timestamp', '')}_{event.get('command', '')}_{event.get('user', '')}"
                
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
    if force_json:
        return load_events_from_json_file()
    
    events = load_events_from_json_file()
    if events:
        return events
    
    try:
        with get_db_client() as db:
            events = db.find_events({})
            if events:
                return events
    except Exception as e:
        pass
    
    return []

