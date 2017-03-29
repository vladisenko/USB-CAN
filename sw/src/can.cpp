#include "stm32f0xx.h"
#include "can.hpp"
#include "fifo.hpp"
#include "timer_led.hpp"


extern "C" void CEC_CAN_IRQHandler (void);

using CANbus::Status;
using CANbus::OpenMode;
using CANbus::Bitrate;
using CANbus::RxMsg;
using CANbus::TxMsg;


static TimerLed timled;
static bool timestamping = false;
static CANbus::RxCallback rx_cb = nullptr;
static bool isopen = false;
static uint32_t btr_reg = static_cast<uint32_t>(Bitrate::br1Mbit);


Status CANbus::init (void)
{
  RCC->AHBENR |= RCC_AHBENR_GPIOBEN;
  GPIOB->MODER &= ~(GPIO_MODER_MODER9 | GPIO_MODER_MODER8);
  GPIOB->MODER |= GPIO_MODER_MODER9_1 | GPIO_MODER_MODER8_1;
  GPIOB->AFR[1] &= ~0x000000FF;
  GPIOB->AFR[1] |=  0x00000044;

  RCC->APB1ENR |= RCC_APB1ENR_CANEN;
  CAN->MCR |= CAN_MCR_SLEEP | CAN_MCR_ABOM;

  CAN->FMR |= CAN_FMR_FINIT; 
  CAN->FM1R = 0;                    // 0: Two 32-bit registers of filter bank x are in Identifier Mask mode.
  CAN->FS1R = CAN_FS1R_FSC0;        // 1: Single 32-bit scale configuration
  CAN->FFA1R = 0;                   // 0: Filter assigned to FIFO 0
  CAN->FA1R = CAN_FA1R_FACT0;       // 1: Filter is active
  CAN->sFilterRegister[0].FR1 = 0;  // ID
  CAN->sFilterRegister[0].FR2 = 0;  // MASK  
  CAN->FMR &=~ CAN_FMR_FINIT;

  timled.init();

  return Status::Ok;
}


Status CANbus::filtermask (uint32_t msk)
{
  CAN->FMR |= CAN_FMR_FINIT; 
  CAN->sFilterRegister[0].FR2 = ~msk; // MASK  
  CAN->FMR &=~ CAN_FMR_FINIT;
  return Status::Ok;
}


Status CANbus::filtercode (uint32_t code)
{
  CAN->FMR |= CAN_FMR_FINIT; 
  CAN->sFilterRegister[0].FR1 = code; // ID  
  CAN->FMR &=~ CAN_FMR_FINIT;
  return Status::Ok;
}


Status CANbus::bitrate (Bitrate br)
{
  return bitrate(static_cast<uint32_t>(br));
}


Status CANbus::bitrate (uint32_t btr)
{
  btr_reg &= ~(CAN_BTR_BRP | CAN_BTR_TS1 | CAN_BTR_TS2 | CAN_BTR_SJW);
  btr_reg |= btr & (CAN_BTR_BRP | CAN_BTR_TS1 | CAN_BTR_TS2 | CAN_BTR_SJW);
  
  // FIXME: check busy?
  if (isopen)
  {
    CAN->MCR |= CAN_MCR_INRQ;
    while (!(CAN->MSR & CAN_MSR_INAK));

    CAN->BTR = btr_reg;
  
    CAN->MCR &= ~((uint32_t)CAN_MCR_INRQ);
    while (CAN->MSR & CAN_MSR_INAK);
  }

  return Status::Ok;
}


Status CANbus::open (OpenMode mode)
{
  CAN->MCR |= CAN_MCR_INRQ;
  while (!(CAN->MSR & CAN_MSR_INAK));

  btr_reg &= ~(CAN_BTR_LBKM | CAN_BTR_SILM);
  switch (mode)
  {
    case OpenMode::Normal: break;
    case OpenMode::LoopBack: btr_reg |= CAN_BTR_LBKM; break;
    case OpenMode::ListenOnly: btr_reg |= CAN_BTR_SILM; break;
  }
  CAN->BTR = btr_reg;

  CAN->MCR &= ~(uint32_t)CAN_MCR_INRQ;
  while (CAN->MSR & CAN_MSR_INAK);
  
  CAN->MCR &= ~CAN_MCR_SLEEP;

  CAN->IER |= CAN_IER_FMPIE0;
  NVIC_SetPriority(CEC_CAN_IRQn, 1);
  NVIC_EnableIRQ(CEC_CAN_IRQn);
  
  isopen = true;
  timled.link(true);
  return  Status::Ok;
}


Status CANbus::close (void)
{
  // FIXME: check busy?
  CAN->MCR |= CAN_MCR_SLEEP;
  NVIC_DisableIRQ(CEC_CAN_IRQn);

  isopen = false;
  timled.link(false);
  return  Status::Ok;
}


Status CANbus::timestamp (bool state)
{
  timestamping = state;
  return Status::Ok;
}


bool CANbus::timestamp (void)
{
  return timestamping;
}


Status CANbus::send (TxMsg &msg)
{
  Status result = Status::Error;
  uint32_t mb;
  mb = (CAN->TSR & CAN_TSR_CODE) >> 24; // TODO: check it
  
//  if (CAN->TSR & CAN_TSR_TME0)
//    mb = 0;
//  else if (CAN->TSR & CAN_TSR_TME1)
//    mb = 1;
//  else if (CAN->TSR & CAN_TSR_TME2)
//    mb = 2;
//  else
//    mb = 3;

  if (mb != 3)
  {
    CAN->sTxMailBox[mb].TDTR = msg.DLC;
    CAN->sTxMailBox[mb].TDLR = msg.Data32[0];
    CAN->sTxMailBox[mb].TDHR = msg.Data32[1];

    uint32_t tmp;
    tmp = msg.Id << (msg.IDE ? 3 : 21);
    tmp |= msg.IDE ? CAN_TI0R_IDE : 0;
    tmp |= msg.RTR ? CAN_TI0R_RTR : 0;
    CAN->sTxMailBox[mb].TIR = tmp | CAN_TI0R_TXRQ;
    
    timled.tx_blink(5);
    result = Status::Ok;
  }

  return result;
}


Status CANbus::set_rx_cb(RxCallback cb)
{
  Status result = Status::Error;
  if (cb)
  {
    rx_cb = cb;
    result = Status::Ok;
  }
  return result;
}


void CEC_CAN_IRQHandler (void)
{
  
  // TODO: add error handling

  if (rx_cb && (CAN->RF0R & CAN_RF0R_FMP0))
  {
    RxMsg msg;
    msg.Time = timled.value();
    msg.IDE = (CAN->sFIFOMailBox[0].RIR & CAN_RI0R_IDE) ? true : false;
    msg.Id = CAN->sFIFOMailBox[0].RIR >> ((msg.IDE) ? 3 : 21);
    msg.RTR = (CAN->sFIFOMailBox[0].RIR & CAN_RI0R_RTR) ? true : false;
    msg.DLC = CAN->sFIFOMailBox[0].RDTR & CAN_RDT0R_DLC;
    msg.Data32[0] = CAN->sFIFOMailBox[0].RDLR;
    msg.Data32[1] = CAN->sFIFOMailBox[0].RDHR;

    CAN->RF0R |= CAN_RF0R_RFOM0;
    timled.rx_blink(5);
    rx_cb(msg);
  }
}



