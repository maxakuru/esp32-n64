#define USE_NIMBLE

#include <Arduino.h>
#include "defines.hpp"
#include "btle.hpp"
#include "io.hpp"
#include "configurator.hpp"


#define CHECK_BIT(var, pos) ((var) & (1 << (pos)))

char bda_str[18];
int tick = 0;
uint8_t iniX = 0x83;
uint8_t iniY = 0x83;
bool initialized = false;
bool startPressed = false;


void setup()
{
  Serial.begin(115200);
  log_i("[main] Starting up...\n");
  delay(2500);

  IO::init();
  BTLE::init();

  IO::setLEDState(LedState::Off);

  IO::onControlPress([](int duration){
    printf("[main] Control pressed CB: %d\n", duration);
    if(wifi_config_enabled()) {
      wifi_config_disable();
      IO::setLEDState(LedState::Off);
    } else if(duration >= 20000) {
      // 20s => reset
      printf("[main] reset..\n");
    } else if(duration >= 5000) {
      printf("[main] config..\n");
      wifi_config_enable();
      IO::setLEDState(LedState::Blink, 1000, 1000);
    } else if(duration >= 3000) {
      printf("[main] sync..\n");
      IO::setLEDState(LedState::Blink, 400, 400);
    } else if(duration >= 1500) {
      printf("[main] off..\n");
    } else {
      printf("[main] click..\n");
    }
    // < 1s == click == nothing (todo: power on)
    // 1s == nothing (todo: power off)
    // 3s => sync mode
    // 5s => config mode
  });
}

void loop()
{
  // IO::loop();
}




