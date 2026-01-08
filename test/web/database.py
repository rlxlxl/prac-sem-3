from db_client import DatabaseClient
from config import DB_HOST, DB_PORT


def get_db_client() -> DatabaseClient:
    client = DatabaseClient(host=DB_HOST, port=DB_PORT)
    if not client.connect():
        raise ConnectionError(f"Cannot connect to database server at {DB_HOST}:{DB_PORT}. "
                           f"Make sure db_server is running.")
    return client

