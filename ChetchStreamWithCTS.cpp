#include <ChetchStreamWithCTS.h>

namespace Chetch{

    StreamWithCTS::StreamWithCTS(int receiveBufferSize, int sendBufferSize, int uartBufferSize){
      _rbuffer = new byte[receiveBufferSize];
      _sbuffer = new byte[sendBufferSize];

      receiveBuffer = RingBuffer(_rbuffer, receiveBufferSize);
      sendBuffer = RingBuffer(_sbuffer, sendBufferSize);

      this->uartBufferSize = uartBufferSize;
      cts = true;
    }

    StreamWithCTS::~StreamWithCTS(){
      if(_rbuffer != NULL)delete[] _rbuffer;
      if(_sbuffer != NULL)delete[] _sbuffer;
    }

    void StreamWithCTS::begin(Stream *stream){
      this->stream = stream;
      reset();
    }

    void StreamWithCTS::reset(){
      receiveBuffer.reset();
      sendBuffer.reset();
      bytesReceived = 0;
      bytesSent = 0;
      cts = true;
    }


    byte StreamWithCTS::readFromStream(){
	bytesReceived++;
	return stream->read();
    }

    void StreamWithCTS::writeToStream(byte b){
	bytesSent++;
	stream->write(b);
    }

    int StreamWithCTS::dataAvailable(){
	return stream->available();
    }

    void StreamWithCTS::receive(){
      while(dataAvailable() > 0){

	byte b = readFromStream();
	if(slashed){
	  slashed = false;
	} else {
	  switch(b){
	    case SLASH_BYTE:
	      Serial.println(">>>>>> Received Slashed");
	      slashed = true;
	      continue;
	   
	    case CTS_BYTE:
	      Serial.println(">>>>>> Received CTS ");
	      cts = true;
	      bytesSent = 0;
	      continue;

	    default:
	      break;
	  }
	
	}

	if(receiveBuffer.isFull() && b != CTS_BYTE){
	  Serial.print("ERROR: Oh no receive buffer full and non cts byte: "); Serial.println(b);
	  error = 1;
	  break;
	}

	receiveBuffer.write(b);
	Serial.print("Received: "); Serial.println(b); 
      } //end data available loop

      if(bytesReceived >= uartBufferSize && receiveBuffer.remaining() >= uartBufferSize){
	Serial.print(" / "); Serial.print(bytesReceived); Serial.print(" / "); Serial.println(receiveBuffer.remaining());
      	writeToStream(CTS_BYTE);
	bytesReceived = 0;
	Serial.println("<<<<<<< Sending CTS");
      }
    }

    void StreamWithCTS::send(){
      while(!sendBuffer.isEmpty() && cts){
	byte b = sendBuffer.read();
	switch(b){
	  case CTS_BYTE:
	  case SLASH_BYTE:
   	    writeToStream(SLASH_BYTE);
	    Serial.println("Adding slash byte");
	    break;
	}
	Serial.print("Sent: "); Serial.println(b);
	writeToStream(b);
	if(bytesSent >= uartBufferSize){
          cts = false;
        }
      }
    }

    bool StreamWithCTS::canRead(){
      return !receiveBuffer.isEmpty();
    }

    bool StreamWithCTS::canWrite(){
	return !sendBuffer.isFull();
    }

    byte StreamWithCTS::read(){
      return receiveBuffer.read();
    }

    bool StreamWithCTS::write(byte b){
      return sendBuffer.write(b);
    }

    bool StreamWithCTS::write(byte *bytes, int size){
      return sendBuffer.write(bytes, size);
    }

    bool StreamWithCTS::isClearToSend(){
      return cts;
    }
} //end namespace