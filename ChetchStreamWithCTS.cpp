#include <ChetchStreamWithCTS.h>

namespace Chetch{

    StreamWithCTS::StreamWithCTS(unsigned int uartBufferSize, unsigned int receiveBufferSize, unsigned int sendBufferSize){

      this->uartBufferSize = uartBufferSize;

      receiveBuffer = new RingBuffer(receiveBufferSize);
      sendBuffer = new RingBuffer(sendBufferSize);

      cts = true;

      commandCallback = NULL;
      //dataHandler = NULL;
    }

    StreamWithCTS::~StreamWithCTS(){
      if(receiveBuffer != NULL)delete receiveBuffer;
      if(sendBuffer != NULL)delete sendBuffer;
      if(dataBuffer != NULL)delete dataBuffer; 
    }

    unsigned int StreamWithCTS::getUartBufferSize(){ 
      return uartBufferSize; 
    }

    void StreamWithCTS::begin(Stream *stream, void (*callback)(StreamWithCTS*, byte)){
      this->stream = stream;
      reset();
      commandCallback = callback;
    }

    void StreamWithCTS::reset(){
      while(stream->available())stream->read();
      receiveBuffer->reset();
      sendBuffer->reset();
      if(dataBuffer != NULL)dataBuffer->reset();
      bytesReceived = 0;
      bytesSent = 0;
      cts = true;
      error = 0;
      rslashed = false;
      endOfDataCount = 0;
    }


    bool StreamWithCTS::setDataHandler(void (*handler)(StreamWithCTS*, RingBuffer*, bool), int size){
      if(dataHandler != NULL)return false;

      dataHandler = handler;
      dataBuffer = new RingBuffer(size);
    }

    byte StreamWithCTS::readFromStream(bool count){
	if(count)bytesReceived++;
	return stream->read();
    }

    void StreamWithCTS::writeToStream(byte b, bool count, bool flush){
	if(count)bytesSent++;
	stream->write(b);
	if(flush)stream->flush();
    }

    byte StreamWithCTS::peekAtStream(){
	return stream->peek();
    }

    int StreamWithCTS::dataAvailable(){
	return stream->available();
    }


    void StreamWithCTS::printVitals(){
	/*Serial.print(" .... ");
	Serial.print("CTS: ");
	Serial.print(cts);
	Serial.print(", ");
	Serial.print("BR: "); 
	Serial.print(bytesReceived); 
	Serial.print(", ");
	Serial.print("RBR: ");
	Serial.print(receiveBuffer->remaining());
	Serial.print(", ");
	Serial.print("BS: "); 
	Serial.print(bytesSent); 
	Serial.print(", ");
	Serial.print("SBR: ");
	Serial.print(sendBuffer->remaining());*/
	Serial.println("");
	
    }
    void StreamWithCTS::receive(){
      bool isData = true;
      bool continueReceiving = true;
      byte b;
      while(dataAvailable() > 0 && continueReceiving){
	b = peekAtStream(); //we peek in case this is a data byte but the receive buffer is full
	if(rslashed){
	  isData = true;
	} else {
	//Serial.print("Peaking at: "); Serial.write(b); Serial.println("");
	  switch(b){
	    case RESET_BYTE:
	      b = readFromStream(); //remove from buffer
	      reset(); //note that this will set 'bytesReceived' to zero!!! this can cause counting problems so beware
	      if(commandCallback != NULL)commandCallback(this, b);
	      isData = false;
	      return; //cos this is a reset

	    case END_BYTE:
	      //Serial.println("End byte received");
	      b = readFromStream(); //remove from buffer
	      isData = false;
	      Serial.write(ERROR_BYTE);
	      Serial.write(238);
	      Serial.write(END_BYTE);
	      endOfDataCount++;
	      
	      if(dataBuffer != NULL){
	        while(!receiveBuffer->isEmpty()){
		  if(dataBuffer->isFull()){
			//Serial.write(ERROR_BYTE);
			//Serial.write(225);
		  }
	    	  dataBuffer->write(read());
	        }
	        if(dataHandler != NULL){
		  //Serial.println("END BYTE");
		  dataHandler(this, dataBuffer, true);
		  dataBuffer->reset();
	        }
	      }
	      continueReceiving = false;
	      break; //allow break so we count this

   	    case ERROR_BYTE:
	      b = readFromStream(); //remove from uart buffer
	      if(commandCallback != NULL)commandCallback(this, b); //TODO: maybe make this return a bool for continueReceiving flag
	      isData = false;
	      break; //allow break so we count this

  	    case SLASH_BYTE:
	      //Serial.print(">>>>>> Received Slashed"); printVitals();
	      b = readFromStream(); //remove from uart buffer
	      isData = false;
	      rslashed = true;
	      break;
	   
	    case CTS_BYTE:
	      //Serial.print(">>>>>> Received CTS "); printVitals();
	      b = readFromStream(false); //remove from uart buffer
	      cts = true;
	      bytesSent = 0;
     	      continueReceiving = false; //we exit the loop so as we can dispatch anything in the send buffer
	      isData = false;
	      break;

	    default:
	      isData = true;
	      break;
	  }
	} //end slashed test
	
	if(isData){
	  if(receiveBuffer->isFull()){
	    //Serial.println("RB buffer full");
	    Serial.write(242);
	    continueReceiving = false;
	  } else {
	    b = readFromStream();
	    if(rslashed)rslashed = false;
	    //Serial.print("Adding: "); Serial.write(b); Serial.println(" to RB");
  	    receiveBuffer->write(b);
	  }
	}
	
	//Serial.print("Received: "); Serial.print(b); printVitals();

	if(bytesReceived >= (uartBufferSize - 2)){
	  writeToStream(CTS_BYTE, false, true);
	  //Serial.print("<<<<<<< Sending CTS"); printVitals();
	  bytesReceived = 0;
	}
      } //end data available loop      
    }

    void StreamWithCTS::process(){
	if(dataBuffer != NULL){
	  bool dataReceived = false;
	    
	  while(!receiveBuffer->isEmpty() && !dataBuffer->isFull()){
	    byte b = read();
	    dataBuffer->write(b);
	    dataReceived = true;
	  }

	  if(dataHandler != NULL && dataReceived){
	    dataHandler(this, dataBuffer, false);	
	  }
	}	
    }

    void StreamWithCTS::send(){
      byte b;
      while(!sendBuffer->isEmpty() && cts){
	b = sendBuffer->read();
	switch(b){
	  case CTS_BYTE:
	  case SLASH_BYTE:
	  case END_BYTE:
	  case RESET_BYTE:
	  case ERROR_BYTE:
	    writeToStream(SLASH_BYTE);
	    break;

	  default:
	    break;
	}
	writeToStream(b);
        if(bytesSent >= (uartBufferSize - 2)){
	  cts = false;
	}
      }
    }

    bool StreamWithCTS::canRead(){
      return !receiveBuffer->isEmpty();
    }

    bool StreamWithCTS::canWrite(){
	return !sendBuffer->isFull();
    }

    byte StreamWithCTS::read(){
      return receiveBuffer->read();
    }

    bool StreamWithCTS::write(byte b){
      return sendBuffer->write(b);
    }

    bool StreamWithCTS::write(byte *bytes, int size){
      return sendBuffer->write(bytes, size);
    }

    bool StreamWithCTS::isClearToSend(){
      return cts;
    }
} //end namespace