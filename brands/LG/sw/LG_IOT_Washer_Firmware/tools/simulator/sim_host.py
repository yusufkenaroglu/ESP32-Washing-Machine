import sys
import time
import threading
import serial
import serial.tools.list_ports
import struct
import binascii
import os
from flask import Flask, render_template, send_from_directory
from flask_socketio import SocketIO, emit

app = Flask(__name__)
app.config['SECRET_KEY'] = 'secret!'
socketio = SocketIO(app, cors_allowed_origins='*')

TEMPLATE_DIR = os.path.join(os.path.dirname(__file__), 'templates')

# Serial Configuration
SERIAL_PORT = None
BAUD_RATE = 1000000
ser = None

def find_esp32():
    ports = list(serial.tools.list_ports.comports())
    for p in ports:
        if "SLAB" in p.description or "CP210" in p.description or "USB" in p.description:
            return p.device
    return None

def serial_reader():
    global ser
    while True:
        if ser and ser.is_open:
            try:
                # Read one byte to detect start of command
                if ser.in_waiting:
                    # Peek or read one byte
                    c = ser.read(1)
                    if not c:
                        continue
                        
                    if c == b'$':
                        # Potential command
                        cmd_type = ser.read(1)
                        
                        if cmd_type == b'b':
                            # Binary Bitmap: $b<x>,<y>,<w>,<h>\n<binary_data>
                            # Read header until \n
                            header = b""
                            while True:
                                b = ser.read(1)
                                if b == b'\n':
                                    break
                                header += b
                            
                            try:
                                parts = header.decode().split(',')
                                if len(parts) == 4:
                                    x, y, w, h = map(int, parts)
                                    total_bytes = w * h * 2
                                    
                                    # Read binary data
                                    data = b""
                                    while len(data) < total_bytes:
                                        chunk = ser.read(total_bytes - len(data))
                                        if chunk:
                                            data += chunk
                                    
                                    # Convert to hex string for frontend compatibility
                                    # Firmware sends Big Endian data now.
                                    # binascii.hexlify converts b'\xF8\x00' to b'f800', which is exactly what we want.
                                    hex_str = binascii.hexlify(data).decode('ascii').upper()
                                    
                                    socketio.emit('draw_bitmap', {
                                        'x': x,
                                        'y': y,
                                        'w': w,
                                        'h': h,
                                        'data': hex_str
                                    })
                            except Exception as e:
                                print(f"Binary Bitmap Error: {e}")

                        else:
                            # Text command or log
                            # Read until newline
                            line_bytes = cmd_type + ser.readline()
                            try:
                                line = line_bytes.decode('utf-8', errors='ignore').strip()
                            except:
                                line = ""
                                
                            if line.startswith('R'): # Was $R, but we ate $
                                # Rect: R<x>,<y>,<w>,<h>,<color>
                                try:
                                    parts = line[1:].split(',')
                                    if len(parts) == 5:
                                        socketio.emit('draw_rect', {
                                            'x': int(parts[0]),
                                            'y': int(parts[1]),
                                            'w': int(parts[2]),
                                            'h': int(parts[3]),
                                            'c': int(parts[4])
                                        })
                                except:
                                    pass
                            elif line.startswith('G'): # Was $G
                                # GPIO: G<pin>,<val>
                                try:
                                    parts = line[1:].split(',')
                                    if len(parts) == 2:
                                        socketio.emit('gpio_update', {
                                            'p': int(parts[0]),
                                            'v': int(parts[1])
                                        })
                                except:
                                    pass
                            elif line.startswith('M'):
                                # Motor telemetry: M<target>,<current>,<dir>
                                try:
                                    parts = line[1:].split(',')
                                    if len(parts) >= 3:
                                        socketio.emit('motor_state', {
                                            'target': float(parts[0]),
                                            'current': float(parts[1]),
                                            'direction': int(parts[2])
                                        })
                                except:
                                    pass
                            else:
                                # Log or unknown
                                # Reconstruct full line for log
                                full_line = "$" + line
                                socketio.emit('log', {'msg': full_line})
                    
                    else:
                        # Not a command start, just log text
                        # Read line
                        line_bytes = c + ser.readline()
                        try:
                            line = line_bytes.decode('utf-8', errors='ignore').strip()
                            if line:
                                socketio.emit('log', {'msg': line})
                        except:
                            pass
                            
            except Exception as e:
                print(f"Serial Error: {e}")
                time.sleep(1)
        else:
            time.sleep(1)

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/assets/drum.3dm')
def drum_asset_3dm():
    return send_from_directory(TEMPLATE_DIR, 'washing machine drum.3dm', mimetype='application/octet-stream')

@app.route('/assets/drum.obj')
def drum_asset_obj():
    return send_from_directory(TEMPLATE_DIR, 'washing machine drum.obj', mimetype='application/octet-stream')

@socketio.on('gpio_input')
def handle_gpio_input(json):
    global ser
    if ser and ser.is_open:
        # Format: $Ipin,val\n
        cmd = f"$I{json['pin']},{json['val']}\n"
        ser.write(cmd.encode())


@socketio.on('dial_delta')
def handle_dial_delta(json):
    global ser
    if not ser or not ser.is_open:
        return
    try:
        delta = int(json.get('delta', 0))
    except Exception:
        return
    cmd = f"$D{delta}\n"
    ser.write(cmd.encode())

if __name__ == '__main__':
    if len(sys.argv) > 1:
        SERIAL_PORT = sys.argv[1]
    else:
        SERIAL_PORT = find_esp32()
    
    if SERIAL_PORT:
        print(f"Connecting to {SERIAL_PORT} at {BAUD_RATE}...")
        try:
            ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=0.1)
            t = threading.Thread(target=serial_reader)
            t.daemon = True
            t.start()
        except Exception as e:
            print(f"Failed to open serial: {e}")
    else:
        print("No ESP32 found. Please specify port as argument.")

    print("Starting Web Server at http://localhost:5000")
    socketio.run(app, host='0.0.0.0', port=5000)
