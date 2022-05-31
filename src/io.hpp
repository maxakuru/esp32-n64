#ifndef IO_H
#define IO_H

#include <Arduino.h>

#define LED_STATE_OFF 0
#define LED_STATE_ON 1
#define LED_STATE_BLINK 2

enum class Input {
  None = -1,
  Button = 0,
  Hat,
  Axis,
  Length
};

enum class Button { 
  None = -1,
  Start = 0, 
  Select = 1, // unassigned
  Menu = 2, // unassigned
  Home = 3, // unassigned
  Back = 4, // unassigned
  VolumeUp = 5, // unassigned
  VolumeDown = 6, // unassigned
  VolumeMute = 7, // unassigned
  A = 8,
  B = 9,
  Z = 10,
  L1 = 11,
  R1 = 12, 
  L2 = 13, // unassigned
  L3 = 14, // unassigned
  R2 = 15, // unassigned
  R3 = 16, // unassigned
  Sync,
  Length
};

enum class Axis {
  None = -1,
  Left = 0,
  Right,
  Length
};

enum class AxisDimension {
  None = -1,
  X = 0,
  Y,
  Length
};

enum class Hat {
  None = -1,
  DPad = 0,
  CPad,
  Length
};

enum class HatDirection { 
  None = -1,
  Up = 0,
  Right,
  Down,
  Left,
  Length
};

typedef struct {
  gpio_num_t pin;
  Button type;
} button_config_t;

typedef struct {
  gpio_num_t x_pin;
  gpio_num_t y_pin;
  Axis type;
} axis_config_t;

typedef struct {
  gpio_num_t up_pin;
  gpio_num_t right_pin;
  gpio_num_t down_pin;
  gpio_num_t left_pin;
  Hat type;
} hat_config_t;

typedef struct {
  button_config_t buttons[20];
  hat_config_t hats[2];
  axis_config_t axes[2];
} io_config_t;

typedef void (*inputchange_cb_t)(Input type, uint8_t index, int id);
typedef void (*buttonchange_cb_t)(Button button, bool pressed);
typedef void (*hatchange_cb_t)(Hat hat, uint8_t data);
typedef void (*axischange_cb_t)(Axis axis, int16_t data[]);

class IO
{
  public:
    static void setConfig(io_config_t);

    static bool init();
    static void reset();
    static void setPriority(uint8_t);
    static void setCore(uint8_t);
    static void onInputChange(inputchange_cb_t);
    static void onButtonChange(buttonchange_cb_t);
    static void onHatChange(hatchange_cb_t);
    static void onAxisChange(axischange_cb_t);

    static void pause();
    static void resume();

    static void setLEDState(uint8_t, uint8_t blinkrate = 0);


  private:
    static int state[10][30][10];
    static bool initialized;
    static bool paused;
    static int count;
    static uint8_t priority;
    static uint8_t core;
    static uint32_t x;
    static uint32_t y;
    static xQueueHandle queue;
    static SemaphoreHandle_t digital_sem;
    static SemaphoreHandle_t analog_sem;


    static io_config_t config;

    // static button_config_t button_conf[];
    // static hat_config_t hat_conf[];
    // static axis_config_t axis_conf[];

    static void initButton(button_config_t);
    static void initHat(hat_config_t, uint8_t index);
    static void initAxis(axis_config_t, uint8_t index);
    static void initInputPin(gpio_num_t pin, Input type, uint8_t index, int id = 0);
    static void initTimer();

    static void scaleAxisValues(int16_t data[]);
    static void updateState(gpio_num_t pin, Input type, uint8_t index, int id);
    static void updateState(Input type, uint8_t index);

    static void IRAM_ATTR interruptDelegate(void*);
    static void IRAM_ATTR interruptHandler(void*);
    static void IRAM_ATTR timerCallback(void*);
    static void IRAM_ATTR timerHandler(void*);

    static inputchange_cb_t inputchange_cb;
    static buttonchange_cb_t buttonchange_cb;
    static hatchange_cb_t hatchange_cb;
    static axischange_cb_t axischange_cb;
};


#endif // IO_H