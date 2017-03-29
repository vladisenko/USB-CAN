#ifndef _TIMER_HPP_
#define _TIMER_HPP_

class Timer
{
public:
  explicit Timer (Timer&);
  Timer (TIM_TypeDef* TIM, uint16_t PSC, uint32_t ARR) : tim(TIM), psc(PSC), arr(ARR) {}
  TIM_TypeDef* instance (void) const { return tim; }
  void init (void);
  uint32_t value (void) const { return tim->CNT; }
  uint32_t top (void) const { return arr-1; }
  void reset (void) const { tim->CNT = 0; }

private:
  TIM_TypeDef* tim;
  uint16_t psc;
  uint32_t arr;
};


#endif // _TIMER_HPP_
