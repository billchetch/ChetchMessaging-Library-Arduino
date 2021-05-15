#ifndef CHETCH_RING_BUFFER_H
#define CHETCH_RING_BUFFER_H

#include <Arduino.h>

namespace Chetch{

class RingBuffer{
  public:
    byte *buffer;
    int size;
    bool deleteBuffer = false;
    int readPosition = 0;
    int writePosition = 0;
    bool full = false;
    
    RingBuffer();
    RingBuffer(int size);
    RingBuffer(byte *buffer, int size);
    ~RingBuffer();

    void reset();
    bool write(byte b);
    bool write(byte *bytes, int size);
    byte read();
    byte peek();
    bool isFull();
    bool isEmpty();
    int used();
    int remaining();
    int getSize();
};
} //end namespace
#endif