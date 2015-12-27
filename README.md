# EspWebServo
An rc-servo controlled by an ESP8266 via wifi commands.

The ESP is both access point and wifi-station. The station can be configured to join an existing SSID by selecting one of the header files in the wifi-settings folder. The access-point is always active to reach the device, and to reconfigure the SSID for the station.

The access-point serves DHCP and has a web-interface on http://192.168.4.1/

The serial console, if connected prints debug output.

When the station connects to an existing network, it tries to reach the time server configured as NTP_SERVER_NAME (default 'time.nist.gov'). After a first success, the device has a real-time clock, and prints the time every 5 seconds on the debug output.

The servo can be moved by accessing e.g.

* http://192.168.4.1/set?name=servo&pos=0
* http://192.168.4.1/set?name=servo&pos=90
* http://192.168.4.1/set?name=servo&pos=180


Happy hacking!
