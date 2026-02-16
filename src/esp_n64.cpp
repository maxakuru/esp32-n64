#define USE_NIMBLE

#include <Arduino.h>
#include "defines.hpp"
#include "btle.hpp"
#include "io.hpp"
#include "configurator.hpp"


static void powerOff() {
  log_i("[main] powering off..\n");
  delay(100); // let serial flush
  digitalWrite(PIN_POWER_TOGGLE, LOW);
  // if USB-powered, the latch drop won't kill power,
  // so fall back to deep sleep
  esp_deep_sleep_start();
}

void setup()
{
  // FIRST: latch power on before anything else.
  // The soft power switch (T6/T8) drops power as soon as the
  // CTRL button is released unless IO32 is driven HIGH.
  pinMode(PIN_POWER_TOGGLE, OUTPUT);
  digitalWrite(PIN_POWER_TOGGLE, HIGH);

  Serial.begin(115200);
  log_i("[main] Starting up...\n");

  IO::init();
  IO::setLEDState(LedState::Off);

  BTLE::onStarted([]() {
      IO::setLEDState(LedState::Blink, 400, 400);
  });

  BTLE::onConnect([]() {
      Serial.printf("[main] BLE connected!\n");
      IO::setLEDState(LedState::Off);
  });

  BTLE::onDisconnect([]() {
      IO::setLEDState(LedState::Blink, 400, 400);
  });

  BTLE::init();


  IO::onControlPress([](int duration){
    log_i("[main] Control pressed CB: %d\n", duration);
    if(wifi_config_enabled()) {
      wifi_config_disable();
      IO::setLEDState(LedState::Off);
    } else if(duration >= 20000) {
      // 20s => reset
      log_i("[main] reset..\n");
      BTLE::reset();
      esp_restart();
    } else if(duration >= 5000) {
      log_i("[main] config..\n");
      wifi_config_enable();
      IO::setLEDState(LedState::Blink, 1000, 1000);
    } else if(duration >= 3000) {
      log_i("[main] sync..\n");
      IO::setLEDState(LedState::Blink, 400, 600);
      BTLE::toggleSync();
    } else if(duration >= 1500) {
      powerOff();
    } else {
      log_i("[main] click..\n");
    }
    // < 1s == click == nothing
    // 1.5s => power off
    // 3s => sync mode
    // 5s => config mode
    // 20s => full reset
  });
}

void loop()
{
  vTaskDelay(portMAX_DELAY);
}
