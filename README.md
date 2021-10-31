# portal-gun-4
Portal Gun IV: Cake is My Weakness


Gun:

[Raspberry Pi 4B](https://www.raspberrypi.org/) + [Laser Beam Pro C200](http://laserbeampro.com/) + Pi NoIR Camera V2 + 3W IR Emitters

CAD Design inspired by [Kirby Downey](https://kirbydowney.com/)

5Ghz Bridge via hostapd + [Fe-Pi Audio Z I2S Sound Card](https://fe-pi.com/) + Onboard BT tethering

[Pololu MinIMU-9 v2](https://www.pololu.com/product/1268) + ADS1115 for Battery Meter and Temp Sensing

4S 5000mAh Lipo + 3 Amp 5v regulator (3x, one for Projector, one for CPU, one for Lighting and Sound)

APA102 LEDs + PAM8302 Class D Amplifier

KMS + EGL + GLES3 thanks to:

[kmscube](https://gitlab.freedesktop.org/mesa/kmscube/) + [OpenGL® ES 3.0 Programming Guide](https://github.com/danginsburg/opengles3-book) + [Modern OpenGL Tutorial Text Rendering 02](https://en.wikibooks.org/wiki/OpenGL_Programming/Modern_OpenGL_Tutorial_Text_Rendering_02)
 
Phone:

[QPython3](http://qpython.com/) + [Requests](http://docs.python-requests.org/en/latest/) + Bluetooth

Website:

[Apache](http://httpd.apache.org/) + [PHP](http://php.net/) + [mySQL](https://www.mysql.com/) on [Ubuntu](http://www.ubuntu.com/)

OS:
```
Raspberry Pi OS Lite
Release date: May 7th 2021
Kernel version: 5.10
Size: 444 MB
SHA-256: c5dad159a2775c687e9281b1a0e586f7471690ae28f2f2282c90e7d59f64273c
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
dos2unix libdrm-dev libgles2-mesa-dev libgbm-dev ifstat python-dbus apt-file libbluetooth-dev
```
Services:

```
systemctl unmask hostapd
systemctl enable hostapd
systemctl start hostapd
```
```
systemctl mask systemd-rfkill.socket systemd-rfkill.service
```
```
systemctl mask rpi-eeprom-update
systemctl disable apt-daily.service
systemctl disable apt-daily.timer
systemctl disable apt-daily-upgrade.timer
systemctl disable apt-daily-upgrade.service
```
