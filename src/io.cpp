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

struct input_message_t {
    gpio_num_t pin;
    Input type;
    uint8_t index;
    int id;
};

struct joystick_message_t {
    gpio_num_t pins[3];
    Input type;
    uint8_t index;
};

bool IO::initialized = false;
bool IO::paused = false;
int IO::count = 0;
bool IO::control_pressed = false;
uint16_t IO::led_blinkrate_on = 0;
uint16_t IO::led_blinkrate_off = 0;
LedState IO::led_state = LedState::On;
uint8_t IO::priority = 1;
uint8_t IO::core = 0;
uint32_t IO::x = 0;
uint32_t IO::y = 0;
xQueueHandle IO::queue = xQueueCreate(25, sizeof(input_message_t));
SemaphoreHandle_t IO::input_sem = xSemaphoreCreateBinary();
SemaphoreHandle_t IO::control_sem = xSemaphoreCreateBinary();
SemaphoreHandle_t IO::joystick_sem = xSemaphoreCreateBinary();
SemaphoreHandle_t IO::led_sem = xSemaphoreCreateBinary();
inputchange_cb_t IO::inputchange_cb = NULL;
hatchange_cb_t IO::hatchange_cb = NULL;
axischange_cb_t IO::axischange_cb = NULL;
buttonchange_cb_t IO::buttonchange_cb = NULL;
controlpress_cb_t IO::controlpress_cb = NULL;

uint8_t interrupt_count = 0;
input_message_t interrupt_params[30];

int control_press_start = 0;

#ifdef USE_DEBOUNCE
#define DEBOUNCE_TIME 10000 // microseconds
int last_timers[40];
#endif // USE_DEBOUNCE

int IO::state[10][30][10];

io_config_t IO::config = {
    // buttons
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
        {(gpio_num_t)-1},
        {(gpio_num_t)-1}
    },

    // hats
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

    // joysticks
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
    },
    
    // LED
    {
        (gpio_num_t)PIN_LED_INFO
    },

    {
        (gpio_num_t)PIN_CTRL_BTN,
        Button::Control
    },
};

void IRAM_ATTR IO::inputIntrDelegate(void *args)
{
    input_message_t *msg = (input_message_t*)args;
    #ifdef USE_DEBOUNCE
    int now = micros();
    int last = last_timers[msg->pin];
    if(now - last > DEBOUNCE_TIME) {
        last_timers[msg->pin] = now;
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xQueueSendFromISR(queue, msg, (TickType_t)0);
        xSemaphoreGiveFromISR(input_sem, &xHigherPriorityTaskWoken);
    }
    #else
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(queue, msg, (TickType_t)0);
    xSemaphoreGiveFromISR(input_sem, &xHigherPriorityTaskWoken);
    #endif
}

void IRAM_ATTR IO::controlIntrDelegate(void *args)
{
    #ifdef USE_DEBOUNCE
    button_config_t *btn = (button_config_t*)args;
    int last = last_timers[btn->pin];
    int now = micros();
    if(now - last > DEBOUNCE_TIME) {
        last_timers[btn->pin] = now;
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(control_sem, &xHigherPriorityTaskWoken);
    }

    #else
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(control_sem, &xHigherPriorityTaskWoken);
    #endif
}

void IRAM_ATTR IO::inputTask(void *_)
{
    for (;;)
    {
        if (xSemaphoreTake(input_sem, portMAX_DELAY) == pdTRUE)
        {
            struct input_message_t args;
            while (xQueueReceive(queue, &args, 0) == pdTRUE)
            {
                IO::updateState(args.pin, args.type, args.index, args.id);
            }
        }
    }
    vTaskDelete(NULL);
}

void IRAM_ATTR IO::controlTask(void *args)
{
    button_config_t *btn = (button_config_t*)args;
    for (;;)
    {
        if (xSemaphoreTake(control_sem, portMAX_DELAY) == pdTRUE)
        {            
            int val = digitalRead(btn->pin);
            if(val == LOW) {
                // pressed, turn on LED and start timer
                control_pressed = true;
                control_press_start = millis();
            } else {
                // released
                int now = millis();
                control_pressed = false;
                if(controlpress_cb != NULL) {
                    controlpress_cb(now - control_press_start);
                }
            }
            xSemaphoreGive(led_sem);
        }
    }
    vTaskDelete(NULL);
}

void IRAM_ATTR IO::ledTask(void *args)
{
    led_config_t *led = (led_config_t*)args;
    for (;;)
    {
        if (xSemaphoreTake(led_sem, portMAX_DELAY) == pdTRUE)
        {
            if(control_pressed) {
                digitalWrite(led->pin, HIGH);
                
                bool on = true;
                int ticks = 0;
                while(control_pressed) {
                    // breakpoints are 1.5/3/5/20 seconds
                    if((ticks >= 15 && ticks <= 18) 
                    || (ticks >= 30 && ticks <= 33) 
                    || (ticks >= 50 && ticks <= 53) 
                    || (ticks >= 200 && ticks <= 203)) {
                        on = !on;
                        digitalWrite(led->pin, on);
                    }
                    ticks++;
                    vTaskDelay(100 / portTICK_PERIOD_MS);
                }
            }
            
            if(led_state == LedState::On) {
                digitalWrite(led->pin, HIGH);
            } else if(led_state == LedState::Off) {
                digitalWrite(led->pin, LOW);
            } else {
                bool s = digitalRead(led->pin);
                while(led_state == LedState::Blink && !control_pressed) {
                    s = !s;
                    digitalWrite(led->pin, s);
                    if(s) {
                        vTaskDelay(led_blinkrate_on / portTICK_PERIOD_MS);
                    } else {
                        vTaskDelay(led_blinkrate_off / portTICK_PERIOD_MS);
                    }
                }
            }
        }
    }
    vTaskDelete(NULL);
}

void IRAM_ATTR IO::joystickTask(void *args) {
    axis_config_t (*axes)[2] = (axis_config_t(*)[2])args;
    for (;;)
    {
        if (xSemaphoreTake(joystick_sem, portMAX_DELAY) == pdTRUE)
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
    }
    vTaskDelete(NULL);
}

void IRAM_ATTR IO::joystickCallback(void *args)
{
    timer_spinlock_take(TIMER_GROUP);
    timer_group_intr_clr_in_isr(TIMER_GROUP, TIMER_N);
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(joystick_sem, &xHigherPriorityTaskWoken);
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

    initLED(&config.led);

    initControlButton(&config.control_btn);

    xTaskCreate(
        IO::inputTask,
        "button input task",
        8192,
        NULL,
        priority,
        NULL
    );

     xTaskCreate(
        IO::controlTask,
        "control button task",
        4096,
        (void*)&config.control_btn,
        max(priority - 1, 1),
        NULL
    );

    xTaskCreatePinnedToCore(
        IO::joystickTask, 
        "joystick analog read task", 
        8192, 
        (void*)&config.axes, 
        min(priority + 1, 5), 
        NULL, 
        core
    );

     xTaskCreate(
        IO::ledTask,
        "LED control task",
        2048,
        (void*)&config.led,
        min(priority + 2, 5),
        NULL
    );


    for(uint8_t i=0; i<MAX_BUTTONS; i++) {
        button_config_t *btn = &config.buttons[i];
        if(btn->type == Button::None || btn->pin < 0) {
            continue;
        }
        log_d("[IO] init button (%d) on pin #%d\n", btn.type, btn.pin);
        initButton(btn);
    }

    for(uint8_t i=0; i<MAX_HATS; i++) {
        hat_config_t *hat = &config.hats[i];
        if(hat->type == Hat::None) {
            continue;
        }
        log_d("[IO] init hat #%d on pins: up=%d, right=%d, down=%d, left=%d\n", i, hat->up_pin, hat->right_pin, hat->down_pin, hat->left_pin);
        initHat(hat, i);
    }

    for(uint8_t i=0; i<MAX_AXES; i++) {
        axis_config_t *axis = &config.axes[i];
        if(axis->type == Axis::None) {
            continue;
        }
        log_d("[IO] init axis #%d on pins: x=%d, y=%d\n", i, axis->x_pin, axis->y_pin);
        initAxis(axis, i);
    }

    initJoystickTimer();

    initialized = true;
    return true;
}

void IO::initLED(led_config_t *led) {
    if(led->pin < 0) {
        return;
    }

    gpio_pad_select_gpio(led->pin);
    esp_err_t err = gpio_set_direction(led->pin, GPIO_MODE_OUTPUT);
    if (err)
    {
        log_e("[IO] initLED(%d) >> error setting mode: %i\n", led->pin, (int)err);
        return;
    }

    // on during boot up
    digitalWrite(led->pin, HIGH);
}

void IO::initControlButton(button_config_t *btn) {
    gpio_pad_select_gpio(btn->pin);
    esp_err_t err = gpio_set_direction(btn->pin, GPIO_MODE_INPUT);
    if (err)
    {
        log_e("[IO] initControlButton(%d) >> error setting mode: %i\n", btn->pin, (int)err);
        return;
    }

    #ifdef USE_DEBOUNCE
    last_timers[btn->pin] = micros();
    #endif

    err = gpio_set_pull_mode(btn->pin, GPIO_PULLUP_ONLY);
    if (err)
    {
        log_e("[IO] initControlButton(%d) >> error setting pull direction: %i\n", btn->pin, (int)err);
        return;
    }

    err = gpio_set_intr_type(btn->pin, GPIO_INTR_ANYEDGE);
    if (err)
    {
        log_e("[IO] initControlButton(%d) >> error setting interrupt type: %i\n", btn->pin, (int)err);
        return;
    }

    err = gpio_intr_enable(btn->pin);
    if (err)
    {
        log_e("[IO] initControlButton(%d) >> error enabling interrupt %i\n", btn->pin, (int)err);
        return;
    }

    err = gpio_isr_handler_add(btn->pin, controlIntrDelegate, (void *)&btn);

    if (err)
    {
        log_e("[IO] initControlButton(%d) >> error registering isr: %i\n", btn->pin, (int)err);
        return;
    }
}

void IO::initJoystickTimer() {
    timer_config_t config;
    config.divider     = TIMER_DIVIDER;
    config.counter_en  = TIMER_PAUSE;
    config.alarm_en    = TIMER_ALARM_EN;
    config.auto_reload = (timer_autoreload_t) 1;

    timer_init(TIMER_GROUP, TIMER_N, &config);
    timer_set_alarm_value(TIMER_GROUP, TIMER_N, (double)TIMER_INTERVAL0_SEC * TIMER_SCALE);
    timer_enable_intr(TIMER_GROUP, TIMER_N);
    timer_isr_register(TIMER_GROUP, TIMER_N, IO::joystickCallback, NULL, 0, NULL);
    timer_start(TIMER_GROUP, TIMER_N);
}

void IO::initHat(hat_config_t *hat, uint8_t index)
{
    initInputPin(hat->up_pin, Input::Hat, index, (int)HatDirection::Up);
    initInputPin(hat->right_pin, Input::Hat, index, (int)HatDirection::Right);
    initInputPin(hat->down_pin, Input::Hat, index, (int)HatDirection::Down);
    initInputPin(hat->left_pin, Input::Hat, index, (int)HatDirection::Left);
}

void IO::initButton(button_config_t *btn)
{
    if(btn->type == Button::Control) {
        initControlButton(btn);
    } else {
        initInputPin(btn->pin, Input::Button, (int)btn->type);
    }
}

void IO::initAxis(axis_config_t *axis, uint8_t index)
{
    log_d("[IO] initAxis() type=%d, x=%d, y=%d\n", axis.type, axis.x_pin, axis.y_pin);
    // #ifdef USE_ATTENUATION
    // adc_attenuation_t att = (adc_attenuation_t)12;
    // #else
    // adc_attenuation_t att = (adc_attenuation_t)8;
    // #endif // USE_12BIT_ADC
    // analogSetPinAttenuation(axis.x_pin, att);

    if(axis->x_pin > 0) {
        pinMode(axis->x_pin, INPUT_PULLDOWN);
    }
    if(axis->y_pin > 0) {
        pinMode(axis->y_pin, INPUT_PULLDOWN);
    }
}

void IO::initInputPin(gpio_num_t pin, Input type, uint8_t index, int id)
{
    if(pin < 0) {
        return;
    }
    log_d("[IO]initInputPin() pin=%d type=%d index=%d id=%d\n", pin, type, index, id);

    #ifdef USE_DEBOUNCE
    last_timers[pin] = micros();
    #endif

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

    struct input_message_t args = {
        pin,
        type,
        index,
        id
    };
    interrupt_params[interrupt_count] = args;
    err = gpio_isr_handler_add(pin, inputIntrDelegate, (void *)&interrupt_params[interrupt_count]);
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

void IO::onControlPress(controlpress_cb_t cb)
{
    controlpress_cb = cb;
}

void IO::setLEDState(LedState s, uint16_t blinkrate_on, uint16_t blinkrate_off){
    led_state = s;
    if(blinkrate_on > 0) {
        led_blinkrate_on = blinkrate_on;
        if(blinkrate_off > 0) {
            led_blinkrate_off = blinkrate_off;
        } else {
            led_blinkrate_off = blinkrate_on;
        }
    }
    xSemaphoreGive(led_sem);
}

void IO::reset()
{
    // noop
}