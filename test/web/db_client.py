
import socket
import struct
import json
from typing import Dict, List, Optional, Any


class DatabaseClient:
    def __init__(self, host: str = "localhost", port: int = 8080):
        self.host = host
        self.port = port
        self.socket = None
    
    def connect(self) -> bool:
        """Подключение к серверу БД"""
        try:
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.connect((self.host, self.port))
            return True
        except Exception as e:
            print(f"Connection error: {e}")
            return False
    
    def disconnect(self):
        """Отключение от сервера"""
        if self.socket:
            self.socket.close()
            self.socket = None
    
    def _read_message(self) -> str:
        """Чтение сообщения от сервера"""
        # Читаем длину сообщения (4 байта)
        length_bytes = self.socket.recv(4)
        if len(length_bytes) != 4:
            raise ConnectionError("Failed to read message length")
        
        length = struct.unpack('!I', length_bytes)[0]
        
        if length == 0 or length > 10 * 1024 * 1024:  # Максимум 10MB
            raise ValueError(f"Invalid message length: {length}")
        
        # Читаем само сообщение
        buffer = b''
        while len(buffer) < length:
            chunk = self.socket.recv(length - len(buffer))
            if not chunk:
                raise ConnectionError("Connection closed")
            buffer += chunk
        
        return buffer.decode('utf-8')
    
    def _send_message(self, message: str):
        """Отправка сообщения серверу"""
        message_bytes = message.encode('utf-8')
        length = struct.pack('!I', len(message_bytes))
        self.socket.sendall(length + message_bytes)
    
    def _execute_request(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """Выполнение запроса к БД"""
        if not self.socket:
            if not self.connect():
                raise ConnectionError(f"Cannot connect to database server at {self.host}:{self.port}")
        
        try:
            request_str = json.dumps(request)
            self._send_message(request_str)
            response_str = self._read_message()
            return json.loads(response_str)
        except (ConnectionError, socket.error) as e:
            # Переподключаемся при ошибке
            self.disconnect()
            if not self.connect():
                raise ConnectionError(f"Connection lost and cannot reconnect to {self.host}:{self.port}")
            # Повторяем запрос
            request_str = json.dumps(request)
            self._send_message(request_str)
            response_str = self._read_message()
            return json.loads(response_str)
    
    def find_events(self, query: Optional[Dict] = None, 
                   database: str = "security_db",
                   collection: str = "security_events") -> List[Dict]:
        """Поиск событий в БД"""
        if query is None:
            query = {}
        
        request = {
            "database": database,
            "operation": "find",
            "collection": collection,
            "query": query
        }
        
        response = self._execute_request(request)
        
        if response.get("status") == "success":
            return response.get("data", [])
        else:
            raise Exception(f"Database error: {response.get('message', 'Unknown error')}")
    
    def insert_event(self, event: Dict, 
                    database: str = "security_db",
                    collection: str = "security_events") -> bool:
        """Вставка события в БД"""
        request = {
            "database": database,
            "operation": "insert",
            "collection": collection,
            "data": event
        }
        
        response = self._execute_request(request)
        return response.get("status") == "success"
    
    def delete_events(self, query: Optional[Dict] = None,
                     database: str = "security_db",
                     collection: str = "security_events") -> int:
        """Удаление событий из БД"""
        if query is None:
            query = {}
        
        request = {
            "database": database,
            "operation": "delete",
            "collection": collection,
            "query": query
        }
        
        response = self._execute_request(request)
        
        if response.get("status") == "success":
            # Возвращаем количество удаленных документов
            return response.get("deleted", 0)
        else:
            raise Exception(f"Database error: {response.get('message', 'Unknown error')}")
    
    def __enter__(self):
        self.connect()
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        self.disconnect()

