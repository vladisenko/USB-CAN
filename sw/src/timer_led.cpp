#include "stm32f0xx.h"
#include "timer_led.hpp"
#include "gpio.hpp"


extern "C" void TIM15_IRQHandler (void);


gpio led_link (GPIOB, 11, Mode::Output);
gpio led_rx   (GPIOB, 10, Mode::Output);
gpio led_tx   (GPIOB,  2, Mode::Output);


void TimerLed::init (void)
{
  Timer::init();
  instance()->CCMR1 = 0;
  instance()->CCER = 0;
  NVIC_SetPriority(TIM15_IRQn, 10);
  NVIC_EnableIRQ(TIM15_IRQn);

  RCC->AHBENR |= RCC_AHBENR_GPIOBEN;
  GPIOB->MODER &= ~(GPIO_MODER_MODER11   | GPIO_MODER_MODER10   | GPIO_MODER_MODER2);
  GPIOB->MODER |=   GPIO_MODER_MODER11_0 | GPIO_MODER_MODER10_0 | GPIO_MODER_MODER2_0;
  GPIOB->ODR &= ~(GPIO_ODR_11 | GPIO_ODR_10 | GPIO_ODR_2);
  //GPIOInit({led_link, led_rx, led_tx});
}


uint32_t TimerLed::calc_ccr(uint32_t tick)
{
  uint32_t top_ = top();
  uint32_t tmp = (tick > top_) ? (value() + top_) : (value() + tick);
  return (tmp > top_) ? (tmp - top_) : (tmp);
}


void TimerLed::rx_blink (uint32_t tick)
{
  led_rx.hi();
  instance()->CCR1 = calc_ccr(tick);
  instance()->SR   = ~TIM_SR_CC1IF;
  instance()->DIER |= TIM_DIER_CC1IE;   // CC1 interrupt enabled
}


void TimerLed::tx_blink (uint32_t tick)
{
  led_tx.hi();
  instance()->CCR2 = calc_ccr(tick);
  instance()->SR   = ~TIM_SR_CC2IF;
  instance()->DIER |= TIM_DIER_CC2IE;   // CC2 interrupt enabled
}


void TimerLed::link(bool state)
{
  if (state)
    led_link.hi();
  else
    led_link.lo();
}


void TIM15_IRQHandler (void)
{
  uint32_t sr = TIM15->SR;
  
  if (sr & TIM_SR_CC1IF)
  {
    TIM15->SR    = ~TIM_SR_CC1IF;
    TIM15->DIER &= ~TIM_DIER_CC1IE;
    led_rx.lo();
  }

  if (sr & TIM_SR_CC2IF)
  {
    TIM15->SR    = ~TIM_SR_CC2IF;
    TIM15->DIER &= ~TIM_DIER_CC2IE;
    led_tx.lo();
  }
}
