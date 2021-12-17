/**
 * IotWebConf07MqttRelay.ino -- IotWebConf is an ESP8266/ESP32
 *   non blocking WiFi/AP web configuration library for Arduino.
 *   https://github.com/prampec/IotWebConf 
 *
 * Copyright (C) 2020 Balazs Kelemen <prampec+arduino@gmail.com>
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

/**
 * Example: MQTT Relay Demo
 * Description:
 *   All IotWebConf specific aspects of this example are described in
 *   previous examples, so please get familiar with IotWebConf before
 *   starting this example. So nothing new will be explained here, 
 *   but a complete demo application will be built.
 *   It is also expected from the reader to have a basic knowledge over
 *   MQTT to understand this code.
 *   
 *   This example starts an MQTT client with the configured
 *   connection settings.
 *   Will receives messages appears in channel "/devices/[thingName]/action"
 *   with payload ON/OFF, and reports current state in channel
 *   "/devices/[thingName]/status" (ON/OFF). Where the thingName can be
 *   configured in the portal. A relay will be switched on/off
 *   corresponding to the received action. The relay can be also controlled
 *   by the push button.
 *   The thing will delay actions arriving within 7 seconds.
 *   
 *   This example also provides the firmware update option.
 *   (See previous examples for more details!)
 * 
 * Software setup for this example:
 *   This example utilizes Joel Gaehwiler's MQTT library.
 *   https://github.com/256dpi/arduino-mqtt
 * 
 * Hardware setup for this example:
 *   - A Relay is attached to the D5 pin (On=HIGH). Note on relay pin!
 *   - An LED is attached to LED_BUILTIN pin with setup On=LOW.
 *   - A push button is attached to pin D2, the other leg of the
 *     button should be attached to GND.
 *
 * Note on relay pin
 *   Some people might want to use Wemos Relay Shield to test this example.
 *   Now Wemos Relay Shield connects the relay to pin D1.
 *   However, when using D1 as output, Serial communication will be blocked.
 *   So you will either keep on using D1 and miss the Serial monitor
 *   feedback, or connect your relay to another digital pin (e.g. D5).
 *   (You can modify your Wemos Relay Shield for that, as I show it in this
 *   video: https://youtu.be/GykA_7QmoXE)
 */

#include <MQTT.h>
#include <IotWebConf.h>
#include <IotWebConfUsing.h> // This loads aliases for easier class names.
#ifdef ESP8266
# include <ESP8266HTTPUpdateServer.h>
#elif defined(ESP32)
# include <IotWebConfESP32HTTPUpdateServer.h>
#endif
#include <EEPROM.h>
// -- Initial name of the Thing. Used e.g. as SSID of the own Access Point.
const char thingName[] = "testThing";

// -- Initial password to connect to the Thing, when it creates an own Access Point.
const char wifiInitialApPassword[] = "testThing";

#define STRING_LEN 128

// -- Configuration specific key. The value should be modified if config structure was changed.
#define CONFIG_VERSION "mqt2"

// -- When BUTTON_PIN is pulled to ground on startup, the Thing will use the initial
//      password to buld an AP. (E.g. in case of lost password)
#define BUTTON_PIN 4

// -- Status indicator pin.
//      First it will light up (kept LOW), on Wifi connection it will blink,
//      when connected to the Wifi it will turn off (kept HIGH).
#define STATUS_PIN 0

// -- Connected output pin. See "Note on relay pin"!
#define RELAY_PIN 2

#define MQTT_TOPIC_PREFIX "termo/"

// -- Ignore/limit status changes more frequent than the value below (milliseconds).
#define ACTION_FEQ_LIMIT 7000
#define NO_ACTION -1

// -- Method declarations.
void handleRoot();
void mqttMessageReceived(String &topic, String &payload);
bool connectMqtt();
// -- Callback methods.
void wifiConnected();
void configSaved();
bool formValidator(iotwebconf::WebRequestWrapper* webRequestWrapper);
float factor = 0.2;

DNSServer dnsServer;
WebServer server(80);
#ifdef ESP8266
ESP8266HTTPUpdateServer httpUpdater;
#elif defined(ESP32)
HTTPUpdateServer httpUpdater;
#endif
WiFiClient net;
MQTTClient mqttClient;

char mqttServerValue[STRING_LEN];

float mqttoda, mqttyatak, mqttcocuk;
float mqttset = 23;
float setmax = 23.5;
float setmin = 22.5;
float temp;
byte odasec = 1;
byte oda1 = 1;
byte yatak1 = 2;
byte cocuk1 = 3;
int addr = 0;
char b[5];
char c[5];
char d[5];
char e[5];
char f[14];
char g[14];
IotWebConf iotWebConf(thingName, &dnsServer, &server, wifiInitialApPassword, CONFIG_VERSION);
// -- You can also use namespace formats e.g.: iotwebconf::TextParameter
IotWebConfTextParameter mqttServerParam = IotWebConfTextParameter("MQTT server", "mqttServer", mqttServerValue, STRING_LEN);
bool needMqttConnect = false;
bool needReset = false;
unsigned long lastMqttConnectionAttempt = 0;
int needAction = NO_ACTION;
int state = LOW;
int mqttstate=LOW;
unsigned long lastAction = 0;

char mqttActionTopic[STRING_LEN];
char mqttStatusTopic[STRING_LEN];


char mqttpubSetTopic[STRING_LEN];
char mqttpubodaTopic[STRING_LEN];
char mqttpubyatakTopic[STRING_LEN];
char mqttpubcocukTopic[STRING_LEN];
char mqttpubIPTopic[STRING_LEN];
char mqttpubdataGelen[STRING_LEN];

char mqttsubdataSet[STRING_LEN];
char mqttsubdataYatak[STRING_LEN];
char mqttsubdataCocuk[STRING_LEN];
char mqttsubdataOda[STRING_LEN];


char ipAddressValue[STRING_LEN];

int TEMPread;
int temphtml;
int weboda, webyatak, webcocuk, webset;

const char* set_input = "setinput";
 const char* role;


void setup() 
{
  Serial.begin(115200); // See "Note on relay pin"!
  Serial.println();
  Serial.println("Starting up...");

  pinMode(RELAY_PIN, OUTPUT);

  iotWebConf.setStatusPin(STATUS_PIN);
  iotWebConf.setConfigPin(BUTTON_PIN);
  iotWebConf.addSystemParameter(&mqttServerParam);
  iotWebConf.setConfigSavedCallback(&configSaved);
  iotWebConf.setFormValidator(&formValidator);
  iotWebConf.setWifiConnectionCallback(&wifiConnected);
  iotWebConf.setupUpdateServer(
    [](const char* updatePath) { httpUpdater.setup(&server, updatePath); },
    [](const char* userName, char* password) { httpUpdater.updateCredentials(userName, password); });

  // -- Initializing the configuration.
  bool validConfig = iotWebConf.init();
  if (!validConfig)
  {
    mqttServerValue[0] = '\0';
  }

  // -- Set up required URL handlers on the web server.
  server.on("/", handleRoot);
  server.on("/config", []{ iotWebConf.handleConfig(); });
  server.onNotFound([](){ iotWebConf.handleNotFound(); });

 String temp = String(MQTT_TOPIC_PREFIX);
  temp += iotWebConf.getThingName();
  temp += "/role";
  temp.toCharArray(mqttActionTopic, STRING_LEN);

  temp = String(MQTT_TOPIC_PREFIX);
  temp += iotWebConf.getThingName();
  temp += "/set";
  temp.toCharArray(mqttsubdataSet, STRING_LEN);

  temp = String(MQTT_TOPIC_PREFIX);
  temp += iotWebConf.getThingName();
  temp += "/oda";
  temp.toCharArray(mqttsubdataOda, STRING_LEN);

  temp = String(MQTT_TOPIC_PREFIX);
  temp += iotWebConf.getThingName();
  temp += "/yatak";
  temp.toCharArray(mqttsubdataYatak, STRING_LEN);

  temp = String(MQTT_TOPIC_PREFIX);
  temp += iotWebConf.getThingName();
  temp += "/cocuk";
  temp.toCharArray(mqttsubdataCocuk, STRING_LEN);

  temp = String(MQTT_TOPIC_PREFIX);
  temp += iotWebConf.getThingName();
  temp += "/status";
  temp.toCharArray(mqttStatusTopic, STRING_LEN);

  ////////////////////////////////////
  /// bunlar veri göndermek için oluşturuldu,aşağısı

  temp = String(MQTT_TOPIC_PREFIX);
  temp += iotWebConf.getThingName();
  temp += "/webset";
  temp.toCharArray(mqttpubSetTopic, STRING_LEN);

  temp = String(MQTT_TOPIC_PREFIX);
  temp += iotWebConf.getThingName();
  temp += "/weboda";
  temp.toCharArray(mqttpubodaTopic, STRING_LEN);

  temp = String(MQTT_TOPIC_PREFIX);
  temp += iotWebConf.getThingName();
  temp += "/webyatak";
  temp.toCharArray(mqttpubyatakTopic, STRING_LEN);

  temp = String(MQTT_TOPIC_PREFIX);
  temp += iotWebConf.getThingName();
  temp += "/webcocuk";
  temp.toCharArray(mqttpubcocukTopic, STRING_LEN);

  temp = String(MQTT_TOPIC_PREFIX);
  temp += iotWebConf.getThingName();
  temp += "/ıpadres";
  temp.toCharArray(mqttpubIPTopic, STRING_LEN);

  temp = String(MQTT_TOPIC_PREFIX);
  temp += iotWebConf.getThingName();
  temp += "/gelen";
  temp.toCharArray(mqttpubdataGelen, STRING_LEN);
  /////////////////////////////////////



  mqttClient.begin(mqttServerValue, net);
  mqttClient.onMessage(mqttMessageReceived);
  
Serial.println(" ");
Serial.println(" ");
  Serial.println("HAZIR.");
Serial.println(" ");
Serial.println(" ");
}

void loop() 
{
  // -- doLoop should be called as frequently as possible.
  iotWebConf.doLoop();
  mqttClient.loop();
  
  if (needMqttConnect)
  {
    if (connectMqtt())
    {
      needMqttConnect = false;
    }
  }
  else if ((iotWebConf.getState() == iotwebconf::OnLine) && (!mqttClient.connected()))
  {
    Serial.println("MQTT reconnect");
    connectMqtt();
  }

  if (needReset)
  {
    Serial.println("Rebooting after 1 second.");
    iotWebConf.delay(1000);
    ESP.restart();
  }

  unsigned long now = millis();

  // -- Check for button push
  if ((digitalRead(BUTTON_PIN) == LOW)
    && ( ACTION_FEQ_LIMIT < now - lastAction))
  {
    needAction = 1 - state; // -- Invert the state
  }
  
  if ((needAction != NO_ACTION)
    && ( ACTION_FEQ_LIMIT < now - lastAction))
  {
      ///////////////////////////////////////////////////////////////////////////


    switch (odasec) {
      case 1:
        Serial.println("oda seçili");
        if (needMqttConnect ) {
          temp = mqttoda;
        } else {
          temp = weboda;
        }
        break;
      case 2:
        Serial.println("yatak seçili");
        if (needMqttConnect ) {
          temp = mqttyatak;
        } else {
          temp = webyatak;
        }
        break;
      case 3:
        Serial.println("cocuk seçili");
        if (needMqttConnect ) {
          temp = mqttcocuk;
        } else {
          temp = webcocuk;
        }
        break;
    }
    if (setmax >= temp ) {
      digitalWrite(RELAY_PIN, HIGH); //ROLE ÇEKİKKEN KOMBİ DURUR
      state == LOW;
      role="0";
    }
    if (setmin <= temp ) {
      digitalWrite(RELAY_PIN, LOW); //ROLE ÇEKİKKEN KOMBİ DURUR
      state == HIGH;
      role="1";
    }

    ///////////////////////////////////////////////////////////////////////////

    state = needAction;
    digitalWrite(RELAY_PIN, state);
    if (state == HIGH)
    {
      iotWebConf.blink(5000, 95);
    }
    else
    {
      iotWebConf.stopCustomBlink();
    }
    mqttClient.publish(mqttStatusTopic, state == HIGH ? "ON" : "OFF", true, 1);
    mqttClient.publish(mqttActionTopic, state == HIGH ? "ON" : "OFF", true, 1);
    Serial.print("Switched ");
    Serial.println(state == HIGH ? "ON" : "OFF");
    needAction = NO_ACTION;
    lastAction = now;
  }
}

/**
 * Handle web requests to "/" path.
 */
void handleRoot()
{
  // -- Let IotWebConf test and handle captive portal requests.
  if (iotWebConf.handleCaptivePortal())
  {
    // -- Captive portal request were already served.
    return;
  }
 String s = F("<!DOCTYPE html><html lang=\"tr\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/>");
  // s +="<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  // s += iotWebConf.getHtmlFormatProvider()->getStyle();
  s += "<html>";
  s += "<head>";
  s += "<title>Kombi Kontrol</title>";
  s += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  s += "<link href='https://fonts.googleapis.com/css?family=Open+Sans:300,400,600' rel='stylesheet'>";
  s += "<style>";
  s += "html { font-family: 'Open Sans', sans-serif; display: block; margin: 0px auto; text-align: center;color: #444444;}";
  s += "body{margin-top: 50px;} ";
  s += "h1 {margin: 50px auto 30px;} ";
  s += ".side-by-side{display: table-cell;vertical-align: middle;position: relative;}";
  s += ".text{font-weight: 600;font-size: 19px;width: 200px;}";
  s += ".temperature{font-weight: 300;font-size: 50px;padding-right: 15px;}";
  s += ".living-room .temperature{color: #3B97D3;}";
  s += ".bedroom .temperature{color: #F29C1F;}";
  s += ".kitchen .temperature{color: #26B99A;}";
  s += ".superscript{font-size: 17px;font-weight: 600;position: absolute;right: -5px;top: 15px;}";
  s += ".data{padding: 10px;}";
  s += ".container{display: table;margin: 0 auto;}";
  s += ".icon{width:82px}";
  s += "</style>";
  s += "</head>";
  s += "<body>";
  s += "<h1>Kombi Kontrol</h1>";
  s += "<div class='container'>";
  s += "<div class='data living-room'>";
  s += "<div class='side-by-side icon'>";
  s += "<svg enable-background='new 0 0 65.178 45.699'height=45.699px id=Layer_1 version=1.1 viewBox='0 0 65.178 45.699'width=65.178px x=0px xml:space=preserve xmlns=http://www.w3.org/2000/svg xmlns:xlink=http://www.w3.org/1999/xlink y=0px><polygon fill=#3B97D3 points='8.969,44.261 8.969,16.469 7.469,16.469 7.469,44.261 1.469,44.261 1.469,45.699 14.906,45.699 ";
  s += "14.906,44.261 '/><polygon fill=#3B97D3 points='13.438,0 3,0 0,14.938 16.438,14.938 '/><polygon fill=#3B97D3 points='29.927,45.699 26.261,45.699 26.261,41.156 32.927,41.156 '/><polygon fill=#3B97D3 points='58.572,45.699 62.239,45.699 62.239,41.156 55.572,41.156 '/><path d='M61.521,17.344c-2.021,0-3.656,1.637-3.656,3.656v14.199H30.594V21c0-2.02-1.638-3.656-3.656-3.656";
  s += "c-2.02,0-3.657,1.636-3.657,3.656v14.938c0,2.021,1.637,3.655,3.656,3.655H61.52c2.02,0,3.655-1.637,3.655-3.655V21";
  s += "C65.177,18.98,63.54,17.344,61.521,17.344z'fill=#3B97D3 /><g><path d='M32.052,30.042c0,2.02,1.637,3.656,3.656,3.656h16.688c2.019,0,3.656-1.638,3.656-3.656v-3.844h-24";
  s += "L32.052,30.042L32.052,30.042z'fill=#3B97D3 /><path d='M52.396,6.781H35.709c-2.02,0-3.656,1.637-3.656,3.656v14.344h24V10.438";
  s += "C56.053,8.418,54.415,6.781,52.396,6.781z'fill=#3B97D3 /></g></svg>";
  s += "</div>";
  s += "<div class='side-by-side text'>Oturma Odasi</div>";
  s += "<div class='side-by-side temperature'>";
  s += (float)mqttoda;
  //s += (float)weboda;
  s += "<span class='superscript'>&deg;C</span></div>";
  s += "</div>";
  s += "<div class='data bedroom'>";
  s += "<div class='side-by-side icon'>";
  s += "<svg enable-background='new 0 0 43.438 35.75'height=35.75px id=Layer_1 version=1.1 viewBox='0 0 43.438 35.75'width=43.438px x=0px xml:space=preserve xmlns=http://www.w3.org/2000/svg xmlns:xlink=http://www.w3.org/1999/xlink y=0px><g><path d='M25.489,14.909H17.95C13.007,14.908,0,15.245,0,20.188v3.688h43.438v-3.688";
  s += "C43.438,15.245,30.431,14.909,25.489,14.909z'fill=#F29C1F /><polygon fill=#F29C1F points='0,31.25 0,35.75 2.5,35.75 4.5,31.25 38.938,31.25 40.938,35.75 43.438,35.75 43.438,31.25 ";
  s += "43.438,25.375 0,25.375  '/><path d='M13.584,11.694c-3.332,0-6.033,0.973-6.033,2.175c0,0.134,0.041,0.264,0.105,0.391";
  s += "c3.745-0.631,7.974-0.709,10.341-0.709h1.538C19.105,12.501,16.613,11.694,13.584,11.694z'fill=#F29C1F /><path d='M30.009,11.694c-3.03,0-5.522,0.807-5.951,1.856h1.425V13.55c2.389,0,6.674,0.081,10.444,0.728";
  s += "c0.069-0.132,0.114-0.268,0.114-0.408C36.041,12.668,33.34,11.694,30.009,11.694z'fill=#F29C1F /><path d='M6.042,14.088c0-2.224,3.376-4.025,7.542-4.025c3.825,0,6.976,1.519,7.468,3.488h1.488";
  s += "c0.49-1.97,3.644-3.489,7.469-3.489c4.166,0,7.542,1.801,7.542,4.025c0,0.17-0.029,0.337-0.067,0.502";
  s += "c1.08,0.247,2.088,0.549,2.945,0.926V3.481C40.429,1.559,38.871,0,36.948,0H6.49C4.568,0,3.009,1.559,3.009,3.481v12.054";
  s += "c0.895-0.398,1.956-0.713,3.095-0.968C6.069,14.41,6.042,14.251,6.042,14.088z'fill=#F29C1F /></g></svg>";
  s += "</div>";
  s += "<div class='side-by-side text'>Yatak odasi</div>";
  s += "<div class='side-by-side temperature'>";
  s += (float)mqttyatak;
  s += "<span class='superscript'>&deg;C</span></div>";
  s += "</div>";
  s += "<div class='data kitchen'>";
  s += "<div class='side-by-side icon'>";
  s += "<svg enable-background='new 0 0 43.438 35.75'height=35.75px id=Layer_1 version=1.1 viewBox='0 0 43.438 35.75'width=43.438px x=0px xml:space=preserve xmlns=http://www.w3.org/2000/svg xmlns:xlink=http://www.w3.org/1999/xlink y=0px><g><path d='M90.8,93.6c-2,1.5-2.5,1.9-3.1,2.5c-0.6-0.6-1.2-1-3.1-2.5c-1.9-1.5-4.4-3.4-4.4-6.2c0-4.1,5-6.3,7.5-2.5";
  s += "c2.5-3.7,7.5-1.5,7.5,2.5C95.1,90.2,92.7,92.2,90.8,93.6'fill=#F38786 /><polygon fill=#F29C1F points='0,31.25 0,35.75 2.5,35.75 4.5,31.25 38.938,31.25 40.938,35.75 43.438,35.75 43.438,31.25 ";
  s += "43.438,25.375 0,25.375  '/><path d='M107.7,48.7C101.5,41.9,84.8,37,65,36.9v-5l0.6-0.1c2.5-0.6,4.2-2.9,4.2-5.5c0-3.1-2.5-5.6-5.6-5.6";
  s += "c-3.1,0-5.6,2.5-5.6,5.6c0,0.6,0.5,1.1,1.1,1.1c0.6,0,1.1-0.5,1.1-1.1c0-1.9,1.5-3.4,3.4-3.4c1.9,0,3.4,1.5,3.4,3.4";
  s += "c0,1.9-1.5,3.4-3.4,3.4l-0.1,0l-0.1,0c0,0-0.1,0-0.1,0c-0.6,0-1.1,0.5-1.1,1.1v6.1c-19.7,0.2-36.3,5.1-42.5,11.8'fill=#F7B2B8 /><path d='M38.4,75.1l2.4,7.3c0.1,0.3,0.4,0.5,0.7,0.5h7.7c0.7,0,1,0.9,0.4,1.3l-6.3,4.5c-0.2,0.2-0.3,0.5-0.3,0.8";
  s += "l2.4,7.3c0.2,0.6-0.5,1.2-1.1,0.8l-6.3-4.5c-0.2-0.2-0.6-0.2-0.8,0l-6.3,4.5c-0.5,0.4-1.3-0.1-1.1-0.8l2.4-7.3";
  s += "c0.1-0.3,0-0.6-0.3-0.8l-6.3-4.5c-0.5-0.4-0.3-1.3,0.4-1.3H34c0.3,0,0.6-0.2,0.7-0.5l2.4-7.3C37.3,74.5,38.2,74.5,38.4,75.1'fill=#F3D04C /><path d='M70.9,90.8c0-4.3,2.6-7.9,6.2-9.5c0.3-0.1,0.3-0.6-0.1-0.7c-0.7-0.1-1.4-0.2-2.1-0.2";
  s += "c-5.7,0.1-10.3,4.7-10.3,10.4c0,5.7,4.7,10.4,10.4,10.4c0.7,0,1.3-0.1,2-0.2c0.3-0.1,0.4-0.5,0.1-0.7";
  s += "C73.4,98.7,70.9,95.1,70.9,90.8'fill=#7ACCCE  /><path d='M54.5,102.6c-1.3,0-2.3-1-2.3-2.3c0-1.3,1-2.3,2.3-2.3c1.3,0,2.3,1,2.3,2.3";
  s += "C56.8,101.5,55.8,102.6,54.5,102.6 M61.7,99.2c0-1.7-1.3-3-3-3c-0.4,0-0.7,0.1-1,0.2c0,0,0-0.1,0-0.1c0-1.7-1.3-3-3-3   c-1.7,0-3,1.3-3,3c0,0,0,0.1,0,0.1c-0.3-0.1-0.7-0.2-1.1-0.2c-1.7,0-3,1.3-3,3c0,1.4,1,2.6,2.3,2.9c-0.6,0.6-1,1.4-1,2.2   c0,1.7,1.4,3,3,3c1.3,0,2.4-0.8,2.8-2c0.5,1,1.5,1.7,2.7,1.7c1.7,0,3-1.3,3-3c0-0.8-0.3-1.5-0.8-2C60.9,101.7,61.7,100.5,61.7,99.2   z'fill=#7ACCCE  /></g></svg>";

  s += "</div>";
  s += "<div class='side-by-side text'>Cocuk Odasi</div>";
  s += "<div class='side-by-side temperature'>";
  s += (float)mqttcocuk;
    s += "<div><h1>SET : ";
  s += (String)mqttset;
  s += "<span class='superscript'>&deg;C</span></div>";
  s += "</div>";
  s += "<div><h1>Wifi ismim : ";
  s += iotWebConf.getThingName();
  s += "<div>KOMBi : ";
  s += (state == LOW ? "ACIK" : "KAPALI");
  s += "</div>";
  s += "<div>MQTT  haberlesme : ";
  s += (mqttstate == HIGH ? "Baglandi" : "Baglanmadi");
  s += "</div>";
  s += "<title>POST Pin controller</title>\n</head>\n<body>\n";
  //s += "</div>";
  //s += "<h1>SET : ";
  //s += "<input type='number' id='fname' name='fname' placeholder='1.' step='0.1' min='15' max='30' required><br><br>";
  //s += "<form action=\"/\"> First name:<br> <input type='text' value=''>\n";

  //s += "<input type=submit value=submit></form>";
  s += "<button type='button' onclick=\"location.href='?action=on';\" >Kombi AC</button>";
  s += "<button type='button' onclick=\"location.href='?action=off';\" >Kombi KAPAT</button>";
  s += "<button type='button' onclick=\"location.href='?';\" >Refresh</button>";
  s += "<div><a href='config'>Kurulum Ayarlari</a></div>";
  //s += "<meta http-equiv=\"refresh\" content=\"0;url=/\">";
  s += "</body></html>\n";




  server.send(200, "text/html", s);
}

void wifiConnected()
{
  needMqttConnect = true;
    Serial.println( WiFi.localIP());
    mqttstate = LOW;
}

void configSaved()
{
  Serial.println("Configuration was updated.");
  needReset = true;
}

bool formValidator(iotwebconf::WebRequestWrapper* webRequestWrapper)
{
  Serial.println("Validating form.");
  bool valid = true;

  int l = webRequestWrapper->arg(mqttServerParam.getId()).length();
  if (l < 3)
  {
    mqttServerParam.errorMessage = "Please provide at least 3 characters!";
    valid = false;
  }

  return valid;
}

bool connectMqtt() {
  unsigned long now = millis();
  if (1000 > now - lastMqttConnectionAttempt)
  {
    // Do not repeat within 1 sec.
    return false;
  }
  Serial.println("Connecting to MQTT server...");
  if (!mqttClient.connect(iotWebConf.getThingName())) {
    lastMqttConnectionAttempt = now;
    return false;
  }
  Serial.println("Connected!");

  mqttstate = HIGH;
  mqttClient.subscribe(mqttsubdataSet);
  mqttClient.subscribe(mqttsubdataYatak);
  mqttClient.subscribe(mqttsubdataOda);
  mqttClient.subscribe(mqttsubdataCocuk);

  mqttClient.subscribe(mqttActionTopic);
  //mqttClient.subscribe(mqttStatusTopic);
  //mqttClient.publish(mqttStatusTopic, role, false, 1);
  //mqttClient.publish(mqttActionTopic, state == LOW ? "ON" : "OFF", true, 1);
  // mqttClient.subscribe(mqttActionTopic);
  //mqttClient.publish(mqttStatusTopic, state == HIGH ? "ON" : "OFF", true, 1);
  //mqttClient.publish(mqttActionTopic, state == HIGH ? "ON" : "OFF", true, 1);

  return true;
}

void mqttMessageReceived(String &topic, String &payload)
{
    Serial.println("mqtt ALINAN VERİ: " + topic + " - " + payload);
  Serial.print("ODA :");
  Serial.println(mqttoda);

  Serial.print("COCUK :");
  Serial.println(mqttcocuk);


  Serial.print("YATAK :");
  Serial.println(mqttyatak);
  Serial.print("setmax :");
  Serial.println(setmax);
  Serial.print("setmin :");
  Serial.println(setmin);

  if (topic.endsWith("role"))
  {
    Serial.println("burda 1");
    needAction = 1 - state;//ters çevirir
    needAction = payload.equals("ON") ? HIGH : LOW;
    if (needAction == state)
    {
      needAction = LOW;
      needAction = NO_ACTION;
    }
  }

  if (topic.endsWith("oda"))
  {
    Serial.print("oda :");
    mqttoda = payload.toFloat();
    weboda = mqttoda;
    Serial.println(mqttoda);
  }
  if (topic.endsWith("yatak"))
  {
    Serial.print("yatak :");
    mqttyatak = payload.toFloat();
    webyatak = mqttyatak;
    Serial.println(mqttyatak);
  }

  if (topic.endsWith("cocuk"))
  {
    Serial.print("cocuk :");

    mqttcocuk = payload.toFloat();
    webcocuk = mqttcocuk;
    Serial.println(mqttcocuk);
  }
  if (topic.endsWith("set"))
  {
    Serial.print("set :");
    Serial.println(payload);
    mqttset = payload.toFloat();
    setmin = mqttset - factor;
    setmax = mqttset + factor;
    EEPROM.write(addr, mqttset );
    EEPROM.commit();
  }
  if (topic.endsWith("odasec"))
  {
    Serial.print("odasec :");
    Serial.println(payload);
    odasec = oda1;
    EEPROM.write(addr, odasec );
    EEPROM.commit();
  }
  if (topic.endsWith("odayatak"))
  {
    Serial.print("odayatak :");
    Serial.println(payload);
    odasec = yatak1;

    EEPROM.write(addr, odasec );
    EEPROM.commit();
  }
  if (topic.endsWith("odacocuk"))
  {
    Serial.print("odacocuk :");
    Serial.println(payload);
    odasec = cocuk1;

 //   EEPROM.write(addr, odasec );
    //EEPROM.commit();
    
  }
}
