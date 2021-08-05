#ifndef CHETCH_STREAM_WITH_CTS_H
#define CHETCH_STREAM_WITH_CTS_H

#include <Arduino.h>
#include "ChetchRingBuffer.h"


namespace Chetch{

class StreamWithCTS{

  public:
    Stream *stream;
    unsigned int uartLocalBufferSize = 0;
    unsigned int uartRemoteBufferSize = 0;
    RingBuffer *sendBuffer = NULL;
    RingBuffer *receiveBuffer = NULL;
    bool cts = true;
    unsigned int bytesSent = 0;
    unsigned int bytesReceived = 0;
    bool rslashed = false;
    bool revent = false;
    bool sslashed = false;
    bool smarked = false;
    
    //callbacks
    void (*resetHandler)(StreamWithCTS*);
    void (*eventHandler)(StreamWithCTS*);
    void (*dataHandler)(StreamWithCTS*, bool);
    void (*receiveHandler)(StreamWithCTS*, int);
    void (*sendHandler)(StreamWithCTS*);
    bool (*readyToReceiveHandler)(StreamWithCTS*);


    //these methods allow inheriting from this class and using an object other than one derived from Stream
    virtual byte readFromStream(bool count = true);
    virtual void writeToStream(byte b, bool count = true, bool flush = false);
    virtual byte peekAtStream();
    virtual int dataAvailable();  
    

  public:
    static const byte CTS_BYTE = (byte)0x74; //116
    static const byte SLASH_BYTE = (byte)0x5c; //92
    static const byte PAD_BYTE = (byte)0x70; //112
    static const byte RESET_BYTE = (byte)0x63; //99
    static const byte END_BYTE = (byte)0x64; //100
    static const byte EVENT_BYTE = (byte)0x73; //115

    enum Event {
        RESET = 1,
	    RECEIVE_BUFFER_FULL = 2,
        CHECKSUM_FAILED = 3,
        UNKNOWN_ERROR = 4,
        ALL_OK = 5
	};
    
    int error = 0;
    
   
    StreamWithCTS(unsigned int uartLocalBufferSize, unsigned int uartRemotelBufferSize, unsigned int receiveBufferSize = 0, unsigned int sendBufferSize = 0);
    ~StreamWithCTS();

    void begin(Stream *stream);
    void setResetHandler(void (*handler)(StreamWithCTS*));
    void setEventHandler(void (*handler)(StreamWithCTS*));
    void setDataHandler(void (*handler)(StreamWithCTS*, bool));
    void setReceiveHandler(void (*handler)(StreamWithCTS*, int));
    void setSendHandler(void (*handler)(StreamWithCTS*));
    void setReadyToReceiveHandler(bool (*handler)(StreamWithCTS*));
    void receive();
    void process();
    void send();
    void handleData(bool endOfData);
    bool readyToReceive();
    bool canRead(unsigned int byteCount = 1);
    bool canWrite(unsigned int byteCount = 1);
    bool sendCTS();
    void sendEvent(byte e);
    bool requiresCTS(unsigned int byteCount, unsigned int bufferSize);
    byte read();
    int bytesToRead(bool untilMarker = true);
    bool write(byte b, bool addMarker = false);
    void endWrite(); //sets send buffer marker
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
    
    void printVitals();    
    void dumpLog();
};

}
#endif