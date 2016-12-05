//  Version 1_0_0    Initial Release
#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager

#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Bounce2.h>
#include <EEPROM.h>
#include <stdlib.h>
#include <string.h>
using namespace std;

//define your default values here, if there are different values in config.json, they are overwritten.
char mqtt_server[40];
char mqtt_port[6] = "1883";
char location[40] = "This Unit Location";

//  Other Variables
char *inTopic = new char[40];
char *outTopic = new char[100];
String messageType = "PowerUp";
String topic;
String msgOut;
int newState;
char *btnOut = new char[100];

//flag for saving data
bool shouldSaveConfig = false;

WiFiClient espClient;
PubSubClient client(espClient);
char msg[250];
int value = 0;
char * strtokIndx; // this is used by strtok() as an index

int relay_pin = 12;
int button_pin = 0;
bool relayState = LOW;

// Instantiate a Bounce object :
Bounce debouncer = Bounce();

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

// callback from mqtt
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.println("mqtt Message arrived");
  char msgIn[length];
  for(unsigned int i = 0; i < length; i++) msgIn[i] = (char)payload[i];
  strtokIndx = strtok(msgIn,"\"");      // get the first part - the string  Throw this away 1
  strtokIndx = strtok(NULL, "\"");       // this continues where the previous call left off  Throw this away
  strtokIndx = strtok(NULL, "\"");       // this continues where the previous call left off  Throw this away
  strtokIndx = strtok(NULL, "\"");       // this continues where the previous call left off  MsgType
  messageType = strtokIndx;
  Serial.print("messageType: ");
  Serial.println(messageType);
  strtokIndx = strtok(NULL, "\"");       // this continues where the previous call left off  Throw this away
  strtokIndx = strtok(NULL, "\"");       // this continues where the previous call left off  Throw this away
  strtokIndx = strtok(NULL, "\"");       // this continues where the previous call left off  Throw this away
  strtokIndx = strtok(NULL, "\"");       // this continues where the previous call left off  Used by if loop

  if (messageType == "StateChanageCmd"){
  Serial.print("State Change Received");
    newState = atoi(strtokIndx);
    // Switch on the LED if an 1 was received as the State
    if (newState == 0) {
      digitalWrite(relay_pin, LOW);   // Turn the LED on (Note that LOW is the voltage level
      relayState = LOW;
      EEPROM.write(0, relayState);    // Write state to EEPROM
      EEPROM.commit();
    } else if (newState == 1) {
      digitalWrite(relay_pin, HIGH);  // Turn the LED off by making the voltage HIGH
      relayState = HIGH;
      EEPROM.write(0, relayState);    // Write state to EEPROM
      EEPROM.commit();
    } else if (newState == 2) {
      relayState = !relayState;
      digitalWrite(relay_pin, relayState);  // Turn the LED off by making the voltage HIGH
      EEPROM.write(0, relayState);    // Write state to EEPROM
      EEPROM.commit();
    }
  } else if (messageType == "LocationChgCmd"){
  Serial.print("Location Change Received");
    strcpy(location, strtokIndx);
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"] = mqtt_port;
    json["location"] = location;  
    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }  
    json.printTo(configFile);
    configFile.close();
    //end save      
  } else if (messageType == "PingRequest"){
  Serial.print("Ping Request Received");
    msgOut = "{\"messagetype\":\"PingResponse\"}";
    strcpy(btnOut, msgOut.c_str());
    client.publish(outTopic, btnOut);
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP8266Client")) {
      // Once connected, publish an announcement...
      Serial.println("connected");
      //   {"deviceid":"switch_basement1","deviceip":"192.168.1.25","devicetype":"switch","mqttpath":"switch/basement1","devicename":"Basement Utility Room Light","status":"0"}
      String strMac = WiFi.macAddress();
      String strIP = (String) WiFi.localIP().toString();
      String strLocation = location;
      String strStatus;
      if (relayState){
        strStatus = "1";
      }else {
        strStatus = "0";
      }
      String strMsg = "{\"deviceip\":\"" + strIP + "\",\"location\":\"" + strLocation +"\",\"messagetype\":\"" + messageType +"\",\"status\":\"" + strStatus +"\"}";
      char* outMsg = "";
      strcpy(outMsg, strMsg.c_str());
      client.publish(outTopic, outMsg);
      // ... and resubscribe
      client.subscribe(inTopic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      for (int i = 0; i < 5000; i++) {
        extButton();
        delay(1);
      }
    }
  }
}

void extButton() {
  debouncer.update();

  // Call code if Bounce fell (transition from HIGH to LOW) :
  if ( debouncer.fell() ) {
    Serial.println("Button Press");
    // Toggle relay state :
    relayState = !relayState;
    digitalWrite(relay_pin, relayState);
    EEPROM.write(0, relayState);    // Write state to EEPROM
    topic = "StateChanageReport";
    msgOut = "{\"messagetype\":\"" + topic + "\",\"status\":\"" + relayState +"\"}";
    strcpy(btnOut, msgOut.c_str());
    client.publish(outTopic, btnOut);
  }
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println();
  EEPROM.begin(512);              // Begin eeprom to store on/off state
  pinMode(relay_pin, OUTPUT);     // Initialize the relay pin as an output
  pinMode(button_pin, INPUT);     // Initialize the relay pin as an output
  pinMode(13, OUTPUT);
  relayState = EEPROM.read(0);
  digitalWrite(relay_pin, relayState);
  debouncer.attach(button_pin);   // Use the bounce2 library to debounce the built in button
  debouncer.interval(50);         // Input must be low for 50 ms
  digitalWrite(13, LOW);          // Blink to indicate setup
  delay(500);
  digitalWrite(13, HIGH);
  delay(500);

  //clean FS, for testing 
//  SPIFFS.format();

  //read configuration from FS json
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);
        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        if (json.success()) {
          Serial.println("\nparsed json");
          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_port, json["mqtt_port"]);
          strcpy(location, json["location"]);
        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read

  // The extra parameters to be configured (can be either global or just in the setup)
  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 6);
  WiFiManagerParameter custom_location("location", "location", location, 40);
  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  //add all your parameters here
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_location);
  //reset settings - for testing
  // wifiManager.resetSettings();
  // wiFiManager fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect("SwitchAP")) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }
  //if you get here you have connected to the WiFi
  Serial.println("connected...");
  //read updated parameters
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(location, custom_location.getValue());
  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"] = mqtt_port;
    json["location"] = location;
    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }
    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
    messageType = "DataReset";
  }
  String strTopic = "/switch/" + WiFi.macAddress();
  String strOutTopic = strTopic + "/in";
  String strInTopic = strTopic + "/out";
  strcpy(outTopic, strOutTopic.c_str());
  strcpy(inTopic, strInTopic.c_str());
  client.setServer(mqtt_server, atoi(mqtt_port));
  client.setCallback(callback);
}

void loop() {
  // put your main code here, to run repeatedly:
  if (!client.connected()) {
    Serial.println("Loop");
    reconnect();
  }
  client.loop();
  extButton();
}
