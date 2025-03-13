/*
 * Copyright (c) 2024/2025 Tobias Guggenberger - software@guggenberger.me
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <Ethernet.h>
#include <PubSubClient.h> // mqtt
#include "PCF8574.h" // Port Expander
#include <SPI.h> // Seriell
#include <Wire.h>

/************************************************************************** Kabelcode
gn        SDA
gn/ 1sw   SDC
gr        GND
rt        +5V
*/

/************************************************************************** Pinbelegung Arduino UNO
A4      SDA -> gn
A5      SCL -> gn/1sw
*/

//************************************************************************** PCF8574 Init
PCF8574 pcf8574(0x20);

//************************************************************************** LAN Network definieren 
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01 };
IPAddress ip(10, 110, 0, 5); //comment this line if you are using DHCP

//IPAddress subnet(255, 255, 0, 0); // Subnet Mask

IPAddress mqtt_server(10, 110, 0, 3);  // IP-Adresse des MQTT Brokers im lokalen Netzwerk

EthernetClient ethClient;
PubSubClient client(ethClient);

//************************************************************************** Variablen
int topic_init = 0; // Topic Variable zum einmaligen aufrufen

int status_tor_auf = 0;
int status_tor_zu = 0;
int status_tor_toggle = 0;


//************************************************************************** Funktionsprototypen
void loop                       ();
void setup                      ();
void reconnect                  ();
void callback(char* topic, byte* payload, unsigned int length);
void topic_mqtt_init            ();
void mqtt_reconnect_intervall   ();
void(* resetFunc) (void) = 0;


//************************************************************************** Intervalle
/* Beispiel
unsigned long previousMillis_BEISPIEL = 0; // 
unsigned long interval_BEISPIEL = 800; 
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ BEISPIEL
  if (millis() - previousMillis_BEISPIEL > interval_BEISPIEL) {
      previousMillis_BEISPIEL = millis(); 
      FUNKTION();
    }  
*/
unsigned long previousMillis_mqtt_reconnect = 0; // 
unsigned long interval_mqtt_reconnect = 200; 

// Relais Abfallverzögerung
unsigned long previousMillis_Tor_Auf;
unsigned long previousMillis_Tor_Zu;
unsigned long previousMillis_Tor_toggle; // Schalten des Stromstoss Relais für Tag Öffnung

// Wie lange sollen die Relais halten
unsigned long dauer_relais_Tor_auf = 1000;
unsigned long dauer_relais_Tor_zu = 1000;
unsigned long dauer_relais_Tor_toggle = 1000;

//************************************************************************** SETUP
void setup() {
  Serial.begin(115200);

// ------------------------------------------------------------------------- Ethernet starten
  Ethernet.begin(mac, ip);
  // Pause Netzwerk Antwort
  delay(1500);  

///////////////////////////////////////////////////////////////////////////  MQTT Broker init
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

/////////////////////////////////////////////////////////////////////////// Konfig Portexpander
  pcf8574.pinMode(P0, OUTPUT);
  pcf8574.pinMode(P1, OUTPUT);
  pcf8574.pinMode(P2, OUTPUT);
  pcf8574.pinMode(P3, OUTPUT);
  pcf8574.pinMode(P4, OUTPUT);
  pcf8574.pinMode(P5, OUTPUT);
  pcf8574.pinMode(P6, OUTPUT);
  pcf8574.pinMode(P7, OUTPUT);        
  pcf8574.begin();

/////////////////////////////////////////////////////////////////////////// Portexpander Relais definieren
pcf8574.digitalWrite(P0, !LOW);
pcf8574.digitalWrite(P1, !LOW);
pcf8574.digitalWrite(P2, !LOW);
pcf8574.digitalWrite(P3, !LOW);
pcf8574.digitalWrite(P4, !LOW);
pcf8574.digitalWrite(P5, !LOW);
pcf8574.digitalWrite(P6, !LOW);
pcf8574.digitalWrite(P7, !LOW);

}

//************************************************************************** Topic MQTT init
void topic_mqtt_init(){
/*
Durch das init der Topic wird einmalig dem System gezeigt welche Topic es gibt
Benötigte Variable
- int topic_init = 0;
*/
    if (topic_init==0)
  {
    topic_init = 1;
    // Topic init
    client.publish("Werktor/K0", "online");
    client.publish("Werktor/K1", "online");
    client.publish("Werktor/K2", "online");
    //client.publish("Werktor/K3", "online");
    //client.publish("Werktor/K4", "online");
    //client.publish("Werktor/K5", "online");
    //client.publish("Werktor/K6", "online");
    //client.publish("Werktor/K7", "online");
  }
}

//************************************************************************** mqtt - reconnect
void reconnect() {
  while (!client.connected()) {
    Serial.print("Verbindung zum MQTT-Server aufbauen...");
    if (client.connect("WerktorClient", "hitesh", "RO9UZ7wANCXzmy")) {
      Serial.println("verbunden");
      client.subscribe("Werktor/K0");
      client.subscribe("Werktor/K1");
      client.subscribe("Werktor/K2");
      //client.subscribe("Werktor/K3");
      //client.subscribe("Werktor/K4");
      //client.subscribe("Werktor/K5");
      //client.subscribe("Werktor/K6");
      //client.subscribe("Werktor/K7");
    } else {
      Serial.print("Fehler, rc=");
      Serial.print(client.state());
      Serial.println(" erneut versuchen in 5 Sekunden");
      delay(5000);
    }
  }
}

//************************************************************************** mqtt - callback
void callback(char* topic, byte* payload, unsigned int length) {

 Serial.print("Nachricht empfangen [");
  Serial.print(topic);
  Serial.print("]: ");
  
  // Payload in einen String umwandeln
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  Serial.println(message);

  // -------------------------------------------------------- Topic Auswerten K0
  if (String(topic) == "Werktor/K0") {
    if (message == "auf") {
        Serial.println("Relais K0 -> Tor AUF Impuls");
        pcf8574.digitalWrite(P0, !HIGH);
        status_tor_auf = 1;
        // Bei aktivierung millis speichern
        previousMillis_Tor_Auf = millis();
    } 
    else if (message == "zu") {
        Serial.println("Relais K0 -> Tor AUF Relais abschalten");
        pcf8574.digitalWrite(P0, !LOW);
        status_tor_auf = 0;
    } 
    else { }} else { }

// -------------------------------------------------------- Topic Auswerten K1
  if (String(topic) == "Werktor/K1") {
    if (message == "zu") {
        Serial.println("Relais K1 -> Tor ZU Impuls");
        pcf8574.digitalWrite(P1, !HIGH);
        status_tor_zu = 1;
        // Bei aktivierung millis speichern
        previousMillis_Tor_Zu = millis();        
    } 
    else if (message == "zu") {
        //Serial.println("Relais K1 -> Tor ZU Relais abschalten");
        //pcf8574.digitalWrite(P1, !LOW);
    } 
    else { }} else { }

// -------------------------------------------------------- Topic Auswerten K2
  if (String(topic) == "Werktor/K2") {
    if (message == "toggleAUF") {
        Serial.println("Toogle Impuls Stromstossrelais");
        pcf8574.digitalWrite(P2, !HIGH);
        status_tor_toggle = 1;
        // Bei aktivierung millis speichern
        previousMillis_Tor_toggle = millis();           
    } 
    else if (message == "toggleZU") {
        Serial.println("Toogle Impuls Stromstossrelais");
        pcf8574.digitalWrite(P2, !LOW);
    } 
    else { }} else { }
/*
// -------------------------------------------------------- Topic Auswerten K3
  if (String(topic) == "Werktor/K3") {
    if (message == "on") {
        Serial.println("Relais K3 -> AN");
        pcf8574.digitalWrite(P3, !HIGH);
    } 
    else if (message == "off") {
        Serial.println("Relais K3 -> AUS");
        pcf8574.digitalWrite(P3, !LOW);
    } 
    else { }} else { }

// -------------------------------------------------------- Topic Auswerten K4
  if (String(topic) == "Werktor/K4") {
    if (message == "on") {
        Serial.println("Relais K4 -> AN");
        pcf8574.digitalWrite(P4, !HIGH);
    } 
    else if (message == "off") {
        Serial.println("Relais K4 -> AUS");
        pcf8574.digitalWrite(P4, !LOW);
    } 
    else {  }} else {  }


// -------------------------------------------------------- Topic Auswerten K5
  if (String(topic) == "Werktor/K5") {
    if (message == "on") {
        Serial.println("Relais K5 -> AN");
        pcf8574.digitalWrite(P5, !HIGH);
    } 
    else if (message == "off") {
        Serial.println("Relais K5 -> AUS");
        pcf8574.digitalWrite(P5, !LOW);
    } 
    else { }} else { }

// -------------------------------------------------------- Topic Auswerten K6
  if (String(topic) == "Werktor/K6") {
    if (message == "on") {
        Serial.println("Relais K6 -> AN");
        pcf8574.digitalWrite(P6, !HIGH);
    } 
    else if (message == "off") {
        Serial.println("Relais K6 -> AUS");
        pcf8574.digitalWrite(P6, !LOW);
    } 
    else { }} else { }

// -------------------------------------------------------- Topic Auswerten K7
  if (String(topic) == "Werktor/K7") {
    if (message == "on") {
        Serial.println("Relais K7 -> AN");
        pcf8574.digitalWrite(P7, !HIGH);
    } 
    else if (message == "off") {
        Serial.println("Relais K7 -> AUS");
        pcf8574.digitalWrite(P7, !LOW);
    } 
    else { }} else { }
*/

}
//************************************************************************** mqtt_reconnect_intervall 
void mqtt_reconnect_intervall() {
    if (!client.connected()) {
      reconnect();
    }
    client.loop();
}

//************************************************************************** LOOP
void loop() {

 //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ Arduino Reset
//if ( millis()  >= 780000) resetFunc(); // Reset alle 13 Min
if ( millis()  >= 60000) resetFunc(); // Reset alle 13 Min

// MQTT Abfrage
    if (millis() - previousMillis_mqtt_reconnect > interval_mqtt_reconnect) {
      previousMillis_mqtt_reconnect = millis(); 
      mqtt_reconnect_intervall();
    }  

// Relais Ausschaltverzögerung - Tor AUF
if (millis() - previousMillis_Tor_Auf > dauer_relais_Tor_auf && status_tor_auf == 1)  
  {
        pcf8574.digitalWrite(P0, !LOW);
        Serial.println("P0/K0 - AUS");
        status_tor_auf = 0;
  } 
// Relais Ausschaltverzögerung - Tor ZU
if (millis() - previousMillis_Tor_Zu > dauer_relais_Tor_zu && status_tor_zu == 1)  
  {
        pcf8574.digitalWrite(P1, !LOW);
        Serial.println("P1/K1 - AUS");
        status_tor_zu = 0;
  }   

// Relais Toggle - Schaltung Pförtner für dauer auf
if (millis() - previousMillis_Tor_toggle > dauer_relais_Tor_toggle && status_tor_toggle == 1)  
  {
        pcf8574.digitalWrite(P2, !LOW);
        Serial.println("P2/K2 - AUS");
        status_tor_toggle = 0;
  }  



/*
        delay(1000);
        pcf8574.digitalWrite(P0, !LOW);
        Serial.println("P0 - AUS");
        status_tor_auf = 0;
  */    

    
// Mqtt Topic einmalig beim Start INIT
topic_mqtt_init();

}