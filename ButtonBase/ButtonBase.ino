//Base sketch of DashButton
//@Autor Kulasov Vladislav
//kulasov[@]gmail.com

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <EEPROM.h>

MDNSResponder mdns;

ESP8266WebServer server(80);
//define buttons
const int ledBluePin = 13;
const int ledRedPin = 14;
const int ledGreenPin = 12;
const int powerPin = 5;
const int buttonPin = 4;

//define blink
const int blinkAmount = 30;
const int blinkPeriodMs = 200;
const int blinkProgAmount = 300;
const int blinkProgPeriodMs = 500;

#define HOST "button"
#define STA_SSID_DEFAULT "HOME-WIFI"
#define STA_PASSWORD_DEFAULT ""
#define AP_SSID_DEFAULT "button"
#define AP_PASSWORD_DEFAULT ""
#define PATH_DEFAULT "index.html"
#define HOST_DEFAULT "127.000.000.001"
#define EEPROM_START 0
boolean error = false;
boolean progMode = false;

uint32_t memcrc; uint8_t *p_memcrc = (uint8_t*)&memcrc;

struct eeprom_data_t {
  char ssid[16];
  char pass[16];
  char path[96];
  char host[15];
} eeprom_data;

void handleRoot() {
  for (uint8_t i = 0; i < server.args(); i++) {
    if (server.argName(i) == "ssid") {
      server.arg(i).toCharArray(eeprom_data.ssid, sizeof(eeprom_data.ssid));
    }
    if (server.argName(i) == "pass") {
      server.arg(i).toCharArray(eeprom_data.pass, sizeof(eeprom_data.pass));
    }
    if (server.argName(i) == "path") {
      server.arg(i).toCharArray(eeprom_data.path, sizeof(eeprom_data.path));
    }
    if (server.argName(i) == "host") {
      server.arg(i).toCharArray(eeprom_data.host, sizeof(eeprom_data.host));
    }

    writeSettingsESP();
  }

  String page = "<html><body><table><tbody><form method=\"get\" action=\"/\">";
  page += "<tr><td>Settings</td></tr><tr><td>wifi ssid:</td><td><input type=\"text\" name=\"ssid\" size = 16 value=\"";
  page += eeprom_data.ssid;
  page += "\"/></td></tr>";
  page += "<tr><td>wifi password:</td><td><input type=\"text\" name=\"pass\" size = 16/></td></tr>";
  page += "<tr><td>client id:</td><td><input type=\"text\" name=\"path\"/ size = 96 value=\"";
  page += eeprom_data.path;
  page += "\"></td></tr>";
  page += "<tr><td>host:</td><td><input type=\"text\" name=\"host\"/ size = 15 value=\"";
  page += eeprom_data.host;
  page += "\"></td></tr>";
  page += "<tr><td></td><td><input type=\"submit\" value=\"Submit\"/></td></tr>";
  page += "</form>";
  page += "<tr><td><a href=\"/test\">TEST</a></td>";
  page += "<td><a href=\"/off\">OFF</a></td></tr>";
  page += "</tbody></table>";
  page += "<body></html>";
  server.send(200, "text/html", page);
}

void handleTest() {
  send();
}

void turnOff() {
  Serial.println("Turn off");
  digitalWrite(powerPin, LOW);
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  Serial.println(message);
  server.send(404, "text/plain", message);
}

void setup(void) {
  pinMode(powerPin, OUTPUT);
  digitalWrite(powerPin, HIGH);// turn on

  pinMode(ledBluePin, OUTPUT);
  digitalWrite(ledBluePin, HIGH);
  pinMode(ledRedPin, OUTPUT);
  pinMode(ledGreenPin, OUTPUT);
  pinMode(buttonPin, INPUT);

  Serial.begin(115200);

  double timePeriod =  millis() + 5000;
  Serial.println("\nButton pressed ");
  while (digitalRead(buttonPin) == LOW) {
    delay(500);
    Serial.print(".");
    if (timePeriod < millis()) {
      break;
    }
  }

  Serial.println("");
  readSettingsESP();

  if (timePeriod > millis()) {
    progMode = false;
    Serial.println("Set to STA mode");
    WiFi.mode(WIFI_STA);

    WiFi.begin(eeprom_data.ssid, eeprom_data.pass);
    Serial.println("\nConnecting");
    // Wait for connection
    double timePeriodWaitConnect =  millis() + 30000;
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
      if (timePeriodWaitConnect < millis()) {
        error = true;
        digitalWrite(ledBluePin, LOW);
        return;
      }
    }

    Serial.print("\nConnected to ");
    Serial.println(eeprom_data.ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    if (mdns.begin("esp8266", WiFi.localIP())) {
      Serial.println("MDNS responder started");
    }

    send();
  } else {
    progMode = true;
    if (WiFi.status() == WL_CONNECTED) {
      WiFi.disconnect();
    }
    Serial.print("\nInit AP");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID_DEFAULT, AP_PASSWORD_DEFAULT);
    delay(200);
    Serial.print("\nAP IP address: ");
    IPAddress myIP = WiFi.softAPIP();
    Serial.print(myIP);
    Serial.println("");
    MDNS.begin(HOST);
    server.on("/", handleRoot);
    server.on("/test", handleTest);
    server.on("/off", turnOff);

    server.onNotFound(handleNotFound);

    server.begin();
    Serial.println("HTTP server started");
  }
}

int counter = 0;
double timeBlink = 0;
double period = 200;
bool powerOn = false;
void loop(void) {
  if (digitalRead(buttonPin) == LOW & powerOn) {
    Serial.println("LOW + power on");
    turnOff();
  } else {
    if (digitalRead(buttonPin) == HIGH) {
      powerOn = true;
    }
  }

  if (timeBlink < millis()) {
    int ledColor = error ? ledRedPin : progMode ? ledBluePin : ledGreenPin;
    if (digitalRead(ledColor) == HIGH) {
      digitalWrite(ledColor, LOW);
    } else {
      digitalWrite(ledColor, HIGH);
    }

    timeBlink = millis() + period;
    counter++;
  }

  if (progMode) {
    server.handleClient();
    period = blinkProgPeriodMs;
    if (counter > blinkProgAmount) {
      counter = 0;
      turnOff();
    }
  } else {
    period = blinkPeriodMs;
    if (counter > blinkAmount) {
      counter = 0;
      turnOff();
    }
  }
}

void send(void) {
  Serial.print("connecting to ");
  Serial.println(eeprom_data.host);

  // Use WiFiClient class to create TCP connections
  WiFiClient client;
  const int httpPort = 80;
  if (!client.connect(eeprom_data.host, httpPort)) {
    Serial.println("connection failed");
    error = true;
    digitalWrite(ledBluePin, LOW);
    return;
  }

  // We now create a URI for the request
  String url = "/";
  url += eeprom_data.path;

  Serial.print("Requesting URL: ");
  Serial.println(url);

  // This will send the request to the server
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "HOST: " + eeprom_data.host + "\r\n" +
               "Connection: close\r\n\r\n");
  delay(10);
  // Read all the lines of the reply from server and print them to Serial
  bool statusOK = false;
  while (client.available()) {
    String line = client.readStringUntil('\r');
    if (strstr(line.c_str(), "200 OK") > 0) {
      statusOK = true;
    }
    Serial.print(line);
  }

  digitalWrite(ledBluePin, LOW);
  error = !statusOK;
}

//EEPROM was found in the internet 
boolean setEEPROM = false;
static PROGMEM prog_uint32_t crc_table[16] = {
  0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac, 0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
  0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c, 0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
};

// ----------------------------------- readSettingsESP -----------------------------------
void readSettingsESP()
{
  int i;
  uint32_t datacrc;
  byte eeprom_data_tmp[sizeof(eeprom_data)];

  EEPROM.begin(sizeof(eeprom_data) + sizeof(memcrc));

  for (i = EEPROM_START; i < sizeof(eeprom_data); i++)
  {
    eeprom_data_tmp[i] = EEPROM.read(i);
  }

  p_memcrc[0] = EEPROM.read(i++);
  p_memcrc[1] = EEPROM.read(i++);
  p_memcrc[2] = EEPROM.read(i++);
  p_memcrc[3] = EEPROM.read(i++);

  datacrc = crc_byte(eeprom_data_tmp, sizeof(eeprom_data_tmp));

  if (memcrc == datacrc)
  {
    setEEPROM = true;
    memcpy(&eeprom_data, eeprom_data_tmp,  sizeof(eeprom_data));
  }
  else
  {
    strncpy(eeprom_data.ssid, STA_SSID_DEFAULT, sizeof(STA_SSID_DEFAULT));
    strncpy(eeprom_data.pass, STA_PASSWORD_DEFAULT, sizeof(STA_PASSWORD_DEFAULT));
    strncpy(eeprom_data.path, PATH_DEFAULT, sizeof(PATH_DEFAULT));
    strncpy(eeprom_data.host, HOST_DEFAULT, sizeof(HOST_DEFAULT));
  }

}
// ----------------------------------- writeSettingsESP -----------------------------------
void writeSettingsESP()
{
  int i;
  byte eeprom_data_tmp[sizeof(eeprom_data)];

  EEPROM.begin(sizeof(eeprom_data) + sizeof(memcrc));

  memcpy(eeprom_data_tmp, &eeprom_data, sizeof(eeprom_data));

  for (i = EEPROM_START; i < sizeof(eeprom_data); i++)
  {
    EEPROM.write(i, eeprom_data_tmp[i]);
  }
  memcrc = crc_byte(eeprom_data_tmp, sizeof(eeprom_data_tmp));

  EEPROM.write(i++, p_memcrc[0]);
  EEPROM.write(i++, p_memcrc[1]);
  EEPROM.write(i++, p_memcrc[2]);
  EEPROM.write(i++, p_memcrc[3]);


  EEPROM.commit();
}
// ----------------------------------- crc_update -----------------------------------
unsigned long crc_update(unsigned long crc, byte data)
{
  byte tbl_idx;
  tbl_idx = crc ^ (data >> (0 * 4));
  crc = pgm_read_dword_near(crc_table + (tbl_idx & 0x0f)) ^ (crc >> 4);
  tbl_idx = crc ^ (data >> (1 * 4));
  crc = pgm_read_dword_near(crc_table + (tbl_idx & 0x0f)) ^ (crc >> 4);
  return crc;
}
// ----------------------------------- crc_byte -----------------------------------
unsigned long crc_byte(byte *b, int len)
{
  unsigned long crc = ~0L;
  uint8_t i;

  for (i = 0 ; i < len ; i++)
  {
    crc = crc_update(crc, *b++);
  }
  crc = ~crc;
  return crc;
}
// -----------------------------------  -----------------------------------

