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
    bool cts = true;
    bool rtr = true;
    unsigned long bytesSent = 0;
    unsigned long bytesReceived = 0;
    bool rslashed = false;
    bool sslashed = false;
    bool smarked = false;
    
    byte readHistory[64];
    int readHistoryIndex = 0;
    int endOfDataCount = 0;
    byte lastPeekedByte = 0;
    byte lastReadByte = 0;
    byte lastReceivedByte = 0;
    byte ctsCount = 0;
    
    
    //allbacks
    void (*commandCallback)(StreamWithCTS*, byte);
    bool (*dataHandler)(StreamWithCTS*, bool);
    bool (*readyToReceive)(StreamWithCTS*);


    //these methods allow inheriting from this class and using an object other than one derived from Stream
    virtual byte readFromStream(bool count = true);
    virtual void writeToStream(byte b, bool count = true, bool flush = false);
    virtual byte peekAtStream();
    virtual int dataAvailable();  
    

  public:
    static const byte ERROR_BYTE = (byte)0x65;
    static const byte CTS_BYTE = (byte)0x74;
    static const byte PING_BYTE = (byte)0x62;
    static const byte SLASH_BYTE = (byte)0x5c;
    static const byte PAD_BYTE = (byte)0x70;
    static const byte RESET_BYTE = (byte)0x63;
    static const byte END_BYTE = (byte)0x64;

    int error = 0;
    
   
    StreamWithCTS(unsigned int uartBufferSize, unsigned int receiveBufferSize = 0, unsigned int sendBufferSize = 0);
    ~StreamWithCTS();

    void begin(Stream *stream, void (*callback)(StreamWithCTS*, byte) = NULL);
    void setCommandCallback(void (*callback)(StreamWithCTS*, byte));
    void setDataHandler(void (*handler)(StreamWithCTS*, bool));
    void setReadyToReceive(bool (*callback)(StreamWithCTS*));
    void receive();
    void process();
    void send();
    bool canRead();
    bool canWrite();
    bool sendCTS();
    bool requiresCTS(unsigned long byteCount);
    byte read();
    bool write(byte b, bool addMarker = false);
    template <typename T> bool write(T t, bool addMarker = false){
        byte b;
		for(int i = 0; i < sizeof(t); i++){
			b = (byte)(t >> 8*i);
			if(!write(b))return false;
		}
		if(addMarker)sendBuffer->setMarker();
		return true;
    };
    bool write(int n, bool addMarker);
    bool write(long n, bool addMarker);
    bool write(byte *bytes, int size, bool addEndMarker = true);
    bool isSystemByte(byte b);
    bool isClearToSend();
    void reset();
    unsigned int getUartBufferSize();
    void printVitals();    
    void dumpLog();
};

}
#endif