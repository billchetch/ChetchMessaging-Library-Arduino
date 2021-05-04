#include <ChetchStreamWithCTS.h>

namespace Chetch{

    StreamWithCTS::StreamWithCTS(unsigned int uartBufferSize, unsigned int receiveBufferSize, unsigned int sendBufferSize){

      this->uartBufferSize = uartBufferSize;

      if(receiveBufferSize == 0) receiveBufferSize = 2*receiveBufferSize;
      if(sendBufferSize == 0) sendBufferSize = 2*uartBufferSize;

      rbuffer = new byte[receiveBufferSize];
      sbuffer = new byte[sendBufferSize];

      receiveBuffer = RingBuffer(rbuffer, receiveBufferSize);
      sendBuffer = RingBuffer(sbuffer, sendBufferSize);

      
      cts = true;
    }

    StreamWithCTS::~StreamWithCTS(){
      if(rbuffer != NULL)delete[] rbuffer;
      if(sbuffer != NULL)delete[] sbuffer;
    }

    unsigned int StreamWithCTS::getUartBufferSize(){ 
      return uartBufferSize; 
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


    void StreamWithCTS::printVitals(){
	Serial.print(" .... ");
	Serial.print("BR: "); 
	Serial.print(bytesReceived); 
	Serial.print(", ");
	Serial.print("RBR: ");
	Serial.print(receiveBuffer.remaining());
	Serial.print(", ");
	Serial.print("BS: "); 
	Serial.print(bytesSent); 
	Serial.print(", ");
	Serial.print("SBR: ");
	Serial.print(sendBuffer.remaining());
	Serial.println("");
	
    }
    void StreamWithCTS::receive(){
      while(dataAvailable() > 0){

	byte b = readFromStream();
	if(slashed){
	  slashed = false;
	} else {
	  switch(b){
	    case SLASH_BYTE:
	      Serial.print(">>>>>> Received Slashed"); printVitals();
	      slashed = true;
	      continue;
	   
	    case CTS_BYTE:
	      Serial.print(">>>>>> Received CTS "); printVitals();
	      cts = true;
	      bytesSent = 0;
	      continue;

	    default:
	      break;
	  }
	
	}

	if(receiveBuffer.isFull()){
	  Serial.print("ERROR: Oh no receive buffer full and non cts byte: "); Serial.println(b);
	  error = 1;
	  break;
	}

	receiveBuffer.write(b);
	Serial.print("Received: "); Serial.print(b); printVitals();
      } //end data available loop

      if(bytesReceived >= uartBufferSize && receiveBuffer.remaining() >= uartBufferSize){
	writeToStream(CTS_BYTE);
	Serial.print("<<<<<<< Sending CTS");
	printVitals();
	bytesReceived = 0;
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
	Serial.print("Sent: "); Serial.print(b); printVitals();
	writeToStream(b);
	if(bytesSent >= uartBufferSize){
	  Serial.println("Setting CTS to false...");
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