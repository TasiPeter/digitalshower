#ifndef AsyncTimer_h
#define AsyncTimer_h

#include <Arduino.h>

class AsyncTimer {
  private:
    double _time;
    bool _started;
    double _interval;
    bool _fired;

  public:
    AsyncTimer ();
    bool start(double interval = 0);
    void stop();
    void update();
    bool on_tick();
    bool running;
};

#endif
