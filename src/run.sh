#!/bin/sh
avr-gcc -g -Os -mmcu=atmega328p -c $1.c
avr-gcc -g -mmcu=atmega328p -o $1.elf $1.o
avr-objcopy -j .text -j .data -O ihex $1.elf $1.hex 

#Load on the board atmega328p
avrdude -p m328p -c usbasp -P usb -U flash:w:"$1.hex":i
