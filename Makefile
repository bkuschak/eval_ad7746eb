INCS = -I/usr/include/libusb-1.0
LIBS = -lusb-1.0
CFLAGS = -O2 -Wall -g

TARGET = eval_ad7746eb
FIRMWARE = firmware/Icv_usb.hex
UDEV_RULES = eval_ad7746eb.rules

$(TARGET): eval_ad7746eb.c
	gcc -o $@ $< $(CFLAGS) $(INCS) $(LIBS)

install:
	install -D -o root -g root -m 644 $(FIRMWARE) /lib/firmware
	install -o root -g root -m 644 $(UDEV_RULES) /etc/udev/rules.d/99-$(UDEV_RULES)
	install -D -o root -g root -m 755 $(TARGET) /usr/bin

clean: 
	rm -f *.o $(TARGET)
