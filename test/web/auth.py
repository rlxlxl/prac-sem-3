from functools import wraps
from flask import request, session, redirect, url_for, Response
from config import AUTH_USERNAME, AUTH_PASSWORD


def check_auth(username: str, password: str) -> bool:
    return username == AUTH_USERNAME and password == AUTH_PASSWORD


def authenticate():
    return Response(
        'Could not verify your access level for that URL.\n'
        'You have to login with proper credentials', 401,
        {'WWW-Authenticate': 'Basic realm="Login Required"'})


def requires_auth(f):
    @wraps(f)
    def decorated(*args, **kwargs):
        if 'username' in session:
            return f(*args, **kwargs)
        
        auth = request.authorization
        if auth and check_auth(auth.username, auth.password):
            session['username'] = auth.username
            return f(*args, **kwargs)
        
        if request.path.startswith('/api/'):
            return authenticate()
        
        return redirect(url_for('login'))
    return decorated

