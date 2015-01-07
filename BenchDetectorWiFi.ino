/*
 *
 *       Arduino bench detector
 *       By: David Rutqvist @ BnearIT (david.rutqvist@bnearit.se)
 *
 *       This is an early prototype and therefore all settings are hardcoded. Future versions should use the built in EEPROM instead.
 *
*/

#include <SPI.h>
#include <WiFi.h>
#include <Adafruit_NeoPixel.h>
#include <avr/wdt.h>

#define WiFiSSID "Strand-Surf"
#define WiFiPassword ""
#define WiFiKeyIndex 0 //Needed only for WEP
#define WiFiConnectMethod 0//0 = No encryption, 1 = WPA, 2 = WEP

#define LED 6
#define SENSOR A0
#define SERVER "home.bnearit.se"
#define PORT 7005

#define DEVICE_ID 5
#define NAME "Strand"
#define PROTOCOLVERSION 1

#define LEDS 144
#define FADE_DELAY_UP  2
#define FADE_DELAY_DOWN 25
#define RED 250
#define GREEN 0
#define BLUE 75


#define EVENT_INIT 1
#define EVENT_DOWN 2
#define EVENT_UP 3

#define SENSITIVITY 960

int ledStatus = 0;
int WiFiStatus = WL_IDLE_STATUS;
long rssi = 0;
int counter = 0;
boolean goToReset = false;

boolean hasSent = false;
WiFiClient client;

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(LEDS, LED, NEO_GRB + NEO_KHZ800);

void setup() {
  wdt_disable(); //Disable watch dog since it may be set from previous reboot
  Serial.begin(9600);
  Serial.println("Initializing...");
  
  goToReset = false;
  pinMode(SENSOR, INPUT);
  pixels.begin();
  pixels.show();//Set all LEDs to off (if they were lit by another run)

  //Reset LEDs
  for (int i = 0; i < LEDS; i++){
    pixels.setPixelColor(i, pixels.Color(0,0,0));
    pixels.show(); // This sends the updated pixel color to the hardware.
  }
  
  //Test LEDS
  for (int i = 0; i < LEDS; i++){
    pixels.setPixelColor(i, pixels.Color(255,0,0));
    pixels.show(); // This sends the updated pixel color to the hardware.
  }
  delay(100);
  for (int i = 0; i < LEDS; i++){
    pixels.setPixelColor(i, pixels.Color(0,255,0));
    pixels.show(); // This sends the updated pixel color to the hardware.
  }
  delay(100);
  for (int i = 0; i < LEDS; i++){
    pixels.setPixelColor(i, pixels.Color(0,0,255));
    pixels.show(); // This sends the updated pixel color to the hardware.
  }
  delay(100);
  for (int i = 0; i < LEDS; i++){
    pixels.setPixelColor(i, pixels.Color(0,0,0));
  }
  pixels.show();

  Serial.println("Pin & LED setup done.");

  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("Could not detect WiFi shield");
    for (int i = 0; i < LEDS; i++){
      pixels.setPixelColor(i, pixels.Color(255,0,0));
    }
    pixels.show(); // This sends the updated pixel color to the hardware.
    
    while(true);
  }
  
  Serial.println("Initializing Shield");
  
  connectToSSID();
  
  Serial.println("Shield initialized");
  
  for (int i = 0; i < LEDS; i++){
      pixels.setPixelColor(i, pixels.Color(0,255,0));
  }
  pixels.show();
  delay(500);
  for (int i = 0; i < LEDS; i++){
      pixels.setPixelColor(i, pixels.Color(0,0,0));
  }
  pixels.show();
  delay(500);
  for (int i = 0; i < LEDS; i++){
      pixels.setPixelColor(i, pixels.Color(0,255,0));
  }
  pixels.show();
  delay(500);
  for (int i = 0; i < LEDS; i++){
      pixels.setPixelColor(i, pixels.Color(0,0,0));
  }
  
  pixels.show();
  
  //Send INIT EVENT
  raiseEvent(EVENT_INIT);
}

void loop() {
  //Read sensor value
  int sensorValue = analogRead(SENSOR);
  
  if (sensorValue >= SENSITIVITY) {
    counter += 1;
  }
  else
  {
    counter -= 1;
  }
  
  counter = constrain(counter, -65, 65);
  
  if (counter >= 55) {
    ledStatus += 1;
    delay(FADE_DELAY_UP);
  }
  
  if (counter <= -55) {
    ledStatus -= 1;
    delay(FADE_DELAY_DOWN);
  }
  
  //Fade LEDs
  ledStatus = constrain(ledStatus, 0, 255);
  float factor = (ledStatus/255.0);
  int red = (int)(RED * factor);
  int green = (int)(GREEN * factor);
  int blue = (int)(BLUE * factor);
  for(int i = 0; i < LEDS; i++)
  {
    pixels.setPixelColor(i, pixels.Color(red, green, blue));
  }
  pixels.show();

  if ((ledStatus == 255) && (hasSent == false)) {
    //Send down event
    raiseEvent(EVENT_DOWN);
    hasSent = true;
  }
  
  if ((ledStatus == 0) && (hasSent == true)) {
    //Send up event
    raiseEvent(EVENT_UP);
    hasSent = false;
  }
  
  delay(1);
}

void reboot() {
  Serial.println("Trying to reboot");
  Serial.println("Setting watchdog");
  wdt_enable(WDTO_15MS);
  while(true);
}

void raiseEvent(int eventID) {
  Serial.print("Event with code ");
  Serial.print(eventID);
  Serial.println(" raised.");

  if (WiFi.status() != WL_CONNECTED) {
    connectToSSID();  
  }
  
  //Connect to server and transmit, then wait for ACK
  if (!client.connected()) {
    Serial.println("Not yet connected to server.");
    int triesLeft = 2;
    int connectResult = 0;
    Serial.println("Connecting to " + String(SERVER) + ":" + String(PORT) + "...");
    connectResult = client.connect(SERVER, PORT);
    while ((connectResult != 1) && (triesLeft > 0)) {
      triesLeft--;
          
      if (!client.connected()) {
        Serial.print("Tries left: ");
        Serial.println(triesLeft);
      }
      
      if(connectResult) {
        Serial.println("Success");
      }
      else
      {
        Serial.println("Could not connect to server");
        
        //Clean up and release sockets
        client.flush();
        client.stop();
        
        if((triesLeft <= 0) && (!goToReset)) {
          Serial.println("Trying to reset connection to network");
          printWifiStatus();
          WiFi.disconnect();
          WiFiStatus = WL_IDLE_STATUS;
          connectToSSID();
          triesLeft = 2;
          goToReset = true;
        }
      }

      delay(100);
      connectResult = client.connect(SERVER, PORT);
    }
    
    if(connectResult == 1) {
      Serial.println("Successfully connected to server");
    }
  }

  //Should now be connected to server
  if (client.connected()) {
    goToReset = false;
    
    //Build message
    char body[8 + sizeof(NAME)] = {0};
    sprintf(body, "%02X%02X:%s:%02X", PROTOCOLVERSION, DEVICE_ID, NAME, eventID);
    //Build message with head, checksum & tail
    char message[sizeof(body) + 6] = {0};
    sprintf(message, "%c%s%04X%c", 0x02, body, checksum(body, sizeof(body)), 0x03);//Complete message is now in message variable
    
    client.print(message);
  }
  else
  {
    if (goToReset) {
      reboot();
    }
    else
    {
      raiseEvent(eventID);
    }
  }
  
  //Wait response
  int timeout = 0;
  while ((timeout <= 2000) && (client.available() < 1)) {
    timeout++;
    delay(1);
  }
  
  delay(200);
  String response = "";
  while (client.available()) {
    response += String((char)client.read());
  }
  
  Serial.print("Response: ");
  Serial.println(response);
  char expectedResponse[8] = {0x02, 0x06, '0', '0', '0', '6', 0x03, 0x00};
  Serial.print("Expecting: ");
  Serial.println(String(expectedResponse));
  
  if (response == String(expectedResponse)) {
    Serial.println("Reponse OK");
  }
  else {
    Serial.println("Bad response");
    
    //Just blink little red on first led
    pixels.setPixelColor(0, pixels.Color(64,0,0));
    pixels.show();
    delay(500);
    float factor = (ledStatus/255.0);
    int red = (int)(RED * factor);
    int green = (int)(GREEN * factor);
    int blue = (int)(BLUE * factor);
    pixels.setPixelColor(0, pixels.Color(red, green, blue));
    pixels.show();
  }
  
  //Disconnect and clean up
  Serial.println("Disconnecting");
  client.flush();
  client.stop();
}

int checksum(char content[], int len) {
  unsigned int sum = 0;
  for(int i = 0; i < len - 1; i++) {
    sum += (int)content[i];
  }
  return sum;
}

void connectToSSID() {
  while (WiFiStatus != WL_CONNECTED) {
    Serial.print("Trying to connect to SSID: ");
    Serial.println(WiFiSSID);
    
    switch (WiFiConnectMethod) {
      case 0:
        Serial.println("Using no encryption (open network)");
        WiFiStatus = WiFi.begin(WiFiSSID);
        break;
        
      case 1:
        Serial.println("Using WPA");
        WiFiStatus = WiFi.begin(WiFiSSID, WiFiPassword);
        break;
        
      case 2:
        Serial.println("Using WEP");
        WiFiStatus = WiFi.begin(WiFiSSID, WiFiKeyIndex, WiFiPassword);
        break;
        
      default:
        Serial.println("Unknown connection method");
    }
    
    delay(7500);
  }
  
  Serial.println("Connected!");
  
  printWifiStatus();
}

void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);
  
  // print your MAC address:
  byte mac[6];  
  WiFi.macAddress(mac);
  Serial.print("MAC address: ");
  Serial.print(mac[5],HEX);
  Serial.print(":");
  Serial.print(mac[4],HEX);
  Serial.print(":");
  Serial.print(mac[3],HEX);
  Serial.print(":");
  Serial.print(mac[2],HEX);
  Serial.print(":");
  Serial.print(mac[1],HEX);
  Serial.print(":");
  Serial.println(mac[0],HEX);
  
  // print your subnet mask:
  IPAddress subnet = WiFi.subnetMask();
  Serial.print("NetMask: ");
  Serial.println(subnet);

  // print your gateway address:
  IPAddress gateway = WiFi.gatewayIP();
  Serial.print("Gateway: ");
  Serial.println(gateway);

  // print the received signal strength:
  rssi = WiFi.RSSI();
  Serial.print("Signal strength (RSSI): ");
  Serial.print(rssi);
  Serial.println(" dBm");
}
