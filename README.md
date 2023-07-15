# portal-gun-4
Portal Gun IV


Gun:

[Raspberry Pi 4B](https://www.raspberrypi.org/) + [Laser Beam Pro C200](http://laserbeampro.com/) + [Pi NoIR Camera V2](https://www.raspberrypi.com/products/pi-noir-camera-v2/) + 3W IR Emitters

CAD Design inspired by [Kirby Downey](https://kirbydowney.com/)

5Ghz Bridge with hostapd via onboard WiFi + Fe-Pi Audio Z I2S Sound Card + USB CSR BT Dongle for tethering (avoids onboard WiFi/BT coexistence issues)

[Pololu MinIMU-9 v2](https://www.pololu.com/product/1268) + ADS1115 for Battery Meter and Temp Sensing

[4S 2200mAh LiPo](https://hobbyking.com/en_us/turnigy-2200mah-4s-20c-lipoly-pack-w-xt60-connector.html) + 3x [Pololu D36V28F5 5V 3.2A Regulators](https://www.pololu.com/product/3782) (Projector, CPU, Lighting and Sound)

APA102 LEDs + PAM8302 Class D Amplifier + ACS712 Current Sensor

KMS + EGL + GLES3 thanks to:

[kmscube](https://gitlab.freedesktop.org/mesa/kmscube/) + [OpenGL® ES 3.0 Programming Guide](https://github.com/danginsburg/opengles3-book) + [Modern OpenGL Tutorial Text Rendering 02](https://en.wikibooks.org/wiki/OpenGL_Programming/Modern_OpenGL_Tutorial_Text_Rendering_02)
 
Phone:

[QPython3](http://qpython.com/) + [Requests](http://docs.python-requests.org/en/latest/) + Bluetooth

Website:

[Apache](http://httpd.apache.org/) + [PHP](http://php.net/) + [mySQL](https://www.mysql.com/) on [Ubuntu](http://www.ubuntu.com/)

OS:
```
Raspberry Pi OS Lite
Release date: May 3rd 2023
System: 64-bit
Kernel version: 6.1
Debian version: 11 (bullseye)
Size: 308MB
Show SHA256 file integrity hash:
bf982e56b0374712d93e185780d121e3f5c3d5e33052a95f72f9aed468d58fa7
```
Packages:

```
gstreamer1.0-plugins-base gstreamer1.0-plugins-good gstreamer1.0-plugins-bad
libvisual-0.4-plugins gstreamer1.0-tools gstreamer1.0-plugins-ugly 
libgstreamer-plugins-base1.0-dev libgstreamer1.0-dev gstreamer1.0-alsa gstreamer1.0-libav
```
```
apache2 php libapache2-mod-php
```
```
dos2unix libdrm-dev libgles2-mesa-dev libgbm-dev ifstat python3-dbus libbluetooth-dev
hostapd libi2c-dev screen apt-file
```
Services:

```
systemctl unmask hostapd
systemctl enable hostapd
systemctl start hostapd
```
```
systemctl mask systemd-rfkill.socket systemd-rfkill.service
sudo apt purge libpam-chksshpwd
sudo apt remove libpam-chksshpwd
sudo apt purge rfkill
```
```
systemctl mask rpi-eeprom-update
systemctl disable apt-daily.service
systemctl disable apt-daily.timer
systemctl disable apt-daily-upgrade.timer
systemctl disable apt-daily-upgrade.service
sudo rpi-eeprom-update -a
```
