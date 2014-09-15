xf86-input-inexio
=================
In a similar vein to the inexio driver also on this github account this is my X86 inexio driver, I wrote it to get around the need for inputattach with the kernel level driver.

It is an import from my original repo http://repo.or.cz/w/xf86-input-inexio.git

There is a proper Kernel driver in the mainline kernel to handle the smaller 17" touchscreens and it should be preferred over this driver.

If you want to use this driver you would have to blacklist the other USB driver as Inexio used the same USB id for all of their devices.

The USB protocol was exactly the same as Inexio's original serial protocol and USB support was provided with an onboard serial to USB converter.

This is why this driver connects to the serial device /dev/ttyACM0.
