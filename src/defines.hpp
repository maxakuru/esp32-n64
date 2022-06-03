#ifndef DEFINES_H_
#define DEFINES_H_

// buttons (input, pulldown)
#define PIN_A 33
#define PIN_B 18
#define PIN_START 15
#define PIN_Z 26

// shoulders (input, pulldown)
#define PIN_SHOULDER_RIGHT 32
#define PIN_SHOULDER_LEFT 25

// d-pad (input, pulldown)
#define PIN_D_UP 14
#define PIN_D_DOWN 36
#define PIN_D_LEFT 17
#define PIN_D_RIGHT 16

// cpad (input, pulldown)
#define PIN_C_UP 27
#define PIN_C_DOWN 5
#define PIN_C_LEFT 23
#define PIN_C_RIGHT 13

// joystick (input, ADC)
#define PIN_JOYSTICK_X 39
#define PIN_JOYSTICK_Y 35

// control/sync (input, pullup)
#define PIN_CTRL_BTN 21

// battery level (input, ADC)
#define PIN_BAT 34

// battery voltage divider ratio
#define BAT_V_RATIO 0.5

// gamepad properties
#define NUM_BUTTONS 6      // L, R, A, B, Start, Z
#define NUM_HAT_SWITCHES 2 // d-pad, c-pad

// btle properties
#define PAIR_MAX_DEVICES 20

// LED (output, PWM)
#ifndef PIN_LED_INFO
#define PIN_LED_INFO 2 // default to built-in LED
#endif // LED_INFO


#endif // DEFINES_H_