import serial
ser = serial.Serial("/dev/ttyUSB0",9600)
print(ser.name)
while True:
    ser.write(b"hello")
