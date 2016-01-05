/*
 * EspWebServo -- a true Internet Of Things device. esp8266 drives one (or more) servo(s) controlled over TCP.
 * 
 * 2015-12-26 (c) juewei@fabfolk.com
 * Distribute under GPL-2.0 or ask.
 * 
 * FIXME: the upload code does not yet work as advertised:
 *   To upload through terminal you can use: curl -F "image=@firmware.bin" esp8266-webupdate.local/update
 *   The upload slows down, and comes to a halt at ca 98% -- never finishes.
 * 
 * CAUTION: call 'make' before compiling this.
 * Added a roboRemo server. Cannot use stock EthernetServer. Must use ESP8266Wifi, WifiServer, ...
 *
 * FIXME: use httpdUrlDecode() or similar instead of String.replace('+', ' ');
 *
 * Syntax of commands for the remoRobo interface:
 *   servo 1 pos 0
 *   servo 1 pos 180
 *   servo 1 speed 255
 *   servo 1 speed 0
 */

extern "C" {
#include <user_interface.h>
#include <sntp.h>       // we want to know the time....
#include "slider.html.h"    // const char[] slider_html
#include "admin.html.h"     // const char[] admin_html
}

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <Servo.h>

struct wifi_cred {
  char *ssid;
  char *pass;
} wifi_creds_list[] = {
#include "wifi-settings/list-of-all.h"
  { NULL, NULL }
};

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
#ifndef SERVO_GPIOS
// # define SERVO_GPIOS 2, 13, 4, 5
# define SERVO_GPIOS 5,2
#endif
#ifndef SERVO_SPEED
# define SERVO_SPEED 250
#endif
#ifndef ROBO_PORT
# define ROBO_PORT 9876
#endif

#define MAX_SERVO_ID 4

const char* host = "esp8266-webupdate";
const char* ssid = NULL;     //  = STA_DEFAULT_SSID;
const char* password = NULL; //  = STA_DEFAULT_PASSWORD;

String ap_ssid = AP_DEFAULT_SSID;
WiFiServer roboServer(ROBO_PORT);      // RoboRemoFree Android app connects here
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;
Servo myservo[MAX_SERVO_ID];
uint8_t servoSpeed[MAX_SERVO_ID];               // 255=fastest, else 255-X msec delay per single degree 
uint8_t servoGPIO[MAX_SERVO_ID] = { SERVO_GPIOS };  // GPIO 3 never seen, but 13 often is.

char *flashSizeStr();
void start_sntp();
void handleNotFound();
void handleRoot();
void handleTestSVG();
void handleServo();
void handleStats();
void handleCfg();
void roboServer_handleClient();

void setup(void){

  Serial.begin(115200);
  Serial.print("\nFlash: ");
  Serial.println(flashSizeStr());
  Serial.println("Build Date: " __DATE__ " " __TIME__);


  uint8_t mac[6];
  char macstr3[10];
  wifi_get_macaddr(SOFTAP_IF, mac);
  sprintf(macstr3, "-%02X%02X%02X", mac[3], mac[4], mac[5]);
  ap_ssid += String(macstr3);
  
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(ap_ssid.c_str());   // did not work after begin. try before...

  struct wifi_cred *w = &wifi_creds_list[0];

  while (!ssid && w->ssid != NULL) {  
    for (int i = 0; i < 5; i++) {
      WiFi.begin(w->ssid, w->pass);
      Serial.printf("%d/5: trying wifi %s ...\n", i, w->ssid);
      delay(1000);
      if (WiFi.waitForConnectResult() == WL_CONNECTED) {
        ssid = w->ssid;
        password = w->pass;
        break;
      }
    }
    w++;
  }
  // WiFi.softAP(ap_ssid.c_str());
  
  start_sntp();
  
  MDNS.begin(host);

  httpUpdater.setup(&httpServer);
  httpServer.on("/", handleRoot);
  httpServer.on("/test.svg", handleTestSVG);
  httpServer.on("/servo", handleServo);
  httpServer.on("/cfg", handleCfg);
  httpServer.on("/stats", handleStats);
  httpServer.on("/slider", HTTP_GET, [](){ httpServer.send(200, "text/html", (char *)slider_html); });
  httpServer.on("/admin",  HTTP_GET, [](){ httpServer.send(200, "text/html", (char *)admin_html); });
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
  Serial.println("HTTP server port: 80");
  
  roboServer.begin();  // do not use stock EthernetServer, this crashes!
  roboServer.setNoDelay(true);
  Serial.printf("Robo server port: %d\n", ROBO_PORT);
  
  MDNS.addService("http", "tcp", 80);
  // Serial.printf("HTTPUpdateServer ready! Open http://%s.local/update in your browser\n", host);
  for (uint8_t i = 1; i <= MAX_SERVO_ID; i++) {
    if (servoGPIO[i-1])
      myservo[i-1].attach(servoGPIO[i-1]);
    servoSpeed[i-1] = SERVO_SPEED;
  }
}

int last_millis = 0;

void loop(void){
  int m = millis();
  httpServer.handleClient();
  delay(1);
  roboServer_handleClient();
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

void handleRoot() {
  httpServer.send ( 200, "text/html",
"<html>\
  <head>\
    <meta charset=\"utf-8\" />\
    <title>" AP_DEFAULT_SSID "</title>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
    </style>\
  </head>\
  <body>\
    <p><a href=\"/slider\">Servo Control</a></p>\
    <p>&nbsp;<a href=\"/servo?id=1&pos=0\">Servo Left</a></p>\
    <p>&nbsp;<a href=\"/servo?id=1&pos=90\">Servo Center</a></p>\
    <p>&nbsp;<a href=\"/servo?id=1&pos=180\">Servo Right</a></p>\
    <br/>\
    <p><a href=\"/stats\">Statistics</a> (<a href=\"/stats.json\">json</a>)</p>\
    <p><a href=\"/admin\">Network Administration</a>\
    <p><br/>Follow us on <a href=\"http://github.com/fablabnbg/EspWebServo\">github</a>!\
    <p><small>(C) 2015 Jürgen Weigert &lt;<a href=\"mailto:juewei@fabfolk.com\">juewei@fabfolk.com</a>&gt;</small>\
  </body>\
</html>");
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

void servoSpeedSet(uint8_t id, uint8_t speed) {
  if (id < 1) id = 1; if (id > MAX_SERVO_ID) id = MAX_SERVO_ID;
  servoSpeed[id-1] = speed;  
}

uint8_t servoPos(uint8_t id) {
  if (id < 1) id = 1; if (id > MAX_SERVO_ID) id = MAX_SERVO_ID;
  return myservo[id-1].read();  
}

void servoMove(uint8_t id, uint8_t pos) {
  if (id < 1) id = 1; if (id > MAX_SERVO_ID) id = MAX_SERVO_ID;
  if (pos > 180) pos = 180;
  if (servoSpeed[id-1] < 255) {
    int p = servoPos(id);
    Serial.printf("moveing servo %d from %d to %d speed %d\n", id, p, pos, servoSpeed[id-1]);
    if (p < pos) {
      for (; p < pos; p += 1) {
        myservo[id-1].write(p);
        delay(255-servoSpeed[id-1]);
      }
    } else {
      for (; p > pos; p -= 1) {
        myservo[id-1].write(p);
        delay(255-servoSpeed[id-1]);      
      }
    }
  }
  Serial.printf("moved servo %d to %d\n", id, pos);
  myservo[id-1].write(pos);
}

void handleServo() {
  char temp[100];

  String servo_id = "1";
  if (httpServer.hasArg("id")) { servo_id = httpServer.arg("id"); }
  int id = servo_id.toInt();
  
  int speed = -1;
  if (httpServer.hasArg("speed")) speed = httpServer.arg("speed").toInt();

  int pos = -1;
  if (httpServer.hasArg("pos")) pos = httpServer.arg("pos").toInt();

  if (speed >= 0) servoSpeedSet(id, speed);
  
  if (pos >= 0) {
    if (pos > 180) pos = 180;
    servoMove(id, pos);
    snprintf(temp, 100, "set id=%d pos=%d", id, pos);
  } else {
    pos = servoPos(id);
    if (pos > 0) pos += 1;   // FIXME: without +1, all values are off by one. why?
    Serial.printf("servo id=%d query pos=%d\n", id, pos);
    snprintf(temp, 100, "%d", pos);
  }
  httpServer.send ( 200, "text/html", temp);
}

void handleCfg() {
  char temp[100];

  String net_if = "STA";
  if (httpServer.hasArg("if")) { net_if = httpServer.arg("if"); }

  struct station_config sta_conf;
  struct softap_config ap_conf;
  wifi_station_get_config(&sta_conf);
  wifi_softap_get_config(&ap_conf);
    
  String sta_ssid = reinterpret_cast<char*>(sta_conf.ssid);
  String sta_pass = reinterpret_cast<char*>(sta_conf.password);
  String ap_ssid = reinterpret_cast<char*>(ap_conf.ssid);
  String ap_pass = reinterpret_cast<char*>(ap_conf.password);
  
  if (httpServer.hasArg("sta_pass") && !httpServer.arg("sta_pass").equals("****")) {
    sta_pass = httpServer.arg("sta_pass");
    sta_pass.replace('+', ' ');
  }
  if (httpServer.hasArg("sta_ssid")) {
    sta_ssid = httpServer.arg("sta_ssid");
    // FIXME: need proper CGI unescape here!
    sta_ssid.replace('+', ' ');
    int r = WiFi.begin(sta_ssid.c_str(), sta_pass.c_str());
    Serial.printf("STA reconfigured ssid='%s' pass='%s' status=%d\n", sta_ssid.c_str(), sta_pass.c_str(), r);
  }
  if (httpServer.hasArg("ap_pass") && !httpServer.arg("ap_pass").equals("****")) {
    ap_pass = httpServer.arg("ap_pass");
    ap_pass.replace('+', ' ');
  }
  if (httpServer.hasArg("ap_ssid")) {
    ap_ssid = httpServer.arg("ap_ssid");
    ap_ssid.replace('+', ' ');
    WiFi.softAP(ap_ssid.c_str(), ap_pass.c_str());
    Serial.printf("AP reconfigured ssid='%s' pass='%s'\n", ap_ssid.c_str(), ap_pass.c_str());
  }
  
  char json[300];
  // FIXME: how do we construct json with proper escaping?
  snprintf(json, 300, "{\"sta\":{\"ssid\":\"%s\",\"pass\":\"%s\"},\"ap\":{\"ssid\":\"%s\",\"pass\":\"%s\"}}",
       sta_ssid.c_str(), "****", ap_ssid.c_str(), "****");
  httpServer.send(200, "text/json", json);
  Serial.println(json);
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


void handleStats() {
  char temp[600];
  int sec = millis() / 1000;
  int min = sec / 60;
  int hr = min / 60;
  
      
  snprintf ( temp, 600,

"<html>\
  <head>\
    <meta http-equiv='refresh' content='5'/>\
    <meta charset='utf-8' />\
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

void roboCmd(char *buf) {
  while (*buf == '\r' || *buf == '\n' || *buf == '\t' || *buf == ' ') buf++;
  Serial.printf("roboCmd: '%s'\n", buf);
  if (!strncmp(buf, "servo", 5)) {
    buf += 5; while (*buf == ' ' || *buf == '\t') buf++;
    int servo_id = atoi(buf);
    if (servo_id < 1) servo_id = 1;
    if (servo_id > MAX_SERVO_ID) servo_id = MAX_SERVO_ID;
    while (*buf != ' ' && *buf != '\t' && *buf != '\0') buf++;
    while (*buf == ' ' || *buf == '\t') buf++;
    if (!strncmp(buf, "speed", 5)) {
      buf += 5; while (*buf == ' ' || *buf == '\t') buf++;
      servoSpeedSet(servo_id, atoi(buf));
    } else if (!strncmp(buf, "pos", 3)) {
      buf += 3; while (*buf == ' ' || *buf == '\t') buf++;
      servoMove(servo_id, atoi(buf));
    } else {
      Serial.printf("roboCmd: unknown servo cmd: '%s'\n", buf);
    }
  } else {
    Serial.printf("roboCmd: unknown command '%s'\n", buf);
  }
}

#define ROBO_CLIENTS 2
#define ROBO_CLIENT_BUF_SZ 200
WiFiClient roboClient[ROBO_CLIENTS];
unsigned char  roboClientBuf[ROBO_CLIENTS][ROBO_CLIENT_BUF_SZ];
int            roboClientBufLen[ROBO_CLIENTS];

void roboServer_handleClient() {
  WiFiClient newClient = roboServer.available();
  uint8_t i;
  
  if (newClient) {
    boolean slotNeeded = true;
    Serial.println("roboServer accept");
    for (i = 0; i < ROBO_CLIENTS; i++) {
      if ( !roboClient[i] || !roboClient[i].connected() ) {
        if (roboClient[i]) {
          roboClient[i].stop();
          Serial.printf("Bye robo client %d\n", i);
          delay(100);
        }
        roboClient[i] = newClient;
        roboClientBufLen[i] = 0;
        Serial.printf("new robo client %d\n", i);
        slotNeeded = false;
        break;
      }
    }
    if (slotNeeded) {   // no slot available, reject
      newClient.write("500 too many connections\n", 25);
      delay(200); // maybe needed to send?
      newClient.stop();
    }
  }

  // check clients for data
  for (i = 0; i < ROBO_CLIENTS; i++) {  
    if (roboClient[i] && roboClient[i].connected() && roboClient[i].available() ) {
      unsigned char *buf = roboClientBuf[i];
      int len = roboClientBufLen[i];
      // Serial.printf("robo %d available\n", i);
      if (len >= ROBO_CLIENT_BUF_SZ) {
        Serial.printf("robo %d overflow\n", i);
        len = 0;
        roboClientBufLen[i] = 0;
        // CAUTION: next command chunk is most likely a partial command
      }
      int n = roboClient[i].read(buf+len, ROBO_CLIENT_BUF_SZ-len);
      if (n <= 0) continue;
      len += n;
      // Serial.printf("robo %d read %d bytes -> buffer %d bytes\n", i, n, len);
      for (int x = 0; x < len; x++) {
        if (buf[x] == '\n' || buf[x] == '\r' || buf[x] == '\0') {
          // process, shift, rescan
          buf[x] = '\0';
          roboCmd((char *)buf);
          delay(1);   // maybe schedule?
          memmove(buf, buf+x+1, len-x-1);
          len = len-x-1;
          x = 0;      // we continue the for loop, adter shifting, to see if there are more commands.
          roboClientBufLen[i] = len;
          // Serial.printf("robo(%d) remaining %d bytes\n", i, len);
        }
      }
    }
  }

  for (i = 0; i < ROBO_CLIENTS; i++) {
    if (roboClient[i] && !roboClient[i].connected()) {
      // stop() invalidates client, now comparison will evaluate false.
      roboClient[i].stop();
      Serial.printf("bye robo client %d\n", i);
    }
  }
}

