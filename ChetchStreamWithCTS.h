#ifndef CHETCH_STREAM_WITH_CTS_H
#define CHETCH_STREAM_WITH_CTS_H

#include <Arduino.h>
#include "ChetchRingBuffer.h"


namespace Chetch{

class StreamWithCTS{

  protected:
    Stream *stream;
    byte *rbuffer = NULL;
    byte *sbuffer = NULL;
    unsigned int uartBufferSize = 0;
    bool cts = false;
    bool slashed = false;
    unsigned long bytesSent = 0;
    unsigned long bytesReceived = 0;

    //these methods allow inheriting from this class and using an object other than one derived from Stream
    virtual byte readFromStream();
    virtual void writeToStream(byte b, bool flush = false);
    virtual int dataAvailable();  
    

  public:
    static const byte CTS_BYTE = (byte)0x62;
    static const byte SLASH_BYTE = (byte)0x5c;

	RingBuffer sendBuffer;

	RingBuffer receiveBuffer;
	int error = 0;
    
   
    StreamWithCTS(unsigned int uartBufferSize, unsigned int receiveBufferSize = 0, unsigned int sendBufferSize = 0);
    ~StreamWithCTS();

    void begin(Stream *stream);
    void receive();
    void send();
    bool canRead();
    bool canWrite();
    byte read();
    bool write(byte b);
    bool write(byte *bytes, int size);
    bool isClearToSend();
    void reset();
    unsigned int getUartBufferSize();
    void printVitals();
};

}
#endif