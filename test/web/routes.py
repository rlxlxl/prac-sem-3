from flask import render_template, session, redirect, url_for, request
from auth import check_auth, requires_auth


def register_routes(app):
    
    @app.route('/')
    def index():
        if 'username' in session:
            return redirect(url_for('dashboard'))
        return redirect(url_for('login'))
    
    
    @app.route('/login', methods=['GET', 'POST'])
    def login():
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
        session.pop('username', None)
        return redirect(url_for('login'))
    
    
    @app.route('/dashboard')
    @requires_auth
    def dashboard():
        return render_template('dashboard.html')
    
    
    @app.route('/events')
    @requires_auth
    def events():
        return render_template('events.html')
    
    @app.route('/health')
    def health():
        """Health check endpoint для Docker"""
        return {'status': 'healthy'}, 200

