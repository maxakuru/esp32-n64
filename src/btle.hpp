#ifndef BTLE_H_
#define BTLE_H_

#include <BleGamepad.h>

class BTLE
{
  public:
    static bool init();
    static void reset();
    static BleGamepad gamepad;

  private:
    static bool initialized;
};

#endif // BTLE_H_