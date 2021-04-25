#ifndef CHETCH_STREAM_WITH_CTS_H
#define CHETCH_STREAM_WITH_CTS_H

#include <Arduino.h>
#include "ChetchRingBuffer.h"


namespace Chetch{

class StreamWithCTS{

  protected:
    Stream *stream;
    byte *_rbuffer = NULL;
    byte *_sbuffer = NULL;
    RingBuffer receiveBuffer;
    RingBuffer sendBuffer;
    int uartBufferSize = 0;
    bool cts = false;
    unsigned long bytesSent = 0;
    unsigned long bytesReceived = 0;

    //these methods allow inheriting from this class and using an object other than one derived from Stream
    virtual byte readFromStream();
    virtual void writeToStream(byte b);
    virtual bool streamIsAvailable();  
    

  public:
    static const byte CTS_BYTE = (byte)0x62;

   
    StreamWithCTS(int receiveBufferSize, int sendBufferSize, int uartBufferSize);
    ~StreamWithCTS();

    void begin(Stream *stream);
    bool receive();
    bool send();
    bool isEmpty();
    byte read();
    bool write(byte b);
    bool write(byte *bytes, int size);
    bool isClearToSend();
};

}
#endif