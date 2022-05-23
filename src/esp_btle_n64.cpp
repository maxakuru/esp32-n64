#define USE_NIMBLE

#include <Arduino.h>
#include "USBSoftHost.hpp"
#include <BleGamepad.h>
// #include "esp_system.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"

#define PD_P 16            // data+
#define PD_M 17            // data-
#define NUM_BUTTONS 6      // L, R, A, B, Start, Z
#define NUM_HAT_SWITCHES 2 // d-pad, c-pad
#define PAIR_MAX_DEVICES 20

#define CHECK_BIT(var, pos) ((var) & (1 << (pos)))

static std::string getDeviceId();
static std::string getGamepadId();
static void detectCB(uint8_t usbNum, void *dev);
static void tickCB(uint8_t usbNum, uint8_t byte_depth, uint8_t *data, uint8_t data_len);
static void resetBluetooth();

usb_pins_config_t USB_Pins_Config = {
    PD_P, PD_M,
    -1, -1, // disabled
    -1, -1, // disabled
    -1, -1  // disabled
};

char bda_str[18];
int tick = 0;
uint8_t iniX = 0x83;
uint8_t iniY = 0x83;
bool initialized = false;
bool startPressed = false;

BleGamepad bleGamepad(getGamepadId(), "Acme Co.", 100); // device, manufacturer, battery level

void setup()
{

  Serial.begin(115200);
  delay(1200);

  USH.init(USB_Pins_Config, detectCB, tickCB);
  USH.setTaskCore(0);
  USH.setTaskPriority(1);

  BleGamepadConfiguration conf;
  conf.setAutoReport(false);
  conf.setControllerType(CONTROLLER_TYPE_JOYSTICK); // CONTROLLER_TYPE_JOYSTICK, CONTROLLER_TYPE_GAMEPAD (DEFAULT), CONTROLLER_TYPE_MULTI_AXIS
  conf.setButtonCount(NUM_BUTTONS);
  conf.setHatSwitchCount(NUM_HAT_SWITCHES);
  conf.setIncludeStart(true);
  conf.setIncludeXAxis(true);
  conf.setIncludeYAxis(true);
  conf.setVid(0xe502);
  conf.setPid(0xbbaa);

  bleGamepad.begin(&conf);

  resetBluetooth();
}

void loop()
{
  vTaskDelete(NULL);
}

static std::string getDeviceId()
{
  uint8_t baseMac[6];
  esp_read_mac(baseMac, ESP_MAC_BT);
  char baseMacChr[18] = {0};
  sprintf(baseMacChr, "%02X%02X%02X", baseMac[0], baseMac[1], baseMac[2]);
  return std::string(baseMacChr);
}

static std::string getGamepadId()
{
  return std::string("ESP32-N64-" + getDeviceId());
}

static void detectCB(uint8_t usbNum, void *dev)
{
  sDevDesc *device = (sDevDesc *)dev;
  printf("New device detected on USB#%d\n", usbNum);
  printf("desc.bcdUSB             = 0x%04x\n", device->bcdUSB);
  printf("desc.bDeviceClass       = 0x%02x\n", device->bDeviceClass);
  printf("desc.bDeviceSubClass    = 0x%02x\n", device->bDeviceSubClass);
  printf("desc.bDeviceProtocol    = 0x%02x\n", device->bDeviceProtocol);
  printf("desc.bMaxPacketSize0    = 0x%02x\n", device->bMaxPacketSize0);
  printf("desc.idVendor           = 0x%04x\n", device->idVendor);
  printf("desc.idProduct          = 0x%04x\n", device->idProduct);
  printf("desc.bcdDevice          = 0x%04x\n", device->bcdDevice);
  printf("desc.iManufacturer      = 0x%02x\n", device->iManufacturer);
  printf("desc.iProduct           = 0x%02x\n", device->iProduct);
  printf("desc.iSerialNumber      = 0x%02x\n", device->iSerialNumber);
  printf("desc.bNumConfigurations = 0x%02x\n", device->bNumConfigurations);
}

static void tickCB(uint8_t usbNum, uint8_t byte_depth, uint8_t *data, uint8_t data_len)
{
  auto px = data[0]; // 0 => left, ff => right
  auto py = data[1]; // 0 => up, ff => down

  if (!initialized)
  {
    printf("Initializing.\n");
    iniX = px;
    iniY = py;
    // printf("iniX, iniY: %d, %d\n", iniX, iniY);
    initialized = true;
    printf("Connecting");
    return;
  }
  else if (!bleGamepad.isConnected())
  {
    printf(".");
    return;
  }

  auto btn_pad = data[5];
  auto btn_dpad = btn_pad & 0x0f;
  auto btn_cpad = (btn_pad & 0xf0) >> 4;
  auto btn_act = data[6];

  auto dx = min(max((px - iniX) * 256, UINT16_MAX), -UINT16_MAX);
  auto dy = min(max((py - iniY) * 256, UINT16_MAX), -UINT16_MAX);
  // if(px != iniX || py != iniY) {
  //   printf("px, py: %d, %d     |      ", px, py);
  //   printf("dx, dy: %d, %d\n", dx, dy);
  // }
  bleGamepad.setLeftThumb(dx, dy);

  // buttons
  // L => 0x01
  // R => 0x02
  // A => 0x04
  // Z => 0x08
  // B => 0x10
  // start => 0x20

  for (int i = 1; i < 6; i++)
  {
    if (CHECK_BIT(btn_act, i - 1))
    {
      // L pressed
      if (!bleGamepad.isPressed(i))
      {
        printf("Pressed: %1i\n", i);
        bleGamepad.press(i);
      }
    }
    else if (bleGamepad.isPressed(i))
    {
      bleGamepad.release(i);
      printf("Released: %1i\n", i);
    }
  }

  if (CHECK_BIT(btn_act, 5))
  {
    // start pressed
    if (!startPressed)
    {
      printf("Pressed: start\n");
      bleGamepad.pressStart();
      startPressed = true;
    }
  }
  else if (startPressed)
  {
    bleGamepad.releaseStart();
    printf("Released: start\n");

    startPressed = false;
  }

  // dpad, cpad
  // printf("btn_dpad %d   |   ", btn_dpad);
  // printf("btn_cpad %d\n", btn_cpad);
  bleGamepad.setHat1(btn_dpad);
  bleGamepad.setHat2(btn_cpad);

  bleGamepad.sendReport();

  // if (tick == 10) {
  // printf("in: ");
  // for(int k=0;k<data_len;k++) {
  //   printf("0x%02x ", data[k] );
  // }
  // printf("\n");
  // tick = 0;
  // }
  // tick++;
}

char *bda2str(const uint8_t *bda, char *str, size_t size)
{
  if (bda == NULL || str == NULL || size < 18)
  {
    return NULL;
  }
  sprintf(str, "%02x:%02x:%02x:%02x:%02x:%02x",
          bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
  return str;
}

static void resetBluetooth()
{
  uint8_t pairedDeviceBtAddr[PAIR_MAX_DEVICES][6];
  char bda_str[18];

  printf("ESP32 bluetooth address: ");
  printf("%x\n", bda2str(esp_bt_dev_get_address(), bda_str, 18));
  // Get the numbers of bonded/paired devices in the BT module
  int count = esp_bt_gap_get_bond_device_num();
  if (!count)
  {
    printf("No bonded device found.\n");
  }
  else
  {
    printf("Bonded device count: %d", count);
    if (PAIR_MAX_DEVICES < count)
    {
      count = PAIR_MAX_DEVICES;
      printf("Reset bonded device count: %d", count);
    }
    esp_err_t tError = esp_bt_gap_get_bond_device_list(&count, pairedDeviceBtAddr);
    if (ESP_OK == tError)
    {
      for (int i = 0; i < count; i++)
      {
        printf("Found bonded device #%d -> %s", i, bda2str(pairedDeviceBtAddr[i], bda_str, 18));
        esp_err_t tError = esp_bt_gap_remove_bond_device(pairedDeviceBtAddr[i]);
        if (ESP_OK == tError)
        {
          printf("Removed bonded device #%i", i);
        }
        else
        {
          printf("Failed to remove bonded device #%i", i);
        }
      }
    }
  }
}