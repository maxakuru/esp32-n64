#ifndef BTLE_H
#define BTLE_H

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

#endif // BTLE_H