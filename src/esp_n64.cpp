#define USE_NIMBLE

#include <Arduino.h>
#include "defines.hpp"
#include "btle.hpp"
#include "io.hpp"

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
}

void loop()
{
  // IO::loop();
}




