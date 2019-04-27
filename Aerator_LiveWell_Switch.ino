#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <SPIFFS.h>
#include <WebSocketsServer.h>

#include <Adafruit_NeoPixel.h>

//Create a web server
WebServer server ( 80 );

// Create a Websocket server
WebSocketsServer webSocket(81);


unsigned int state;

#define SEQUENCE_IDLE 0x00
#define GET_SAMPLE 0x10

#define GET_SAMPLE__WAITING 0x12

// Which pin on the Arduino is connected to the NeoPixels?
#define PIN        13 // On Trinket or Gemma, suggest changing this to 1

// How many NeoPixels are attached to the Arduino?
#define NUMPIXELS 8 // Popular NeoPixel ring size

const char WiFiAPPSK[] = "Livewell";

#define USE_SERIAL Serial
#define DBG_OUTPUT_PORT Serial

uint8_t remote_ip;
uint8_t socketNumber;
float value;
int i;
int offtime;  //Off time setting from mobile web app

//These will need to be updated to the GPIO pins for each control circuit.
int POWER = 13; 
int MOMENTARY = 4;
int SPEED = 14; 
int LEFT = 12; 
int RIGHT = 13;
const int ANALOG_PIN = A0;

// Set pixel id to 0
int t = 0;

int onoff = 0; 

volatile byte switch_state = LOW;
boolean pumpOn = false;
boolean timer_state = false;
boolean timer_started = false;

int Clock_seconds;

//Keep this code for reference to determine how to get voltage for pump
//extern "C" {
//uint16 readvdd33(void);
//}

hw_timer_t * timer = NULL;

Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

#define DELAYVAL 500 // Time (in milliseconds) to pause between pixels

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) {
    String text = String((char *) &payload[0]);
    char * textC = (char *) &payload[0];
    String voltage;
    String temp;
    float percentage;
    float actual_voltage;  //voltage calculated based on percentage drop from 5.0 circuit.
    int nr;
    int on;
    uint32_t rmask;
    int i;
    
    switch(type) {
        case WStype_DISCONNECTED:
            //Reset the control for sending samples of ADC to idle to allow for web server to respond.
            USE_SERIAL.printf("[%u] Disconnected!\n", num);
            state = SEQUENCE_IDLE;
            break;
        case WStype_CONNECTED:
          {
            //Display client IP info that is connected in Serial monitor and set control to enable samples to be sent every two seconds (see analogsample() function)
            IPAddress ip = webSocket.remoteIP(num);
            USE_SERIAL.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
            socketNumber = num;
            state = GET_SAMPLE;
          }
            break;
 
        case WStype_TEXT:
            if (payload[0] == '+')
                {
                Serial.printf("[%u] On Time Setting Control Msg: %s\n", num, payload);
                Serial.println((char *)payload);
                i = abs(atoi((char *)payload));
                //uint32_t ontime = (uint32_t) strtol((const char *) &payload[1], NULL, 2);
                //Serial.print("On Time = ");
                //Serial.println(ontime);*/
                if (i < t)
                  {
                  pixels.setPixelColor(i, pixels.Color(0, 0, 0));
                  pixels.show();   // Send the updated pixel colors to the hardware.
                  } else {
                    pixels.setPixelColor(i, pixels.Color(0, 160, 0));
                    pixels.show();   // Send the updated pixel colors to the hardware.
                  }
                  t = i;
                }
            if (payload[0] == '-')
                {
                Serial.printf("[%u] Off Time Setting Control Msg: %s\n", num, payload);
                Serial.println((char *)&payload[1]);
                offtime = abs(atoi((char *)payload));
                //uint32_t offtime = (uint32_t) strtol((const char *) &payload[1], NULL, 2);
                Serial.printf("Off Time = ");
                Serial.println(offtime);
                }
            if (payload[0] == 'O')
                {
                Serial.printf("[%u] Timer state event %s\n", num, payload);
                //Serial.println((char *)&payload);
                if (payload[1] == 'N') timer_state = true;
                if (payload[2] == 'F') timer_state = false;
                }
                
            if (payload[0] == '#')
                Serial.printf("[%u] Digital GPIO Control Msg: %s\n", num, payload);
                if (payload[1] == 'I')
                {
                  if (payload[2] == 'D')
                    {
                    Serial.printf("Direction Down");
                    //value=readvdd33();
                Serial.print("Vcc:");
                Serial.println(value/1000);
                percentage = (value/1000)/3.0;
                Serial.print("percentage:");
                Serial.println(percentage);
                actual_voltage = 11.8*percentage; //Using 11.8 to conservative
                Serial.print("act_volt:");
                Serial.println(percentage);
                voltage = String(actual_voltage);
                webSocket.sendTXT(num, voltage);
                    digitalWrite(POWER, HIGH);
                    }
                  if (payload[2] == 'U')
                    {
                    Serial.printf("Direction Up");
                    digitalWrite(POWER, LOW);
                    }
                  break;
                }
                if (payload[1] == 'M')
                {
                  if (payload[2] == 'D')
                    {
                    Serial.printf("Direction Down");
                    digitalWrite(MOMENTARY, HIGH);
                    }
                  if (payload[2] == 'U')
                    {
                    Serial.printf("Direction Up");
                    digitalWrite(MOMENTARY, LOW);
                    }
                  break;
                }
                if (payload[1] == 'L')
                {
                  if (payload[2] == 'D')
                    {
                    Serial.printf("Direction Down");
                    digitalWrite(LEFT, HIGH);
                    }
                  if (payload[2] == 'U')
                    {
                    Serial.printf("Direction Up");
                    digitalWrite(LEFT, LOW);
                    }
                  break;
                 }
                 if (payload[1] == 'R')
                 {
                  if (payload[2] == 'D')
                    {
                    Serial.printf("Direction Down");
                    digitalWrite(RIGHT, HIGH);
                    }
                  if (payload[2] == 'U')
                    {
                    Serial.printf("Direction Up");
                    digitalWrite(RIGHT, LOW);
                    }
                  break;
                 }
            if (payload[0] == 'S')
              {
                Serial.printf("[%u] Analog GPIO Control Msg: %s\n", num, payload);
              }
              
         case WStype_BIN:
         {
            /*USE_SERIAL.printf("[%u] get binary lenght: %u\n", num, lenght);
            //hexdump(payload, lenght);
            //analogWrite(13,atoi((const char *)payload));
            Serial.printf("Payload");
            Serial.printf("[%u] Analog GPIO Control Msg: %s\n", num, payload);
            Serial.println((char *)payload);
            int temp = atoi((char *)payload);
            uint32_t time_setting = (uint32_t) strtol((const char *) &payload[1], NULL, 8);
            //analogWrite(SPEED,temp);   ******************** NEED TO CREATE ANALOG FUNCTION ***********
            //webSocket.sendTXT(num,"Got Speed Change");*/
         }
         break;
         
         case WStype_ERROR:
            USE_SERIAL.printf(WStype_ERROR + " Error [%u] , %s\n",num, payload); 
    }
}

void handleRoot() {
  server.send(200, "text/html", "<h1>You are connected</h1>");
}

String getContentType(String filename){
  if(server.hasArg("download")) return "application/octet-stream";
  else if(filename.endsWith(".htm")) return "text/html";
  else if(filename.endsWith(".html")) return "text/html";
  else if(filename.endsWith(".css")) return "text/css";
  else if(filename.endsWith(".js")) return "application/javascript";
  else if(filename.endsWith(".png")) return "image/png";
  else if(filename.endsWith(".gif")) return "image/gif";
  else if(filename.endsWith(".jpg")) return "image/jpeg";
  else if(filename.endsWith(".ico")) return "image/x-icon";
  else if(filename.endsWith(".xml")) return "text/xml";
  else if(filename.endsWith(".pdf")) return "application/x-pdf";
  else if(filename.endsWith(".zip")) return "application/x-zip";
  else if(filename.endsWith(".gz")) return "application/x-gzip";
  else if(filename.endsWith(".svg")) return "image/svg+xml";
  return "text/plain";
}

bool handleFileRead(String path){
  DBG_OUTPUT_PORT.println("handleFileRead: " + path);
  if(path.endsWith("/"))
    {
      path += "relay.html";
      state = SEQUENCE_IDLE;
      File file = SPIFFS.open(path, "r");
      Serial.print("Sending relay ");
      Serial.print(path);
      String contentType = getContentType(path);
      size_t sent = server.streamFile(file, contentType);
      file.close();
      return true;
    }
  
  String pathWithGz = path + ".gz";
  DBG_OUTPUT_PORT.println("PathFile: " + pathWithGz);
  if(SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)){
    if(SPIFFS.exists(pathWithGz))
      path += ".gz";
    File file = SPIFFS.open(path, "r");
    //size_t sent = server.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}

void setupWiFi()
{
  WiFi.mode(WIFI_AP);
  
  String AP_NameString = "NeoPixel Fun";

  char AP_NameChar[AP_NameString.length() + 1];
  memset(AP_NameChar, 0, AP_NameString.length() + 1);

  for (int i=0; i<AP_NameString.length(); i++)
    AP_NameChar[i] = AP_NameString.charAt(i);

  WiFi.softAP(AP_NameChar);
}

void IRAM_ATTR onoffTimer(){

  switch (onoff) {

    case 0:
      Serial.println("Turning Off Pump " + String(Clock_seconds));
      digitalWrite(POWER,LOW);
      pumpOn = false;
      Clock_seconds++;
      if (Clock_seconds > offtime){
        onoff = 1;
        Clock_seconds = 0;
      }
      break;
    
    case 1:
     
      break;
    }  
}

void startTimer(){
  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onoffTimer, true);
  timerAlarmWrite(timer, 1000000, true);
  yield();
  timerAlarmEnable(timer);
  timer_started = true;
  Serial.println("Timer Started");
}

void stopTimer(){
  if (timer != NULL) {
    timerAlarmDisable(timer);
    timerDetachInterrupt(timer);
    timerEnd(timer);
    timer = NULL;
    timer_started = false;
    Serial.println("Timer Stopped");
    digitalWrite(POWER,LOW);
  }
}

void setup() {

  pixels.begin(); // INITIALIZE NeoPixel strip object (REQUIRED)
  
  pinMode(POWER, OUTPUT);
  pinMode(MOMENTARY, OUTPUT);
  pinMode(SPEED, OUTPUT);
  pinMode(LEFT, OUTPUT);
  pinMode(RIGHT, OUTPUT);

  digitalWrite(POWER, LOW);
  digitalWrite(MOMENTARY, LOW);
  digitalWrite(SPEED, LOW);
  digitalWrite(LEFT, LOW);
  digitalWrite(RIGHT, LOW);
  
  Serial.begin(115200);
  SPIFFS.begin();
  Serial.println();
  Serial.print("Configuring access point...");
  /* You can remove the password parameter if you want the AP to be open. */
  setupWiFi();
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);


  server.on("/", HTTP_GET, [](){
    handleFileRead("/");
    //handleRoot();
  });

//Handle when user requests a file that does not exist
  server.onNotFound([](){
    if(!handleFileRead(server.uri()))
  server.send(404, "text/plain", "FileNotFound");
  });
  server.begin();
  Serial.println("HTTP server started");

  // start webSocket server
  webSocket.begin();
  Serial.println("Websocket server started");
  webSocket.onEvent(webSocketEvent);

//+++++++ MDNS will not work when WiFi is in AP mode but I am leave this code in place incase this changes++++++
//if (!MDNS.begin("esp8266")) {
//    Serial.println("Error setting up MDNS responder!");
//    while(1) { 
//      delay(1000);
//    }
//  }
//  Serial.println("mDNS responder started");

  // Add service to MDNS
  //  MDNS.addService("http", "tcp", 80);
  //  MDNS.addService("ws", "tcp", 81);
}

void loop() {
  webSocket.loop();
  server.handleClient();
  if ((timer_state == true) && (timer_started == false)) startTimer();
  if ((timer_state == false) && (timer_started == true)) stopTimer();
  // Add logic to enable timer with NO/OFF switch in web app
  
}
