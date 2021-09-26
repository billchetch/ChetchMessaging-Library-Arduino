#ifndef CHETCH_STREAM_FLOW_CONTROLLER_H
#define CHETCH_STREAM_FLOW_CONTROLLER_H

#include <Arduino.h>
#include "ChetchRingBuffer.h"


namespace Chetch{

class StreamFlowController{

  public: //TODO: change to private
    Stream *stream;
    unsigned int uartLocalBufferSize = 0;
    unsigned int uartRemoteBufferSize = 0;
    RingBuffer *sendBuffer = NULL;
    RingBuffer *receiveBuffer = NULL;
    bool localReset = false;
    bool remoteReset = false;
    bool cts = true;
    int ctsTimeout = -1; //wait for cts timeout
    bool remoteRequestedCTS = false;
    unsigned long lastRemoteCTSRequest = 0;
    bool localRequestedCTS = false;
    unsigned long lastCTSrequired = 0; //the last time cts flag set to false (i.e requires remoe to send a CTS byte)
    unsigned int bytesSent = 0;
    unsigned int bytesReceived = 0;
    int maxDatablockSize = NO_LIMIT;
    bool rslashed = false;
    bool rcommand = false;
    bool revent = false;
    bool sslashed = false;
    bool smarked = false;
   
  protected:  
    //callbacks
    void (*commandHandler)(StreamFlowController*, byte);
    bool (*localEventHandler)(StreamFlowController*, byte);
    void (*remoteEventHandler)(StreamFlowController*, byte);
    void (*dataHandler)(StreamFlowController*, bool);
    void (*receiveHandler)(StreamFlowController*, int);
    bool (*readyToReceiveHandler)(StreamFlowController*, bool);
    void (*sendHandler)(StreamFlowController*, int);


    //these methods allow inheriting from this class and using an object other than one derived from Stream
    virtual byte readFromStream(bool count = true);
    virtual void writeToStream(byte b, bool count = true, bool flush = false);
    virtual byte peekAtStream();
    virtual int dataAvailable();  
    

  public:
    static const byte CTS_BYTE = (byte)0x74; //116
    static const byte SLASH_BYTE = (byte)0x5c; //92
    static const byte PAD_BYTE = (byte)0x70; //112
    static const byte END_BYTE = (byte)0x64; //100
    //static const byte RESET_BYTE = (byte)0x63; //99
    static const byte COMMAND_BYTE = (byte)0x63; //99
    static const byte EVENT_BYTE = (byte)0x73; //115

    static const byte RESERVED_BUFFER_SIZE = 2;

    static const int NO_LIMIT = -1; 
    static const int UART_LOCAL_BUFFER_SIZE = -2;
    static const int UART_REMOTE_BUFFER_SIZE = -3;
    static const int RECEIVE_BUFFER_SIZE = -4;
    static const int SEND_BUFFER_SIZE = -5;
    static const int MAX_DATABLOCK_SIZE = -6;
    static const int CTS_LOCAL_LIMIT = -7;
    static const int CTS_REMOTE_LIMIT = -8;
    
    
    enum class Command{
        RESET = 1,
        DEBUG_ON = 2,
        DEBUG_OFF = 3,
        RESET_RECEIVE_BUFFER = 4,
        RESET_SEND_BUFFER = 5,
        PING = 6,
        REQUEST_STATUS = 100, 
        
    };

    enum class Event {
        RESET = 1,
	    RECEIVE_BUFFER_FULL = 2,
        RECEIVE_BUFFER_OVERFLOW_ALERT = 3,
        SEND_BUFFER_FULL = 4,
        SEND_BUFFER_OVERFLOW_ALERT = 5,
        CHECKSUM_FAILED = 6,
        UNKNOWN_ERROR = 7,
        ALL_OK = 8,
        CTS_TIMEOUT = 9,
        MAX_DATABLOCK_SIZE_EXCEEDED = 10,
        CTS_REQUEST_TIMEOUT = 11,
        PING_RECEIVED = 12,
    };
    
    int error = 0;
    
    StreamFlowController(unsigned int uartLocalBufferSize, unsigned int uartRemotelBufferSize, unsigned int receiveBufferSize = 0, unsigned int sendBufferSize = 0);
    ~StreamFlowController();

    void begin(Stream *stream);
    void setCommandHandler(void (*handler)(StreamFlowController*, byte)); //this stream, the command byte
    void setEventHandlers(bool (*handler1)(StreamFlowController*, byte), void (*handler2)(StreamFlowController*, byte));  //this stream, the event byte
    void setDataHandler(void (*handler)(StreamFlowController*, bool));
    void setReceiveHandler(void (*handler)(StreamFlowController*, int));
    void setReadyToReceiveHandler(bool (*handler)(StreamFlowController*, bool));
    void setSendHandler(void (*handler)(StreamFlowController*, int));
    void setCTSTimeout(int ms); //in millis
    void setMaxDatablockSize(int max);
    bool isReady();
    void reset(bool sendCommandByte);
    void receive();
    void process();
    void send();
    void loop();
    void handleData(bool endOfData);
    bool readyToReceive(bool request4cts);
    bool canReceive(int byteCount = 1); //-ve value checks if receive buffer is empty or uart buffer size
    bool canSend(int byteCount = 1); //-ve value checks if send buffer is empty or uart buffer size
    bool sendCTS(bool overrideFlowControl = false);
    void sendCommand(Command c);
    void sendCommand(byte b);
    void sendEvent(Event e);
    void sendEvent(byte b);
    void ping();
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
    int getLimit(int limit);
    unsigned int getBytesReceived();
    unsigned int getBytesSent();
    void printVitals();    
    void dumpLog();
};

}
#endif