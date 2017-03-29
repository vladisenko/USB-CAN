#include "gpio.hpp"


//
//void GPIOInit (std::initializer_list<gpio> pins)
//{
//  GPIO_TypeDef gpios[6] = {0};
//  uint32_t rcc = 0;
//  
//  std::memset (gpios, 0, sizeof(gpios));
//
//  for (auto p=pins.begin(); p!=pins.end(); ++p)
//  {
//    uint8_t n;
//    if (p->port == GPIOA)
//      n = 0, rcc |= RCC_AHBENR_GPIOAEN;
//    else if (p->port == GPIOB)
//      n = 1, rcc |= RCC_AHBENR_GPIOBEN;
//    else if (p->port == GPIOC)
//      n = 2, rcc |= RCC_AHBENR_GPIOCEN;
//    else if (p->port == GPIOD)
//      n = 3, rcc |= RCC_AHBENR_GPIODEN;
//    else if (p->port == GPIOE)
//      n = 4, rcc |= RCC_AHBENR_GPIOEEN;
//    else if (p->port == GPIOF)
//      n = 5, rcc |= RCC_AHBENR_GPIOFEN;
//    else
//      continue;
//
//    gpios[n].MODER |= static_cast<uint32_t>(p->mode) << 2*(p->pin);
//  }
//
//  RCC->AHBENR |= rcc;
//  
//  GPIOA->MODER |= gpios[0].MODER;
//  GPIOB->MODER |= gpios[1].MODER;
//  GPIOC->MODER |= gpios[2].MODER;
//  GPIOD->MODER |= gpios[3].MODER;
//  GPIOE->MODER |= gpios[4].MODER;
//  GPIOF->MODER |= gpios[5].MODER;
//}

