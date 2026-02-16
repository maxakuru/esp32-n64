#ifndef DEFINES_H_
#define DEFINES_H_

// soft power latch (output, active HIGH keeps power on)
#define PIN_POWER_TOGGLE 32

// buttons (input, active LOW with pullup)
#define PIN_A 4
#define PIN_B 16
#define PIN_START 12
#define PIN_Z 13

// shoulders (input, active LOW with pullup)
#define PIN_SHOULDER_RIGHT 21
#define PIN_SHOULDER_LEFT 33

// d-pad (input, pulldown)
#define PIN_D_UP 25
#define PIN_D_DOWN 14
#define PIN_D_LEFT 26
#define PIN_D_RIGHT 27

// cpad (input, pulldown)
#define PIN_C_UP 19
#define PIN_C_DOWN 17
#define PIN_C_LEFT 5
#define PIN_C_RIGHT 18

// joystick (input, ADC)
#define PIN_JOYSTICK_X 34
#define PIN_JOYSTICK_Y 35

// control/sync button (input, pullup)
// Uses the BOOT button (IO0) -- only special during first 100ms of boot,
// then becomes a regular GPIO. Active LOW with internal pullup.
#define PIN_CTRL_BTN 0

// battery level (input, ADC via SENSOR_VP)
// Voltage divider: R17 (100K) + R18 (100K) = 0.5 ratio
#define PIN_BAT 36

// battery voltage divider ratio
#define BAT_V_RATIO 0.5

// gamepad properties
#define NUM_BUTTONS 14     // A, B, Z, L, R, Start, C-Up/Down/Left/Right, D-Up/Down/Left/Right
#define NUM_HAT_SWITCHES 0 // D-pad is buttons; hat switch computed in btle.cpp

// btle properties
#define PAIR_MAX_DEVICES 20

// LED (output, active HIGH, via current-limiting resistors to RGB LED)
#define PIN_LED_R 23
#define PIN_LED_G 22
#define PIN_LED_B 15

#ifndef PIN_LED_INFO
#define PIN_LED_INFO PIN_LED_G // use green as primary status LED
#endif // LED_INFO


#endif // DEFINES_H_
