import serial
import time

print("Opening COM10...")
print("Reset the board now and watch the e-ink display!\n")

try:
    ser = serial.Serial('COM10', 115200, timeout=1)
    time.sleep(0.5)
    
    for i in range(30):
        if ser.in_waiting:
            data = ser.read(ser.in_waiting).decode('utf-8', errors='ignore')
            print(data, end='')
        time.sleep(0.5)
    
    ser.close()
except Exception as e:
    print(f"Error: {e}")
