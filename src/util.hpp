#ifndef UTIL_H_
#define UTIL_H_

#include <Arduino.h>

static char *bda2str(const uint8_t *bda, char *str, size_t size)
{
  if (bda == NULL || str == NULL || size < 18)
  {
    return NULL;
  }
  sprintf(str, "%02x:%02x:%02x:%02x:%02x:%02x",
          bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
  return str;
}

static std::string getDeviceId() {
  uint8_t baseMac[6];
  esp_read_mac(baseMac, ESP_MAC_BT);
  char baseMacChr[18] = {0};
  sprintf(baseMacChr, "%02X%02X%02X", baseMac[0], baseMac[1], baseMac[2]);
  return std::string(baseMacChr);
}

static std::string getGamepadId() {
  return std::string("ESP32-N64-" + getDeviceId());
}

static uint16_t analogReadAverage(gpio_num_t pin, uint8_t samples, uint16_t break_min = 0, uint16_t break_max = UINT16_MAX, uint16_t break_val = 0) {
    uint16_t val = analogRead(pin);
    if(val < break_min || val > break_max) {
        // otherwise sample for an average
        for(int i=1 ; i < samples ; i++) {
            val+=analogRead(pin);
        }
        val = val/samples;
    } else {
        if(break_val == 0) {
            return val;
        }
        return break_val;
    }

    // if in the home range, return the home value
    if(val >= break_min && val <= break_max && break_val != 0) {
        return break_val;
    }
    return val;
}

static int16_t scaleToRange(int16_t val, int16_t min, int16_t max, int16_t og_min, int16_t og_max) {
    //        (max-min)(val - og_min)
    // f(x) = -----------------------  + min
    //         og_max - og_min
    return ((max-min)*(val-og_min)/(og_max-og_min))+min;
}

#endif // UTIL_H_