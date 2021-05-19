#ifndef CHETCH_STREAM_WITH_CTS_H
#define CHETCH_STREAM_WITH_CTS_H

#include <Arduino.h>
#include "ChetchRingBuffer.h"


namespace Chetch{

class StreamWithCTS{

  public:
    Stream *stream;
    unsigned int uartBufferSize = 0;
    RingBuffer *sendBuffer = NULL;
    RingBuffer *receiveBuffer = NULL;
    RingBuffer *dataBuffer = NULL;
    bool cts = false;
    unsigned long bytesSent = 0;
    unsigned long bytesReceived = 0;
    bool rslashed = false;
    int endOfDataCount = 0;
    
    //command byte callback
    void (*commandCallback)(StreamWithCTS*, byte);
    void (*dataHandler)(StreamWithCTS*, RingBuffer*, bool);


    //these methods allow inheriting from this class and using an object other than one derived from Stream
    virtual byte readFromStream(bool count = true);
    virtual void writeToStream(byte b, bool count = true, bool flush = false);
    virtual byte peekAtStream();
    virtual int dataAvailable();  
    

  public:
    static const byte ERROR_BYTE = (byte)0x21;
    static const byte CTS_BYTE = (byte)0x62;
    static const byte SLASH_BYTE = (byte)0x5c;
    static const byte RESET_BYTE = (byte)0x63;
    static const byte END_BYTE = (byte)0x64;

    int error = 0;
    
   
    StreamWithCTS(unsigned int uartBufferSize, unsigned int receiveBufferSize = 0, unsigned int sendBufferSize = 0);
    ~StreamWithCTS();

    void begin(Stream *stream, void (*callback)(StreamWithCTS*, byte) = NULL);
    bool setDataHandler(void (*handler)(StreamWithCTS*, RingBuffer*, bool), int size);
    void receive();
    void process();
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