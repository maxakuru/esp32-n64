#include <NimBLEDevice.h>
#include <NimBLEUtils.h>
#include <NimBLEServer.h>
#include "NimBLEHIDDevice.h"
#include "HIDTypes.h"
#include "HIDKeyboardTypes.h"
#include <driver/adc.h>
#include "sdkconfig.h"

#include "BleConnectionStatus.hpp"
#include "BleGamepad.hpp"
#include "BleGamepadConfiguration.hpp"

#if defined(CONFIG_ARDUHAL_ESP_LOG)
#include "esp32-hal-log.h"
#define LOG_TAG "BLEGamepad"
#else
#include "esp_log.h"
static const char *LOG_TAG = "BLEGamepad";
#endif

uint16_t vid;
uint16_t pid;

// Static HID Report Descriptor: 14 buttons, 1 hat switch, 2 axes (X, Y)
// D-pad is exposed as BOTH buttons 11-14 AND a hat switch for max compatibility.
// Report format (7 bytes after report ID):
//   [0]    buttons 1-8  (A,B,Z,L,R,Start,CUp,CDown)
//   [1]    buttons 9-14 (CLeft,CRight,DUp,DDown,DLeft,DRight) + 2 padding bits
//   [2]    hat switch   (0=centered, 1=N, 2=NE, 3=E, 4=SE, 5=S, 6=SW, 7=W, 8=NW)
//   [3-4]  X axis       (int16_t LE)
//   [5-6]  Y axis       (int16_t LE)
static const uint8_t hidReportDescriptorStatic[] = {
    0x05, 0x01,        // USAGE_PAGE (Generic Desktop)
    0x09, 0x05,        // USAGE (Gamepad)
    0xA1, 0x01,        // COLLECTION (Application)
    0x85, 0x01,        //   REPORT_ID (1)

    // --- 14 Buttons ---
    0x05, 0x09,        //   USAGE_PAGE (Button)
    0x19, 0x01,        //   USAGE_MINIMUM (Button 1)
    0x29, 0x0E,        //   USAGE_MAXIMUM (Button 14)
    0x15, 0x00,        //   LOGICAL_MINIMUM (0)
    0x25, 0x01,        //   LOGICAL_MAXIMUM (1)
    0x75, 0x01,        //   REPORT_SIZE (1)
    0x95, 0x0E,        //   REPORT_COUNT (14)
    0x81, 0x02,        //   INPUT (Data,Var,Abs)
    0x75, 0x01,        //   REPORT_SIZE (1)
    0x95, 0x02,        //   REPORT_COUNT (2) - padding
    0x81, 0x03,        //   INPUT (Const,Var,Abs)

    // --- Hat Switch (D-pad, mirrors buttons 11-14) ---
    0x05, 0x01,        //   USAGE_PAGE (Generic Desktop)
    0x09, 0x39,        //   USAGE (Hat Switch)
    0x15, 0x01,        //   LOGICAL_MINIMUM (1)
    0x25, 0x08,        //   LOGICAL_MAXIMUM (8)
    0x35, 0x00,        //   PHYSICAL_MINIMUM (0)
    0x46, 0x3B, 0x01,  //   PHYSICAL_MAXIMUM (315)
    0x65, 0x14,        //   UNIT (Eng Rot: Degree)
    0x75, 0x08,        //   REPORT_SIZE (8)
    0x95, 0x01,        //   REPORT_COUNT (1)
    0x81, 0x42,        //   INPUT (Data,Var,Abs,Null)

    // --- Joystick Axes (X, Y) ---
    0x05, 0x01,        //   USAGE_PAGE (Generic Desktop)
    0xA1, 0x00,        //   COLLECTION (Physical)
    0x09, 0x30,        //     USAGE (X)
    0x09, 0x31,        //     USAGE (Y)
    0x16, 0x01, 0x80,  //     LOGICAL_MINIMUM (-32767)
    0x26, 0xFF, 0x7F,  //     LOGICAL_MAXIMUM (32767)
    0x75, 0x10,        //     REPORT_SIZE (16)
    0x95, 0x02,        //     REPORT_COUNT (2)
    0x81, 0x02,        //     INPUT (Data,Var,Abs)
    0xC0,              //   END_COLLECTION (Physical)

    0xC0               // END_COLLECTION (Application)
};

// ondisconnect_cb_t BleGamepad::disconnect = NULL;


BleGamepad::BleGamepad(std::string deviceName, std::string deviceManufacturer, uint8_t batteryLevel) : _buttons(),
                                                                                                       _specialButtons(0),
                                                                                                       _x(0),
                                                                                                       _y(0),
                                                                                                       _z(0),
                                                                                                       _rZ(0),
                                                                                                       _rX(0),
                                                                                                       _rY(0),
                                                                                                       _slider1(0),
                                                                                                       _slider2(0),
                                                                                                       _rudder(0),
                                                                                                       _throttle(0),
                                                                                                       _accelerator(0),
                                                                                                       _brake(0),
                                                                                                       _steering(0),
                                                                                                       _hat1(0),
                                                                                                       _hat2(0),
                                                                                                       _hat3(0),
                                                                                                       _hat4(0),
                                                                                                       hid(0)
{
    this->resetButtons();
    this->deviceName = deviceName;
    this->deviceManufacturer = deviceManufacturer;
    this->batteryLevel = batteryLevel;
    this->connectionStatus = new BleConnectionStatus();
}

void BleGamepad::resetButtons()
{
    memset(&_buttons, 0, sizeof(_buttons));
}

void BleGamepad::begin(BleGamepadConfiguration *config)
{
    configuration = *config;

    vid = configuration.getVid();
    pid = configuration.getPid();

    uint8_t high = highByte(vid);
    uint8_t low = lowByte(vid);
    vid = low << 8 | high;

    high = highByte(pid);
    low = lowByte(pid);
    pid = low << 8 | high;

    xTaskCreate(this->taskServer, "server", 20000, (void *)this, 5, NULL);
}

void BleGamepad::end(void)
{
}

void BleGamepad::setAxes(int16_t x, int16_t y, int16_t z, int16_t rZ, int16_t rX, int16_t rY, int16_t slider1, int16_t slider2)
{
    if (x == -32768)
    {
        x = -32767;
    }
    if (y == -32768)
    {
        y = -32767;
    }
    if (z == -32768)
    {
        z = -32767;
    }
    if (rZ == -32768)
    {
        rZ = -32767;
    }
    if (rX == -32768)
    {
        rX = -32767;
    }
    if (rY == -32768)
    {
        rY = -32767;
    }
    if (slider1 == -32768)
    {
        slider1 = -32767;
    }
    if (slider2 == -32768)
    {
        slider2 = -32767;
    }

    _x = x;
    _y = y;
    _z = z;
    _rZ = rZ;
    _rX = rX;
    _rY = rY;
    _slider1 = slider1;
    _slider2 = slider2;

    if (configuration.getAutoReport())
    {
        sendReport();
    }
}

void BleGamepad::setSimulationControls(int16_t rudder, int16_t throttle, int16_t accelerator, int16_t brake, int16_t steering)
{
    if (rudder == -32768)
    {
        rudder = -32767;
    }
    if (throttle == -32768)
    {
        throttle = -32767;
    }
    if (accelerator == -32768)
    {
        accelerator = -32767;
    }
    if (brake == -32768)
    {
        brake = -32767;
    }
    if (steering == -32768)
    {
        steering = -32767;
    }

    _rudder = rudder;
    _throttle = throttle;
    _accelerator = accelerator;
    _brake = brake;
    _steering = steering;

    if (configuration.getAutoReport())
    {
        sendReport();
    }
}

void BleGamepad::setHats(signed char hat1, signed char hat2, signed char hat3, signed char hat4)
{
    _hat1 = hat1;
    _hat2 = hat2;
    _hat3 = hat3;
    _hat4 = hat4;

    if (configuration.getAutoReport())
    {
        sendReport();
    }
}

void BleGamepad::setSliders(int16_t slider1, int16_t slider2)
{
    if (slider1 == -32768)
    {
        slider1 = -32767;
    }
    if (slider2 == -32768)
    {
        slider2 = -32767;
    }

    _slider1 = slider1;
    _slider2 = slider2;

    if (configuration.getAutoReport())
    {
        sendReport();
    }
}

void BleGamepad::sendReport(void)
{
    if (this->isConnected())
    {
        uint8_t m[7];
        memset(m, 0, sizeof(m));

        // Buttons (bytes 0-1): 14 buttons packed as bits
        m[0] = _buttons[0];          // buttons 1-8
        m[1] = _buttons[1] & 0x3F;   // buttons 9-14, upper 2 bits padding

        // Hat switch (byte 2): 0 = centered (null), 1-8 = directions
        m[2] = (uint8_t)_hat1;

        // X axis (bytes 3-4): int16_t little-endian
        m[3] = (uint8_t)(_x & 0xFF);
        m[4] = (uint8_t)((_x >> 8) & 0xFF);

        // Y axis (bytes 5-6): int16_t little-endian
        m[5] = (uint8_t)(_y & 0xFF);
        m[6] = (uint8_t)((_y >> 8) & 0xFF);

        this->inputGamepad->setValue(m, sizeof(m));
        this->inputGamepad->notify();
    }
}

void BleGamepad::press(uint8_t b)
{
    uint8_t index = (b - 1) / 8;
    uint8_t bit = (b - 1) % 8;
    uint8_t bitmask = (1 << bit);

    uint8_t result = _buttons[index] | bitmask;

    if (result != _buttons[index])
    {
        _buttons[index] = result;
    }

    if (configuration.getAutoReport())
    {
        sendReport();
    }
}

void BleGamepad::release(uint8_t b)
{
    uint8_t index = (b - 1) / 8;
    uint8_t bit = (b - 1) % 8;
    uint8_t bitmask = (1 << bit);

    uint64_t result = _buttons[index] & ~bitmask;

    if (result != _buttons[index])
    {
        _buttons[index] = result;
    }

    if (configuration.getAutoReport())
    {
        sendReport();
    }
}

uint8_t BleGamepad::specialButtonBitPosition(uint8_t b)
{
    if (b >= POSSIBLESPECIALBUTTONS)
        throw std::invalid_argument("Index out of range");
    uint8_t bit = 0;
    for (int i = 0; i < b; i++)
    {
        if (configuration.getWhichSpecialButtons()[i])
            bit++;
    }
    return bit;
}

void BleGamepad::pressSpecialButton(uint8_t b)
{
    uint8_t button = specialButtonBitPosition(b);
    uint8_t bit = button % 8;
    uint8_t bitmask = (1 << bit);

    uint64_t result = _specialButtons | bitmask;

    if (result != _specialButtons)
    {
        _specialButtons = result;
    }

    if (configuration.getAutoReport())
    {
        sendReport();
    }
}

void BleGamepad::releaseSpecialButton(uint8_t b)
{
    uint8_t button = specialButtonBitPosition(b);
    uint8_t bit = button % 8;
    uint8_t bitmask = (1 << bit);

    uint64_t result = _specialButtons & ~bitmask;

    if (result != _specialButtons)
    {
        _specialButtons = result;
    }

    if (configuration.getAutoReport())
    {
        sendReport();
    }
}

void BleGamepad::pressStart()
{
    pressSpecialButton(START_BUTTON);
}

void BleGamepad::releaseStart()
{
    releaseSpecialButton(START_BUTTON);
}

void BleGamepad::pressSelect()
{
    pressSpecialButton(SELECT_BUTTON);
}

void BleGamepad::releaseSelect()
{
    releaseSpecialButton(SELECT_BUTTON);
}

void BleGamepad::pressMenu()
{
    pressSpecialButton(MENU_BUTTON);
}

void BleGamepad::releaseMenu()
{
    releaseSpecialButton(MENU_BUTTON);
}

void BleGamepad::pressHome()
{
    pressSpecialButton(HOME_BUTTON);
}

void BleGamepad::releaseHome()
{
    releaseSpecialButton(HOME_BUTTON);
}

void BleGamepad::pressBack()
{
    pressSpecialButton(BACK_BUTTON);
}

void BleGamepad::releaseBack()
{
    releaseSpecialButton(BACK_BUTTON);
}

void BleGamepad::pressVolumeInc()
{
    pressSpecialButton(VOLUME_INC_BUTTON);
}

void BleGamepad::releaseVolumeInc()
{
    releaseSpecialButton(VOLUME_INC_BUTTON);
}

void BleGamepad::pressVolumeDec()
{
    pressSpecialButton(VOLUME_DEC_BUTTON);
}

void BleGamepad::releaseVolumeDec()
{
    releaseSpecialButton(VOLUME_DEC_BUTTON);
}

void BleGamepad::pressVolumeMute()
{
    pressSpecialButton(VOLUME_MUTE_BUTTON);
}

void BleGamepad::releaseVolumeMute()
{
    releaseSpecialButton(VOLUME_MUTE_BUTTON);
}

void BleGamepad::setLeftThumb(int16_t x, int16_t y)
{
    if (x == -32768)
    {
        x = -32767;
    }
    if (y == -32768)
    {
        y = -32767;
    }

    _x = x;
    _y = y;

    if (configuration.getAutoReport())
    {
        sendReport();
    }
}
void BleGamepad::setRightThumb(int16_t z, int16_t rZ)
{
    if (z == -32768)
    {
        z = -32767;
    }
    if (rZ == -32768)
    {
        rZ = -32767;
    }

    _z = z;
    _rZ = rZ;

    if (configuration.getAutoReport())
    {
        sendReport();
    }
}

void BleGamepad::setLeftTrigger(int16_t rX)
{
    if (rX == -32768)
    {
        rX = -32767;
    }

    _rX = rX;

    if (configuration.getAutoReport())
    {
        sendReport();
    }
}

void BleGamepad::setRightTrigger(int16_t rY)
{
    if (rY == -32768)
    {
        rY = -32767;
    }

    _rY = rY;

    if (configuration.getAutoReport())
    {
        sendReport();
    }
}

void BleGamepad::setTriggers(int16_t rX, int16_t rY)
{
    if (rX == -32768)
    {
        rX = -32767;
    }
    if (rY == -32768)
    {
        rY = -32767;
    }

    _rX = rX;
    _rY = rY;

    if (configuration.getAutoReport())
    {
        sendReport();
    }
}

void BleGamepad::setHat(signed char hat)
{
    _hat1 = hat;

    if (configuration.getAutoReport())
    {
        sendReport();
    }
}

void BleGamepad::setHat1(signed char hat1)
{
    _hat1 = hat1;

    if (configuration.getAutoReport())
    {
        sendReport();
    }
}

void BleGamepad::setHat2(signed char hat2)
{
    _hat2 = hat2;

    if (configuration.getAutoReport())
    {
        sendReport();
    }
}

void BleGamepad::setHat3(signed char hat3)
{
    _hat3 = hat3;

    if (configuration.getAutoReport())
    {
        sendReport();
    }
}

void BleGamepad::setHat4(signed char hat4)
{
    _hat4 = hat4;

    if (configuration.getAutoReport())
    {
        sendReport();
    }
}

void BleGamepad::setX(int16_t x)
{
    if (x == -32768)
    {
        x = -32767;
    }

    _x = x;

    if (configuration.getAutoReport())
    {
        sendReport();
    }
}

void BleGamepad::setY(int16_t y)
{
    if (y == -32768)
    {
        y = -32767;
    }

    _y = y;

    if (configuration.getAutoReport())
    {
        sendReport();
    }
}

void BleGamepad::setZ(int16_t z)
{
    if (z == -32768)
    {
        z = -32767;
    }

    _z = z;

    if (configuration.getAutoReport())
    {
        sendReport();
    }
}

void BleGamepad::setRZ(int16_t rZ)
{
    if (rZ == -32768)
    {
        rZ = -32767;
    }

    _rZ = rZ;

    if (configuration.getAutoReport())
    {
        sendReport();
    }
}

void BleGamepad::setRX(int16_t rX)
{
    if (rX == -32768)
    {
        rX = -32767;
    }

    _rX = rX;

    if (configuration.getAutoReport())
    {
        sendReport();
    }
}

void BleGamepad::setRY(int16_t rY)
{
    if (rY == -32768)
    {
        rY = -32767;
    }

    _rY = rY;

    if (configuration.getAutoReport())
    {
        sendReport();
    }
}

void BleGamepad::setSlider(int16_t slider)
{
    if (slider == -32768)
    {
        slider = -32767;
    }

    _slider1 = slider;

    if (configuration.getAutoReport())
    {
        sendReport();
    }
}

void BleGamepad::setSlider1(int16_t slider1)
{
    if (slider1 == -32768)
    {
        slider1 = -32767;
    }

    _slider1 = slider1;

    if (configuration.getAutoReport())
    {
        sendReport();
    }
}

void BleGamepad::setSlider2(int16_t slider2)
{
    if (slider2 == -32768)
    {
        slider2 = -32767;
    }

    _slider2 = slider2;

    if (configuration.getAutoReport())
    {
        sendReport();
    }
}

void BleGamepad::setRudder(int16_t rudder)
{
    if (rudder == -32768)
    {
        rudder = -32767;
    }

    _rudder = rudder;

    if (configuration.getAutoReport())
    {
        sendReport();
    }
}

void BleGamepad::setThrottle(int16_t throttle)
{
    if (throttle == -32768)
    {
        throttle = -32767;
    }

    _throttle = throttle;

    if (configuration.getAutoReport())
    {
        sendReport();
    }
}

void BleGamepad::setAccelerator(int16_t accelerator)
{
    if (accelerator == -32768)
    {
        accelerator = -32767;
    }

    _accelerator = accelerator;

    if (configuration.getAutoReport())
    {
        sendReport();
    }
}

void BleGamepad::setBrake(int16_t brake)
{
    if (brake == -32768)
    {
        brake = -32767;
    }

    _brake = brake;

    if (configuration.getAutoReport())
    {
        sendReport();
    }
}

void BleGamepad::setSteering(int16_t steering)
{
    if (steering == -32768)
    {
        steering = -32767;
    }

    _steering = steering;

    if (configuration.getAutoReport())
    {
        sendReport();
    }
}

bool BleGamepad::isPressed(uint8_t b)
{
    uint8_t index = (b - 1) / 8;
    uint8_t bit = (b - 1) % 8;
    uint8_t bitmask = (1 << bit);

    if ((bitmask & _buttons[index]) > 0)
        return true;
    return false;
}

bool BleGamepad::isConnected(void)
{
    return this->connectionStatus->connected;
}

void BleGamepad::setBatteryLevel(uint8_t level)
{
    this->batteryLevel = level;
    if (hid != 0)
        this->hid->setBatteryLevel(this->batteryLevel);
}

void BleGamepad::taskServer(void *pvParameter)
{
    BleGamepad *BleGamepadInstance = (BleGamepad *)pvParameter; // static_cast<BleGamepad *>(pvParameter);
    NimBLEDevice::init(BleGamepadInstance->deviceName);
    NimBLEServer *pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(BleGamepadInstance->connectionStatus);

    BleGamepadInstance->hid = new NimBLEHIDDevice(pServer);
    BleGamepadInstance->inputGamepad = BleGamepadInstance->hid->inputReport(1); // report ID 1 matching static descriptor
    BleGamepadInstance->connectionStatus->inputGamepad = BleGamepadInstance->inputGamepad;

    BleGamepadInstance->hid->manufacturer()->setValue(BleGamepadInstance->deviceManufacturer);

    BleGamepadInstance->hid->pnp(0x01, vid, pid, 0x0110);
    BleGamepadInstance->hid->hidInfo(0x00, 0x01);

    NimBLESecurity *pSecurity = new NimBLESecurity();

    pSecurity->setAuthenticationMode(ESP_LE_AUTH_BOND);

    Serial.printf("[HID] descriptor (%d bytes): ", sizeof(hidReportDescriptorStatic));
    for (size_t i = 0; i < sizeof(hidReportDescriptorStatic); i++)
        Serial.printf("%02x", hidReportDescriptorStatic[i]);
    Serial.println();

    BleGamepadInstance->hid->reportMap((uint8_t *)hidReportDescriptorStatic, sizeof(hidReportDescriptorStatic));
    BleGamepadInstance->hid->startServices();

    if(BleGamepadInstance->onstarted_cb != NULL){
        BleGamepadInstance->onstarted_cb(pServer);
    }

    NimBLEAdvertising *pAdvertising = pServer->getAdvertising();
    pAdvertising->setAppearance(HID_GAMEPAD);
    pAdvertising->addServiceUUID(BleGamepadInstance->hid->hidService()->getUUID());
    pAdvertising->start();
    BleGamepadInstance->hid->setBatteryLevel(BleGamepadInstance->batteryLevel);

    ESP_LOGD(LOG_TAG, "Advertising started!");
    vTaskDelay(portMAX_DELAY); // delay(portMAX_DELAY);
}

void BleGamepad::onConnect(onconnect_cb_t cb){
    connectionStatus->onconnect_cb = cb;
    // connectionStatus.onconnect_cb = cb;
}

void BleGamepad::onDisconnect(ondisconnect_cb_t cb) {
    connectionStatus->ondisconnect_cb = cb;
}

void BleGamepad::onStarted(onstarted_cb_t cb) {
    this->onstarted_cb = cb;
}