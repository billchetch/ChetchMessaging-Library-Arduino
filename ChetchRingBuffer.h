#ifndef CHETCH_RING_BUFFER_H
#define CHETCH_RING_BUFFER_H

#include <Arduino.h>

namespace Chetch{

class RingBuffer{
  public:
    byte *buffer;
    int size;
    int readPosition = 0;
    int writePosition = 0;
    bool full = false;
    
    RingBuffer(byte *buffer, int size);
   
    void reset();
    bool write(byte b);
    byte read();
    bool isFull();
    bool isEmpty();
    int used();
    int remaining();
};
} //end namespace
#endif