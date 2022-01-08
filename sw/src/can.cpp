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
static bool disable_auto_retx = false;
static CANbus::RxCallback rx_cb = nullptr;
static CANbus::ErrCallback err_cb = nullptr;
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
  CAN->MCR = CAN_MCR_SLEEP | CAN_MCR_ABOM | CAN_MCR_TXFP;

  CAN->FMR |= CAN_FMR_FINIT; 
  CAN->FM1R = 0;                    // 0: Two 32-bit registers of filter bank x are in Identifier Mask mode.
  CAN->FS1R = CAN_FS1R_FSC0;        // 1: Single 32-bit scale configuration
  CAN->FFA1R = 0;                   // 0: Filter assigned to FIFO 0
  CAN->FA1R = CAN_FA1R_FACT0;       // 1: Filter is active
  CAN->sFilterRegister[0].FR1 = 0;  // ID
  CAN->sFilterRegister[0].FR2 = 0;  // MASK  
  CAN->FMR &= ~(uint32_t)CAN_FMR_FINIT;

  timled.init();

  return Status::Ok;
}


Status CANbus::filtermask (uint32_t msk)
{
  CAN->FMR |= CAN_FMR_FINIT; 
  CAN->sFilterRegister[0].FR2 = ~msk; // MASK  
  CAN->FMR &= ~(uint32_t)CAN_FMR_FINIT;
  return Status::Ok;
}


Status CANbus::filtercode (uint32_t code)
{
  CAN->FMR |= CAN_FMR_FINIT; 
  CAN->sFilterRegister[0].FR1 = code; // ID  
  CAN->FMR &= ~(uint32_t)CAN_FMR_FINIT;
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
  /* Force a master reset of the bxCAN -> Sleep mode activated after reset */
  CAN->MCR |= CAN_MCR_RESET;
  while(CAN->MCR & CAN_MCR_RESET);

  /* Request the CAN hardware to enter initialization mode */
  CAN->MCR |= CAN_MCR_INRQ;
  while (!(CAN->MSR & CAN_MSR_INAK));

  /* Initialize CAN */
  CAN->MCR |= CAN_MCR_ABOM | CAN_MCR_TXFP;
  if (disable_auto_retx)
  {
    CAN->MCR |= CAN_MCR_NART;
  }
  else
  {
    CAN->MCR &= ~CAN_MCR_NART;
  }

  /* Set CAN mode */
  btr_reg &= ~(CAN_BTR_LBKM | CAN_BTR_SILM);
  switch (mode)
  {
    case OpenMode::Normal: break;
    case OpenMode::LoopBack: btr_reg |= CAN_BTR_LBKM; break;
    case OpenMode::ListenOnly: btr_reg |= CAN_BTR_SILM; break;
  }
  CAN->BTR = btr_reg;

  /* Switch the hardware into normal mode */
  CAN->MCR &= ~(uint32_t)CAN_MCR_INRQ;
  while (CAN->MSR & CAN_MSR_INAK);

  /* Clear Error Interrupt flag */
  CAN->MSR = CAN_MSR_ERRI;

  /* Enable Interrupt */
  CAN->IER |= CAN_IER_FMPIE0 |  /* Interrupt generated when state of FMP[1:0] bits are not 00b (FIFO0 is not empty) */
              CAN_IER_LECIE  |  /* ERRI bit will be set when the error code in LEC[2:0] is set by hardware on error detection */
              CAN_IER_ERRIE;    /* An interrupt will be generation when an error condition is pending in the CAN_ESR */

  /* exit Sleep mode */
  CAN->MCR &= ~(uint32_t)CAN_MCR_SLEEP;

  NVIC_SetPriority(CEC_CAN_IRQn, 1);
  NVIC_EnableIRQ(CEC_CAN_IRQn);
  
  isopen = true;
  timled.link(true);
  return Status::Ok;
}


Status CANbus::close (void)
{
  /* Request Sleep mode */
  CAN->MCR |= CAN_MCR_SLEEP;
  while (!(CAN->MSR & CAN_MSR_SLAK));

  /* Abort request for all mailbox */
  CAN->TSR = CAN_TSR_ABRQ0 | CAN_TSR_ABRQ1 | CAN_TSR_ABRQ2;

  NVIC_DisableIRQ(CEC_CAN_IRQn);

  isopen = false;
  timled.link(false);
  return Status::Ok;
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


Status CANbus::disableAutoRetransm(bool state)
{
  disable_auto_retx = state;
  return Status::Ok;
}


Status CANbus::send (TxMsg &msg)
{
  Status result = Status::Error;

  if (CAN->TSR & (CAN_TSR_TME))
  {
    uint32_t mb = (CAN->TSR & CAN_TSR_CODE) >> 24;

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
  if (cb != nullptr)
  {
    rx_cb = cb;
    result = Status::Ok;
  }
  return result;
}


Status CANbus::set_err_cb(ErrCallback cb)
{
  Status result = Status::Error;
  if (cb != nullptr)
  {
    err_cb = cb;
    result = Status::Ok;
  }
  return result;
}


void CEC_CAN_IRQHandler (void)
{
  /* Check error */
  if (CAN->MSR & CAN_MSR_ERRI)
  {
    /* Clear ERRI flag */
    CAN->MSR = CAN_MSR_ERRI;

    /* Get error code */
    uint8_t lec = (CAN->ESR & CAN_ESR_LEC) >> 4;

    if (err_cb != nullptr)
    {
      err_cb(lec, timled.value());
    }
  }

  /* Check new message */
  if (CAN->RF0R & CAN_RF0R_FMP0)
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
    if (rx_cb != nullptr)
    {
      rx_cb(msg);
    }
  }
}
