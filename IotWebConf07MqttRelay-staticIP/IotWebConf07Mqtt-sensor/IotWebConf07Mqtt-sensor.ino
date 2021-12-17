/*
   IotWebConf06MqttApp.ino -- IotWebConf is an ESP8266/ESP32
     non blocking WiFi/AP web configuration library for Arduino.
     https://github.com/prampec/IotWebConf

   Copyright (C) 2020 Balazs Kelemen <prampec+arduino@gmail.com>

   This software may be modified and distributed under the terms
   of the MIT license.  See the LICENSE file for details.
*/

/**
   Example: MQTT Demo Application
   Description:
     All IotWebConf specific aspects of this example are described in
     previous examples, so please get familiar with IotWebConf before
     starting this example. So nothing new will be explained here,
     but a complete demo application will be built.
     It is also expected from the reader to have a basic knowledge over
     MQTT to understand this code.

     This example starts an MQTT client with the configured
     connection settings.
     Will post the status changes of the D2 pin in channel "/test/status".
     Receives messages appears in channel "/test/action", and writes them to serial.
     This example also provides the firmware update option.
     (See previous examples for more details!)

   Software setup for this example:
     This example utilizes Joel Gaehwiler's MQTT library.
     https://github.com/256dpi/arduino-mqtt

   Hardware setup for this example:
     - An LED is attached to LED_BUILTIN pin with setup On=LOW.
     - [Optional] A push button is attached to pin D2, the other leg of the
       button should be attached to GND.
*/

#include <MQTT.h>
#include <IotWebConf.h>
#include <IotWebConfUsing.h> // This loads aliases for easier class names.
#ifdef ESP8266
# include <ESP8266HTTPUpdateServer.h>
#include <OneWire.h>
#include <DallasTemperature.h>
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
#define CONFIG_VERSION "sensor1"

// -- When CONFIG_PIN is pulled to ground on startup, the Thing will use the initial
//      password to buld an AP. (E.g. in case of lost password)
#define CONFIG_PIN 4
#define ONE_WIRE_BUS 2
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature DS18B20(&oneWire);
DeviceAddress insideThermometer;
// -- Status indicator pin.
//      First it will light up (kept LOW), on Wifi connection it will blink,
//      when connected to the Wifi it will turn off (kept HIGH).
#define STATUS_PIN 0 //tx

// -- Method declarations.
void handleRoot();
void mqttMessageReceived(String &topic, String &payload);
bool connectMqtt();
bool connectMqttOptions();
// -- Callback methods.
void wifiConnected();
void configSaved();
bool formValidator(iotwebconf::WebRequestWrapper* webRequestWrapper);
float mqttoda;
int state = LOW;
int mqttstate = LOW;
char a[5];
 int room=1;
 String url = "";
String cihazno = "";
const char* url3 = "192.168.1.61";

String ipadresim = "";
DNSServer dnsServer;
WebServer server(80);
WiFiClient net;
MQTTClient mqttClient;
#ifdef ESP8266
ESP8266HTTPUpdateServer httpUpdater;
#elif defined(ESP32)
HTTPUpdateServer httpUpdater;
#endif
char mqttServerValue[STRING_LEN];
char mqttUserNameValue[STRING_LEN];
char mqttUserPasswordValue[STRING_LEN];
char ServerTopicValue[STRING_LEN];
char sensorNoTopicValue[STRING_LEN];
char kombiipadres[STRING_LEN];


char mqttStatusTopic[STRING_LEN];
char mqttSensorTopic[STRING_LEN];
char mqttSensorTopic2[STRING_LEN];
char mqttpubIPTopic[STRING_LEN];
char mqttsubIPTopic[STRING_LEN];
char mqttsubcihaznoTopic[STRING_LEN];

IotWebConf iotWebConf(thingName, &dnsServer, &server, wifiInitialApPassword, CONFIG_VERSION);
// -- You can also use namespace formats e.g.: iotwebconf::ParameterGroup
IotWebConfParameterGroup mqttGroup = IotWebConfParameterGroup("mqtt", "MQTT configuration");
IotWebConfTextParameter mqttServerParam = IotWebConfTextParameter("MQTT server", "mqttServer", mqttServerValue, STRING_LEN);
IotWebConfTextParameter mqttServerNameParam = IotWebConfTextParameter(  "Kombi Merkez adı", "Kombimqtt adı", ServerTopicValue, STRING_LEN);
IotWebConfTextParameter mqttCihazNoNameParam = IotWebConfTextParameter(  "Cihaz NO :(1-3)", "cihazno", sensorNoTopicValue, STRING_LEN);
//IotWebConfTextParameter webkombiipadres = IotWebConfTextParameter(  "Kombi IP adres(girme)", "ıpadres", kombiipadres, STRING_LEN);
//IotWebConfPasswordParameter mqttUserPasswordParam = IotWebConfPasswordParameter("MQTT password", "mqttPass", mqttUserPasswordValue, STRING_LEN);

bool needMqttConnect = false;
bool needReset = false;
int pinState = HIGH;
unsigned long lastReport = 0;
unsigned long lastMqttConnectionAttempt = 0;
float sensor;

#define MQTT_TOPIC_PREFIX "sens/" /////////////***********

void setup()
{
  Serial.begin(115200);
  Serial.println();
  Serial.println("Starting up...");
  DS18B20.begin();
  Serial.print("Found ");
  Serial.print(DS18B20.getDeviceCount(), DEC);
  Serial.println(" devices.");
  DS18B20.setResolution(insideThermometer, 9);

  mqttGroup.addItem(&mqttServerParam);
  mqttGroup.addItem(&mqttServerNameParam);
  mqttGroup.addItem(&mqttCihazNoNameParam);
  //mqttGroup.addItem(&webkombiipadres);
  iotWebConf.setStatusPin(STATUS_PIN);
  iotWebConf.setConfigPin(CONFIG_PIN);
  iotWebConf.addSystemParameter(&mqttGroup);
  iotWebConf.setConfigSavedCallback(&configSaved);
  iotWebConf.setFormValidator(&formValidator);
  iotWebConf.setWifiConnectionCallback(&wifiConnected);

  /* iotWebConf.setupUpdateServer(
    [](const char* updatePath) {
     httpUpdater.setup(&server, updatePath);
    },
    [](const char* userName, char* password) {
     httpUpdater.updateCredentials(userName, password);
    });*/
  // -- Initializing the configuration.
  bool validConfig = iotWebConf.init();
  if (!validConfig)
  {
    mqttServerValue[0] = '\0';
    mqttUserNameValue[0] = '\0';
    mqttUserPasswordValue[0] = '\0';
    ServerTopicValue[0] = '\0';
    sensorNoTopicValue[0] = '\0';
    kombiipadres[0] = '\0';
    mqttStatusTopic[0] = '\0';
    mqttSensorTopic[0] = '\0';
    mqttSensorTopic2[0] = '\0';
    mqttpubIPTopic[0] = '\0';
    mqttsubIPTopic[0] = '\0';
    mqttsubcihaznoTopic[0] = '\0';

  }

  // -- Set up required URL handlers on the web server.
  server.on("/", handleRoot);
  server.on("/config", [] { iotWebConf.handleConfig(); });
  server.onNotFound([]() {
    iotWebConf.handleNotFound();
  });


  String temp = String(MQTT_TOPIC_PREFIX);
  temp += iotWebConf.getThingName();
  temp += "/status";
  temp.toCharArray(mqttStatusTopic, STRING_LEN);

  temp = String(MQTT_TOPIC_PREFIX);
  temp += iotWebConf.getThingName();
  temp += "/sensor";
  temp.toCharArray(mqttSensorTopic, STRING_LEN);

  temp = String(MQTT_TOPIC_PREFIX);
  temp += iotWebConf.getThingName();
  temp += "/ıpadresim";
  temp.toCharArray(mqttpubIPTopic, STRING_LEN);

  temp = "termo/";    ////merkezin ıp adresini alır
  temp += ServerTopicValue;
  temp += "/ıpadres";
  temp.toCharArray(mqttsubIPTopic, STRING_LEN);

  temp = String(MQTT_TOPIC_PREFIX);
  temp += iotWebConf.getThingName();
  temp += "/cihazno";
  temp.toCharArray(mqttsubcihaznoTopic, STRING_LEN);


  mqttClient.begin(mqttServerValue, net);
  mqttClient.onMessage(mqttMessageReceived);

  Serial.println("Hazır.");
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
      mqttstate = LOW;
    }
  }
  else if ((iotWebConf.getState() == iotwebconf::OnLine) && (!mqttClient .connected()))
  {

    Serial.println("MQTT reconnect");
    connectMqtt();
  //  ipadresim = WiFi.localIP().toString().c_str();
  }

  if (needReset)
  {
    Serial.println("Rebooting after 1 second.");
    iotWebConf.delay(1000);
    ESP.restart();
  }


  unsigned long now = millis();
  if ((6000 < now - lastReport))
  {
    pinState = 1 - pinState; // invert pin state as it is changed
    lastReport = now;
    Serial.println("Sending on MQTT channel '/test/status' :");
    Serial.print("topic  :");
    Serial.print(mqttSensorTopic );
    Serial.print(" : ");
    Serial.println(  mqttpubIPTopic);
    String websensor = "";
    websensor += "/?tempoda";
    websensor += sensor;
    Serial.print(" mqttsubIPTopic : ");
    Serial.println( mqttsubIPTopic);
    Serial.print("");
    Serial.print(" ServerTopicValue : ");
    Serial.println( ServerTopicValue);
    Serial.print("");
    Serial.print(" sensorNoTopicValue : ");
    Serial.println( sensorNoTopicValue);
    Serial.print(" room : ");
    Serial.println( room);
     room = (int)mqttSensorTopic;
    switch (room) {
      case 1: {

          String temp = "termo/";  //termo kombi merkez topic ön ismi
          temp += ServerTopicValue;
          temp += "/oda";
          temp.toCharArray(mqttSensorTopic2, STRING_LEN);
         Serial.println(" oda bilgisi gönderildi : ");
          }
        break;
      case 2: {
          String   temp = "termo/";
          temp += ServerTopicValue;
          temp += "/yatak";
          temp.toCharArray(mqttSensorTopic2, STRING_LEN);
           Serial.println(" yatak bilgisi gönderildi : ");

        }
        break;
      case 3: {
          String temp = "termo/";
          temp += ServerTopicValue;
          temp += "/cocuk";
          temp.toCharArray(mqttSensorTopic2, STRING_LEN);
            Serial.println(" cocuk bilgisi gönderildi : ");

        }
        break;
    }
    DS18B20.requestTemperatures();
    sensor = DS18B20.getTempCByIndex(0);
    Serial.print("Temperature: ");
    Serial.println(sensor);


    // Serial.println("server connected");
    /*   net.print(String("GET ") + url2 + " HTTP/1.1\r\n" +
         "Host: " + url + "\r\n" +
         "Connection: close\r\n\r\n");*/


    String strb;
    strb = String(sensor);
    strb.toCharArray(a, 5);

    char message[16];
    sprintf(message, "%u.%u.%u.%u", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
  //  mqttClient.publish(mqttpubIPTopic, message, true, 1);//set
  //  mqttClient.publish(mqttSensorTopic, a, false, 1);
   // mqttClient.publish(mqttSensorTopic2, a, false, 1);
  }
}

/**
   Handle web requests to "/" path.
*/
void handleRoot()
{
  // -- Let IotWebConf test and handle captive portal requests.
  if (iotWebConf.handleCaptivePortal())
  {
    // -- Captive portal request were already served.
    return;
  }
  String s = "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/>";
  s += "<title>IotWebConf 06 MQTT+Web Server</title></head><body>MQTT+Web Server";
  s += "<title>Kombi Kontrol</title>";
  s += "<html>";
  s += "<head>";
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
  s += "<h1>Kombi Sensor</h1>";
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
  s += "<div class='side-by-side text'> Odam sicaklik </div>";
  s += "<div class='side-by-side temperature'>";
  s += (float)sensor;
  s += "<span class='superscript'>&deg;C</span></div>";
  s += "</div>";
  s += "<div><h2>MQTT  haberlesme : ";
  s += (mqttstate == HIGH ? "Baglandi" : "Baglanmadi");
  s += "</div>";
  s += "<div><h2>Kombi adi : ";
  s += (String)ServerTopicValue;
  s += "</div>";
  s += "<div><h2>Kombi adresi : ";
  s += (String)url3;
  s += "</div>";
  s += "<div><h2>Wifi ismim : ";
  s += iotWebConf.getThingName();
  //  s += " ip adresim : ";
  //s += (String)ipadresim;
  s += "</div>";
  s += "<div><h2>Cihaz NO : ";
  s += (String)sensorNoTopicValue;
  s += "</div>";
  s += "<button type='button' onclick=\"location.href='?';\" >Refresh</button>";
  s += "Go to <a href='config'>Kurulum Ayarlari</a> to change values.";
  s += "</body></html>\n";

  server.send(200, "text/html", s);
}


void wifiConnected()
{
  needMqttConnect = true;

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
    mqttServerParam.errorMessage = "En az 3 karakter girin! (broker.hivemq.com)";
    valid = false;
  }
  l = webRequestWrapper->arg(mqttCihazNoNameParam.getId()).length();
  if (l != 1)
  {
    mqttCihazNoNameParam.errorMessage = "Cihaz-no-girin 1-3 arasında!";
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
  Serial.println("            Connecting to MQTT server...");
  if (!connectMqttOptions()) {
    lastMqttConnectionAttempt = now;
    return false;
  }
  Serial.println("Connected!");
  mqttClient.subscribe(mqttsubIPTopic);
  mqttClient.subscribe(mqttStatusTopic);
  //mqttClient.publish(mqttSensorTopic, a, true, 1);
  mqttstate = HIGH;
  return true;
}

/*
  // -- This is an alternative MQTT connection method.
  bool connectMqtt() {
  Serial.println("Connecting to MQTT server...");
  while (!connectMqttOptions()) {
    iotWebConf.delay(1000);
  }
  Serial.println("Connected!");
  mqttClient.subscribe("/test/action");
  return true;
  }
*/

bool connectMqttOptions()
{
  bool result;
  if (mqttUserPasswordValue[0] != '\0')
  {
    result = mqttClient.connect(iotWebConf.getThingName(), mqttUserNameValue, mqttUserPasswordValue);
  }
  else if (mqttUserNameValue[0] != '\0')
  {
    result = mqttClient.connect(iotWebConf.getThingName(), mqttUserNameValue);
  }
  else
  {
    result = mqttClient.connect(iotWebConf.getThingName());
  }
  return result;
}

void mqttMessageReceived(String &topic, String &payload)
{

  Serial.println("mqtt ALINAN VERİ: " + topic + " - " + payload);


  if (topic.endsWith("ıpadress"))
  {
    Serial.print("ıpadress :");
    payload.toCharArray(kombiipadres, 20);
    url = payload;
    Serial.println(kombiipadres);
  }
  if (topic.endsWith("servertopic"))
  {
    Serial.print("servertopic :");
    payload.toCharArray(ServerTopicValue, 20);
    Serial.println(ServerTopicValue);
  }
  if (topic.endsWith("cihazno"))
  {
    Serial.print("cihazno :");
    payload.toCharArray(sensorNoTopicValue, 1);
    cihazno = payload;
    Serial.println(sensorNoTopicValue);
  }


}
