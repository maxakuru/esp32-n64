#include "BleConnectionStatus.hpp"

BleConnectionStatus::BleConnectionStatus(void)
{

}

void BleConnectionStatus::onConnect(NimBLEServer *pServer)
{
    this->connected = true;
    if(this->onconnect_cb != NULL) {
        this->onconnect_cb(pServer);
    }
}

void BleConnectionStatus::onDisconnect(NimBLEServer *pServer)
{
    this->connected = false;
    if(this->ondisconnect_cb != NULL) {
        this->ondisconnect_cb(pServer);
    }
}