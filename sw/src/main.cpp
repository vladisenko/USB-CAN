#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "stm32f0xx.h"

#include "can.hpp"
#include "fifo.hpp"

extern "C" 
{
#include "usbd_cdc_core.h"
#include "usbd_cdc_vcp.h"
#include "usbd_usr.h"
};


enum {
  GetVersionSW    = 'v',
  GetVersionHW    = 'V',
  GetStatus       = 'F',
  OpenCAN         = 'O',
  OpenCANLoopback = 'l',
  OpenCANListen   = 'L',
  CloseCAN        = 'C',
  SetTimestamping = 'Z',
  SetBitrate      = 'S',
  SetBitrateCustom= 's',
  SetFilterMask   = 'm',
  SetFilterCode   = 'M',
  SendStd         = 't',
  SendStdRTR      = 'r',
  SendExt         = 'T',
  SendExtRTR      = 'R'
};


USB_CORE_HANDLE  USB_Device_dev;

FIFO<uint8_t, 4096> rxfifo; 
FIFO<uint8_t, 4096> txfifo;


uint16_t VCP_callback(uint8_t* Buf, uint32_t Len)
{
  while (Len--) rxfifo.push(*Buf++); // TODO: control overflow!
  return USBD_OK;
}


inline void VCP_PutStr (const char* str)
{
  VCP_DataTx((uint8_t*)str, strlen(str));
}


inline CANbus::Bitrate GetBitrate (uint8_t ch)
{
  switch(ch)
  {
    case '0': return CANbus::Bitrate::br10kbit; 
    case '1': return CANbus::Bitrate::br20kbit; 
    case '2': return CANbus::Bitrate::br50kbit; 
    case '3': return CANbus::Bitrate::br100kbit; 
    case '4': return CANbus::Bitrate::br125kbit; 
    case '5': return CANbus::Bitrate::br250kbit; 
    case '6': return CANbus::Bitrate::br500kbit; 
    case '7': return CANbus::Bitrate::br800kbit; 
    case '8': default: return CANbus::Bitrate::br1Mbit; 
  }
}  


inline uint8_t char_to_hex (char c)
{
  uint8_t res;
  if (isdigit(c))
    res = c - '0';
  else if (isxdigit(c))
    res = tolower(c) - 'a' + 10;
  else
    res = 0xFF;
  return res;
}


inline char hex_to_char (uint8_t hex)
{
  char res;
  hex &= 0x0F;
  if (hex < 10)
    res = hex + '0';
  else
    res = hex + 'a' - 10;
  return res;
}


inline uint32_t get_word (void)
{
  uint8_t tmp;
  uint8_t i = 8;
  uint32_t result = 0;
  do
  {
    while (false == rxfifo.pop(tmp)){};
    result = (result << 4) | char_to_hex(tmp);
  } while ( tmp != '\r' && --i);
  return  result;
}


inline uint32_t GetBitrateCustom (void)
{
  uint8_t tmp;
  uint8_t i = 4;
  uint32_t btr = 0;

  do
  {
    while (false == rxfifo.pop(tmp)){};
    btr = (btr << 4) | char_to_hex(tmp);
  } while ( tmp != '\r' && --i);
  btr = (btr & 0x01FF) | ((btr & 0xFE00) << 7);
  return  btr;
}


CANbus::Status SebdCANMsg (uint8_t type)
{
  CANbus::TxMsg msg;
  msg.Id = 0;
  msg.IDE = (type == 'T' || type == 'R') ? true : false; 
  msg.RTR = (type == 'r' || type == 'R') ? true : false;
  memset(msg.Data8, 0, sizeof(msg.Data8));

  uint32_t idx = 0;

  for (uint32_t i = (msg.IDE) ? 0 : 5; true; i++) // if STD use only 3 chars for ID
  {
    uint8_t tmp;
    while (false == rxfifo.pop(tmp)){};
    if (tmp == '\r') return CANbus::Status::Error; // FIXME: rewrite this 
    if (i < 8)                            // get ID
    {
      msg.Id = (msg.Id << 4) | char_to_hex(tmp);
    }
    else if (i < 9)                       // get DLC
    {
      msg.DLC = char_to_hex(tmp);
      if (msg.IDE) break;                 // if this is remote frame no need send data
    }
    else                                  // get DATA
    {
      msg.Data8[idx >> 1] <<= 4;
      msg.Data8[idx >> 1] |= char_to_hex(tmp);
      if (++idx >= 2*msg.DLC) break;
    }
  }
  return CANbus::send(msg);
}  


void ReceiveCANMsg (CANbus::RxMsg &msg) // FIXME: rewrite function
{
  uint8_t tmp[30];
  uint32_t len = 0;
  
  if (msg.IDE)  // if Ext frame
  {
    tmp[len++] = (msg.RTR) ? 'R' : 'T';
    tmp[len++] = hex_to_char(msg.Id >> 28); 
    tmp[len++] = hex_to_char(msg.Id >> 24);
    tmp[len++] = hex_to_char(msg.Id >> 20);    
    tmp[len++] = hex_to_char(msg.Id >> 16); 
    tmp[len++] = hex_to_char(msg.Id >> 12);
    tmp[len++] = hex_to_char(msg.Id >> 8); 
    tmp[len++] = hex_to_char(msg.Id >> 4);
    tmp[len++] = hex_to_char(msg.Id >> 0); 
  }
  else
  {
    tmp[len++] = (msg.RTR) ? 'r' : 't';
    tmp[len++] = hex_to_char(msg.Id >> 8); 
    tmp[len++] = hex_to_char(msg.Id >> 4);
    tmp[len++] = hex_to_char(msg.Id >> 0);
  }

  tmp[len++] = hex_to_char(msg.DLC >> 0);
  uint16_t i = 0;
  while (!msg.RTR && i < msg.DLC)
  {
  tmp[len++] = hex_to_char(msg.Data8[i] >> 4);
  tmp[len++] = hex_to_char(msg.Data8[i] >> 0);
  i++;
  }

  if (CANbus::timestamp())
  {
    tmp[len++] = hex_to_char( msg.Time >> 12);
    tmp[len++] = hex_to_char( msg.Time >> 8);
    tmp[len++] = hex_to_char( msg.Time >> 4);
    tmp[len++] = hex_to_char( msg.Time >> 0);
  }

  tmp[len++] = '\r';	
 
  for (uint8_t i = 0; i < len; i++)
  {
    txfifo.push(tmp[i]);
  }
}


/*********************************************************************
*
*       main()
*
*  Function description
*   Application entry point.
*/
int main(void)
{
  
  RCC->AHBENR |= RCC_AHBENR_GPIOBEN;
  GPIOB->MODER &= ~GPIO_MODER_MODER10;
  GPIOB->MODER |= GPIO_MODER_MODER10_0; // output

  CANbus::init();
  CANbus::set_rx_cb(ReceiveCANMsg);

  USBD_Init(&USB_Device_dev, &USR_desc, &USBD_CDC_cb, &USR_cb);
  
  while (1)
  {
    if (!rxfifo.empty())
    {
      CANbus::Status st = CANbus::Status::Error;
      uint8_t tmp;
      rxfifo.pop(tmp);
      
      switch (tmp)
      {
        case GetVersionSW: VCP_PutStr("vSTM32"); st = CANbus::Status::Ok; break;
        case GetVersionHW: VCP_PutStr("V0102"); st = CANbus::Status::Ok; break;
        case GetStatus:    VCP_PutStr("F00"); st = CANbus::Status::Ok; break;      // TODO: add response
        case OpenCAN:         st = CANbus::open(CANbus::OpenMode::Normal); break;
        case OpenCANLoopback: st = CANbus::open(CANbus::OpenMode::LoopBack); break;
        case OpenCANListen:   st = CANbus::open(CANbus::OpenMode::ListenOnly); break;
        case CloseCAN:        st = CANbus::close(); break;       
        case SetTimestamping: 
          while (false == rxfifo.pop(tmp)){};
          st = CANbus::timestamp((tmp=='0') ? false : true);
          break;
        case SetBitrate:
          while (false == rxfifo.pop(tmp)){};
          st = CANbus::bitrate(GetBitrate(tmp));
          break;
        case SetBitrateCustom:
          st = CANbus::bitrate(GetBitrateCustom());
          break;
        case SendStd: case SendStdRTR:
          st = SebdCANMsg(tmp);
          if (st==CANbus::Status::Ok) VCP_PutStr("z");
          break;		
        case SendExt: case SendExtRTR:
          st = SebdCANMsg(tmp);
          if (st==CANbus::Status::Ok) VCP_PutStr("Z");
          break;		
        case SetFilterCode:
          st = CANbus::filtercode(get_word());
          break;
        case SetFilterMask:
          st = CANbus::filtermask(get_word());
          break;
        default:
          st = CANbus::Status::Ok;
          break;
      }
      VCP_PutStr ((st==CANbus::Status::Ok) ? "\r" : "\a");
    }
  
    while (!txfifo.empty())
    {
      uint8_t tmp;
      txfifo.pop(tmp);
      VCP_DataTx(&tmp, 1);
    }
    
  }
}

/*************************** End of file ****************************/
