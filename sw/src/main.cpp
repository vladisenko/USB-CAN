#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>

#include "stm32f0xx.h"
#include "stm32f0xx_flash.h"
#include "eeprom.h"
#include "xprintf.h"

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
  SendExtRTR      = 'R',
  SetErrorReport  = 'E',
  DisableAutoReTX = 'D',
  Help            = 'h',
  HelpAlt         = '?',
};

typedef struct
{
  union
  {
    uint16_t mode;
    struct
    {
      uint16_t bitrate  : 4;
      uint16_t custom_br : 1;
      uint16_t timestamp : 1;
      uint16_t err_reprt : 1;
      uint16_t dis_aretx : 1;
      uint16_t ee_valid  : 1;
    };
  };
  uint16_t c_bitrate_lo;
  uint16_t c_bitrate_hi;
  uint16_t filter_mask_lo;
  uint16_t filter_mask_hi;
  uint16_t filter_code_lo;
  uint16_t filter_code_hi;
} Settings_t;


USB_CORE_HANDLE  USB_Device_dev;

static Settings_t settings;

FIFO<uint8_t, 4096> rxfifo; 
FIFO<uint8_t, 4096> txfifo;

static const char *help_str = "USB-CAN adapter based on STM32F072. Compiled: " __DATE__ " " __TIME__ "\n"
                        "Command list:\n"
                        "Sn[CR]    - Setup with standard CAN bit-rates where n is 0-8\n"
                        "sxxxx[CR] - Setup custom CAN bit-rates\n"
                        "O[CR]     - Open the CAN channel in normal mode\n"
                        "L[CR]     - Open the CAN channel in listen only mode\n"
                        "l[CR]     - Open the CAN channel in loopback mode\n"
                        "C[CR]     - Close the CAN channel\n"
                        "v[CR]     - Get SW Version number\n"
                        "V[CR]     - Get HW Version number\n"
                        "Zn[CR]    - Sets Time Stamp ON/OFF for received frames only\n"
                        "En[CR]    - Sets Error Reporting ON/OFF\n"
                        "Dn[CR]    - Disable Auto Retransmitting\n"
                        "tiiildd...[CR]      - Transmit a standard (11bit) CAN frame\n"
                        "Tiiiiiiiildd...[CR] - Transmit an extended (29bit) CAN frame\n"
                        "riiil[CR]           - Transmit an standard RTR (11bit) CAN frame\n"
                        "Riiiiiiiil[CR]      - Transmit an extended RTR (29bit) CAN frame\n"
                        "Mxxxxxxxx[CR]       - Sets Acceptance Code Register\n"
                        "mxxxxxxxx[CR]       - Sets Acceptance Mask Register\n";


/* Virtual address defined by the user: 0xFFFF value is prohibited */
uint16_t VirtAddVarTab[NB_OF_VAR] =
{
    offsetof(Settings_t, mode),
    offsetof(Settings_t, c_bitrate_lo),
    offsetof(Settings_t, c_bitrate_hi),
    offsetof(Settings_t, filter_mask_lo),
    offsetof(Settings_t, filter_mask_hi),
    offsetof(Settings_t, filter_code_lo),
    offsetof(Settings_t, filter_code_hi),
    14,
};


static inline CANbus::Bitrate GetBitrate (uint8_t ch);
static inline void VCP_PutStr (const char* str);


static void EE_Load(void)
{
  uint16_t res = 0;

  res |= EE_ReadVariable(offsetof(Settings_t, mode), &settings.mode);
  res |= EE_ReadVariable(offsetof(Settings_t, c_bitrate_lo), &settings.c_bitrate_lo);
  res |= EE_ReadVariable(offsetof(Settings_t, c_bitrate_hi), &settings.c_bitrate_hi);
  res |= EE_ReadVariable(offsetof(Settings_t, filter_mask_lo), &settings.filter_mask_lo);
  res |= EE_ReadVariable(offsetof(Settings_t, filter_mask_hi), &settings.filter_mask_hi);
  res |= EE_ReadVariable(offsetof(Settings_t, filter_code_lo), &settings.filter_code_lo);
  res |= EE_ReadVariable(offsetof(Settings_t, filter_code_hi), &settings.filter_code_hi);

  if (res == 0)
  {
    if (settings.custom_br)
    {
      CANbus::bitrate(((uint32_t)settings.c_bitrate_hi << 16) | settings.c_bitrate_lo);
    }
    else
    {
      CANbus::bitrate(GetBitrate('0'+ settings.bitrate));
    }

    CANbus::timestamp(static_cast<bool>(settings.timestamp));
    CANbus::disableAutoRetransm(static_cast<bool>(settings.dis_aretx));
    CANbus::filtercode(((uint32_t)settings.filter_code_hi << 16) | settings.filter_code_lo);
    CANbus::filtermask(((uint32_t)settings.filter_mask_hi << 16) | settings.filter_mask_lo);
  }
  else
  {
    settings.ee_valid = 0;
  }
}


static void EE_Print(void)
{
  char str[32];

  xsprintf(str,"Current settings (EE%svalid):\n", settings.ee_valid ? " " : " not ");
  VCP_PutStr(str);

  if (settings.custom_br)
  {
    uint32_t tmp = ((uint32_t)settings.c_bitrate_hi << 16) | settings.c_bitrate_lo;
    xsprintf(str, "s%04X\n", (tmp & 0x01FF) | ((tmp & (0xFE00 << 7)) >> 7));
    VCP_PutStr(str);
  }
  else
  {
    xsprintf(str, "S%c\n", '0' + settings.bitrate);
    VCP_PutStr(str);
  }

  xsprintf(str, "Z%c\n", '0' + settings.timestamp);
  VCP_PutStr(str);

  xsprintf(str, "E%c\n", '0' + settings.err_reprt);
  VCP_PutStr(str);

  xsprintf(str, "D%c\n", '0' + settings.dis_aretx);
  VCP_PutStr(str);

  xsprintf(str, "M%08lX\n", ((uint32_t)settings.filter_code_hi << 16) | settings.filter_code_lo);
  VCP_PutStr(str);

  xsprintf(str, "m%08lX\n", ((uint32_t)settings.filter_mask_hi << 16) | settings.filter_mask_lo);
  VCP_PutStr(str);
}


static void EE_SaveFilterCode(uint32_t code)
{
  uint32_t tmp = ((uint32_t)settings.filter_code_hi << 16) | settings.filter_code_lo;

  if (tmp != code)
  {
    settings.filter_code_hi = code >> 16;
    settings.filter_code_lo = code;

    EE_WriteVariable(offsetof(Settings_t, filter_code_lo), settings.filter_code_lo);
    EE_WriteVariable(offsetof(Settings_t, filter_code_hi), settings.filter_code_hi);
  }
}


static void EE_SaveFilterMask(uint32_t mask)
{
  uint32_t tmp = ((uint32_t)settings.filter_mask_hi << 16) | settings.filter_mask_lo;

  if (tmp != mask)
  {
    settings.filter_mask_hi = mask >> 16;
    settings.filter_mask_lo = mask;

    EE_WriteVariable(offsetof(Settings_t, filter_mask_lo), settings.filter_mask_lo);
    EE_WriteVariable(offsetof(Settings_t, filter_mask_hi), settings.filter_mask_hi);
  }
}

static void EE_SaveMode(void)
{
  settings.ee_valid = 1;
  EE_WriteVariable(offsetof(Settings_t, mode), settings.mode);
}


static void EE_SaveBitRate(uint8_t br)
{
  if ((settings.custom_br != 0) || (settings.bitrate != br))
  {
    settings.custom_br = 0;
    settings.bitrate = br;

    EE_SaveMode();
  }
}

static void EE_SaveCusBitRate(uint32_t br)
{
  uint32_t tmp = ((uint32_t)settings.c_bitrate_hi << 16) | settings.c_bitrate_lo;

  if (tmp != br)
  {
    settings.c_bitrate_hi = br >> 16;
    settings.c_bitrate_lo = br;

    EE_WriteVariable(offsetof(Settings_t, c_bitrate_lo), settings.c_bitrate_lo);
    EE_WriteVariable(offsetof(Settings_t, c_bitrate_hi), settings.c_bitrate_hi);
  }

  if (settings.custom_br != 1)
  {
    settings.custom_br = 1;
    EE_SaveMode();
  }
}


static void EE_SaveTimestamp(bool state)
{
  if (settings.timestamp != state)
  {
    settings.timestamp = state;
    EE_SaveMode();
  }
}


static void EE_SaveErrReport(bool state)
{
  if (settings.err_reprt != state)
  {
    settings.err_reprt = state;
    EE_SaveMode();
  }
}


static void EE_SaveDisReTX(bool state)
{
  if (settings.dis_aretx != state)
  {
    settings.dis_aretx = state;
    EE_SaveMode();
  }
}


uint16_t VCP_callback(uint8_t* Buf, uint32_t Len)
{
  while (Len--) rxfifo.push(*Buf++); // TODO: control overflow!
  return USBD_OK;
}


static inline void VCP_PutStr (const char* str)
{
  VCP_DataTx((uint8_t*)str, strlen(str));
}


static inline CANbus::Bitrate GetBitrate (uint8_t ch)
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


static inline uint8_t char_to_hex (char c)
{
  uint8_t res;
  if (isdigit(c))
    res = c - '0';
  else if (c >= 'a' && c <= 'f')
    res = c - 'a' + 10;
  else if (c >= 'A' && c <= 'F')
    res = c - 'A' + 10;
  else
    res = 0xFF;
  return res;
}


static inline char hex_to_char (uint8_t hex)
{
  const char tbl[16] = {'0', '1', '2', '3',
                        '4', '5', '6', '7',
                        '8', '9', 'A', 'B',
                        'C', 'D', 'E', 'F'};
  return tbl[hex & 0x0F];
}


static inline uint32_t get_word (void)
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


static inline uint32_t GetBitrateCustom (void)
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


CANbus::Status SendCANMsg (uint8_t type)
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
      if ((msg.RTR) || (msg.DLC == 0)) break; // if this is remote frame or zero data length, no need to send data
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
  uint8_t tmp[40];
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


void CanErrorHandler(uint8_t err_code, uint32_t time)
{
  if (settings.err_reprt)
  {
    uint8_t tmp[20];
    uint32_t len = 0;

    tmp[len++] = 'E';
    tmp[len++] = hex_to_char(err_code >> 4);
    tmp[len++] = hex_to_char(err_code >> 0);

    if (CANbus::timestamp())
    {
      tmp[len++] = hex_to_char(time >> 12);
      tmp[len++] = hex_to_char(time >> 8);
      tmp[len++] = hex_to_char(time >> 4);
      tmp[len++] = hex_to_char(time >> 0);
    }

    tmp[len++] = '\r';

    for (uint8_t i = 0; i < len; i++)
    {
      txfifo.push(tmp[i]);
    }
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
  CANbus::set_err_cb(CanErrorHandler);

  FLASH_Unlock();
  EE_Init();
  EE_Load();

  USBD_Init(&USB_Device_dev, &USR_desc, &USBD_CDC_cb, &USR_cb);

  while (1)
  {
    if (!rxfifo.empty())
    {
      CANbus::Status st = CANbus::Status::Error;
      uint8_t tmp;
      rxfifo.pop(tmp);
      bool skip_resp = false;

      switch (tmp)
      {
        case GetVersionSW: VCP_PutStr("vSTM32"); st = CANbus::Status::Ok; break;
        case GetVersionHW: VCP_PutStr("V0112"); st = CANbus::Status::Ok; break;
        case GetStatus:    VCP_PutStr("F00"); st = CANbus::Status::Ok; break;      // TODO: add response
        case OpenCAN:
          txfifo.clear();
          st = CANbus::open(CANbus::OpenMode::Normal);
          break;
        case OpenCANLoopback: st = CANbus::open(CANbus::OpenMode::LoopBack); break;
        case OpenCANListen:   st = CANbus::open(CANbus::OpenMode::ListenOnly); break;
        case CloseCAN:        st = CANbus::close(); break;       
        case SetTimestamping: 
          {
            while (false == rxfifo.pop(tmp)){};
            bool state = (tmp == '1') ? true : false;
            st = CANbus::timestamp(state);
            EE_SaveTimestamp(state);
          }
          break;
        case SetErrorReport:
          {
            while (false == rxfifo.pop(tmp)){};
            bool state = (tmp == '1') ? true : false;
            EE_SaveErrReport(state);
            settings.err_reprt = state;
            st = CANbus::Status::Ok;
          }
          break;
        case DisableAutoReTX:
          {
            while (false == rxfifo.pop(tmp)){};
            bool state = (tmp == '1') ? true : false;
            st = CANbus::disableAutoRetransm(state);
            EE_SaveDisReTX(state);
          }
          break;
        case SetBitrate:
          while (false == rxfifo.pop(tmp)){};
          st = CANbus::bitrate(GetBitrate(tmp));
          EE_SaveBitRate(tmp - '0');
          break;
        case SetBitrateCustom:
          {
            uint32_t br = GetBitrateCustom();
            st = CANbus::bitrate(br);
            EE_SaveCusBitRate(br);
          }
          break;
        case SendStd: case SendStdRTR:
          st = SendCANMsg(tmp);
          if (st==CANbus::Status::Ok) VCP_PutStr("z");
          break;		
        case SendExt: case SendExtRTR:
          st = SendCANMsg(tmp);
          if (st==CANbus::Status::Ok) VCP_PutStr("Z");
          break;		
        case SetFilterCode:
          {
            uint32_t code = get_word();
            st = CANbus::filtercode(code);
            EE_SaveFilterCode(code);
          }
          break;
        case SetFilterMask:
          {
            uint32_t mask = get_word();
            st = CANbus::filtermask(mask);
            EE_SaveFilterMask(mask);
          }
          break;
        case Help:
        case HelpAlt:
          VCP_PutStr(help_str);
          EE_Print();
          /* no break here! */
        default:
          st = CANbus::Status::Ok;
          skip_resp = true;
          break;
      }
      
      if (!skip_resp)
      {
        VCP_PutStr ((st==CANbus::Status::Ok) ? "\r" : "\a");
        skip_resp = false;
      }
    }

    while(1)
    {
      uint8_t tmp;
      if (txfifo.front(tmp) && (VCP_DataTx(&tmp, 1) == 1))
      {
        txfifo.pop(tmp); /* remove char from queue only if it successfully written to VCP */
      }
      else
      {
        break;
      }
    }
  }
}

/*************************** End of file ****************************/
