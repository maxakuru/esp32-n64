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

static uint8_t computeHatFromDpad() {
    bool u = BTLE::gamepad.isPressed((uint8_t)Button::DUp + 1);
    bool d = BTLE::gamepad.isPressed((uint8_t)Button::DDown + 1);
    bool l = BTLE::gamepad.isPressed((uint8_t)Button::DLeft + 1);
    bool r = BTLE::gamepad.isPressed((uint8_t)Button::DRight + 1);

    if(u && !d) {
        if(r) return 2;      // NE
        if(l) return 8;      // NW
        return 1;             // N
    }
    if(d && !u) {
        if(r) return 4;      // SE
        if(l) return 6;      // SW
        return 5;             // S
    }
    if(r && !l) return 3;    // E
    if(l && !r) return 7;    // W
    return 0;                 // centered
}

void addCallbacks() {
  Serial.printf("[BTLE] addCallbacks: wiring IO -> BLE HID\n");

  IO::onAxisChange([](Axis axis, int16_t data[]){
      BTLE::gamepad.setX(data[0]);
      BTLE::gamepad.setY(data[1]);
      BTLE::gamepad.sendReport();
  });

  IO::onButtonChange([](Button btn, bool pressed){
      if(btn == Button::None || btn >= Button::Control) return;
      uint8_t n = (uint8_t)btn + 1; // HID buttons are 1-based
      if(pressed) BTLE::gamepad.press(n);
      else BTLE::gamepad.release(n);

      // Mirror D-pad buttons to hat switch for Steam/GCController compatibility
      if(btn >= Button::DUp && btn <= Button::DRight) {
          BTLE::gamepad.setHat1(computeHatFromDpad());
      }

      BTLE::gamepad.sendReport();
  });
}

void rmCallbacks() {
  IO::onAxisChange(NULL);
  IO::onButtonChange(NULL);
}

bool BTLE::init(){
    if(initialized) {
        return false;
    }

    log_d("[BTLE] init()\n");

    BleGamepadConfiguration conf;
    conf.setAutoReport(false); // we call sendReport() manually for atomic updates
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