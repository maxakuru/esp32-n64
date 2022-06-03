#include "btle.hpp"
#include "defines.hpp"
#include "util.hpp"

#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "io.hpp"

bool BTLE::initialized = false;
BleGamepad BTLE::gamepad = BleGamepad(getGamepadId(), "Acme Co.", 100);

bool BTLE::init(){
    if(initialized) {
        return false;
    }

    log_d("[BTLE] init()\n");

    BleGamepadConfiguration conf;
    conf.setAutoReport(true);
    conf.setControllerType(CONTROLLER_TYPE_GAMEPAD); // CONTROLLER_TYPE_JOYSTICK, CONTROLLER_TYPE_GAMEPAD (DEFAULT), CONTROLLER_TYPE_MULTI_AXIS
    conf.setButtonCount(NUM_BUTTONS); // A, B, Z, R, L
    conf.setHatSwitchCount(NUM_HAT_SWITCHES); // d-pad, c-pad
    conf.setIncludeStart(true);
    conf.setIncludeXAxis(true);
    conf.setIncludeYAxis(true);

    conf.setVid(0xe502);
    conf.setPid(0xbbaa);

    gamepad.begin(&conf);

    IO::onHatChange([](Hat hat, uint8_t data){
        // log_d("[BTLE] onHatChange(): hat=%d data=%d\n", hat, data);
        if(hat == Hat::DPad) {
            gamepad.setHat1(data);
        } else if(hat == Hat::CPad) {
            gamepad.setHat2(data);
        }
    });

    IO::onAxisChange([](Axis axis, int16_t data[]){
        // log_d("[BTLE] onAxisChange(): axis=%d data=%d ndims=%d\n", axis, data, nDims);
        if(axis == Axis::Left) {
            gamepad.setLeftThumb(data[0], data[1]);
        } else if(axis == Axis::Right) {
            gamepad.setRightThumb(data[0], data[1]);
        }
    });

    IO::onButtonChange([](Button btn, bool pressed){
        // log_d("[BTLE] onButtonChange(): btn=%d pressed=%d\n", btn, pressed);
        if(btn >= Button::Control || btn == Button::None) return;
        if(btn < Button::VolumeMute) {
            if(pressed) gamepad.pressSpecialButton((uint8_t)btn);
            else gamepad.releaseSpecialButton((uint8_t)btn);
            return;
        }

        // otherwise button #X
        uint8_t n = (uint8_t)btn - (uint8_t) Button::VolumeMute;
        if (pressed) {
            gamepad.press(n);  
        } else {
            gamepad.release(n);  
        }
    });

    initialized = true;
    return true;
}

void BTLE::reset() {
  uint8_t pairedDeviceBtAddr[PAIR_MAX_DEVICES][6];
  char bda_str[18];

  log_i("ESP32 bluetooth address: ");
  log_i("%x\n", bda2str(esp_bt_dev_get_address(), bda_str, 18));
  // Get the numbers of bonded/paired devices in the BT module
  int count = esp_bt_gap_get_bond_device_num();
  if (!count)
  {
    log_e("No bonded device found.\n");
  }
  else
  {
    log_i("Bonded device count: %d", count);
    if (PAIR_MAX_DEVICES < count)
    {
      count = PAIR_MAX_DEVICES;
      log_i("Reset bonded device count: %d", count);
    }
    esp_err_t tError = esp_bt_gap_get_bond_device_list(&count, pairedDeviceBtAddr);
    if (ESP_OK == tError)
    {
      for (int i = 0; i < count; i++)
      {
        log_i("Found bonded device #%d -> %s", i, bda2str(pairedDeviceBtAddr[i], bda_str, 18));
        esp_err_t tError = esp_bt_gap_remove_bond_device(pairedDeviceBtAddr[i]);
        if (ESP_OK == tError)
        {
          log_i("Removed bonded device #%i", i);
        }
        else
        {
          log_e("Failed to remove bonded device #%i", i);
        }
      }
    }
  }
}