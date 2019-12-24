import numpy as np
import matplotlib.pyplot as plot
import math 
import serial
import time
import re

if __name__ == "__main__":
    #Init the lists for 
    theta = []
    r = []
    ser = None
    ins = None
    #Create the figure for the plot and set it to polar
    fig  = plot.figure()
    fig.add_subplot(111, projection='polar')
    #Try to open the connection with the serial port
    while ser == None:
        try:
            #Open port at 9600,8,N,1 no timeout
            ser = serial.Serial('/dev/rfcomm0')
        except:
            print("Connection error. Unable to connect with ATMEGA328P")
            time.sleep(2)
    
    while (ins != 'y'):
        ins = input('Start  mapping? [y/n]: ')

    print("Loading...")
    time.sleep(1)
    #Send char for enable the atmega328p 
    ser.write(b'A')

    #read line from serial port
    line = ser.readline().strip()
    coordinate = line.decode('ascii')
    #Remove special char
    coordinate = coordinate.replace('\r', '')
    coordinate = coordinate.replace('\n', '')
    #print("Line: {}".format(coordinate))
    while (coordinate != 'E'):
        #Get value from the string
        degree = re.split('[;]', coordinate)[0]
        distance = re.split('[;]', coordinate)[1]
        #Save and convert value on float
        theta.append(math.radians(float(degree)))
        r.append(float(distance))
        print("\nDegree: {} Distance: {}".format(degree, distance))

        #read line from serial port
        line = ser.readline().strip()
        coordinate = line.decode('ascii')
        #Remove special char
        coordinate = coordinate.replace('\r', '')
        coordinate = coordinate.replace('\n', '')

    plot.polar(theta, r)
    # Display the mapping
    ser.close()
    plot.show()
