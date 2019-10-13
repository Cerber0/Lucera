#include <ESP8266WiFi.h>
#include <Arduino.h>
//#include <Adafruit_NeoPixel.h> //https://github.com/adafruit/Adafruit_NeoPixel
#include <PubSubClient.h> // https://github.com/knolleary/pubsubclient/releases/tag/v2.3
#include <SimpleTimer.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ESP8266HTTPClient.h>
#include <FS.h>
#include <AutoConnect.h>
//#define FASTLED_ALLOW_INTERRUPTS 0
#include <FastLED.h>

typedef ESP8266WebServer WiFiWebServer;

#define PARAM_FILE      "/param.json"
#define AUX_SETTING_URI "/mqtt_setting"
#define AUX_SAVE_URI    "/mqtt_save"
#define HOME_URI    "/"

//-------- Customise these values -----------
const char* ssid = "Cerber0";
const char* password = "C4nc3rb3r0";
String mqtt_server = "192.168.0.162";
//-------- Customise the above values --------


#define LED_TOTAL 240 // Your Led strip pixel
#define LED_HALF LED_TOTAL/2
#define VISUALS 6
#define analog_pin A0
#define LED_PIN D5
#define LED_PIN2 5
#define online_pin 4

// Define the array of leds

CRGB leds[LED_TOTAL];

SimpleTimer timer;
int myNum = 5;          //Number of times to call the repeatMe function
int myInterval = 30000;  //time between funciton calls in millis

// Command Topic
String mode_com = "/casa/lucera/cmd/mode/fmt/String";
String pattern_com = "/casa/lucera/cmd/pattern/fmt/String";
String color_com = "/casa/lucera/cmd/color/fmt/String";
String cheer_com = "/casa/lucera/cmd/cheer/fmt/String";
String realColor_com = "/casa/lucera/cmd/realcolor/fmt/String";
String telemetry = "/casa/lucera/telemetry/fmt/String";
String  hostName;

int iotStatus = 0;
int modes = 2;
int ledPattern = 0;
int realColor = 0;

int reading;
int previous = LOW;

long times = 0;
long debounce = 200;

uint32_t redValue = 0;
uint32_t greenValue = 0;
uint32_t blueValue = 0;
uint32_t redcheer = 0;
uint32_t greencheer = 0;
uint32_t bluecheer = 0;

uint16_t gradient = 0;
uint16_t thresholds[] = {1529, 1019, 764, 764, 764, 1274};
uint8_t palette = 0;
uint8_t visual = 0;
uint8_t volume = 0;
uint8_t last = 0;

float maxVol = 15;
float avgBump = 0;
float avgVol = 0;
float shuffleTime = 0;

int8_t pos[LED_TOTAL] = {-2};
uint8_t rgb[LED_TOTAL][3] = {0};

bool shuffle = true;
bool bump = true;

bool left = false;
int8_t dotPos = 0;
float timeBump = 0;
float avgTime = 0;

WiFiWebServer Server;
WiFiClient wifiClient;


AutoConnect Portal(Server);
AutoConnectConfig Config;

//Adafruit_NeoPixel strand = Adafruit_NeoPixel(LED_TOTAL, LED_PIN, NEO_GRB + NEO_KHZ800);

// JSON definition of AutoConnectAux.
// Multiple AutoConnectAux can be defined in the JSON array.
// In this example, JSON is hard-coded to make it easier to understand
// the AutoConnectAux API. In practice, it will be an external content
// which separated from the sketch, as the mqtt_RSSI_FS example shows.
static const char AUX_mqtt_setting[] PROGMEM = R"raw(
[
  {
    "title": "MQTT Setting",
    "uri": "/mqtt_setting",
    "menu": true,
    "element": [
      {
        "name": "style",
        "type": "ACStyle",
        "value": "label+input,label+select{position:sticky;left:120px;width:230px!important;box-sizing:border-box;}"
      },
      {
        "name": "header",
        "type": "ACText",
        "value": "<h2>MQTT broker settings</h2>",
        "style": "text-align:center;color:#2f4f4f;padding:10px;"
      },
      {
        "name": "mqttserver",
        "type": "ACInput",
        "value": "",
        "label": "Server",
        "pattern": "^(([a-zA-Z0-9]|[a-zA-Z0-9][a-zA-Z0-9\\-]*[a-zA-Z0-9])\\.)*([A-Za-z0-9]|[A-Za-z0-9][A-Za-z0-9\\-]*[A-Za-z0-9])$",
        "placeholder": "MQTT broker server"
      },
      {
        "name": "channelid_mode",
        "type": "ACInput",
        "label": "Mode Channel ID",
        "pattern": "^(\/[a-zA-Z0-9]([a-zA-Z0-9-])*)*$"
      },
      {
        "name": "channelid_pattern",
        "type": "ACInput",
        "label": "Pattern Channel ID",
        "pattern": "^(\/[a-zA-Z0-9]([a-zA-Z0-9-])*)*$"
      },
      {
        "name": "channelid_color",
        "type": "ACInput",
        "label": "Color Channel ID",
        "pattern": "^(\/[a-zA-Z0-9]([a-zA-Z0-9-])*)*$"
      },
      {
        "name": "channelid_real",
        "type": "ACInput",
        "label": "Real Color Channel ID",
        "pattern": "^(\/[a-zA-Z0-9]([a-zA-Z0-9-])*)*$"
      },
      {
        "name": "channelid_telemetry",
        "type": "ACInput",
        "label": "Telemetry Channel ID",
        "pattern": "^(\/[a-zA-Z0-9]([a-zA-Z0-9-])*)*$"
      },
      {
        "name": "newline",
        "type": "ACElement",
        "value": "<hr>"
      },
      {
        "name": "hostname",
        "type": "ACInput",
        "value": "",
        "label": "ESP host name",
        "pattern": "^([a-zA-Z0-9]([a-zA-Z0-9-])*[a-zA-Z0-9]){1,24}$"
      },
      {
        "name": "save",
        "type": "ACSubmit",
        "value": "Save&amp;Start",
        "uri": "/mqtt_save"
      },
      {
        "name": "discard",
        "type": "ACSubmit",
        "value": "Discard",
        "uri": "/_ac"
      }
    ]
  },
  {
    "title": "MQTT Setting",
    "uri": "/mqtt_save",
    "menu": false,
    "element": [
      {
        "name": "caption",
        "type": "ACText",
        "value": "<h4>Parameters saved as:</h4>",
        "style": "text-align:center;color:#2f4f4f;padding:10px;"
      },
      {
        "name": "parameters",
        "type": "ACText"
      }
    ]
  }
]
)raw";

void getParams(AutoConnectAux& aux) {
  mqtt_server = String(aux["mqttserver"].value).c_str();
  //mqtt_server.trim();
  telemetry = String(aux["channelid_telemetry"].value).c_str();
  //telemetry.trim();
  mode_com = String(aux["channelid_mode"].value).c_str();
  //mode_com.trim();
  pattern_com = String(aux["channelid_pattern"].value).c_str();
  //pattern_com.trim();
  color_com = String(aux["channelid_color"].value).c_str();
  //color_com.trim();
  realColor_com = String(aux["channelid_real"].value).c_str();
  //realColor_com.trim();
  hostName = aux["hostname"].value;
  hostName.trim();
}

// Load parameters saved with  saveParams from SPIFFS into the
// elements defined in /mqtt_setting JSON.
String loadParams(AutoConnectAux& aux, PageArgument& args) {
  (void)(args);
  File param = SPIFFS.open(PARAM_FILE, "r");
  if (param) {
    if (aux.loadElement(param)) {
      getParams(aux);
      Serial.println(PARAM_FILE " loaded");
    }
    else
      Serial.println(PARAM_FILE " failed to load");
    param.close();
  }
  else {
    Serial.println(PARAM_FILE " open failed");
#ifdef ARDUINO_ARCH_ESP32
    Serial.println("If you get error as 'SPIFFS: mount failed, -10025', Please modify with 'SPIFFS.begin(true)'.");
#endif
  }
  return String("");
}

// Save the value of each element entered by '/mqtt_setting' to the
// parameter file. The saveParams as below is a callback function of
// /mqtt_save. When invoking this handler, the input value of each
// element is already stored in '/mqtt_setting'.
// In Sketch, you can output to stream its elements specified by name.
String saveParams(AutoConnectAux& aux, PageArgument& args) {
  // The 'where()' function returns the AutoConnectAux that caused
  // the transition to this page.
  AutoConnectAux&   mqtt_setting = *Portal.aux(Portal.where());
  getParams(mqtt_setting);
  AutoConnectInput& mqttserver = mqtt_setting["mqttserver"].as<AutoConnectInput>();

  // The entered value is owned by AutoConnectAux of /mqtt_setting.
  // To retrieve the elements of /mqtt_setting, it is necessary to get
  // the AutoConnectAux object of /mqtt_setting.
  File param = SPIFFS.open(PARAM_FILE, "w");
  mqtt_setting.saveElement(param, { "mqttserver", "channelid_mode", "channelid_pattern", "channelid_color", "channelid_real", "channelid_telemetry", "hostname" });
  param.close();

  // Echo back saved parameters to AutoConnectAux page.
  AutoConnectText&  echo = aux["web"].as<AutoConnectText>();
  echo.value = "Server: " + mqtt_server;
  echo.value += mqttserver.isValid() ? String(" (OK)") : String(" (ERR)");
  echo.value += "<br>Mode channel ID: " + String(mode_com) + "<br>";
  echo.value += "<br>Pattern channel ID: " + String(pattern_com) + "<br>";
  echo.value += "<br>Color channel ID: " + String(color_com) + "<br>";
  echo.value += "<br>Real color channel ID: " + String(realColor_com) + "<br>";
  echo.value += "<br>Telemetry Channel ID: " + String(telemetry) + "<br>";
  echo.value += "ESP host name: " + hostName + "<br>";

  return String("");
}

void wifiConnect() {
  if (WiFi.status() != WL_CONNECTED) {
  Serial.print("Connecting to ");
  Serial.print(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print("nWiFi connected, IP address: "); Serial.println(WiFi.localIP());
}
}

void callback(char* topic, byte* payload, unsigned int payloadLength) {
  Serial.print("callback invoked for topic: ");
  Serial.println(topic);
  if (!strcmp(topic, mode_com.c_str())) {
    char message_buff[100];
    int i;
    for(i= 0; i < payloadLength; i++) {
      message_buff[i] = payload[i];
      }
      message_buff[i] = '\0';
       String s = String((char*)message_buff);
      modes = s.toInt();
      //sendState();
  }

  if (modes == 2 && (!strcmp(topic, pattern_com.c_str()))) {
    char message_buff[100];
    int i;
    for(i= 0; i < payloadLength; i++) {
      message_buff[i] = payload[i];
      }
      message_buff[i] = '\0';
       String s = String((char*)message_buff);
      ledPattern = s.toInt();
      Serial.print("Led pattern ");
      Serial.println(ledPattern);
      //sendState();
  }

    if (modes == 3 && (!strcmp(topic, color_com.c_str()))) {
    char message_buff[100];
    int i;
      for(i=0; i < payloadLength; i++) {
        message_buff[i] = payload[i];
        }
      message_buff[i] = '\0';
      String RGB = String((char*)message_buff);
    int SP0 = RGB.indexOf('(');
    int SP1 = RGB.indexOf(',', SP0+1);
    int SP2 = RGB.indexOf(',', SP1+1);
    int SP3 = RGB.indexOf(')', SP2+1);
    String R = RGB.substring(SP0+1, SP1);
    String G = RGB.substring(SP1+1, SP2);
    String B = RGB.substring(SP2+1, SP3);
    redValue = R.toInt();
    if (redValue > 128) {
      redValue = 128;
    }
    greenValue = G.toInt();
    blueValue = B.toInt();
    Serial.println("Colores");
    Serial.println(redValue);
    Serial.println(greenValue);
    Serial.println(blueValue);
    //client.publish(telemetry, R.c_str());
    //client.publish(telemetry, G.c_str());
    //client.publish(telemetry, B.c_str());
    //sendState();
  }

  if (modes == 4 && (!strcmp(topic, cheer_com.c_str()))) {
    char message_buff[100];
    int i;
    for(i= 0; i < payloadLength; i++) {
      message_buff[i] = payload[i];
      }
      message_buff[i] = '\0';
      String RGB = String((char*)message_buff);
      int number = (int) strtol( &RGB[1], NULL, 16);
      redcheer = number >> 16;
      greencheer = number >> 8 & 0xFF;
      bluecheer = number & 0xFF;
      Serial.println("Alegria");
      Serial.println(redcheer);
      Serial.println(greencheer);
      Serial.println(bluecheer);
      //sendState();
  }


  if (modes == 5 && (!strcmp(topic, realColor_com.c_str()))) {
    char message_buff[100];
    int i;
    for(i= 0; i < payloadLength; i++) {
      message_buff[i] = payload[i];
      }
      message_buff[i] = '\0';
       String s = String((char*)message_buff);
       realColor = s.toInt();
       Serial.print("Real Color ");
       Serial.println(realColor);
       //endState();
  }
    Serial.print("Modo :");
    Serial.println(modes);
}

//PubSubClient client(wifiClient);
PubSubClient client(mqtt_server.c_str(), 1883, callback, wifiClient);

void initManagedDevice() {
  if (client.subscribe(cheer_com.c_str())) {
    Serial.println("subscribe to Cheers Light OK");
  } else {
    Serial.println("subscribe to Cheers Light FAILED");
  }

  if (client.subscribe(mode_com.c_str())) {
    Serial.println("subscribe to Mode OK");
  } else {
    Serial.println("subscribe to Mode FAILED");
  }

  if (client.subscribe(pattern_com.c_str())) {
    Serial.println("subscribe to Pattern OK");
  } else {
    Serial.println("subscribe to Pattern FAILED");
  }

  if (client.subscribe(color_com.c_str())) {
    Serial.println("subscribe to Color OK");
  } else {
    Serial.println("subscribe to Color FAILED");
  }

  if (client.subscribe(realColor_com.c_str())) {
    Serial.println("subscribe to Real Color OK");
  } else {
    Serial.println("subscribe to Real Color FAILED");
  }
}

void sendState() {
  String s = String(modes);
  client.publish(telemetry.c_str(), String(modes).c_str());
  Serial.print("Telemetry: ");
  Serial.println(modes);
}


////////////////////////////////////////////////////////

void mqttConnect() {
  int retries = 0;

  // Loop until we're reconnected
  while (!client.connected()) {
    retries++;
    Serial.print("MQTT connection...");
    // Attempt to connect
    char buffer[10]="";
    sprintf(buffer, "espLucera_%i", ESP.getChipId());
    if (client.connect(buffer)) {
      Serial.println("connected");
      // ... and resubscribe
      initManagedDevice();
      client.publish(telemetry.c_str(), String("2").c_str());
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
      if (retries>2) {
        retries = 0;
        Serial.println("Resetting ESP");
        ESP.restart(); //ESP.reset();     // Send the IP address of the ESP8266 to the computer
      }
    }
  }
}


/////////////////////////////////////////////////////
/*

void CyclePalette() {
  if (shuffle && millis() / 1000.0 - shuffleTime > 30 && gradient % 2) {
    shuffleTime = millis() / 1000.0;
    palette++;
    if (palette >= sizeof(thresholds) / 2) palette = 0;
    gradient %= thresholds[palette];
    maxVol = avgVol;
  }
}



//////////////////////////////////////////////////////



void CycleVisual() {
  if (shuffle && millis() / 1000.0 - shuffleTime > 30 && !(gradient % 2)) {
    shuffleTime = millis() / 1000.0;
    visual++;
    gradient = 0;
    if (visual > VISUALS) visual = 0;
    if (visual == 1) memset(pos, -2, sizeof(pos));
    if (visual == 2 || visual == 3) {
      randomSeed(analogRead(analog_pin));
      dotPos = random(strand.numPixels());
    }
    maxVol = avgVol;
  }
}


//////////////////////////////////////////////////


uint32_t Rainbow(unsigned int i) {
  if (i > 1529) return Rainbow(i % 1530);
  if (i > 1274) return strand.Color(255, 0, 255 - (i % 255));
  if (i > 1019) return strand.Color((i % 255), 0, 255);
  if (i > 764) return strand.Color(0, 255 - (i % 255), 255);
  if (i > 509) return strand.Color(0, 255, (i % 255));
  if (i > 255) return strand.Color(255 - (i % 255), 255, 0);
  return strand.Color(255, i, 0);
}

uint32_t Sunset(unsigned int i) {
  if (i > 1019) return Sunset(i % 1020);
  if (i > 764) return strand.Color((i % 255), 0, 255 - (i % 255));
  if (i > 509) return strand.Color(255 - (i % 255), 0, 255);
  if (i > 255) return strand.Color(255, 128 - (i % 255) / 2, (i % 255));
  return strand.Color(255, i / 2, 0);
}

uint32_t Ocean(unsigned int i) {
  if (i > 764) return Ocean(i % 765);
  if (i > 509) return strand.Color(0, i % 255, 255 - (i % 255));
  if (i > 255) return strand.Color(0, 255 - (i % 255), 255);
  return strand.Color(0, 255, i);
}

uint32_t PinaColada(unsigned int i) {
  if (i > 764) return PinaColada(i % 765);
  if (i > 509) return strand.Color(255 - (i % 255) / 2, (i % 255) / 2, (i % 255) / 2);
  if (i > 255) return strand.Color(255, 255 - (i % 255), 0);
  return strand.Color(128 + (i / 2), 128 + (i / 2), 128 - i / 2);
}

uint32_t Sulfur(unsigned int i) {
  if (i > 764) return Sulfur(i % 765);
  if (i > 509) return strand.Color(i % 255, 255, 255 - (i % 255));
  if (i > 255) return strand.Color(0, 255, i % 255);
  return strand.Color(255 - i, 255, 0);
}

uint32_t NoGreen(unsigned int i) {
  if (i > 1274) return NoGreen(i % 1275);
  if (i > 1019) return strand.Color(255, 0, 255 - (i % 255));
  if (i > 764) return strand.Color((i % 255), 0, 255);
  if (i > 509) return strand.Color(0, 255 - (i % 255), 255);
  if (i > 255) return strand.Color(255 - (i % 255), 255, i % 255);
  return strand.Color(255, i, 0);
}

/////////////////////////////////////////////////

uint32_t ColorPalette(float num) {
  switch (palette) {
    case 0: return (num < 0) ? Rainbow(gradient) : Rainbow(num);
    case 1: return (num < 0) ? Sunset(gradient) : Sunset(num);
    case 2: return (num < 0) ? Ocean(gradient) : Ocean(num);
    case 3: return (num < 0) ? PinaColada(gradient) : PinaColada(num);
    case 4: return (num < 0) ? Sulfur(gradient) : Sulfur(num);
    case 5: return (num < 0) ? NoGreen(gradient) : NoGreen(num);
    default: return Rainbow(gradient);
  }
}


/////////////////////////////////////////////////////////////////////////////////


uint8_t split(uint32_t color, uint8_t i ) {
  if (i == 0) return color >> 16;
  if (i == 1) return color >> 8;
  if (i == 2) return color >> 0;
  return -1;
}

//////////////////////////////////////////////////////////////


void fade(float damper) {
  for (int i = 0; i < strand.numPixels(); i++) {
    uint32_t col = strand.getPixelColor(i);
    if (col == 0) continue;
    float colors[3];
    for (int j = 0; j < 3; j++) colors[j] = split(col, j) * damper;
    strand.setPixelColor(i, strand.Color(colors[0] , colors[1], colors[2]));
  }
}

///////////////////////////////////////////////////////////

void Pulse() {
  fade(0.75);
  if (bump) gradient += thresholds[palette] / 24;
  if (volume > 0) {
    uint32_t col = ColorPalette(-1);
    int start = LED_HALF - (LED_HALF * (volume / maxVol));
    int finish = LED_HALF + (LED_HALF * (volume / maxVol)) + strand.numPixels() % 2;

    for (int i = start; i < finish; i++) {
      float damp = sin((i - start) * PI / float(finish - start));
      damp = pow(damp, 2.0);
      uint32_t col2 = strand.getPixelColor(i);
      uint8_t colors[3];
      float avgCol = 0;
      float avgCol2 = 0;
      for (int k = 0; k < 3; k++) {
        colors[k] = split(col, k) * damp * pow(volume / maxVol, 2);
        avgCol += colors[k];
        avgCol2 += split(col2, k);
      }
      avgCol /= 3.0, avgCol2 /= 3.0;
      if (avgCol > avgCol2) strand.setPixelColor(i, strand.Color(colors[0], colors[1], colors[2]));
    }
  }
  showStrip();
}

//////////////////////////////////////////////////////////////////////////////////////////


void PalettePulse() {
  fade(0.75);
  if (bump) gradient += thresholds[palette] / 24;
  if (volume > 0) {
    int start = LED_HALF - (LED_HALF * (volume / maxVol));
    int finish = LED_HALF + (LED_HALF * (volume / maxVol)) + strand.numPixels() % 2;
    for (int i = start; i < finish; i++) {
      float damp = sin((i - start) * PI / float(finish - start));
      damp = pow(damp, 2.0);
      int val = thresholds[palette] * (i - start) / (finish - start);
      val += gradient;
      uint32_t col = ColorPalette(val);
      uint32_t col2 = strand.getPixelColor(i);
      uint8_t colors[3];
      float avgCol = 0, avgCol2 = 0;
      for (int k = 0; k < 3; k++) {
        colors[k] = split(col, k) * damp * pow(volume / maxVol, 2);
        avgCol += colors[k];
        avgCol2 += split(col2, k);
      }
      avgCol /= 3.0, avgCol2 /= 3.0;
      if (avgCol > avgCol2) strand.setPixelColor(i, strand.Color(colors[0], colors[1], colors[2]));
    }
  }
  showStrip();
}


/////////////////////////////////////////////////////////////////////////////////////////////



void Snake() {
  if (bump) {
    gradient += thresholds[palette] / 30;
    left = !left;
  }
  fade(0.975);
  uint32_t col = ColorPalette(-1);
  if (volume > 0) {
    strand.setPixelColor(dotPos, strand.Color(
                           float(split(col, 0)) * pow(volume / maxVol, 1.5),
                           float(split(col, 1)) * pow(volume / maxVol, 1.5),
                           float(split(col, 2)) * pow(volume / maxVol, 1.5))
                        );
    if (avgTime < 0.15)                                               dotPos += (left) ? -1 : 1;
    else if (avgTime >= 0.15 && avgTime < 0.5 && gradient % 2 == 0)   dotPos += (left) ? -1 : 1;
    else if (avgTime >= 0.5 && avgTime < 1.0 && gradient % 3 == 0)    dotPos += (left) ? -1 : 1;
    else if (gradient % 4 == 0)                                       dotPos += (left) ? -1 : 1;
  }
  showStrip();
  if (dotPos < 0)    dotPos = strand.numPixels() - 1;
  else if (dotPos >= strand.numPixels())  dotPos = 0;
}


////////////////////////////////////////////////////////////////


void bleed(uint8_t Point) {
  for (int i = 1; i < strand.numPixels(); i++) {
    int sides[] = {Point - i, Point + i};

    for (int i = 0; i < 2; i++) {
      int point = sides[i];
      uint32_t colors[] = {strand.getPixelColor(point - 1), strand.getPixelColor(point), strand.getPixelColor(point + 1)  };
      strand.setPixelColor(point, strand.Color(
                             float( split(colors[0], 0) + split(colors[1], 0) + split(colors[2], 0) ) / 3.0,
                             float( split(colors[0], 1) + split(colors[1], 1) + split(colors[2], 1) ) / 3.0,
                             float( split(colors[0], 2) + split(colors[1], 2) + split(colors[2], 2) ) / 3.0)
                          );
    }
  }
}


///////////////////////////////////////////////////////////////////////



void Glitter() {
  gradient += thresholds[palette] / 204;
  for (int i = 0; i < strand.numPixels(); i++) {
    unsigned int val = float(thresholds[palette] + 1) *
                       (float(i) / float(strand.numPixels()))
                       + (gradient);
    val %= thresholds[palette];
    uint32_t  col = ColorPalette(val);
    strand.setPixelColor(i, strand.Color(
                           split(col, 0) / 6.0,
                           split(col, 1) / 6.0,
                           split(col, 2) / 6.0)
                        );
  }
  if (bump) {
    randomSeed(micros());
    dotPos = random(strand.numPixels() - 1);
    strand.setPixelColor(dotPos, strand.Color(
                           255.0 * pow(volume / maxVol, 2.0),
                           255.0 * pow(volume / maxVol, 2.0),
                           255.0 * pow(volume / maxVol, 2.0)
                         ));
  }
  bleed(dotPos);
  showStrip();
}


//////////////////////////////////////////////////////////////////



void Paintball() {
  if ((millis() / 1000.0) - timeBump > avgTime * 2.0) fade(0.99);
  if (bump) {
    randomSeed(micros());
    dotPos = random(strand.numPixels() - 1);
    uint32_t col = ColorPalette(random(thresholds[palette]));
    uint8_t colors[3];
    for (int i = 0; i < 3; i++) colors[i] = split(col, i) * pow(volume / maxVol, 2.0);
    strand.setPixelColor(dotPos, strand.Color(colors[0], colors[1], colors[2]));
    for (int i = 0; i < 3; i++) colors[i] *= .8;
    strand.setPixelColor(dotPos - 1, strand.Color(colors[0], colors[1], colors[2]));
    strand.setPixelColor(dotPos + 1, strand.Color(colors[0], colors[1], colors[2]));
  }
  showStrip();
}


////////////////////////////////////////////////////


void Cycle() {
  for (int i = 0; i < strand.numPixels(); i++) {
    float val = float(thresholds[palette]) * (float(i) / float(strand.numPixels())) + (gradient);
    val = int(val) % thresholds[palette];
    strand.setPixelColor(i, ColorPalette(val));
  }
  showStrip();
  gradient += 32;
}

///////////////////////////////////////////////////////////////



void PaletteDance() {
  if (bump) left = !left;
  if (volume > avgVol) {
    for (int i = 0; i < strand.numPixels(); i++) {
      float sinVal = abs(sin(
                           (i + dotPos) *
                           (PI / float(strand.numPixels() / 1.25) )
                         ));
      sinVal *= sinVal;
      sinVal *= volume / maxVol;
      unsigned int val = float(thresholds[palette] + 1)
                         //map takes a value between -LED_TOTAL and +LED_TOTAL and returns one between 0 and LED_TOTAL
                         * (float(i + map(dotPos, -1 * (strand.numPixels() - 1), strand.numPixels() - 1, 0, strand.numPixels() - 1))
                            / float(strand.numPixels()))
                         + (gradient);
      val %= thresholds[palette];
      uint32_t col = ColorPalette(val);
      strand.setPixelColor(i, strand.Color(
                             float(split(col, 0))*sinVal,
                             float(split(col, 1))*sinVal,
                             float(split(col, 2))*sinVal)
                          );
    }
    dotPos += (left) ? -1 : 1;
  }

  else  fade(0.8);
  showStrip();
  if (dotPos < 0) dotPos = strand.numPixels() - strand.numPixels() / 6;
  else if (dotPos >= strand.numPixels() - strand.numPixels() / 6)  dotPos = 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////


void Traffic() {
  fade(0.8);
  if (bump) {
    int8_t slot = 0;
    for (slot; slot < sizeof(pos); slot++) {
      if (pos[slot] < -1) break;
      else if (slot + 1 >= sizeof(pos)) {
        slot = -3;
        break;
      }
    }

    if (slot != -3) {
      pos[slot] = (slot % 2 == 0) ? -1 : strand.numPixels();
      uint32_t col = ColorPalette(-1);
      gradient += thresholds[palette] / 24;
      for (int j = 0; j < 3; j++) {
        rgb[slot][j] = split(col, j);
      }
    }
  }

  if (volume > 0) {
    for (int i = 0; i < sizeof(pos); i++) {
      if (pos[i] < -1) continue;
      pos[i] += (i % 2) ? -1 : 1;
      if (pos[i] >= strand.numPixels()) pos[i] = -2;
      strand.setPixelColor( pos[i], strand.Color(
                              float(rgb[i][0]) * pow(volume / maxVol, 2.0),
                              float(rgb[i][1]) * pow(volume / maxVol, 2.0),
                              float(rgb[i][2]) * pow(volume / maxVol, 2.0))
                          );
    }
  }
  showStrip();
}

///////////////////////////////////////////////////////////////////////


void Visualize() {
  switch (visual) {
    case 0: return Pulse();
    case 1: return PalettePulse();
    case 2: return Traffic();
    case 3: return Snake();
    case 4: return PaletteDance();
    case 5: return Glitter();
    case 6: return Paintball();
    default: return Pulse();
  }
}

void getDance() {
    volume = analogRead(analog_pin);

    if (volume < avgVol / 2.0 || volume < 15) volume = 0;
    else avgVol = (avgVol + volume) / 2.0;

    if (volume > maxVol) maxVol = volume;
    CyclePalette();
    CycleVisual();

    if (gradient > thresholds[palette]) {
      gradient %= thresholds[palette] + 1;
      maxVol = (maxVol + volume) / 2.0;
    }
    if (volume - last > 10) avgBump = (avgBump + (volume - last)) / 2.0;
    bump = (volume - last > avgBump * .9);
    if (bump) {
      avgTime = (((millis() / 1000.0) - timeBump) + avgTime) / 2.0;
      timeBump = millis() / 1000.0;
    }

    Visualize();
    gradient++;
    last = volume;
    delay(30);

}

// ***************************************
// ** FastLed/NeoPixel Common Functions **
// ***************************************
*/
void showStrip() {
 #ifdef ADAFRUIT_NEOPIXEL_H
   // NeoPixel
   strand.show();
 #endif
 #ifndef ADAFRUIT_NEOPIXEL_H
   // FastLED
   FastLED.show();
 #endif
}

// Set a LED color (not yet visible)
void setPixel(int Pixel, byte red, byte green, byte blue) {
 #ifdef ADAFRUIT_NEOPIXEL_H
   // NeoPixel
   strip.setPixelColor(Pixel, strip.Color(red, green, blue));
 #endif
 #ifndef ADAFRUIT_NEOPIXEL_H
   // FastLED
   leds[Pixel].r = red;
   leds[Pixel].g = green;
   leds[Pixel].b = blue;
 #endif
}

// Set all LEDs to a given color and apply it (visible)
void setAll(byte red, byte green, byte blue) {
  for(int i = 0; i < LED_TOTAL; i++ ) {
    setPixel(i, red, green, blue);
  }
  showStrip();
}




void RGBLoop() {
  for (int j = 0; j < 3; j++ ) {
    // Fade IN
    for (int k = 0; k < 128; k++) {
      switch (j) {
        case 0: setAll(k, 0, 0); break;
        case 1: setAll(0, k, 0); break;
        case 2: setAll(0, 0, k); break;
      }
      showStrip();
      FastLED.delay(3);
    }
    // Fade OUT
    for (int k = 128; k >= 0; k--) {
      switch (j) {
        case 0: setAll(k, 0, 0); break;
        case 1: setAll(0, k, 0); break;
        case 2: setAll(0, 0, k); break;
      }
      showStrip();
      FastLED.delay(3);
    }
  }
}

void FadeInOut(byte red, byte green, byte blue){
  float r, g, b;

  for(int k = 0; k < 256; k=k+1) {
    r = (k/256.0)*red;
    g = (k/256.0)*green;
    b = (k/256.0)*blue;
    setAll(r,g,b);
    showStrip();
  }

  for(int k = 255; k >= 0; k=k-2) {
    r = (k/256.0)*red;
    g = (k/256.0)*green;
    b = (k/256.0)*blue;
    setAll(r,g,b);
    showStrip();
  }
}

// used by meteorrain
void fadeToBlack(int ledNo, byte fadeValue) {
 #ifdef ADAFRUIT_NEOPIXEL_H
    // NeoPixel
    uint32_t oldColor;
    uint8_t r, g, b;
    int value;

    oldColor = strip.getPixelColor(ledNo);
    r = (oldColor & 0x00ff0000UL) >> 16;
    g = (oldColor & 0x0000ff00UL) >> 8;
    b = (oldColor & 0x000000ffUL);

    r=(r<=10)? 0 : (int) r-(r*fadeValue/256);
    g=(g<=10)? 0 : (int) g-(g*fadeValue/256);
    b=(b<=10)? 0 : (int) b-(b*fadeValue/256);

    strip.setPixelColor(ledNo, r,g,b);
 #endif
 #ifndef ADAFRUIT_NEOPIXEL_H
   // FastLED
   leds[ledNo].fadeToBlackBy( fadeValue );
 #endif
}

////////////////////////////////////////////

void meteorRain(byte red, byte green, byte blue, byte meteorSize, byte meteorTrailDecay, boolean meteorRandomDecay, int SpeedDelay) {
  //setAll(0,0,0);

  for(int i = 0; i < LED_TOTAL+LED_TOTAL; i++) {


    // fade brightness all LEDs one step
    for(int j=0; j<LED_TOTAL; j++) {
      if( (!meteorRandomDecay) || (random(10)>5) ) {
        fadeToBlack(j, meteorTrailDecay );
      }
    }

    // draw meteor
    for(int j = 0; j < meteorSize; j++) {
      if( ( i-j <LED_TOTAL) && (i-j>=0) ) {
        setPixel(i-j, red, green, blue);
      }
    }

    showStrip();
    FastLED.delay(SpeedDelay);
  }
}


void Strobe(byte red, byte green, byte blue, int StrobeCount, int FlashDelay, int EndPause) {
  for (int j = 0; j < StrobeCount; j++) {
    setAll(red, green, blue);
    showStrip();
    FastLED.delay(FlashDelay);
    setAll(0, 0, 0);
    showStrip();
    FastLED.delay(FlashDelay);
  }

  FastLED.delay(EndPause);
}

///////////////////////////////////////



void HalloweenEyes(byte red, byte green, byte blue,
                   int EyeWidth, int EyeSpace,
                   boolean Fade, int Steps, int FadeDelay,
                   int EndPause) {
  randomSeed(analogRead(analog_pin));

  int i;
  int StartPoint  = random( 0, LED_TOTAL - (2 * EyeWidth) - EyeSpace );
  int Start2ndEye = StartPoint + EyeWidth + EyeSpace;

  for (i = 0; i < EyeWidth; i++) {
    setPixel(StartPoint + i, red, green, blue);
    setPixel(Start2ndEye + i, red, green, blue);
  }

  showStrip();

  if (Fade == true) {
    float r, g, b;

    for (int j = Steps; j >= 0; j--) {
      r = j * (red / Steps);
      g = j * (green / Steps);
      b = j * (blue / Steps);

      for (i = 0; i < EyeWidth; i++) {
        setPixel(StartPoint + i, r, g, b);
        setPixel(Start2ndEye + i, r, g, b);
      }

      showStrip();
      FastLED.delay(FadeDelay);
    }
  }

  setAll(0, 0, 0); // Set all black

  FastLED.delay(EndPause);
}


/////////////////////////////////////////////////////


void CylonBounce(byte red, byte green, byte blue, int EyeSize, int SpeedDelay, int ReturnDelay) {

  for (int i = 0; i < LED_TOTAL - EyeSize - 2; i++) {
    setAll(0, 0, 0);
    setPixel(i, red / 10, green / 10, blue / 10);
    for (int j = 1; j <= EyeSize; j++) {
      setPixel(i + j, red, green, blue);
    }
    setPixel(i + EyeSize + 1, red / 10, green / 10, blue / 10);
    showStrip();
    FastLED.delay(SpeedDelay);
  }

  FastLED.delay(ReturnDelay);

  for (int i = LED_TOTAL - EyeSize - 2; i > 0; i--) {
    setAll(0, 0, 0);
    setPixel(i, red / 10, green / 10, blue / 10);
    for (int j = 1; j <= EyeSize; j++) {
      setPixel(i + j, red, green, blue);
    }
    setPixel(i + EyeSize + 1, red / 10, green / 10, blue / 10);
    showStrip();
    FastLED.delay(SpeedDelay);
  }

  FastLED.delay(ReturnDelay);
}


//////////////////////////////////////////////////////////////////

void CenterToOutside(byte red, byte green, byte blue, int EyeSize, int SpeedDelay, int ReturnDelay) {
  for (int i = ((LED_TOTAL - EyeSize) / 2); i >= 0; i--) {
    setAll(0, 0, 0);

    setPixel(i, red / 10, green / 10, blue / 10);
    for (int j = 1; j <= EyeSize; j++) {
      setPixel(i + j, red, green, blue);
    }
    setPixel(i + EyeSize + 1, red / 10, green / 10, blue / 10);

    setPixel(LED_TOTAL - i, red / 10, green / 10, blue / 10);
    for (int j = 1; j <= EyeSize; j++) {
      setPixel(LED_TOTAL - i - j, red, green, blue);
    }
    setPixel(LED_TOTAL - i - EyeSize - 1, red / 10, green / 10, blue / 10);

    showStrip();
    FastLED.delay(SpeedDelay);
  }
  FastLED.delay(ReturnDelay);
}

void OutsideToCenter(byte red, byte green, byte blue, int EyeSize, int SpeedDelay, int ReturnDelay) {
  for (int i = 0; i <= ((LED_TOTAL - EyeSize) / 2); i++) {
    setAll(0, 0, 0);

    setPixel(i, red / 10, green / 10, blue / 10);
    for (int j = 1; j <= EyeSize; j++) {
      setPixel(i + j, red, green, blue);
    }
    setPixel(i + EyeSize + 1, red / 10, green / 10, blue / 10);

    setPixel(LED_TOTAL - i, red / 10, green / 10, blue / 10);
    for (int j = 1; j <= EyeSize; j++) {
      setPixel(LED_TOTAL - i - j, red, green, blue);
    }
    setPixel(LED_TOTAL - i - EyeSize - 1, red / 10, green / 10, blue / 10);

    showStrip();
    FastLED.delay(SpeedDelay);
  }
  FastLED.delay(ReturnDelay);
}

void LeftToRight(byte red, byte green, byte blue, int EyeSize, int SpeedDelay, int ReturnDelay) {
  for (int i = 0; i < LED_TOTAL - EyeSize - 2; i++) {
    setAll(0, 0, 0);
    setPixel(i, red / 10, green / 10, blue / 10);
    for (int j = 1; j <= EyeSize; j++) {
      setPixel(i + j, red, green, blue);
    }
    setPixel(i + EyeSize + 1, red / 10, green / 10, blue / 10);
    showStrip();
    FastLED.delay(SpeedDelay);
  }
  FastLED.delay(ReturnDelay);
}

void RightToLeft(byte red, byte green, byte blue, int EyeSize, int SpeedDelay, int ReturnDelay) {
  for (int i = LED_TOTAL - EyeSize - 2; i > 0; i--) {
    setAll(0, 0, 0);
    setPixel(i, red / 10, green / 10, blue / 10);
    for (int j = 1; j <= EyeSize; j++) {
      setPixel(i + j, red, green, blue);
    }
    setPixel(i + EyeSize + 1, red / 10, green / 10, blue / 10);
    showStrip();
    FastLED.delay(SpeedDelay);
  }
  FastLED.delay(ReturnDelay);
}


void NewKITT(byte red, byte green, byte blue, int EyeSize, int SpeedDelay, int ReturnDelay) {
  RightToLeft(red, green, blue, EyeSize, SpeedDelay, ReturnDelay);
  LeftToRight(red, green, blue, EyeSize, SpeedDelay, ReturnDelay);
  OutsideToCenter(red, green, blue, EyeSize, SpeedDelay, ReturnDelay);
  CenterToOutside(red, green, blue, EyeSize, SpeedDelay, ReturnDelay);
  LeftToRight(red, green, blue, EyeSize, SpeedDelay, ReturnDelay);
  RightToLeft(red, green, blue, EyeSize, SpeedDelay, ReturnDelay);
  OutsideToCenter(red, green, blue, EyeSize, SpeedDelay, ReturnDelay);
  CenterToOutside(red, green, blue, EyeSize, SpeedDelay, ReturnDelay);
}

//////////////////////////////////////////////////////////////////////////////////////


void Twinkle(byte red, byte green, byte blue, int Count, int SpeedDelay, boolean OnlyOne) {
  setAll(0, 0, 0);

  for (int i = 0; i < Count; i++) {
    setPixel(random(LED_TOTAL), red, green, blue);
    showStrip();
    FastLED.delay(SpeedDelay);
    if (OnlyOne) {
      setAll(0, 0, 0);
    }
  }

  FastLED.delay(SpeedDelay);
}




/////////////////////////////////////////



void TwinkleRandom(int Count, int SpeedDelay, boolean OnlyOne) {
  setAll(0, 0, 0);

  for (int i = 0; i < Count; i++) {
    setPixel(random(LED_TOTAL), random(0, 255), random(0, 255), random(0, 255));
    showStrip();
    FastLED.delay(SpeedDelay);
    if (OnlyOne) {
      setAll(0, 0, 0);
    }
  }

  FastLED.delay(SpeedDelay);
}

//////////////////////////////////////////


void Sparkle(byte red, byte green, byte blue, int SpeedDelay) {
  int Pixel = random(LED_TOTAL);
  setPixel(Pixel, red, green, blue);
  showStrip();
  FastLED.delay(SpeedDelay);
  setPixel(Pixel, 0, 0, 0);
}


/////////////////////////////////////////////////////


void SnowSparkle(byte red, byte green, byte blue, int SparkleDelay, int SpeedDelay) {
  setAll(red, green, blue);

  int Pixel = random(LED_TOTAL);
  setPixel(Pixel, 0xff, 0xff, 0xff);
  showStrip();
  FastLED.delay(SparkleDelay);
  setPixel(Pixel, red, green, blue);
  showStrip();
  FastLED.delay(SpeedDelay);
}

//////////////////////////////////////////////////////////


void RunningLights(byte red, byte green, byte blue, int WaveDelay) {
  int Position = 0;

  for (int i = 0; i < LED_TOTAL * 2; i++)
  {
    Position++; // = 0; //Position + Rate;
    for (int i = 0; i < LED_TOTAL; i++) {
      setPixel(i, ((sin(i + Position) * 127 + 128) / 255)*red,
               ((sin(i + Position) * 127 + 128) / 255)*green,
               ((sin(i + Position) * 127 + 128) / 255)*blue);
    }

    showStrip();
    FastLED.delay(WaveDelay);
  }
}


///////////////////////////////////////////////////////////////////////



void colorWipe(byte red, byte green, byte blue, int SpeedDelay) {
  for (uint16_t i = 0; i < LED_TOTAL; i++) {
    setPixel(i, red, green, blue);
    showStrip();
    FastLED.delay(SpeedDelay);
  }
}


////////////////////////////////////////////////////////


byte* Wheel(byte WheelPos) {
  static byte c[3];

  if (WheelPos < 85) {
    c[0] = WheelPos * 3;
    c[1] = 255 - WheelPos * 3;
    c[2] = 0;
  }
  else if (WheelPos < 170) {
    WheelPos -= 85;
    c[0] = 255 - WheelPos * 3;
    c[1] = 0;
    c[2] = WheelPos * 3;
  }
  else {
    WheelPos -= 170;
    c[0] = 0;
    c[1] = WheelPos * 3;
    c[2] = 255 - WheelPos * 3;
  }

  return c;
}


void rainbowCycle(int SpeedDelay) {
  byte *c;
  uint16_t i, j;

  for (j = 0; j < 256*5; j++) {
    for (i = 0; i < LED_TOTAL; i++) {
      c = Wheel(((i * 256 / LED_TOTAL) + j) & 255);
      setPixel(i, *c, *(c + 1), *(c + 2));
    }
    showStrip();
    FastLED.delay(SpeedDelay);
}
}

//////////////////////////////////////////////////////



void theaterChase(byte red, byte green, byte blue, int SpeedDelay) {
  for (int j = 0; j < 10; j++) { //do 10 cycles of chasing
    for (int q = 0; q < 3; q++) {
      for (int i = 0; i < LED_TOTAL; i = i + 3) {
        setPixel(i + q, red, green, blue);
      }
      showStrip();

      FastLED.delay(SpeedDelay);

      for (int i = 0; i < LED_TOTAL; i = i + 3) {
        setPixel(i + q, 0, 0, 0);
      }
    }
  }
}

/////////////////////////////////////////////////////////////////////////



void theaterChaseRainbow(int SpeedDelay) {
  byte *c;

  for (int j = 0; j < 256; j++) {
    for (int q = 0; q < 3; q++) {
      for (int i = 0; i < LED_TOTAL; i = i + 3) {
        c = Wheel( (i + j) % 255);
        setPixel(i + q, *c, *(c + 1), *(c + 2));
      }
      showStrip();

      FastLED.delay(SpeedDelay);

      for (int i = 0; i < LED_TOTAL; i = i + 3) {
        setPixel(i + q, 0, 0, 0);
      }
    }
  }
}




/////////////////////////////////////////////////////

void setPixelHeatColor (int Pixel, byte temperature) {
  // Scale 'heat' down from 0-255 to 0-191
  byte t192 = round((temperature / 255.0) * 191);

  // calculate ramp up from
  byte heatramp = t192 & 0x3F; // 0..63
  heatramp <<= 2; // scale up to 0..252

  // figure out which third of the spectrum we're in:
  if ( t192 > 0x80) {                    // hottest
    setPixel(Pixel, 255, 255, heatramp);
  } else if ( t192 > 0x40 ) {            // middle
    setPixel(Pixel, 255, heatramp, 0);
  } else {                               // coolest
    setPixel(Pixel, heatramp, 0, 0);
  }
}

void Fire(int Cooling, int Sparking, int SpeedDelay) {
  static byte heat[LED_TOTAL];
  int cooldown;

  // Step 1.  Cool down every cell a little
  for ( int i = 0; i < LED_TOTAL; i++) {
    cooldown = random(0, ((Cooling * 10) / LED_TOTAL) + 2);

    if (cooldown > heat[i]) {
      heat[i] = 0;
    } else {
      heat[i] = heat[i] - cooldown;
    }
  }

  // Step 2.  Heat from each cell drifts 'up' and diffuses a little
  for ( int k = LED_TOTAL - 1; k >= 2; k--) {
    heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
  }

  // Step 3.  Randomly ignite new 'sparks' near the bottom
  if ( random(255) < Sparking ) {
    int y = random(7);
    heat[y] = heat[y] + random(160, 255);
    //heat[y] = random(160,255);
  }

  // Step 4.  Convert heat to LED colors
  for ( int j = 0; j < LED_TOTAL; j++) {
    setPixelHeatColor(j, heat[j] );
  }

  showStrip();
  FastLED.delay(SpeedDelay);
}

//////////////////////////////////////////////////////////////////



void BouncingBalls(byte red, byte green, byte blue, int BallCount) {
  float Gravity = -9.81;
  int StartHeight = 1;

  float Height[BallCount];
  float ImpactVelocityStart = sqrt( -2 * Gravity * StartHeight );
  float ImpactVelocity[BallCount];
  float TimeSinceLastBounce[BallCount];
  int   Position[BallCount];
  long  ClockTimeSinceLastBounce[BallCount];
  float Dampening[BallCount];
  int whilecounter=0;

  for (int i = 0 ; i < BallCount ; i++) {
    ClockTimeSinceLastBounce[i] = millis();
    Height[i] = StartHeight;
    Position[i] = 0;
    ImpactVelocity[i] = ImpactVelocityStart;
    TimeSinceLastBounce[i] = 0;
    Dampening[i] = 0.90 - float(i) / pow(BallCount, 2);
  }

 while (whilecounter<100) {
    whilecounter++;
    for (int i = 0 ; i < BallCount ; i++) {
      TimeSinceLastBounce[i] =  millis() - ClockTimeSinceLastBounce[i];
      Height[i] = 0.5 * Gravity * pow( TimeSinceLastBounce[i] / 1000 , 2.0 ) + ImpactVelocity[i] * TimeSinceLastBounce[i] / 1000;

      if ( Height[i] < 0 ) {
        Height[i] = 0;
        ImpactVelocity[i] = Dampening[i] * ImpactVelocity[i];
        ClockTimeSinceLastBounce[i] = millis();

        if ( ImpactVelocity[i] < 0.01 ) {
          ImpactVelocity[i] = ImpactVelocityStart;
        }
      }
      Position[i] = round( Height[i] * (LED_TOTAL - 1) / StartHeight);
    }

    for (int i = 0 ; i < BallCount ; i++) {
      setPixel(Position[i], red, green, blue);
    }

    showStrip();
    setAll(0, 0, 0);
    FastLED.delay(100);
  }
}


////////////////////////////////////////////////////////////



void BouncingColoredBalls(int BallCount, byte colors[][3]) {
  float Gravity = -9.81;
  int StartHeight = 1;

  float Height[BallCount];
  float ImpactVelocityStart = sqrt( -2 * Gravity * StartHeight );
  float ImpactVelocity[BallCount];
  float TimeSinceLastBounce[BallCount];
  int   Position[BallCount];
  long  ClockTimeSinceLastBounce[BallCount];
  float Dampening[BallCount];
  int whilecounter=0;

  for (int i = 0 ; i < BallCount ; i++) {
    ClockTimeSinceLastBounce[i] = millis();
    Height[i] = StartHeight;
    Position[i] = 0;
    ImpactVelocity[i] = ImpactVelocityStart;
    TimeSinceLastBounce[i] = 0;
    Dampening[i] = 0.90 - float(i) / pow(BallCount, 2);
  }

 while (whilecounter<100) {
    whilecounter++;
    for (int i = 0 ; i < BallCount ; i++) {
      TimeSinceLastBounce[i] =  millis() - ClockTimeSinceLastBounce[i];
      Height[i] = 0.5 * Gravity * pow( TimeSinceLastBounce[i] / 1000 , 2.0 ) + ImpactVelocity[i] * TimeSinceLastBounce[i] / 1000;

      if ( Height[i] < 0 ) {
        Height[i] = 0;
        ImpactVelocity[i] = Dampening[i] * ImpactVelocity[i];
        ClockTimeSinceLastBounce[i] = millis();

        if ( ImpactVelocity[i] < 0.01 ) {
          ImpactVelocity[i] = ImpactVelocityStart;
        }
      }
      Position[i] = round( Height[i] * (LED_TOTAL - 1) / StartHeight);
    }

    for (int i = 0 ; i < BallCount ; i++) {
      setPixel(Position[i], colors[i][0], colors[i][1], colors[i][2]);
    }
    showStrip();
    setAll(0, 0, 0);
    FastLED.delay(100);
  }
}


///////////////////////////////////////////////////////////////////////////


void getPattern() {
  if (ledPattern == 1) {
    RGBLoop();
  }

  else if (ledPattern == 2) {
    Strobe(128, 128, 128, 10, 50, 1000);
  }

  else if (ledPattern == 3) {
    HalloweenEyes(0xff, 0x00, 0x00,
                  1, 4,
                  true, random(5, 50), random(50, 150),
                  random(1000, 10000));
    HalloweenEyes(0xff, 0x00, 0x00,
                  1, 4,
                  true, random(5,50), random(50,150),
                  random(1000, 10000));
  }

  else if (ledPattern == 4) {
    CylonBounce(random(0, 255), random(0, 255), random(0, 255), 4, 200, 100);
  }

  else if (ledPattern == 5) {
    NewKITT(random(0, 255), random(0, 255), random(0, 255), 3, 10, 1000);
  }
  else if (ledPattern == 6) {
    NewKITT(255, 0, 0, 25, 10, 10);
  }

  else if (ledPattern == 7) {
    Twinkle(0xE3, 0x20, 0xD6, 20, 100, false);
  }

  else if (ledPattern == 8) {
    TwinkleRandom(6, 100, false);
  }

  else if (ledPattern == 9) {
    Sparkle(random(255), random(255), random(255), 100);
  }

  else if (ledPattern == 10) {
    SnowSparkle(0x10, 0x10, 0x10, 100, random(100,1000));
  }

  else if (ledPattern == 11) {
    RunningLights(random(0, 255), random(0, 255), random(0, 255), 200);
  }

  else if (ledPattern == 12) {
    colorWipe(random(0, 255), random(0, 255), random(0, 255), 50);
  }

  else if (ledPattern == 13) {
    rainbowCycle(20);
  }

  else if (ledPattern == 14) {
    theaterChase(0xff,0,0,200);
  }

  else if (ledPattern == 15) {
    theaterChaseRainbow(100);
  }

  else if (ledPattern == 16) {
    Fire(55,120,15);
  }

  else if (ledPattern == 17) {
    BouncingBalls(random(0, 255), random(0, 255), random(0, 255), 3);
  }

  else if (ledPattern == 18) {
    byte colors[3][3] = { {0xE3, 0x30, 0x14},
                        {0x20, 0xE3, 0x19},
                        {0x17, 0x17, 0xE3} };
  BouncingColoredBalls(3, colors);
  }

  else if (ledPattern == 19) {
    meteorRain(random(0, 255), random(0, 255), random(0, 255), 10, 64, true, 30);
  }

  else if (ledPattern == 20) {
    // FadeInOut - Color (red, green. blue)
    FadeInOut(0xff, 0x00, 0x00); // red
    FadeInOut(0xff, 0xff, 0xff); // white
    FadeInOut(0x00, 0x00, 0xff); // blue
  }
/*  else if (ledPattern == 19) {
    RGBLoopF();
  }*/

}

//////////////////////////////////////////////////////////////////////////

void setLed() {
   for (int y = 0; y < LED_TOTAL; y++ ) {
   setPixel(y, redValue, greenValue, blueValue);
   }
    showStrip();
    FastLED.delay(100);
}

//////////////////////////////////////////////////////////////////
void getOff() {
  for (int y = 0; y < LED_TOTAL; y++) {
    setPixel(y, 0, 0, 0);
  }
  showStrip();
}

void getReal() {
if (realColor == 1) {                //Candle
      redValue = 125;
      greenValue = 47;
      blueValue = 0;
}
else if (realColor == 2) {                 //40W Tungsten
      redValue = 125;
      greenValue = 97;
      blueValue = 43;
}
else if (realColor == 3) {                 //100 Tungsten
      redValue = 105;
      greenValue = 64;
      blueValue = 20;
}
else if (realColor == 4) {                //Halogen
      redValue = 105;
      greenValue = 91;
      blueValue = 74;
}
else if (realColor == 5) {                 //Carbon Arc
      redValue = 105;
      greenValue = 100;
      blueValue = 94;
}
else if (realColor == 6) {                 //High Noon Sun
      redValue = 105;
      greenValue = 105;
      blueValue = 101;
}
 else if (realColor == 7) {                 //Direct Sunlight
      redValue = 105;
      greenValue = 105;
      blueValue = 105;
}
else if (realColor == 8) {                 //Overcast Sky;
      redValue = 51;
      greenValue = 76;
      blueValue = 105;
}

else if (realColor == 9) {                 //Clear Blue Sky
      redValue = 0;
      greenValue = 56;
      blueValue = 155;
}
else if (realColor == 10) {                //Warm Fluorescent
      redValue = 105;
      greenValue = 94;
      blueValue = 79;
}
else if (realColor == 11) {                //Standard Fluorescent
      redValue = 94;
      greenValue = 105;
      blueValue = 100;
}
else if (realColor == 12) {                //Cool White Fluorescent
      redValue = 62;
      greenValue = 85;
      blueValue = 105;
}
else if (realColor == 13) {                //Full Spectrum Fluorescent
      redValue = 105;
      greenValue = 94;
      blueValue = 92;
}
else if (realColor == 14) {                //Grow Light Fluorescent
      redValue = 105;
      greenValue = 89;
      blueValue = 97;
}
else if (realColor == 15) {                //Black Light Fluorescent
      redValue = 37;
      greenValue = 0;
      blueValue = 105;
}
else if (realColor == 16) {                //Mercury Vapor
      redValue = 66;
      greenValue = 97;
      blueValue = 105;
}
else if (realColor == 17) {                //Sodium Vapor
      redValue = 105;
      greenValue = 59;
      blueValue = 28;
}
else if (realColor == 18) {                //Metal Halide
      redValue = 92;
      greenValue = 102;
      blueValue = 105;
}
else if (realColor == 19) {                //High Pressure Sodium
      redValue = 105;
      greenValue = 133;
      blueValue = 0;
    }

      setLed();
}

void setCheer() {
   for (int y = 0; y < LED_TOTAL; y++ ) {
   setPixel(y, redcheer, greencheer, bluecheer);
   }
    showStrip();
    FastLED.delay(100);
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  pinMode(online_pin, OUTPUT);
  Serial.begin(115200);
  Serial.println();
  //strand.begin();

  //FastLED.addLeds<NEOPIXEL, LED_PIN2>(leds, LED_TOTAL);
  FastLED.addLeds<WS2812B, LED_PIN2, GRB>(leds, LED_TOTAL).setCorrection( TypicalLEDStrip );
  SPIFFS.begin();

  if (Portal.load(FPSTR(AUX_mqtt_setting))) {
    AutoConnectAux& mqtt_setting = *Portal.aux(AUX_SETTING_URI);
    PageArgument  args;
    loadParams(mqtt_setting, args);
    if (hostName.length()) {
      Config.hostName = hostName;
      Serial.println("hostname set to " + Config.hostName);
    }
    Config.bootUri = AC_ONBOOTURI_HOME;
    Config.title = "Relay";
    Portal.config(Config);

    Portal.on(AUX_SETTING_URI, loadParams);
    Portal.on(AUX_SAVE_URI, saveParams);
  }
  else
    Serial.println("load error");
  //root.insert(Server);
  if (Portal.begin()) {
    /*ESP8266WebServer& IntServer = Portal.host();
    IntServer.on("/", handleRoot);
    Portal.onNotFound(handleNotFound);    // For only onNotFound.*/
    Serial.println("Connection established!");
    Serial.print("IP address:\t");
    Serial.println(WiFi.localIP());         // Send the IP address of the ESP8266 to the computer
  }

  ArduinoOTA.setHostname(String(Config.hostName).c_str());

  // No authentication by default
  ArduinoOTA.setPassword("adminLucera");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });

  Serial.println("Initialize ArduinoOTA");
  ArduinoOTA.begin();
  //client.setServer(mqtt_server.c_str(), 1883);
  //client.setCallback(callback);
  timer.setInterval(myInterval, sendState);
  Serial.println("Ready!");
}

void loop() {

  //reading = digitalRead(online_pin);
  //if (reading == HIGH && previous == LOW && millis() - times > debounce) {
//    if (iotStatus == 1){
      //client.disconnect();
      //modes = 0;
      //iotStatus = 0;
      //Serial.println(modes);
      //Serial.println(iotStatus);
    //}
    //else {
    Portal.handleClient();
    //  wifiConnect();

    /*if (!client.connected()) {
          mqttConnect();
    }*/
      iotStatus = 1;
    //}
    times = millis();
  //}
  //previous = reading;

  if (iotStatus == 1 && !client.loop()) {
    mqttConnect();
    getOff();
  }
  ArduinoOTA.handle();
  client.loop();
//Serial.println("Modes : ");
//Serial.println(modes);
//Serial.println("Status : ");
//Serial.println(iotStatus);

  if (modes == 0) {
    getOff();
  }

/*  else if (modes == 1) {
      getDance();
  }*/

  else if (modes == 2) {
      getPattern();
  }

  else if (modes == 3) {
   setLed();
    }

  else if (modes == 4) {
   setCheer();
    }

  else if (modes == 5) {
      getReal();
    }
  else if (modes == 6) {
    Serial.println("Resetting ESP");
    ESP.restart(); //ESP.reset();
  }
  timer.run();
}
