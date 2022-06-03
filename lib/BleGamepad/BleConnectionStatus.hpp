#ifndef ESP32_BLE_CONNECTION_STATUS_H
#define ESP32_BLE_CONNECTION_STATUS_H
#include "sdkconfig.h"
#if defined(CONFIG_BT_ENABLED)

#include "nimconfig.h"
#if defined(CONFIG_BT_NIMBLE_ROLE_PERIPHERAL)

#include <NimBLEServer.h>
#include "NimBLECharacteristic.h"

typedef void (*onconnect_cb_t)(NimBLEServer *pServer);
typedef void (*ondisconnect_cb_t)(NimBLEServer *pServer);


class BleConnectionStatus : public NimBLEServerCallbacks
{
public:
    BleConnectionStatus(void);
    onconnect_cb_t onconnect_cb = NULL;
    ondisconnect_cb_t ondisconnect_cb = NULL;
    bool connected = false;
    void onConnect(NimBLEServer *pServer);
    void onDisconnect(NimBLEServer *pServer);
    NimBLECharacteristic *inputGamepad;
};

#endif // CONFIG_BT_NIMBLE_ROLE_PERIPHERAL
#endif // CONFIG_BT_ENABLED
#endif // ESP32_BLE_CONNECTION_STATUS_H