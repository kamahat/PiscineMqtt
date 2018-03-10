
/*
    V3.1 2017-04-09
  - Suppression de l'EEPROM
  - changement du port Onewire
  - ajout clignotement LED
  - mise ) jour de la libfrairei MQTT pub en 2.6
*/


/* MQTT Specifique
  to avoid timeout errors "MQTT_CONNECTION_TIMEOUT" -4 when connecting,
  set the version in PubSubClient.h to the older version 3_1
  // MQTT_VERSION : Pick the version
  #define MQTT_VERSION MQTT_VERSION_3_1 <<- uncomment this line
*/

#include <ESP8266WiFi.h> // Library for Wifi management
#include <ESP8266Ping.h> // Library for Ping 

#include <PubSubClient.h> // Library for the MQTT client

#include <OneWire.h>

#include <DallasTemperature.h>

#include <EEPROM.h>

#include <ArduinoJson.h> // Library for Json Parsing

// Data wire is plugged into pin 2 on the Arduino
#define ONE_WIRE_BUS 14 // D5 (IO, SCK) = GPIO14
#define LED 2         // D4(IO, 10k Pull-up, BUILTIN_LED) =   GPIO2

// Init des relais
const int relay_pin[] = {12, 13}; // Relais D6 = GPIO12 //moteur; // Relais D7 = GPIO13 //lampe
bool relayState[] = {HIGH, HIGH}; //etat relay moteur HIGH= stop

// Setup a oneWire instance to communicate with any OneWire devices
// (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

DallasTemperature sensors(&oneWire);

// Wifi Data
// Update these with values suitable for your network.
const char* ssid = "dd-wrt";
const char* password = "LiBeBCNOF";
WiFiClient espClient;

//MQTT Configuration data
const char* mqtt_server = "mqtt.zalin.home";
const int mqtt_port = 1883;
const char* DomotOutTopic = "domoticz/in";
const char* mqtt_JeedomOutTopic = "piscine_mqtt/capteur";
const char* mqtt_moteur_JeedomInTopic = "piscine_moteur_mqtt/statut";
const char* mqtt_lumiere_JeedomInTopic = "piscine_lumiere_mqtt/statut";

const char* mqtt_all_info_topic = "piscine_mqtt/all_info";
const char* mqqt_identifier = "ESP8266_piscine_Bhw8dNqFEDKS0Ey";
//const String room_name = "Regulation Piscine"; // String to be displayed at the top of the display

PubSubClient MQTTclient(espClient);

//JSON

long lastMsg = 0;
float temp = 0; // derniere temperature enregistre
float pH = 7;
int inPin = 5;

void setup_wifi() {
  Serial.print("INFO - Setup Wifi.");
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA); // wifi client uniquement
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  Serial.print("Pinging host ");
  Serial.print(mqtt_server);

  if (Ping.ping(mqtt_server)) {
    Serial.println(".....Success!!");
  } else {
    Serial.println(".....Error :(");
  }
  Serial.println("INFO - End Setup Wifi.");
}

void MQTTclientReconnect() {
  // Loop until we're reconnected
  while (!MQTTclient.connected()) {
    Serial.print("INFO - Attempting connection to MQTT server.");

    // Attempt to connect
    if (MQTTclient.connect(mqqt_identifier)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      MQTTclient.publish(mqtt_all_info_topic, "Piscine booted");
      // ... and resubscribe
      MQTTclient.subscribe(mqtt_moteur_JeedomInTopic);
      MQTTclient.subscribe(mqtt_lumiere_JeedomInTopic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(MQTTclient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void MQTTcallback(char* topic, byte* payload, unsigned int length) {
  //void callback(const char[] topic, byte* payload, unsigned int length)
  int consigne;
  digitalWrite(LED, HIGH);
  Serial.print("INFO - Message arrived Topic : [ ");
  Serial.print(topic);
  Serial.println(" ] ");
  String cstring = (char *) payload;
  Serial.print("INFO - Message arrived Subject : [ ");
  Serial.print(cstring);
  Serial.println(" ] ");


  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(payload);

  if (!root.success()) {
    Serial.println("ERROR - Incomming JSON Parsing Fail");
  } else {
    Serial.print("INFO - Incomming JSON Parsing Succeed");
    root.printTo(Serial); Serial.println();
    consigne = root["consigne"];
  }

  int relay = idRelay(topic);
  Serial.print("relay #" + (String)relay);
  /*
    if ((String)topic==(String)mqtt_lumiere_JeedomInTopic){
     int relay=0;
    } else if ((String)topic==(String)mqtt_moteur_JeedomInTopic){
     int relay=1;
    } else {Serial.println("ERROR - unknown Relay"); relay=255;}*/

  switch (consigne) {
    case 0 :
      relayState[relay] = HIGH;
      Serial.println(" -> LOW");
      break;
    case 1 :
      relayState[relay] = LOW;
      Serial.println(" -> HIGH");
      break;
    case 2 :
      relayState[relay] = !relayState[relay];
      Serial.println(" -> switched to " + (String) relayState[relay]);
      break;
    default:
      Serial.println("ERROR - Consigne inconnue : ");
      break;
  }

  digitalWrite(relay_pin[relay], relayState[relay]);  // Turn the Relay on (Note that LOW is the voltage level
  BlinkRelay(relay_pin[relay],relayState[relay]);
  //EEPROM.write(relay, relayState[relay]);    // Write state to EEPROM
  //EEPROM.commit();
}// - See more at: http://www.esp8266.com/viewtopic.php?f=29&t=8746#sthash.QkJGl8lV.dpuf

// function id relay
int idRelay(String s1) {
  int result = 255;
  if (s1 == (String)mqtt_lumiere_JeedomInTopic) {
    result = 1;
  } else if (s1 == (String)mqtt_moteur_JeedomInTopic) {
    result = 0;
  } else {
    Serial.println("ERROR - unknown Relay");
  }
  Serial.println("INFO - Relay ID = " + (String) result);
  return result;
}

void MQTTclientSetup() {
  MQTTclient.setServer(mqtt_server, mqtt_port);
  MQTTclient.setCallback(MQTTcallback);
}

void BlinkRelay(int relay, boolean status) {
  digitalWrite(LED, LOW);
  for (int i = 0; i < relay; i++) {
    digitalWrite(LED, HIGH);
    delay(250);
    digitalWrite(LED, LOW);
  }
  digitalWrite(LED, HIGH);
  delay(500);
  digitalWrite(LED, LOW);
  if (status) {
    digitalWrite(LED, HIGH);
    delay(500);
    digitalWrite(LED, LOW);
  }
    digitalWrite(LED, LOW);
}

void setup()
{
  Serial.begin(115200);
  Serial.println();
  Serial.println("INFO - Serial port is ready.");
  Serial.println("INFO - Beggining of the Setup.");
  // Init LEd
  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);
  delay(1000);
  //

  digitalWrite(LED, HIGH);
  setup_wifi();  // Wifi client initialization
  digitalWrite(LED, LOW);
  //
  digitalWrite(LED, HIGH);
  pinMode(inPin, INPUT);
  sensors.begin();
  digitalWrite(LED, LOW);
  //
  //EEPROM.begin(512); // Begin eeprom to store on/off state

  for (int i = 0; i < (sizeof(relay_pin) / sizeof(int)); i++) {

    pinMode(relay_pin[i], OUTPUT);     // Initialize the relay pin as an output
    //relayState[i] = EEPROM.read(i);    // restaure les valeurs anterieur
    Serial.println("INFO -Init Relay Pin : " + (String) i + " = " + (String)relayState[i]);
    digitalWrite(relay_pin[i], relayState[i]);
  }

  //
  //MQTT
  digitalWrite(LED, HIGH);
  MQTTclientSetup();  // MQTT client setup (port, adress, callback)
  digitalWrite(LED, LOW);
  Serial.println("INFO - End of the Setup... Starting the loop.");
}

void loop()
{
  String tempMSG;
  StaticJsonBuffer<200> jsonBuffer1;
  StaticJsonBuffer<200> jsonBuffer2;
  JsonObject& root1 = jsonBuffer1.createObject();
  JsonObject& root2 = jsonBuffer2.createObject();


  if (!MQTTclient.connected()) {
    MQTTclientReconnect();
  }
  MQTTclient.loop();

  long now = millis();
  if (now - lastMsg > 60000) {
    lastMsg = now;
    sensors.setResolution(12);
    sensors.requestTemperatures(); // Send the command to get temperatures
    temp = sensors.getTempCByIndex(0);
    //Serial.println(temp);

    // Id du capteur de temperature
    root1["idx"] = 7;
    root1["nvalue"] = 0;

    if ((temp > -20) && (temp < 60))
    {
      //Domoticz publish
      //String tempMSG=tempToMQTT+String(temp)+String("\" }");
      root1["svalue"] = String(temp);
      root2["temp"] = String(temp);
      root2["pH"] = String(pH);

      //domoticz
      root1.printTo(tempMSG);
      Serial.println((String)DomotOutTopic + " == " + tempMSG);
      MQTTclient.publish(DomotOutTopic, tempMSG.c_str() , TRUE); //'{ "idx" : 7, "nvalue" : 0, "svalue" : "25.0" }' -t 'domoticz/in'

      //jeedom
      tempMSG = "";
      root2.printTo(tempMSG);
      Serial.println((String)mqtt_JeedomOutTopic + " == " + tempMSG);
      MQTTclient.publish(mqtt_JeedomOutTopic, tempMSG.c_str(), TRUE); //  { "temp" : "22.62" } -t 'sensor/piscine/capteur'
    }
  }
}
