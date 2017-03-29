#ifndef _GPIO_HPP_
#define _GPIO_HPP_

#include "stm32f0xx.h"
//#include <initializer_list>


enum class Mode : uint32_t
{
  Input = 0,
  Output = GPIO_MODER_MODER0_0,
  Alternate = GPIO_MODER_MODER0_1,
  Analog = GPIO_MODER_MODER0_1 | GPIO_MODER_MODER0_0
};


enum class OType : uint16_t
{
  PushPull = 0,
  OpenDrain = 1,
};


enum class OSpeed : uint32_t
{
  Low = 0,
  Medium = GPIO_OSPEEDER_OSPEEDR0_0,
  High = GPIO_OSPEEDER_OSPEEDR0
};       


class gpio
{
public:
  GPIO_TypeDef* const port;
  const uint32_t pin;
  const Mode mode;

  gpio(GPIO_TypeDef* GPIO, uint32_t PIN, Mode MODE = Mode::Input) : port(GPIO), pin(PIN), mode(MODE) {};
  
  void lo (void) const { port->BSRR = GPIO_BSRR_BR_0 << pin; }
  void hi (void) const { port->BSRR = GPIO_BSRR_BS_0 << pin; }
  void toggle(void) const { port->ODR ^= 1 << pin; };

  uint16_t get (void) const { return port->IDR & (1 << pin); }
};


//void GPIOInit (std::initializer_list<gpio> pins);

#endif // _GPIO_HPP_
