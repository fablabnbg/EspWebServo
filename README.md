# EspWebServo
An rc-servo controlled by an ESP8266 via wifi commands.

The ESP is both access point and wifi-station. The station can be configured to join an existing SSID by selecting one of the header files in the wifi-settings folder. Not all header files listed in EspWebServo.ino are found in wifi-settings to protect some real-life credentials. Take the existing ones to model your own header file.

The access-point is always active to reach the device, and to reconfigure the network for the station interface.
The access-point's own SSID is configured via AP_DEFAULT_SSID (default 'jw-ESP-ADC', as I was playing with an ESP-ADC device. Please change this). It serves DHCP and has a web-interface on http://192.168.4.1/

The serial console, if connected prints debug output.

## Hardware & circuit

The following components were used:

* ESP-12E Module from AI-Thinker http://de.aliexpress.com/item/Esp8266-WiFi-series-of-model-ESP-12-ESP-12F-esp12F-esp12-authenticity-guaranteed/32468324806.html
* Standard Servo Modelcraft MC410 from conrad.de
* Step down voltage regulator from http://de.aliexpress.com/item/Free-Shipping-5PCS-Ultra-small-power-supply-module-DC-DC-BUCK-3A-adjustable-buck-module-regulator/1727878724.html
* 4x Resistor 2K2 1/8W
* 2S LiPo 7.4V 1.15 Ah from conrad.de
* 2 Jumper wires Male/Female
* Connector female 4pin.


Lipo Modifications:

* The big connector on the lipo (4mm pitch, male(!)) was replaced with a 4pin female pitch 2.5mm
so that we can charge the cells using a balancer and supply 7.4 V  to the servo.

Servo modifications: 

* The standard 3pin female servo connector was replaced with the two male ends of the jumper wires, to connect to the servo.
* The housing was prepared to mount the ESP-12 Module using a milling machine. Diameter of the holes is 0.6mm, pitch is 2mm and 14mm across. The back side was leveled down by 0.7mm for a perfect fit of the the Module. Not really necessary.
See drawing and Photos.


## Network access
When the station connects to an existing network, it tries to reach the time server configured as NTP_SERVER_NAME (default 'time.nist.gov'). After a first success, the device has a real-time clock, and prints the time every 5 seconds on the debug output.

The servo can be moved by accessing e.g.

* http://192.168.4.1/servo?id=1&pos=0
* http://192.168.4.1/servo?id=1&pos=90
* http://192.168.4.1/servo?id=1&pos=180

A simple TCP protocol for use with e.g. RoboRemo is also supported:

 *   servo 1 pos 0
 *   servo 1 pos 180
 *   servo 1 speed 255
 *   servo 1 speed 0

The network for the station can be reconfigured with e.g.

* http://192.168.4.1/cfg?sta_ssid=FabLab_NBG?sta_pass=****

The web interface also has a nice javascript slider to control the servo, and an admin form to control the configuration.
The web interface is identical on both access-point and station. You can choose any.

Happy hacking!
