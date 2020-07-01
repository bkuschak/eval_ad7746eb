# Controlling the Analog Devices AD7746 Evaluation Board under Linux

The Analog Devices [EVAL_AD7746EB](https://www.analog.com/media/en/technical-documentation/evaluation-documentation/EVAL-AD7746EB.PDF) 
is a low cost evaluation board for the AD7746 capacitance to digital converter.
This board uses a Cypress FX2 as a USB-I2C bridge. The kit ships with a Windows
GUI that provides basic control of the device. The GUI is written in LabView and
Analog has provided [this code](https://ez.analog.com/data_converters/precision_adcs/w/documents/3398/can-you-send-me-the-ad7746-labview-source-code)
as a demo. They do not provide support for communicating with this board using 
other means. 

This code provides a simple example of how to communicate with the board using
fxload and libusb in C under Linux, using the Analog-provided FX2 firmware.

## Install

1) Install prerequisites:

        sudo apt-get install libusb-dev
        sudo apt-get install fxload

2) Optional: You can skip this step and just use the firmware file in this repo.
Download and install the Analog Devices 
[EVAL-AD7746 software](https://www.analog.com/en/design-center/evaluation-hardware-and-software/evaluation-boards-kits/eval-ad7746.html#eb-relatedsoftware) 
for Windows. Grab this firmware file and copy it to ./firmware/Icv_usb.hex:

        C:\Program Files (x86)\Analog Devices\AD7745-46EB\data\Icv_usb.hex

3) Build and install.

        make
        sudo make install

4) Plug the board into USB. Run the example control program. 

        ./eval_ad7746eb

    or log data to a file:

        ./eval_ad7746eb > data.log 2>/dev/null

You can modify the config_ad7746() and config_board() functions as needed for 
your application.

## Troubleshooting

When the eval board is plugged in, the udev rule will invoke fxload to load the
firmware. You can check that the device was detected:

```
$ lsusb -d 0456:b481
Bus 001 Device 003: ID 0456:b481 Analog Devices, Inc.
```
You should see something like this in syslog indicating firmware was loaded:

```
$ tail /var/log/syslog
Jul  1 16:19:59 hostname kernel: [74691.005245] usb 1-1: new high-speed USB device number 16 using musb-hdrc
Jul  1 16:19:59 hostname kernel: [74691.153842] usb 1-1: New USB device found, idVendor=0456, idProduct=b481, bcdDevice= 0.00
Jul  1 16:19:59 hostname kernel: [74691.153890] usb 1-1: New USB device strings: Mfr=0, Product=0, SerialNumber=0
Jul  1 16:20:00 hostname root: AD7746EB firmware loaded.
```

There was a note in the Analog docs that the USB microcontroller would lock up
if the I2C communication fails. I did not see any failures, but beware.

If the device is unplugged or a communication failure occurs the program will 
exit.

The settings in config_ad7746() are specific to my application. You will need to
change them for your hardware.

