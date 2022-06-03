#ifndef BTLE_H_
#define BTLE_H_

#include "BleGamepad.hpp"
#include "defines.hpp"

typedef void (*btle_onstarted_cb_t)();
typedef void (*btle_onconnected_cb_t)();
typedef void (*btle_ondisconnected_cb_t)();

class BTLE
{
  public:
    static bool connected;
    static BleGamepad gamepad;

    static bool init();
    static void reset();
    static void startSync();
    static void stopSync();
    static bool toggleSync();

    static void onStarted(btle_onstarted_cb_t);
    static void onConnect(btle_onconnected_cb_t);
    static void onDisconnect(btle_ondisconnected_cb_t);

  private:
    static bool initialized;
    static NimBLEServer *bleServer;
    static btle_onstarted_cb_t onstarted_cb;
    static btle_onconnected_cb_t onconnected_cb;
    static btle_ondisconnected_cb_t ondisconnected_cb;
};

#endif // BTLE_H_