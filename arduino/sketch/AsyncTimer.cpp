#include "AsyncTimer.h"

AsyncTimer::AsyncTimer() {
  _started = false;
  _time = 0;
  _fired = false;
  running = false;
}

bool AsyncTimer::start(double interval = 0) {
  if(interval == 0) { 
    if(_interval == 0) {
      _started = false;
      running = false;
      return false;
    }    
  }
  else {
    _interval = interval;
  }
  _started = true;  
  running = true;
  _time = millis(); 
  return true;
}

void AsyncTimer::stop() {
  _started = false;
  running = false;
}

void AsyncTimer::update() {
  if(_started) {
    if((millis() - _interval) > _time) {
    _time = millis(); 
    _fired = true;
    }
  }
}

bool AsyncTimer::on_tick() {
  if(_fired) {
    _fired = false;
    return true;
  }
  else
  {
    return false;
  }
}