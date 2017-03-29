#include "stm32f0xx.h"
#include "Timer.hpp"


static inline void clock_enable (TIM_TypeDef* tim)
{
  if (tim == TIM2)
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
  else if (tim == TIM3)
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
  else if (tim == TIM6)
    RCC->APB1ENR |= RCC_APB1ENR_TIM6EN;
  else if (tim == TIM7)
    RCC->APB1ENR |= RCC_APB1ENR_TIM7EN;
  else if (tim == TIM14)
    RCC->APB1ENR |= RCC_APB1ENR_TIM14EN;
  else if (tim == TIM1)
    RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;
  else if (tim == TIM15)
    RCC->APB2ENR |= RCC_APB2ENR_TIM15EN;
  else if (tim == TIM16)
    RCC->APB2ENR |= RCC_APB2ENR_TIM16EN;
  else if (tim == TIM17)
    RCC->APB2ENR |= RCC_APB2ENR_TIM17EN;
  //else 
    // TODO: add assert
}


void Timer::init (void)
{
  clock_enable(tim);

  tim->PSC = psc - 1;
  tim->ARR = arr - 1;
  tim->CNT = 0;
  tim->EGR = TIM_EGR_UG;
  while ( (tim->SR & TIM_SR_UIF) == 0);
  tim->SR = 0;
  tim->CR1 = TIM_CR1_CEN;
}
