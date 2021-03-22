/* Smart Tooth Brush ESP Firmware
 *  
 *  Reads the State of a Normally Closed Limit Switch (which is closed by the weight of a battery powered kids toothbrush 
 *  Triggers a MQTT event when the tooth brush is removed for 5 seconds, or placed back on the holder for 5 seconds
 *  In my use case these are send to Mosquito running on Home Assistant
 *  
 *  Author: SchreiverJ
 *  Date: 3/21/21
 */

//Default ESP Wifi Lib in Arduino
#include <ESP8266WiFi.h>

// PubSubClient Installable in Arduino Library Manager
// Version 2.8.0
// https://github.com/knolleary/pubsubclient
#include <PubSubClient.h>

/* CHANGE THESE SETTING PER YOUR NETWORK */
#define wifi_ssid "PUT_SSID_HERE"
#define wifi_password "PUT_PASSWORD_HERE"

#define mqtt_server "PUT_Mosquito_IP_HERE"
#define mqtt_user "toothbrushholder"
#define mqtt_password "*********"

#define slot_1_topic "sensor/toothbrush1"
#define slot_2_topic "sensor/toothbrush2"


// Pins D1 and D2 on The ESP Board
int button1_pin = 4;
int button2_pin = 5;

WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
  Serial.begin(9600);
  setup_wifi();
  client.setServer(mqtt_server, 1883);

  pinMode(LED_BUILTIN, OUTPUT);     // Initialize the LED_BUILTIN pin as an output
  pinMode(button1_pin, INPUT);
  pinMode(button2_pin, INPUT);
}

//Connect to the Wifi Report info over Serial for Debugging
void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(wifi_ssid);

  WiFi.begin(wifi_ssid, wifi_password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

//Reconnect if we lose the mqtt
//From: https://www.baldengineer.com/mqtt-tutorial.html
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    // If you do not want to use a username and password, change next line to
    // if (client.connect("ESP8266Client")) {
    if (client.connect("ESP8266Client", mqtt_user, mqtt_password)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

// The last time we set set a message
// user to determine if approximately a second has past
long lastMsg = 0;

//Keep track of how many transitions for a state change
//Does a poor mans Debouncing
long button1_last = 0;
long button2_last = 0;

//What is the state of the limit switch currently
long button1_state = 2;
long button2_state = 2;

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  
  long now = millis();
  if (now - lastMsg > 1000) {
    lastMsg = now;


  int switchStatus1 = digitalRead(button1_pin);
  int switchStatus2 = digitalRead(button2_pin);
  
  //If we just booted up set the state but dont report
  if (button1_state == 2) button1_state = switchStatus1;
  if (button2_state == 2) button2_state = switchStatus2;

  //Turn on the LED if anyone is brushing
  //NOTE we are active low not high
  digitalWrite(LED_BUILTIN, !(switchStatus1 & switchStatus2));  

  //Teeth Brushing should always take more than 5 seconds,
  //We can use this to our advantage and worry less out debounce
  //If we see a state change for 5 consecutive seconds we will
  //report a stage change to MQTT
    if (switchStatus1 != button1_state) {
      button1_last++;
    } else {
      button1_last = 0;
    }
    
    if (switchStatus2 != button2_state) {
      button2_last++;
    } else {
      button2_last = 0;
    }
    
    //We shouldnt have states of less than 5 seconds so dont register them
    if (button1_last >= 5) {
      button1_state = switchStatus1;
      button1_last = 0;
      client.publish(slot_1_topic, String(button1_state).c_str(), true);
      Serial.println("State Change Detected Slot 1");
    }

    if (button2_last >= 5) {
      button2_state = switchStatus2;
      button2_last = 0;
      client.publish(slot_2_topic, String(button2_state).c_str(), true);
      Serial.println("State Change Detected Slot 2");
    }
    
    //Just a little delay between cycles we dont need to be running continouslly for this use case
    delay(50);  
  }
}
