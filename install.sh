#!/bin/bash

if [ "$EUID" -ne 0 ]
  then echo "Please run as root"
  exit
fi
#install special lirc
/usr/bin/apt-get remove lirc
/usr/bin/dpkg -i *.deb
#install dependencies
/usr/bin/apt-get -f install

confpath=/etc/lirc/lircd.conf.d
sysdpath=/etc/systemd/system

#install configs and systemd
rm -v "$confpath/*.conf"
cp -v configs/*.conf $confpath
cp -v systemd/*service $sysdpath

/bin/systemctl reenable lircd.service
/bin/systemctl reenable lircd-lirc1.service

#build modules
make
make modules_install
depmod

/usr/sbin/armbian-add-overlay gpio-ook-tx-overlay.dts
/usr/sbin/armbian-add-overlay pwm-ir-tx-overlay.dts

echo "please reboot"

exit
