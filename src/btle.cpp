#include "btle.hpp"
#include "util.hpp"

#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "io.hpp"

bool BTLE::initialized = false;
bool BTLE::connected = false;
btle_onstarted_cb_t BTLE::onstarted_cb = NULL;
btle_onconnected_cb_t BTLE::onconnected_cb = NULL;
btle_ondisconnected_cb_t BTLE::ondisconnected_cb = NULL;

NimBLEServer* BTLE::bleServer = nullptr;

BleGamepad BTLE::gamepad = BleGamepad(getGamepadId(), "Acme Co.", 100);

void addCallbacks() {
  IO::onHatChange([](Hat hat, uint8_t data){
      // log_d("[BTLE] onHatChange(): hat=%d data=%d\n", hat, data);
      if(hat == Hat::DPad) {
          BTLE::gamepad.setHat1(data);
      } else if(hat == Hat::CPad) {
          BTLE::gamepad.setHat2(data);
      }
  });

  IO::onAxisChange([](Axis axis, int16_t data[]){
      // log_d("[BTLE] onAxisChange(): axis=%d data=%d ndims=%d\n", axis, data, nDims);
      if(axis == Axis::Left) {
          BTLE::gamepad.setLeftThumb(data[0], data[1]);
      } else if(axis == Axis::Right) {
          BTLE::gamepad.setRightThumb(data[0], data[1]);
      }
  });

  IO::onButtonChange([](Button btn, bool pressed){
      // log_d("[BTLE] onButtonChange(): btn=%d pressed=%d\n", btn, pressed);
      if(btn >= Button::Control || btn == Button::None) return;
      if(btn < Button::VolumeMute) {
          if(pressed) BTLE::gamepad.pressSpecialButton((uint8_t)btn);
          else BTLE::gamepad.releaseSpecialButton((uint8_t)btn);
          return;
      }

      // otherwise button #X
      uint8_t n = (uint8_t)btn - (uint8_t) Button::VolumeMute;
      if (pressed) {
          BTLE::gamepad.press(n);  
      } else {
          BTLE::gamepad.release(n);  
      }
  });
}

void rmCallbacks() {
  IO::onHatChange(NULL);
  IO::onAxisChange(NULL);
  IO::onButtonChange(NULL);
}

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

    gamepad.onStarted([](NimBLEServer *pServer) {
        log_d("[BTLE] onStarted()\n");
        bleServer = pServer;
        if(onstarted_cb != NULL) {
          onstarted_cb();
        }
    });

    gamepad.onConnect([](NimBLEServer *pServer) {
        log_d("[BTLE] onConnect()\n");
        connected = true;
        addCallbacks();
        if(onconnected_cb != NULL) {
          onconnected_cb();
        }
    });

    gamepad.onDisconnect([](NimBLEServer *pServer) {
        log_d("[BTLE] onDisconnect()\n");
        connected = false;
        rmCallbacks();
        if(ondisconnected_cb != NULL) {
          ondisconnected_cb();
        }
    });

    initialized = true;
    return true;
}

void BTLE::startSync() {
  if(bleServer == nullptr || bleServer->getAdvertising()->isAdvertising()) {
    return;
  }

  bleServer->startAdvertising();
}

void BTLE::stopSync() {
  if(bleServer == nullptr || !bleServer->getAdvertising()->isAdvertising()) {
    return;
  }

  bleServer->stopAdvertising();
}

bool BTLE::toggleSync() {
  if(bleServer == nullptr) {
    return false;
  }

  if(bleServer->getAdvertising()->isAdvertising()) {
    bleServer->stopAdvertising();
    return false;
  } else {
    bleServer->startAdvertising();
    return true;
  }
}

void BTLE::reset() {
  if(bleServer == nullptr) {
    return;
  }

  auto cons = bleServer->getPeerDevices();
  for(auto con : cons) {
    bleServer->disconnect(con);
  }
}

void BTLE::onStarted(btle_onstarted_cb_t cb) {
  onstarted_cb = cb;
}

void BTLE::onConnect(btle_onconnected_cb_t cb) {
  onconnected_cb = cb;
}

void BTLE::onDisconnect(btle_ondisconnected_cb_t cb) {
  ondisconnected_cb = cb;
}