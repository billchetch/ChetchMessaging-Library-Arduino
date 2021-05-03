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
    RingBuffer sendBuffer;
    int uartBufferSize = 0;
    bool cts = false;
    bool slashed = false;
    unsigned long bytesSent = 0;
    unsigned long bytesReceived = 0;

    //these methods allow inheriting from this class and using an object other than one derived from Stream
    virtual byte readFromStream();
    virtual void writeToStream(byte b);
    virtual int dataAvailable();  
    

  public:
    static const byte CTS_BYTE = (byte)0x62;
    static const byte SLASH_BYTE = (byte)0x5c;

	RingBuffer receiveBuffer;
	int error = 0;
    
   
    StreamWithCTS(int receiveBufferSize, int sendBufferSize, int uartBufferSize);
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
};

}
#endif