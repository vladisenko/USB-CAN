#ifndef _TIM_LED_HPP_
#define _TIM_LED_HPP_

#include "Timer.hpp"


class TimerLed : public Timer
{
public:
  explicit TimerLed (TimerLed&);
  TimerLed (void) : Timer(TIM15, 48000, 60000) {};
  void init (void);
  void rx_blink (uint32_t);
  void tx_blink (uint32_t);
  void link(bool);

protected:
  uint32_t calc_ccr(uint32_t);
};

#endif // _TIM_LED_HPP_