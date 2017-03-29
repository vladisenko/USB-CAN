#ifndef _CAN_HPP_
#define _CAN_HPP_

namespace CANbus
{
  template<uint32_t SJW, uint32_t TS2, uint32_t TS1, uint32_t BRP>
  constexpr uint32_t timing()
  {
    static_assert (SJW >= 1 && SJW <= 4, "SJW must be from 1 to 4!");
    static_assert (TS2 >= 1 && TS2 <= 8, "TS2 must be from 1 to 8!");
    static_assert (TS1 >= 1 && TS1 <= 16, "TS1 must be from 1 to 16!");
    static_assert (BRP >= 1 && BRP <= 1024, "BRP must be from 1 to 1024!");
    return (SJW - 1) << 24 | (TS2 - 1) << 20 | (TS1 -1) << 16 | (BRP - 1);
  }

  enum class Bitrate : uint32_t
  {
    br10kbit =  timing<1, 7, 8, 300>(),
    br20kbit =  timing<1, 7, 8, 150>(),
    br50kbit =  timing<1, 7, 8, 60>(),
    br100kbit = timing<1, 7, 8, 30>(),
    br125kbit = timing<1, 7, 8, 24>(),
    br250kbit = timing<1, 7, 8, 12>(),
    br500kbit = timing<1, 7, 8, 6>(),
    br800kbit = timing<1, 5, 6, 5>(),
    br1Mbit =   timing<1, 7, 8, 3>()
  };
  enum class Status : uint8_t     { Ok, Error };
  enum class OpenMode : uint8_t   { Normal, LoopBack, ListenOnly };

  typedef struct
  {
    uint32_t Id;
    uint32_t DLC;
    union 
    {
      uint8_t  Data8[8];
      uint32_t Data32[2];
    };
    bool IDE;
    bool RTR;
  } TxMsg;
  
  typedef struct
  {
    uint32_t Id;
    uint32_t DLC;
    union 
    {
      uint8_t  Data8[8];
      uint32_t Data32[2];
    };
    uint16_t Time;
    bool IDE;
    bool RTR;
  } RxMsg;

  typedef void (*RxCallback) (CANbus::RxMsg &msg);
  
  Status init (void);
  Status bitrate (Bitrate br);
  Status bitrate (uint32_t btr); 
  Status open (OpenMode mode);
  Status close (void);
  Status send (TxMsg &msg);
  Status set_rx_cb(RxCallback cb);
  Status filtermask (uint32_t msk);
  Status filtercode (uint32_t code);
  Status timestamp (bool state);
  bool timestamp (void); 
};
#endif // _CAN_HPP_
