#ifndef _FIFO_HPP_
#define _FIFO_HPP_

#include <stdlib.h>

template <class T, size_t SIZE>
class FIFO
{
private:
  T buf[SIZE];
  size_t in;
  size_t out;

public:
  FIFO (void) : in(0), out(0) {};
  
  bool empty (void) const { return (in == out); }

  bool full (void) const
  {
    size_t tmp = (in + 1)%SIZE;
    return (tmp == out);
  }
   
  bool push (T& data)
  {
    size_t tmp = (in + 1)%SIZE;
    if (tmp != out)
    {
      buf[tmp] = data;
      in = tmp;
      return true;
    }
    return false;
  }

  bool pop (T& data)
  {
    if (!empty())
    {
      size_t tmp = (out + 1)%SIZE;
      data = buf[tmp];
      out = tmp;
      return true;
    }
    return false;
  }

  bool front (T& data)
  {
    if (!empty())
    {
      size_t tmp = (out + 1)%SIZE;
      data = buf[tmp];
      return true;
    }
    return false;
  }

  void clear (void)
  {
    in = out = 0;
  }

};

#endif // _FIFO_HPP_
