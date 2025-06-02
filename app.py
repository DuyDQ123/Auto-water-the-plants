from flask import Flask, render_template, jsonify, request, Response
from flask_mysqldb import MySQL
from datetime import datetime
import json

app = Flask(__name__)

# MySQL Configuration
app.config['MYSQL_HOST'] = 'localhost'
app.config['MYSQL_USER'] = 'root'
app.config['MYSQL_PASSWORD'] = ''
app.config['MYSQL_DB'] = 'iot_control'

mysql = MySQL(app)

# Global variables for current sensor data
current_temp = 0
current_humidity = 0
pump_state = False
pending_command = None

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/update_sensor', methods=['POST'])
def update_sensor():
    global current_temp, current_humidity, pump_state
    data = request.get_json()
    current_temp = data.get('temperature')
    current_humidity = data.get('humidity')
    new_pump_state = data.get('pump_state')
    
    # If pump state changed, log it to database
    if new_pump_state != pump_state:
        cur = mysql.connection.cursor()
        cur.execute("INSERT INTO pump_events (state, timestamp) VALUES (%s, %s)", 
                   (new_pump_state, datetime.now()))
        mysql.connection.commit()
        cur.close()
        pump_state = new_pump_state
    
    return jsonify({'status': 'success'})

@app.route('/get_sensor_data')
def get_sensor_data():
    return jsonify({
        'temperature': current_temp,
        'humidity': current_humidity,
        'pump_state': pump_state
    })

@app.route('/toggle_pump', methods=['POST'])
def toggle_pump():
    global pump_state, pending_command
    pump_state = not pump_state
    pending_command = pump_state  # Set the pending command for ESP32
    
    # Log pump state change
    cur = mysql.connection.cursor()
    cur.execute("INSERT INTO pump_events (state, timestamp) VALUES (%s, %s)",
                (pump_state, datetime.now()))
    mysql.connection.commit()
    cur.close()
    
    return jsonify({'status': 'success', 'pump_state': pump_state})

@app.route('/check_command')
def check_command():
    global pending_command
    command = pending_command
    pending_command = None  # Clear the pending command after it's retrieved
    return jsonify({
        'has_command': command is not None,
        'command': command
    })

@app.route('/get_pump_stats')
def get_pump_stats():
    cur = mysql.connection.cursor()
    cur.execute("""
        SELECT 
            SUM(CASE WHEN state = 1 THEN 1 ELSE 0 END) as on_count,
            SUM(CASE WHEN state = 0 THEN 1 ELSE 0 END) as off_count
        FROM pump_events
    """)
    result = cur.fetchone()
    cur.close()
    
    return jsonify({
        'on_count': int(result[0] or 0),
        'off_count': int(result[1] or 0)
    })

if __name__ == '__main__':
    app.run(debug=True, host='0.0.0.0', port=5000)