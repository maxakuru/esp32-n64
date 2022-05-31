#include "io.hpp"
#include "util.hpp"
#include "defines.hpp"
#include "driver/timer.h"

#ifdef USE_12BIT_ADC
// 12-bit range: 1890 - 1911
#define MIN_JOYSTICK_HOME_X 1890
#define MAX_JOYSTICK_HOME_X 1911
#define AVG_JOYSTICK_HOME_X 1900

// 12-bit range: 270 - 3400
#define MIN_JOYSTICK_X 270  // left
#define MAX_JOYSTICK_X 3400 // right

// 12-bit range: 1915 - 1933
#define MIN_JOYSTICK_HOME_Y 1915
#define MAX_JOYSTICK_HOME_Y 1933
#define AVG_JOYSTICK_HOME_Y 1920

// 12-bit range: 400 - 3750
#define MIN_JOYSTICK_Y 400  // up
#define MAX_JOYSTICK_Y 3750 // down
#else
// 9-bit range: 236 - 239
#define MIN_JOYSTICK_HOME_X 235
#define MAX_JOYSTICK_HOME_X 240
#define AVG_JOYSTICK_HOME_X 237

// 9-bit range: 27 - 429
#define MIN_JOYSTICK_X 27  // left
#define MAX_JOYSTICK_X 429 // right

// 9-bit range: 239 - 241
#define MIN_JOYSTICK_HOME_Y 238
#define MAX_JOYSTICK_HOME_Y 242
#define AVG_JOYSTICK_HOME_Y 240

// 9-bit range: 50 - 475
#define MIN_JOYSTICK_Y 50  // up
#define MAX_JOYSTICK_Y 475 // down
#endif // USE_12BIT_ADC

#define JOYSTICK_RANGE_MIN INT16_MIN
#define JOYSTICK_RANGE_MAX INT16_MAX

#define TIMER_GROUP TIMER_GROUP_1
#define TIMER_N TIMER_0

#define NUM_ANALOG_SAMPLES 6     // number of analog readings to average over
#define TIMER_DIVIDER 2
#define TIMER_SCALE           (TIMER_BASE_CLK / TIMER_DIVIDER)  // convert counter value to seconds
#define TIMER_INTERVAL0_SEC   (0.001) // sample test interval for the first timer

struct interrupt_message_t {
    gpio_num_t pin;
    Input type;
    uint8_t index;
    int id;
};

struct interrupt_analog_message_t {
    gpio_num_t pins[3];
    Input type;
    uint8_t index;
};

bool IO::initialized = false;
bool IO::paused = false;
int IO::count = 0;
uint8_t IO::priority = 1;
uint8_t IO::core = 0;
uint32_t IO::x = 0;
uint32_t IO::y = 0;
xQueueHandle IO::queue = xQueueCreate(25, sizeof(interrupt_message_t));
SemaphoreHandle_t IO::digital_sem = xSemaphoreCreateBinary();
SemaphoreHandle_t IO::analog_sem = xSemaphoreCreateBinary();
inputchange_cb_t IO::inputchange_cb = NULL;
hatchange_cb_t IO::hatchange_cb = NULL;
axischange_cb_t IO::axischange_cb = NULL;
buttonchange_cb_t IO::buttonchange_cb = NULL;


uint8_t interrupt_count = 0;
interrupt_message_t interrupt_params[30];

uint8_t analog_interrupt_count = 0;
interrupt_analog_message_t analog_interrupt_params[4];

int IO::state[10][30][10];

io_config_t IO::config = {
    {
        {
            (gpio_num_t)PIN_A, // pin
            Button::A          // type
        },
        {
            (gpio_num_t)PIN_B,
            Button::B
        },
        {
            (gpio_num_t)PIN_START,
            Button::Start
        },
        {   
            (gpio_num_t)PIN_Z,
            Button::Z
        },
        {
            (gpio_num_t)PIN_SHOULDER_LEFT,
            Button::L1
        },
        {
            (gpio_num_t)PIN_SHOULDER_RIGHT,
            Button::R1
        },
        {
            (gpio_num_t)PIN_SYNC,
            Button::Sync
        },
        {(gpio_num_t)-1},
        {(gpio_num_t)-1},
        {(gpio_num_t)-1},
        {(gpio_num_t)-1},
        {(gpio_num_t)-1},
        {(gpio_num_t)-1},
        {(gpio_num_t)-1},
        {(gpio_num_t)-1},
        {(gpio_num_t)-1},
        {(gpio_num_t)-1},
        {(gpio_num_t)-1},
        {(gpio_num_t)-1},
        {(gpio_num_t)-1}
    },

    {
        {
            (gpio_num_t)PIN_D_UP,    // up pin
            (gpio_num_t)PIN_D_RIGHT, // right pin
            (gpio_num_t)PIN_D_DOWN,  // down pin
            (gpio_num_t)PIN_D_LEFT,  // left pin
            Hat::DPad                // type
        },
        {
            (gpio_num_t)PIN_C_UP,
            (gpio_num_t)PIN_C_RIGHT,
            (gpio_num_t)PIN_C_DOWN,
            (gpio_num_t)PIN_C_LEFT,
            Hat::CPad
        }
    },

    {
        {
            (gpio_num_t)PIN_JOYSTICK_X, // x
            (gpio_num_t)PIN_JOYSTICK_Y, // y
            Axis::Left                  // type
        },
        {
            (gpio_num_t)-1,
            (gpio_num_t)-1,
            Axis::None
        }
    }
};

void IRAM_ATTR IO::interruptDelegate(void *args)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(queue, (interrupt_message_t*)args, (TickType_t)0);
    xSemaphoreGiveFromISR(digital_sem, &xHigherPriorityTaskWoken);
}

void IRAM_ATTR IO::interruptHandler(void *_)
{
    for (;;)
    {
        if (xSemaphoreTake(digital_sem, portMAX_DELAY) == pdTRUE)
        {
            struct interrupt_message_t args;
            while (xQueueReceive(queue, &args, 0) == pdTRUE)
            {
                IO::updateState(args.pin, args.type, args.index, args.id);
            }
        }
    }
    vTaskDelete(NULL);
}

void IRAM_ATTR IO::timerHandler(void *args) {
    axis_config_t (*axes)[2] = (axis_config_t(*)[2])args;
    for (;;)
    {
        if (xSemaphoreTake(analog_sem, portMAX_DELAY) == pdTRUE)
        {
            for(auto axis : *axes) {
                if(axis.type == Axis::None) {
                    continue;
                }

                bool needsUpdate = false;
                if(axis.x_pin > 0) {
                    // x
                    uint16_t val = analogReadAverage(axis.x_pin, NUM_ANALOG_SAMPLES, MIN_JOYSTICK_HOME_X, MAX_JOYSTICK_HOME_X, AVG_JOYSTICK_HOME_X);
                    uint16_t prev = IO::state[(int)Input::Axis][(int)axis.type][(int)AxisDimension::X];
                    if(prev != val) {
                        needsUpdate = true;
                        IO::state[(int)Input::Axis][(int)axis.type][(int)AxisDimension::X] = val;
                    }
                }
                if(axis.y_pin > 0) {
                    // y
                    uint16_t val = analogReadAverage(axis.y_pin, NUM_ANALOG_SAMPLES, MIN_JOYSTICK_HOME_Y, MAX_JOYSTICK_HOME_Y, AVG_JOYSTICK_HOME_Y);
                    uint16_t prev = IO::state[(int)Input::Axis][(int)axis.type][(int)AxisDimension::Y];
                    if(prev != val) {
                        needsUpdate = true;
                        IO::state[(int)Input::Axis][(int)axis.type][(int)AxisDimension::Y] = val;
                    }
                }
                if(needsUpdate) {
                    IO::updateState(Input::Axis, (int)axis.type);
                }
            }
            timer_group_enable_alarm_in_isr(TIMER_GROUP, TIMER_N);
        }
        // vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

void IRAM_ATTR IO::timerCallback(void *args)
{
    timer_spinlock_take(TIMER_GROUP);
    timer_group_intr_clr_in_isr(TIMER_GROUP, TIMER_N);
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(analog_sem, &xHigherPriorityTaskWoken);
    timer_spinlock_give(TIMER_GROUP);
}

void IO::setConfig(io_config_t config_)
{
    config = config_;
}

void IO::pause()
{
  if(!paused) {
    log_d("pausing timer");
    timer_pause(TIMER_GROUP, TIMER_N);
    paused = true;
    vTaskDelay(1);
  } else {
    log_e("timer already paused");
  }
}

void IO::resume()
{
  if(paused) {
    log_d("resuming timer");
    timer_start(TIMER_GROUP, TIMER_N);
    paused = false;
    vTaskDelay(1);
  } else {
    log_e("timer already running");
  }
}

void IO::updateState(Input type, uint8_t index) {
    IO::updateState((gpio_num_t)-1, type, index, -1);
}

void IO::updateState(gpio_num_t pin, Input type, uint8_t index, int id) {
    if(type == Input::Button) {
        int v = digitalRead(pin);
        if(IO::state[(int)type][index][id] == v) {
            return;
        }
        IO::state[(int)type][index][id] = v;
        if(IO::buttonchange_cb != NULL) {
            IO::buttonchange_cb((Button)index, v == HIGH);
        }
    } else if(type == Input::Hat) {
        int v = digitalRead(pin);
        auto prev = IO::state[(int)type][index][id];
        IO::state[(int)type][index][id] = v;

        if(prev == v) {
            return;
        }

        auto raw = IO::state[(int)type][index];
        uint8_t d = (raw[(int)HatDirection::Up]) ^ 
            (raw[(int)HatDirection::Right] << 1) ^
            (raw[(int)HatDirection::Down] << 2) ^
            (raw[(int)HatDirection::Left] << 3);

        if(IO::hatchange_cb != NULL) {
            /**
             *  Hat switch data => outgoing value
             *   9  1  3            8  1  2
             *   8  +  2            7     3
             *  12  4  6            6  5  4
             */
            uint8_t data;
            switch(d) {
                case 0:
                case 1:
                    data = d;
                    break;
                case 2:
                case 4:
                    data = d + 1;
                    break;
                case 3:
                case 8:
                case 9:
                    data = d - 1;
                    break;
                case 6:
                    data = 4;
                    break;
                case 12:
                    data = 6;
                    break;
                default:
                    data = 0;
                    break;
            }
            IO::hatchange_cb((Hat)index, data);
        }
    } else if(type == Input::Axis) {
        // already checked if state should be updated in timer task,
        // so just send the data immediately
        if(IO::axischange_cb != NULL) {
            int16_t data[2] = { 
                (int16_t)state[(int)type][index][(int)AxisDimension::X], 
                (int16_t)state[(int)type][index][(int)AxisDimension::Y], 
            };
            scaleAxisValues(data);
            IO::axischange_cb((Axis)index, data);
        }
    }

    if(IO::inputchange_cb != NULL) {
        IO::inputchange_cb(type, index, id);
    }
}

void IO::scaleAxisValues(int16_t data[]) {
    data[(int)AxisDimension::X] = scaleToRange(data[(int)AxisDimension::X], JOYSTICK_RANGE_MIN, JOYSTICK_RANGE_MAX, MIN_JOYSTICK_X, MAX_JOYSTICK_X);
    data[(int)AxisDimension::Y] = scaleToRange(data[(int)AxisDimension::Y], JOYSTICK_RANGE_MIN, JOYSTICK_RANGE_MAX,  MIN_JOYSTICK_Y, MAX_JOYSTICK_Y);
}

void IO::setPriority(uint8_t priority_)
{
    priority = priority_;
}

void IO::setCore(uint8_t core_)
{
    core = core_;
}

bool IO::init()
{
    if (initialized)
    {
        return false;
    }

    log_d("[IO] init()\n");

#ifdef USE_12BIT_ADC
    analogReadResolution(12);
    log_i("[IO] Using 12-bit ADC resolution\n");
#else
    analogReadResolution(9);
    log_i("[IO] Using 9-bit ADC resolution\n");
#endif // USE_12BIT_ADC

    esp_err_t err = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    if (err)
    {
        log_e("[IO] init() >> error setting pull direction: %i\n", (int)err);
        return false;
    }

    xTaskCreate(
        IO::interruptHandler,
        "digital IO change interrupt",
        8192,
        NULL,
        priority,
        NULL);

    pinMode(LED_INFO, OUTPUT);

    int i = 0;
    for (button_config_t btn : config.buttons)
    {
        if(btn.type == Button::None || btn.pin < 0) {
            continue;
        }
        log_d("[IO] init button (%d) on pin #%d\n", btn.type, btn.pin);
        initButton(btn);
        i++;
    }

    i = 0;
    for (hat_config_t hat : config.hats)
    {
        if(hat.type == Hat::None) {
            continue;
        }
        log_d("[IO] init hat #%d on pins: up=%d, right=%d, down=%d, left=%d\n", i, hat.up_pin, hat.right_pin, hat.down_pin, hat.left_pin);
        initHat(hat, i);
        i++;
    }

    i = 0;
    for (axis_config_t axis : config.axes)
    {
        if(axis.type == Axis::None) {
            continue;
        }
        log_d("[IO] init axis #%d on pins: x=%d, y=%d\n", i, axis.x_pin, axis.y_pin);
        initAxis(axis, i);
        i++;
    }

    initTimer();

    initialized = true;
    return true;
}

void IO::initTimer() {
    xTaskCreatePinnedToCore(IO::timerHandler, "analog timer handler", 8192, (void*)&config.axes, max(priority+1, 5), NULL, core);

    timer_config_t config;
    config.divider     = TIMER_DIVIDER;
    config.counter_en  = TIMER_PAUSE;
    config.alarm_en    = TIMER_ALARM_EN;
    config.auto_reload = (timer_autoreload_t) 1;

    timer_init(TIMER_GROUP, TIMER_N, &config);
    timer_set_alarm_value(TIMER_GROUP, TIMER_N, (double)TIMER_INTERVAL0_SEC * TIMER_SCALE);
    timer_enable_intr(TIMER_GROUP, TIMER_N);
    timer_isr_register(TIMER_GROUP, TIMER_N, IO::timerCallback, NULL, 0, NULL);
    timer_start(TIMER_GROUP, TIMER_N);
}

void IO::initHat(hat_config_t hat, uint8_t index)
{
    initInputPin(hat.up_pin, Input::Hat, index, (int)HatDirection::Up);
    initInputPin(hat.right_pin, Input::Hat, index, (int)HatDirection::Right);
    initInputPin(hat.down_pin, Input::Hat, index, (int)HatDirection::Down);
    initInputPin(hat.left_pin, Input::Hat, index, (int)HatDirection::Left);
}

void IO::initButton(button_config_t btn)
{
    initInputPin(btn.pin, Input::Button, (int)btn.type);
}

void IO::initAxis(axis_config_t axis, uint8_t index)
{
    log_d("[IO] initAxis() type=%d, x=%d, y=%d\n", axis.type, axis.x_pin, axis.y_pin);
    // #ifdef USE_ATTENUATION
    // adc_attenuation_t att = (adc_attenuation_t)12;
    // #else
    // adc_attenuation_t att = (adc_attenuation_t)8;
    // #endif // USE_12BIT_ADC
    // analogSetPinAttenuation(axis.x_pin, att);

    if(axis.x_pin > 0) {
        pinMode(axis.x_pin, INPUT_PULLDOWN);
    }
    if(axis.y_pin > 0) {
        pinMode(axis.y_pin, INPUT_PULLDOWN);
    }
}

void IO::initInputPin(gpio_num_t pin, Input type, uint8_t index, int id)
{
    if(pin < 0) {
        return;
    }
    log_d("[IO]initInputPin() pin=%d type=%d index=%d id=%d\n", pin, type, index, id);


    gpio_pad_select_gpio(pin);
    esp_err_t err = gpio_set_direction(pin, GPIO_MODE_INPUT);
    if (err)
    {
        log_e("[IO] initInputPin(%d) >> error setting mode: %i\n", pin, (int)err);
        return;
    }

    err = gpio_set_pull_mode(pin, GPIO_PULLDOWN_ONLY);
    if (err)
    {
        log_e("[IO] initInputPin(%d) >> error setting pull direction: %i\n", pin, (int)err);
        return;
    }

    err = gpio_set_intr_type(pin, GPIO_INTR_ANYEDGE);
    if (err)
    {
        log_e("[IO] initInputPin(%d) >> error setting interrupt type: %i\n", pin, (int)err);
        return;
    }

    err = gpio_intr_enable(pin);
    if (err)
    {
        log_e("[IO] initInputPin(%d) >> error enabling interrupt %i\n", pin, (int)err);
        return;
    }

    struct interrupt_message_t args = {
        pin,
        type,
        index,
        id
    };
    interrupt_params[interrupt_count] = args;
    err = gpio_isr_handler_add(pin, interruptDelegate, (void *)&interrupt_params[interrupt_count]);
    interrupt_count++;

    if (err)
    {
        log_e("[IO] initInputPin(%d) >> error registering isr: %i\n", pin, (int)err);
        return;
    }
}

void IO::onInputChange(inputchange_cb_t cb)
{
    inputchange_cb = cb;
}

void IO::onButtonChange(buttonchange_cb_t cb)
{
    buttonchange_cb = cb;
}

void IO::onHatChange(hatchange_cb_t cb)
{
    hatchange_cb = cb;
}

void IO::onAxisChange(axischange_cb_t cb)
{
    axischange_cb = cb;
}

void IO::reset()
{
    // noop
}