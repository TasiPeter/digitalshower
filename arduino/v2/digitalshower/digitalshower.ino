#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <NetworkClient.h>
#include <WiFiAP.h>
#include "FS.h"
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <SimpleFTPServer.h>
#include <ESP2SOTA.h>

#define scl 14
#define sda 13  
#define cs_1 27
#define cs_2 26

#define o_3way_on 4
#define o_3way_off 15
#define o_2way_on 17
#define o_2way_off 18
#define o_v_hnd 19
#define o_v_shw 21

#define TRUE 1
#define FALSE 0

const char *ssid = "DigitalShower";
const char *password = "Ret1sas!2025";
AsyncWebServer server(80);

FtpServer ftpSrv; 

Adafruit_NeoPixel strip(99, 23, NEO_GRB + NEO_KHZ800);

TaskHandle_t RGBTask;
TaskHandle_t MainTask;

long encoder_reading_time;
long rgb_time;
long firstPixelHue = 0;

void setup() {
  strip.begin();           // INITIALIZE NeoPixel strip object (REQUIRED)
  strip.show();            // Turn OFF all pixels ASAP
  strip.setBrightness(20);

  strip.clear();

  Serial.begin(115200);

  pinMode(scl, OUTPUT);
  pinMode(sda, INPUT);
  pinMode(cs_1, OUTPUT);
  pinMode(cs_2, OUTPUT);
  digitalWrite(cs_1, HIGH);  
  digitalWrite(cs_2, HIGH);

  pinMode(o_3way_on, OUTPUT);
  pinMode(o_3way_off, OUTPUT);
  pinMode(o_2way_on, OUTPUT);
  pinMode(o_2way_off, OUTPUT);
  pinMode(o_v_hnd, OUTPUT);
  pinMode(o_v_shw, OUTPUT);

  xTaskCreatePinnedToCore(RGBTaskFunc, "RGBTask", 4096, NULL, 1, &RGBTask, 1);
  xTaskCreatePinnedToCore(MainTaskFunc, "MainTask", 4096, NULL, 1, &MainTask, 0);

  initSPIFFS();

  if (!WiFi.softAP(ssid, password)) {
    log_e("Soft AP creation failed.");
    while (1);
  }
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);

  setupServer();
  
  ftpSrv.begin("DigitalShower","Ret1sas!2025");    //username, password for ftp.  set ports in ESP8266FtpServer.h  (default 21, 50009 for PASV)

  ESP2SOTA.begin(&server);
  
  Serial.println("Server started");
}

void RGBTaskFunc(void* param)
{
	while (1)
	{		
    if(firstPixelHue < 5*65536) {    
      strip.rainbow(firstPixelHue);   
      strip.show(); // Update strip with new contents   

      firstPixelHue += 64;
    }
    else {
      firstPixelHue = 0;
    }

    delay(10);
	}
}

void initSPIFFS() {
    if (!SPIFFS.begin(true)) {
        Serial.println("An error occurred while mounting SPIFFS");
        return;
    }
    Serial.println("SPIFFS mounted successfully");
}

void setupServer() {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/index.html", "text/html");
    });
    server.on("/temp", HTTP_GET, [](AsyncWebServerRequest *request) {
        String data = "32.5";
       request->send(200, "text/plain", data);
    });

    server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");

    server.begin();
}

void MainTaskFunc(void* param) {
 while (1)
	{    
   
    ftpSrv.handleFTP();
   
    /*
    unsigned int raw_input = read_encoder(1);
    float valve_pos_temp = ((raw_input / 16384.0) * 360.0);
    Serial.print(valve_pos_temp);
    Serial.print("\n");    
    delay(100);*/
    delay(1);
  }
}

void loop(){
  delay(1);
}

uint16_t read_encoder(unsigned char device_id) {
  // Wait for the SSI device to initiate the frame sync
  if(device_id == 1) { 
    digitalWrite(cs_1, LOW);
  }
  else if(device_id == 2) { 
    digitalWrite(cs_2, LOW);
  }
  else
  { return 0;}

  // Read the SSI data bits MSB first
  uint16_t response = 0;
  for (int i = 13; i >= 0; i--) {
    digitalWrite(scl, HIGH);
    __asm__("nop\n\t");//delayMicroseconds(1);
    response |= digitalRead(sda) << i;
    digitalWrite(scl, LOW);
    __asm__("nop\n\t");//delayMicroseconds(1);
  }
 
  if(device_id == 1) { 
    digitalWrite(cs_1, HIGH);
  }
  else if(device_id == 2) { 
    digitalWrite(cs_2, HIGH);
  }

  return response;  
}