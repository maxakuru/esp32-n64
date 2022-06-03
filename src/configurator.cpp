#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoOTA.h>
#include "driver/timer.h"
#include "configurator.hpp"
#include <uri/UriBraces.h>

xQueueHandle queue = xQueueCreate(2, sizeof(bool));
bool enabled_ = false;

WebServer server(80);

void ota_task(void* args) {
    ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

    ArduinoOTA.begin();
  
    for(;;)
    {   
        bool exit;
        if (xQueueReceive(queue, &exit, 0) == pdTRUE)
        {
            if(exit) {
                break;
            }
        }
        ArduinoOTA.handle();
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

void config_task(void* args) {
    server.on("/", handle_get_index);
    server.on(UriBraces("/get/{}"), handle_get_value);
    server.on(UriBraces("/set/{}"), handle_set_value);

    server.begin();
    for(;;)
    {   
        server.handleClient();
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

void wifi_config_enable() {
    if(enabled_) {
      return;
    }
    enabled_ = true;

    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(IPAddress(4, 3, 2, 1), IPAddress(4, 3, 2, 1), IPAddress(255, 255, 255, 0));
    WiFi.softAP(AP_SSID, AP_PASS);


    xTaskCreatePinnedToCore(
        ota_task, 
        "wifi ota updater task", 
        2048, 
        NULL, 
        3, 
        NULL, 
        0
    );

    xTaskCreatePinnedToCore(
        config_task, 
        "wifi configurator task", 
        2048, 
        NULL, 
        3, 
        NULL, 
        0
    );
}

void wifi_config_disable() {
    if(!enabled_) {
        return;
    }

    enabled_ = false;

    bool exit = true;
    xQueueSend(queue, (void*)&exit, 0);
    xQueueSend(queue, (void*)&exit, 0);

    WiFi.mode(WIFI_OFF);
}

bool wifi_config_enabled() {
    return enabled_;
}

String makeHTML(uint8_t led1stat, uint8_t led2stat) {
  String ptr = "<!DOCTYPE html> <html>\n";
  ptr +="<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  ptr +="<title>ESP32 N64</title>\n";
  ptr +="<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
  ptr +="body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;} h3 {color: #444444;margin-bottom: 50px;}\n";
  ptr +=".button {display: block;width: 80px;background-color: #3498db;border: none;color: white;padding: 13px 30px;text-decoration: none;font-size: 25px;margin: 0px auto 35px;cursor: pointer;border-radius: 4px;}\n";
  ptr +=".button-on {background-color: #3498db;}\n";
  ptr +=".button-on:active {background-color: #2980b9;}\n";
  ptr +=".button-off {background-color: #34495e;}\n";
  ptr +=".button-off:active {background-color: #2c3e50;}\n";
  ptr +="p {font-size: 14px;color: #888;margin-bottom: 10px;}\n";
  ptr +="</style>\n";
  ptr += "<script>window.setValue = (k,v) => fetch(`/set/${k}`, { method: 'POST', headers: {'content-type': 'application/json'}, body: JSON.stringify(v)});</script>\n";
  ptr += "<script>window.getValue = (k) => fetch(`/get/${k}`).then(r => r.json());</script>\n";
  ptr +="</head>\n";
  ptr +="<body>\n";
  ptr +="<h1>ESP32 N64 Configuration</h1>\n";

  ptr += "TODO...\n";

  ptr +="</body>\n";
  ptr +="</html>\n";
  return ptr;
}

void handle_get_index() {
    server.send(200, "text/html", makeHTML(false, false)); 
}

void handle_get_value() {
    server.send(200, "text/html", makeHTML(false, false)); 
}

void handle_set_value() {
    server.send(200, "text/html", makeHTML(false, false)); 
}