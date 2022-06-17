#!/bin/bash

BOOTCFG="/boot/config.txt"

echo 'Enabling 2ch RS485 HAT'
grep -qxF 'dtoverlay=sc16is752-spi1,int_pin=24' "$BOOTCFG" || echo 'dtoverlay=sc16is752-spi1,int_pin=24' >> "$BOOTCFG"
grep -qxF 'gpio=22,27' "$BOOTCFG" || echo 'gpio=22,27=op,dh'  >> "$BOOTCFG"


echo 'Enabling GPIO shutdown (disables I2C)'
sed -i -e 's/^dtparam=i2c_arm=on.*/dtparam=i2c_arm=off/' /boot/config.txt
grep -qxF 'dtoverlay=gpio-shutdown' "$BOOTCFG" || echo 'dtoverlay=gpio-shutdown,gpio_pin=3'  >> "$BOOTCFG"

echo 'Configuring power LED (default to on)'
grep -qxF 'gpio=5' "$BOOTCFG" || echo 'gpio=5=op,dh'  >> "$BOOTCFG"
