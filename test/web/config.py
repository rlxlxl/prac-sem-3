import os

DB_HOST = os.environ.get('DB_HOST', 'localhost')
DB_PORT = int(os.environ.get('DB_PORT', '8080'))
DB_NAME = 'security_db'
COLLECTION_NAME = 'security_events'

JSON_EVENTS_FILE = os.environ.get('JSON_EVENTS_FILE', '/tmp/security_events.json')

AUTH_USERNAME = os.environ.get('SIEM_USERNAME', 'admin')
AUTH_PASSWORD = os.environ.get('SIEM_PASSWORD', 'admin123')

WEB_PORT = int(os.environ.get('WEB_PORT', '5001'))

SECRET_KEY = os.environ.get('SECRET_KEY', 'siem-secret-key-change-in-production')

