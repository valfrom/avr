#CPU=atmega8
#AVRDUDE=avrdude
AVRDUDE=/Applications/Arduino.app/Contents/Resources/Java/hardware/tools/avr/bin/avrdude
CPU=atmega328p
CONF=/Applications/Arduino.app/Contents/Resources/Java/hardware/tools/avr/etc/avrdude.conf
PORT=/dev/cu.usbmodem1421
MACROS=-D__DELAY_BACKWARD_COMPATIBLE__ -DF_CPU=16000000 -DGLCD_USE_AVR_DELAY -DGLCD_DEVICE_AVR8 -DGLCD_CONTROLLER_PCD8544 -DGLCD_RESET_TIME=1

# all-glcd: main.o glcd.o
# 	avr-gcc -mmcu=$(CPU) main.o glcd.o AVR8.o graphics.o text.o PCD8544.o -Os -o timer.bin
# 	avr-objcopy -j .text -O ihex timer.bin timer.hex
# 	avr-size timer.bin 


all: main.o n5110.o
	avr-gcc -mmcu=$(CPU) main.o n5110.o -Os -o timer.bin
	avr-objcopy -j .text -O ihex timer.bin timer.hex
	avr-size timer.bin 

upload: all
	$(AVRDUDE) -p $(CPU) -cstk500v1 -P $(PORT) -b19200 -Uflash:w:timer.hex:i
	
burn:	all
	$(AVRDUDE) -C $(CONF) -q -q -p $(CPU) -carduino -P $(PORT) -b115200 -D -Uflash:w:timer.hex:i

main.o: main.cpp
#	avr-gcc -mmcu=$(CPU) $(MACROS) -c main.cpp -Os -I glcd
	avr-gcc -mmcu=$(CPU) $(MACROS) -c main.cpp -Os -I n5110

n5110.o:
	avr-gcc -mmcu=$(CPU) $(MACROS) -c nokia5110/n5110.c -Os

# glcd.o:
# 	avr-gcc -mmcu=$(CPU) $(MACROS) -c glcd/glcd.c -c glcd/devices/AVR8.c -c glcd/graphics.c -c glcd/graphs.c -c glcd/text.c -c glcd/controllers/PCD8544.c -Os

clean:
	rm -rf *.o timer.bin timer.hex
