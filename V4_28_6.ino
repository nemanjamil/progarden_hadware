#include <OneWire.h>
#include <ESP8266WebServer.h>
#include <WiFiClient.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <DHT_U.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_TSL2561_U.h>
#include <DallasTemperature.h>
#include <EEPROM.h>

#include "filename.h" 

WiFiClient client;
ESP8266WebServer server(80);

#define DHTPIN 14 // D5  
#define DHTTYPE DHT22     
DHT dht(DHTPIN, DHTTYPE);

// Variables
String _ssid = "";
String _pass = "";
boolean enter_configure_mode = false;
#define CLEAR_CONFIG_PIN 16 // 13 D7; D0 16
#define LED 0 // D3 
uint64_t startwifi = millis();
int timeout = 5000;
String macID = "";
String trenutnoStanje = "OK";

boolean debug = true;
// Light
const int address = TSL2561_ADDR_FLOAT;
Adafruit_TSL2561_Unified tsl = Adafruit_TSL2561_Unified(address, 12345);

// Water temperature
#define ONE_WIRE_BUS D1 // isti pin kao kod senzora svetlosti
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature DS18B20(&oneWire);
char temperatureString[6];

// Moisture
int sense_Pin = 0; // sensor input at Analog pin A0
int waterState = 0;
int waterStateMinimum = 100;

// Liquid sensor
float liquid_level;
int liquid_percentage;
int top_level = 512;
int bottom_level = 3;

// Relay
int relayInput = 12; // D6

// LED STATUS
#define blueLed_od3led 15 // D8 15
#define redLed_od3led 13 // D7 13
// #define greenLed_od3led 16 // D0 16

int smallDelay = 500;
int bigDelay = 10000;
int prosaoLoop = 0;
int brojLoopa = bigDelay/smallDelay;

// EEPROM
  int addr = 0;
  struct {
    uint val = 0;
    char str[50] = "";
    int ownerOfNode = 0;
  } data;

// broj errora
int brojerora = 0;  
  
void setup() {
  Serial.begin(9600);
  dht.begin();
  // DEFINE STATUS OF PINS
  pinMode(relayInput,OUTPUT);
  stopParte(); // put Relay to LOW
  
  
  pinMode(CLEAR_CONFIG_PIN, OUTPUT);
  
  pinMode(blueLed_od3led, OUTPUT);
  digitalWrite(blueLed_od3led, LOW);

  pinMode(redLed_od3led, OUTPUT);
  digitalWrite(redLed_od3led, LOW);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
 
  const int sensorPin= A0; //sensor pin connected to analog pin A0
  pinMode(sense_Pin, INPUT);
  
  String AP_NameString = setUpWiFiMAC(); // get WIFI MAC Address

  server.on( "/", handleRoot );
  server.on("/konekcija",konekcija);
  server.on("/diskonekcija",diskonekcija);
  server.on("/validateConn",validateConn);
  server.on("/status",statusTrenutnoStanje);
  server.on("/disconnect",disconnectserver);
  server.on("/restartnode",restartnode);
  server.onNotFound(handleNotFound);  
  server.begin();
 
  WiFi.begin();
  delay(3000);
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Nije konektovan na net");
    setupWaitServerToSendUsrPass(); 
  }
 
  int userIdEprom = epromSetup();
  Serial.print("userIdEprom : "); Serial.println(userIdEprom);
  if (!userIdEprom) {
     Serial.print("EpromUserId not exist : "); Serial.println(userIdEprom);
     failUserId(AP_NameString); 
  }

  setupLight();
  statistics();

 
}

void loop() {
    server.handleClient();
    delay(500);
    prosaoLoop++;
    Serial.println(prosaoLoop);
    
    if (WiFi.status() == WL_CONNECTED) {
      if (prosaoLoop == brojLoopa) {
            prosaoLoop  = 0;
            getSensorDetailLoop();
            Serial.println(digitalRead(CLEAR_CONFIG_PIN));
      }
   
    } else {
         if (debug) {
          Serial.println("Not connecter to WIfi");
         }
         
        digitalWrite(blueLed_od3led, LOW); 
        delay(50);
        digitalWrite(blueLed_od3led, HIGH); 
        delay(50);
        digitalWrite(blueLed_od3led, LOW); 
        delay(50);
        digitalWrite(blueLed_od3led, HIGH); 
        delay(50);
        digitalWrite(blueLed_od3led, LOW); 
        delay(50);
        digitalWrite(blueLed_od3led, HIGH); 
        delay(3000);
        //restartnode(); // we can put node to restart.... will check???? for notConnected....
    }

}

void getSensorDetailLoop(){
          float h = dht.readHumidity();
          float t = dht.readTemperature();
          float lux = getLuxValue();
          
          float wtemp1 = getTemperature();
          dtostrf(wtemp1, 2, 2, temperatureString);
          
          waterState = analogRead(sense_Pin);
          
          
          if (debug) {
            Serial.print("h : "); Serial.println(h);
            Serial.print("t : "); Serial.println(t);
            Serial.print("lux : "); Serial.println(lux);
            Serial.print("wtemp1 : "); Serial.println(wtemp1);
            Serial.print("waterState : "); Serial.println(waterState);
            Serial.print("macID : "); Serial.println(macID);
          }
          
          if (!isnan(h) and !isnan(t) and (lux!=65536.00) and wtemp1 and waterState>waterStateMinimum and macID) {
            httpRequest(t,h,lux,wtemp1,waterState,macID,waterState);
            brojerora = 0;
          } else {
            if (debug) {
              Serial.println("Some of values doesn't exist");
            }
            chechExist(t,h,lux,wtemp1,macID,waterState);
            brojerora++;
          }

          //paligasi();
          //delay(2000);
          //statistics();

          EEPROM.get(addr,data);
          if (debug) {
           Serial.println("LOOP => New values are: "+String(data.val)+","+String(data.str)+","+String(data.ownerOfNode));
          }

          ledDaJeSveOk();
          
}
void  ledDaJeSveOk(){
  if (debug) {
          digitalWrite(redLed_od3led, HIGH);
          delay(200);
          digitalWrite(redLed_od3led, LOW);
          delay(200);
          digitalWrite(redLed_od3led, HIGH);
          delay(200);
          digitalWrite(redLed_od3led, LOW);
          delay(200);
  }
}
void validateConn(){
   if (WiFi.status() == WL_CONNECTED) {
     Serial.print("Connection Validated : "); Serial.println(WiFi.localIP());
     String localIP = WiFi.localIP().toString();
      server.send(200, "text/html", "{\"success\":\"true\",\"ssid\":\""+WiFi.SSID()+"\",\"ip\":\""+WiFi.localIP().toString()+"\"}");
   } else {
      server.send(200, "text/html", "{\"success\":false}");
   }
}
void konekcija(){
  
  if( ! server.hasArg("username") || ! server.hasArg("password") || ! server.hasArg("userid") || server.arg("username") == NULL || server.arg("password") == NULL || server.arg("userid") == NULL) {
       //server.send(400, "text/plain", "400: Invalid Request");         // The request is invalid, so send HTTP status 400
       server.send(200, "text/html", "{\"success\":false}");
       return;
  } else {
    // WiFi.disconnect();  // (true) Erases SSID/password
    delay(1000);
    _ssid = server.arg("username");
    Serial.print("Ssid : konekcija : "); Serial.println(_ssid);
    _pass = server.arg("password");
    Serial.println(_pass);
    
    String _userid = server.arg("userid");
    Serial.print("_userid : konekcija : "); Serial.println(_userid);
    data.ownerOfNode = _userid.toInt();
    EEPROM.put(addr,data);
    EEPROM.commit();
    Serial.println("Owner Of Node: "+String(data.ownerOfNode));
    enter_configure_mode = false;
    trenutnoStanje = "OK - connection sucess!!!";
    server.send(200, "text/html", "{\"success\":true, \"userid\":\""+_userid+"\"}");
    
    //server.sendHeader("Location","/");  // Add a header to respond with a new location for the browser to go to the home page again     
    //server.send(303); // Send it back to the browser with an HTTP status 303 (See Other) to redirect
    //server.stop();
    
  }
}

void handleRoot(){
  server.send ( 200, "text/html", "<h1>Welcome!</h1>" );
  Serial.println("Server Send");
}
void handleNotFound(){
  server.send(404, "text/plain", "404: Not found"); // Send HTTP status 404 (Not Found) when there's no handler for the URI in the request
}

void diskonekcija(){
  WiFi.disconnect(true); 
  server.send ( 200, "text/html", "{\"success\":true}" ); 
}

void statistics(){
  Serial.println("");
  Serial.print("Entered config mode : AP adresa :");
  Serial.println(WiFi.softAPIP());
  Serial.printf("Default hostname: %s\n", WiFi.hostname().c_str());
  Serial.printf("SSID: %s\n", WiFi.SSID().c_str());
  Serial.print("GET MODE : "); Serial.println(WiFi.getMode());
  Serial.print("Connection IP address:\t");
  Serial.println(WiFi.localIP()); 
  Serial.println('\n');
}
void udjiuconf(){
  
  if (digitalRead(CLEAR_CONFIG_PIN) == HIGH){
    int low_count = 0;
    for (int i = 0; i<3; i++)
    {
      if (digitalRead(CLEAR_CONFIG_PIN) == HIGH){
        low_count++;
        delay(200);
      }else{
        break;
      }
    }
    if (low_count > 2){
      enter_configure_mode = true;
      //digitalWrite(LED, HIGH); 
    }
  }
}

void failUserId (String  AP_NameString_in){
  stopParte();
  while (data.ownerOfNode <= 0){
        delay(1000);
        digitalWrite(blueLed_od3led, HIGH); 
        Serial.println("Waiting Server to sent connect informations for USER ID");
        Serial.print("AP name : "); Serial.println(WiFi.softAPIP());
        Serial.print("AP_NameString : "); Serial.println(AP_NameString_in);
        
        server.handleClient();

        if (data.ownerOfNode > 0) {
          break;
        }
    
  }
}
void setupWaitServerToSendUsrPass(){
  
  while (WiFi.status() != WL_CONNECTED){   
        delay(400);
        digitalWrite(blueLed_od3led, HIGH); 
        Serial.println("Waiting Server to sent connect informations for WIFI");
        server.handleClient();
        
        if (_ssid.length()>2 && _pass.length()>2) {
          WiFi.begin(_ssid.c_str(), _pass.c_str()); 
          delay(5000);
          digitalWrite(blueLed_od3led, LOW); 
          break;
        }
        delay(400);
        digitalWrite(blueLed_od3led, LOW); 
        delay(50);
        digitalWrite(blueLed_od3led, HIGH); 
        delay(50);
        digitalWrite(blueLed_od3led, LOW); 
        delay(50);
        digitalWrite(blueLed_od3led, HIGH); 
        delay(50);
        digitalWrite(blueLed_od3led, LOW); 
        delay(50);
        digitalWrite(blueLed_od3led, HIGH); 
        
  } 
}

float getLuxValue(){
          sensors_event_t event;
          tsl.getEvent(&event);
          float vrednostlux = event.light;
          if (!vrednostlux) {
            vrednostlux = 0;
          }
          return vrednostlux;
}

void setupLight(){
   Serial.println("Light Sensor Test"); Serial.println("");
  if(!tsl.begin())
  {
    Serial.print("Ooops, no TSL2561 detected ... Check your wiring or I2C ADDR!");
    while(1);
  }
  //displaySensorDetails();
  //configureSensor();
  Serial.println("");
}

float getTemperature() {
  float temp;
  do {
    DS18B20.requestTemperatures(); 
    temp = DS18B20.getTempCByIndex(0);
    delay(100);
  } while (temp == 85.0 || temp == (-127.0));
  return temp;
}


void httpRequest(float temp,float h,float lux,float wtemp1,int moisture, String macID, int waterState) {

    digitalWrite(redLed_od3led, LOW);// disable RED LED;
    digitalWrite(blueLed_od3led, LOW);// disable blue LED;
    
    String temps = String(temp);
    String luxss = String(lux);
    String hs = String(h);
    String wtemp1s = String(wtemp1);
    String moistures = String(moisture);
    String waterState_s = String(waterState);

    boolean StateRelayWPump = digitalRead(relayInput);

    HTTPClient http;
    http.begin("http://masinealati.rs/parametrigarden.php?action=osnovniparametri");  // //http.begin("http://jsonplaceholder.typicode.com/users/1");
    http.setTimeout(2000);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    int httpCode = http.POST("status=true&TEMP="+temps+"&LIGHT="+luxss+"&WTEMP1="+wtemp1s+"&HUM="+hs+"&MOIS="+moistures+"&ID="+macID+"&userid="+data.ownerOfNode+"&srwp="+StateRelayWPump+"&waterState="+waterState_s+"");
    //http.writeToStream(&Serial);
    if (debug) {
      Serial.println("status=true&TEMP="+temps+"&LIGHT="+luxss+"&WTEMP1="+wtemp1s+"&HUM="+hs+"&MOIS="+moistures+"&ID="+macID+"&userid="+data.ownerOfNode+"&srwp="+StateRelayWPump+"&waterState="+waterState_s+"");
    }
          
    if (httpCode > 0) { 

        // {"tag":"idSifra","success":false,"error":3,"error_msg":"Ne postoji idSifra","error_podaci":"Id -> ","error_msg_opis":""}
        //const size_t bufferSize = JSON_OBJECT_SIZE(6) + 110;
        //const size_t bufferSize = JSON_OBJECT_SIZE(3) + JSON_ARRAY_SIZE(2) + 60;
        const size_t bufferSize = JSON_ARRAY_SIZE(2) + 3*JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(6) + JSON_OBJECT_SIZE(7) + 330;
        DynamicJsonBuffer jsonBuffer(bufferSize);
        String payload = http.getString();
        if (debug) { 
          Serial.print("Payload : "); Serial.println(payload);
        }
        JsonObject& root = jsonBuffer.parseObject(payload);
        if (!root.success())
          {
            Serial.print("ParseObject() failed");
            int odgovor = 0;
          }

        const char* tag = root["tag"]; // "upucavenjaSenzora"
        bool success = root["success"]; // true
        int error = root["error"]; // 0
        const char* error_msg = root["error_msg"]; // "Sve je upucano kako treba"
        const char* error_podaci = root["error_podaci"]; // "Id -> 5ECF7F0167F1"
        const char* error_msg_opis = root["error_msg_opis"]; // ""
        
        JsonObject& odgovori0_relejpumpa = root["odgovori"][0]["relejpumpa"];
        int odgovori0_relejpumpa_stanje = odgovori0_relejpumpa["stanje"]; // 1
        int odgovori0_relejpumpa_stanjeonoff = odgovori0_relejpumpa["stanjepumpeonoff"]; // 1
        
        int odgovori0_relejpumpa_secRazlika = odgovori0_relejpumpa["secRazlika"]; // 7
        const char* odgovori0_relejpumpa_dateTimeUBazi = odgovori0_relejpumpa["dateTimeUBazi"]; // "2018-05-19 08:21:50"
        const char* odgovori0_relejpumpa_dateTimeSad = odgovori0_relejpumpa["dateTimeSad"]; // "2018-05-19 08:22:43"
        int odgovori0_relejpumpa_duzinaUljucenoMin = odgovori0_relejpumpa["duzinaUljucenoMin"]; // 3
        int odgovori0_relejpumpa_duzinaIskljucenoMin = odgovori0_relejpumpa["duzinaIskljucenoMin"]; // 1
        
        int odgovori1_automod_stanje = root["odgovori"][1]["automod"]["stanje"]; // 1
        if (debug) {
          Serial.print("ModPumpe Pumpe : "); Serial.println(odgovori0_relejpumpa_stanje);
          Serial.print("Stanje Pumpe : "); Serial.println(odgovori0_relejpumpa_stanjeonoff);
          Serial.print("Sekunde razlika : "); Serial.println(odgovori0_relejpumpa_secRazlika);
          Serial.print("Stanje releja : "); Serial.println(digitalRead(relayInput));
         }

       
        if (success) { // ako je uspesno dobio odgovor
            if (odgovori0_relejpumpa_stanje==2) { // ako je setovano da pumpa uvek bude ukljuncea
              Serial.print("Usao u stanje = 2 : "); Serial.println(digitalRead(relayInput));
              digitalWrite(relayInput, HIGH); 
              Serial.print("Usao u stanje = 2 - Nakon Ukljucivanja : "); Serial.println(digitalRead(relayInput));
            } else if (odgovori0_relejpumpa_stanje==0) {
               digitalWrite(relayInput, LOW); 
               Serial.print("Usao u stanje = 0 : "); Serial.println(digitalRead(relayInput));
            }
            else if (odgovori0_relejpumpa_stanje==1) {
               Serial.print("Usao u stanje = 1 : "); Serial.println(digitalRead(relayInput));
                if (odgovori0_relejpumpa_stanjeonoff!=digitalRead(relayInput)){
                  digitalWrite(relayInput, odgovori0_relejpumpa_stanjeonoff); 
                } 
            } else {
              Serial.print("Usao u stanje = NN : "); Serial.println(digitalRead(relayInput));
            }
            
            ledDaJeSveOk();
        } else {
          if (debug) {
            Serial.println("False odgovor sa servera -> proveriti o cemu se radi. Odmah se gasi pumpa");  // mozda poslati na server info ili staviti u log kartice
            Serial.println("status=true&TEMP="+temps+"&LIGHT="+luxss+"&WTEMP1="+wtemp1s+"&HUM="+hs+"&MOIS="+moistures+"&ID="+macID+"&srwp="+StateRelayWPump+"&waterState="+waterState_s+"");
            Serial.println(error_msg);
            Serial.println(error_podaci);
          }
          trenutnoStanje = error_msg;
            Serial.print("trenutnoStanje : "); Serial.println(trenutnoStanje);
          stopParte();
        }

       if (debug) {
        Serial.print("Stanje releja posle update : "); Serial.println(digitalRead(relayInput));
        Serial.println("");
        Serial.println("-------------------------------");
       }
       
    } else {
      Serial.print("Error on sending POST: ");
      Serial.println(httpCode);
    }
    http.end();  
}

void chechExist(float temp,float h,float lux,float wtemp1,String macID, int waterState) {

   if (brojerora>5) { 
      digitalWrite(redLed_od3led, HIGH);
      if (isnan(temp)){
        Serial.println("Temperature doesn't exist"); 
        sentFailInfoToServer("temp",macID);
      }

      if (isnan(h)){
        Serial.println("Humidity doesn't exist"); 
        sentFailInfoToServer("humidity",macID);
      }

      if (lux==65536.00){
        Serial.println("Lux doesn't exist"); 
        sentFailInfoToServer("lux",macID);
      }

      if (!wtemp1){
        Serial.println("Water Temp doesn't exist"); 
        sentFailInfoToServer("watertemp",macID);
      }

      if (waterState<waterStateMinimum){
        Serial.print("WaterState value "); Serial.println(waterState);
        Serial.print("WaterState is less then "); Serial.println(waterStateMinimum);
        sentFailInfoToServer("waterstate",macID);
      }

      if (!macID){
        Serial.print("MAC Address value "); Serial.println(macID);
        sentFailInfoToServer("mac",macID);
      }
   } else {
    Serial.print("Sada je broj errora : "); Serial.println(brojerora);
   }
}

void statusTrenutnoStanje(){
  server.send ( 200, "text/html", "<h1>"+trenutnoStanje+"</h1>" );
  Serial.println("Server Send");
  
}
void sentFailInfoToServer(String value,String macID) {

    String value_s = String(value);
    HTTPClient http;
    http.begin("http://masinealati.rs/parametrigarden.php?action=failsensorvalue");  
    http.setTimeout(200);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    int httpCode = http.POST("status=false&value="+value_s+"&ID="+macID+"");
    if (httpCode > 0) { 
      Serial.println("Sent Fail to server -> OK"); 
      String response = http.getString();  //Get the response to the request
      Serial.print("httpCode : "); Serial.println(httpCode);   //Print return code
      Serial.print("Response : "); Serial.println(response);   //Print request answer
      Serial.println("Zaustaljamo Sistem"); 
      stopParte();
    
    } else {
      Serial.println("Sent Fail to server -> FAIL"); 
      stopParte();
    }
    brojerora = 0;
}

void stopParte(){
  digitalWrite(relayInput, LOW); 
  digitalWrite(blueLed_od3led, HIGH); 
}

int epromSetup(){

  int epromUid = 0;
  // https://circuits4you.com/2016/12/16/esp8266-internal-eeprom-arduino/
  // https://arduino.stackexchange.com/questions/25945/how-to-read-and-write-eeprom-in-esp8266
  EEPROM.begin(512);

  
  EEPROM.get(addr,data);
  Serial.println("Found: "+String(data.val)+","+String(data.str)+","+String(data.ownerOfNode)); 

  // change data in RAM 
  data.val += 1;
  strncpy(data.str,"hello",20);

  // replace values in EEPROM
  EEPROM.put(addr,data);
  EEPROM.commit();

    // reload data for EEPROM, see the change
  EEPROM.get(addr,data);
  Serial.println("New values are: "+String(data.val)+","+String(data.str)+","+String(data.ownerOfNode));

  // get ID od owner of sensors
  if (data.ownerOfNode <= 0) {
     Serial.println("There is no Owner Of Node: "+String(data.ownerOfNode));  
     epromUid = 0;  
  } else {
    //data.ownerOfNode = 15;
    //EEPROM.put(addr,data.ownerOfNode);
    //EEPROM.commit();
    Serial.println("Owner Of Node: "+String(data.ownerOfNode));
    epromUid = data.ownerOfNode;
  }

  return epromUid;

}

void disconnectserver(){
   server.send(200, "text/html", "{\"success\":\"GardenSensor is DisConnected from Home Wifi, and GardenSensor will RESTART\"}");
   delay(1500);
   WiFi.disconnect(true);  // Erases SSID/password
   ESP.restart();
}
void restartnode(){
  digitalWrite(CLEAR_CONFIG_PIN,HIGH); // https://github.com/esp8266/Arduino/issues/1722
  ESP.restart();
}

String setUpWiFiMAC(){
  
  uint8_t mac[WL_MAC_ADDR_LENGTH];
  WiFi.softAPmacAddress(mac);
  String ss = String(mac[WL_MAC_ADDR_LENGTH - 2], HEX) + String(mac[WL_MAC_ADDR_LENGTH - 1], HEX);
  ss.toUpperCase();
  
  String AP_NameString = "GardenLocal-" + ss;
  Serial.print("AP_NameString : "); Serial.println(AP_NameString);

  for (int i=0; i< WL_MAC_ADDR_LENGTH; i++){
    if (mac[i] < 16){
      macID = macID + "0";
    }
    macID = macID + String(mac[i], HEX);
  }
  macID.toUpperCase();

  Serial.print("Chip ID: ");
  Serial.println(ESP.getFlashChipId());
  
  Serial.print("MAC ID : ");
  Serial.println(macID);

  Serial.print("WiFi.macAddress : ");
  Serial.println(WiFi.macAddress());

  Serial.print("Unique for try : ");
  String uniqueId = macID+ESP.getFlashChipId();
  Serial.println(uniqueId);
  

  char AP_NameChar[AP_NameString.length() + 1];
  memset(AP_NameChar, 0, AP_NameString.length() + 1);
  
  for (int i=0; i<AP_NameString.length(); i++) {
    AP_NameChar[i] = AP_NameString.charAt(i);
  }

  WiFi.softAP(AP_NameChar, "password011"); 
  WiFi.hostname("GardenHost");

  return AP_NameString;
  

}

