from flask import Flask
from config import (
    SECRET_KEY, DB_HOST, DB_PORT, AUTH_USERNAME, 
    JSON_EVENTS_FILE, WEB_PORT
)
from event_utils import sync_events_from_json_to_db
from routes import register_routes
from api import register_api_routes

app = Flask(__name__)
app.secret_key = SECRET_KEY

register_routes(app)
register_api_routes(app)


if __name__ == '__main__':
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
    
    print(f"Starting SIEM web server on http://localhost:{WEB_PORT}")
    app.run(debug=True, host='0.0.0.0', port=WEB_PORT)
