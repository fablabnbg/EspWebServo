/*
  To upload through terminal you can use: curl -F "image=@firmware.bin" esp8266-webupdate.local/update
*/

extern "C" {
#include "user_interface.h"
#include "sntp.h"       // we want to know the time....
}

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <Servo.h>

#include "wifi-settings/home.h"
// #include "wifi-settings/tethering-jw.h"
// #include "wifi-settings/fablabnbg.h"
// #include "wifi-settings/franken-freifunk.h"

#ifndef AP_DEFAULT_SSID
# define AP_DEFAULT_SSID "jw-ESP-ADC"
#endif
#ifndef AP_DEFAULT_PASSWORD
# define AP_DEFAULT_PASSWORD ""
#endif
#ifndef NTP_SERVER_NAME
# define NTP_SERVER_NAME "time.nist.gov"
#endif
#ifndef UTC_OFFSET
# define UTC_OFFSET +1           // Timezone in Germany
#endif
#ifndef SERVO_GPIO
# define SERVO_GPIO 2
#endif

const char* host = "esp8266-webupdate";
const char* ssid = STA_DEFAULT_SSID;
const char* password = STA_DEFAULT_PASSWORD;

String ap_ssid(AP_DEFAULT_SSID);
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;
Servo myservo;

char *flashSizeStr();
void start_sntp();
void handleNotFound();
void handleRoot();
void handleTestSVG();
void handleSet();
void handleStats();

void setup(void){

  Serial.begin(115200);
  Serial.print("\nFlash: ");
  Serial.println(flashSizeStr());
  Serial.println("Build Date: " __DATE__ " " __TIME__);
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(ssid, password);

  while(WiFi.waitForConnectResult() != WL_CONNECTED){
    WiFi.begin(ssid, password);
    Serial.println("WiFi failed, retrying.");
  }

  uint8_t mac[6];
  char macstr3[10];
  wifi_get_macaddr(SOFTAP_IF, mac);
  sprintf(macstr3, "-%02X%02X%02X", mac[3], mac[4], mac[5]);
  ap_ssid += String(macstr3);
  
  start_sntp();
  
  MDNS.begin(host);

  httpUpdater.setup(&httpServer);
  httpServer.on("/", handleRoot);
  httpServer.on("/test.svg", handleTestSVG);
  httpServer.on("/set", handleSet);
  httpServer.on("/stats", handleStats);
  // get heap status, analog input value and all GPIO statuses in one json call
  httpServer.on("/stats.json", HTTP_GET, [](){
    String json = "{";
    json += "\"heap\":"+String(ESP.getFreeHeap());
    json += ", \"analog\":"+String(analogRead(A0));
    json += ", \"gpio\":"+String((uint32_t)(((GPI | GPO) & 0xFFFF) | ((GP16I & 0x01) << 16)));
    json += "}";
    httpServer.send(200, "text/json", json);
    json = String();
  });

  httpServer.onNotFound( handleNotFound);
  httpServer.begin();

  MDNS.addService("http", "tcp", 80);
  Serial.printf("HTTPUpdateServer ready! Open http://%s.local/update in your browser\n", host);
  myservo.attach(SERVO_GPIO);
}

int last_millis = 0;

void loop(void){
  int m = millis();
  httpServer.handleClient();
  delay(1);

  if (m < last_millis || m > (last_millis+5000))
    {
      last_millis = m;
      Serial.print("Ding ");
      Serial.print(WiFi.localIP());
      Serial.print(" ");
      int32 tstamp = sntp_get_current_timestamp();
      Serial.print(sntp_get_real_time(tstamp));
    }
}

char *flashSizeStr() {
  enum flash_size_map f = system_get_flash_size_map();
  switch (f) {
    case FLASH_SIZE_4M_MAP_256_256:     return (char *)"4M_MAP_256_256";
    case FLASH_SIZE_2M:                 return (char *)"2M";
    case FLASH_SIZE_8M_MAP_512_512:     return (char *)"8M_MAP_512_512";
    case FLASH_SIZE_16M_MAP_512_512:    return (char *)"16M_MAP_512_512";
    case FLASH_SIZE_32M_MAP_512_512:    return (char *)"32M_MAP_512_512";
    case FLASH_SIZE_16M_MAP_1024_1024:  return (char *)"16M_MAP_1024_1024";
    case FLASH_SIZE_32M_MAP_1024_1024:  return (char *)"32M_MAP_1024_1024";
    default:                            return (char *)"enum flash_size_map missing";
  }
}

void start_sntp() {
  sntp_setservername(0, (char *)NTP_SERVER_NAME);   // 0, 1, 2 can be set.
  sntp_set_timezone(UTC_OFFSET);
  sntp_init();
}

void handleSet() {
  char temp[100];

  if (!httpServer.hasArg("name")) { httpServer.send(500, "text/plain", "missing parameter: name"); }
  String name = httpServer.arg("name");
  
  int pos = -1;
  if (httpServer.hasArg("pos")) pos = httpServer.arg("pos").toInt();

  if (name.startsWith("servo") && pos >= 0) {
    if (pos > 200) pos = 200;
    myservo.write(pos);
    Serial.printf("set name=%s pos=%d\n", name.c_str(), pos);
  }
  
  snprintf(temp, 100, "<html><body>set name=%s pos=%d</body></html>", name.c_str(), pos);
  httpServer.send ( 200, "text/html", temp);
}

void handleTestSVG() {
  String out = "";
  char temp[100];
  int x, y;
  out += "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\" width=\"400\" height=\"150\">\n";
  out += "<rect width=\"400\" height=\"150\" fill=\"rgb(250, 230, 210)\" stroke-width=\"1\" stroke=\"rgb(0, 0, 0)\" />\n";
  out += "<g stroke=\"lightgray\">\n";
  for (y = 20; y < 150; y+=20) { sprintf(temp, " <line x1=\"0\" y1=\"%d\" x2=\"400\" y2=\"%d\" />\n", y, y); out += temp; }
  out += "</g>\n<g stroke=\"lightgray\">\n";
  for (x = 20; x < 400; x+=20) { sprintf(temp, " <line x1=\"%d\" y1=\"0\" x2=\"%d\" y2=\"150\" />\n", x, x); out += temp; }
  out += "</g><g stroke=\"black\">\n";
  y = rand() % 130;
  for (x = 3; x < 390; x+= 3) {
    int y2 = rand() % 130;
    sprintf(temp, " <line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke-width=\"1\" />\n", x, 140 - y, x + 3, 140 - y2);
    out += temp;
    y = y2;
  }
  out += "</g>\n</svg>\n";

  httpServer.send ( 200, "image/svg+xml", out);
}

void handleRoot() {
  httpServer.send ( 200, "text/html",
"<html>\
  <head>\
    <title>" AP_DEFAULT_SSID "</title>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
    </style>\
  </head>\
  <body>\
    <p><a href=\"/stats\">Statistics</a> (<a href=\"/stats.json\">json</a>)</p>\
    <p><a href=\"/set?name=servo&pos=0\">Servo left</a></p>\
    <p><a href=\"/set?name=servo&pos=90\">Servo center</a></p>\
    <p><a href=\"/set?name=servo&pos=180\">Servo right</a></p>\
  </body>\
</html>");
}

void handleStats() {
  char temp[600];
  int sec = millis() / 1000;
  int min = sec / 60;
  int hr = min / 60;
  
      
  snprintf ( temp, 600,

"<html>\
  <head>\
    <meta http-equiv='refresh' content='5'/>\
    <title>%s</title>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
    </style>\
  </head>\
  <body>\
    <h1>Hello from %s!</h1>\
    <p>Uptime: %02d:%02d:%02d</p>\
    <p>NTP Time: %s</p>\
    <p>IP-Address: <b>%s</b> at SSID <b>%s</b></p>\
    <p>ADC: %d</p>\
    <p>GPIO: 0x%05x</p>\
    <img src=\"/test.svg\" />\
  </body>\
</html>",
    ap_ssid.c_str(), ap_ssid.c_str(),
    hr, min % 60, sec % 60,
    sntp_get_real_time(sntp_get_current_timestamp()),
    WiFi.localIP().toString().c_str(), ssid,
    analogRead(A0),
    (uint32_t)(((GPI | GPO) & 0xFFFF) | ((GP16I & 0x01) << 16))
  );
  httpServer.send ( 200, "text/html", temp );
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += httpServer.uri();
  message += "\nMethod: ";
  message += ( httpServer.method() == HTTP_GET ) ? "GET" : "POST";
  message += "\nArguments: ";
  message += httpServer.args();
  message += "\n";

  for ( uint8_t i = 0; i < httpServer.args(); i++ ) {
    message += " " + httpServer.argName ( i ) + ": " + httpServer.arg ( i ) + "\n";
  }

  httpServer.send ( 404, "text/plain", message );
}
