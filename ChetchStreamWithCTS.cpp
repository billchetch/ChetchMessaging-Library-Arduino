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
      
      //temp
      ctsCount = 0;
      endOfDataCount = 0;
      lastPeekedByte = 0;
      lastReadByte = 0;
      lastReceivedByte = 0;
      for(int i = 0; i < 64; i++){
	readHistory[i] = 0;
      }
    }


    bool StreamWithCTS::setDataHandler(void (*handler)(StreamWithCTS*, RingBuffer*, bool), int size){
      if(dataHandler != NULL)return false;

      dataHandler = handler;
      dataBuffer = new RingBuffer(size);
    }

    byte StreamWithCTS::readFromStream(bool count){
	if(count)bytesReceived++;
	byte b = stream->read();
	readHistory[readHistoryIndex] = b;
	readHistoryIndex = (readHistoryIndex + 1) % 64;
	return b;
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

    void StreamWithCTS::dumpLog(){
	Serial.write(ctsCount);
	Serial.write((byte)bytesReceived);
	for(int i = 0; i < 64; i++){
		int idx = (readHistoryIndex + i) % 64;
		Serial.write(readHistory[idx]);
	}
    }
    void StreamWithCTS::receive(){
      bool isData = true;
      bool continueReceiving = true;
      byte b;
      while(dataAvailable() > 0 && continueReceiving){
	b = peekAtStream(); //we peek in case this is a data byte but the receive buffer is full
	byte k = lastPeekedByte;
	lastPeekedByte = b;
	if(rslashed){
	  //Serial.write(130);
	  /*if(b == CTS_BYTE){
	      Serial.write(ERROR_BYTE);
	      Serial.write(251);
	      Serial.write(k);
      	      Serial.write(b);
      	      dumpLog();
	      Serial.write(252);
	  }*/
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
		Serial.write(247);
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
	      Serial.write(ERROR_BYTE);
	      Serial.write(248);
	      dumpLog();
	      Serial.write(248);
	      if(commandCallback != NULL)commandCallback(this, b); //TODO: maybe make this return a bool for continueReceiving flag
	      isData = false;
	      break; //allow break so we count this

  	    case SLASH_BYTE:
	      //Serial.print(">>>>>> Received Slashed"); printVitals();
	      b = readFromStream(); //remove from uart buffer
	      lastReadByte = b;
	      isData = false;
	      rslashed = true;
	      //Serial.write(131);
	      break;

	    case PAD_BYTE:
	      b = readFromStream(); //remove from buffer (and therfore count)
	      lastReadByte = b;
	      isData = false; //so we jus discard this byte afer counting
	      break;

	    case PING_BYTE:
	      Serial.write(ERROR_BYTE);
	      Serial.write(249);
	      b = readFromStream(); //remove from uart buffer
	      lastReadByte = b;
	      isData = false;
	      dumpLog();
	      Serial.write(249);
	      break;

	    case CTS_BYTE:
	      //Serial.print(">>>>>> Received CTS "); printVitals();
	      //Serial.write(132);
	      b = readFromStream(false); //remove from uart buffer
	      lastReadByte = b;
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
	    if(lastReceivedByte > 0 && abs(b - lastReceivedByte) != 1){
		Serial.write(ERROR_BYTE);
		Serial.write(243);
		dumpLog();
	    }
	    lastReadByte = b;
	    if(rslashed)rslashed = false;
	    //Serial.print("Adding: "); Serial.write(b); Serial.println(" to RB");
	    lastReceivedByte = b;
  	    receiveBuffer->write(b);
	    //Serial.write(133);
	  }
	}
	
	//Serial.print("Received: "); Serial.print(b); printVitals();

	if(requiresCTS(bytesReceived)){
	  writeToStream(CTS_BYTE, false, true);
	  ctsCount++;
	  //Serial.print("<<<<<<< Sending CTS"); printVitals();
	  bytesReceived = 0;
	}
      } //end data available loop      
    }

    void StreamWithCTS::process(){
	if(dataBuffer != NULL){
	  bool dataReceived = canRead() && !dataBuffer->isFull();
	    
	  while(canRead() && !dataBuffer->isFull()){
	    byte b = read();
	    dataBuffer->write(b);
	  }

	  if(dataHandler != NULL && dataReceived){
	    dataHandler(this, dataBuffer, false);	
	  }
	}	
    }

    void StreamWithCTS::send(){
      byte b;
      while(!sendBuffer->isEmpty() && cts){
	b = sendBuffer->peek();
	if(sslashed){
	  sslashed = false;
	  b = sendBuffer->read();
	} else {
	  switch(b){
	    case CTS_BYTE:
	    case SLASH_BYTE:
	    case PAD_BYTE:
	    case END_BYTE:
	    case RESET_BYTE:
	    case ERROR_BYTE:
	    case PING_BYTE:
	      if(requiresCTS(bytesSent + 1)){
		//Serial.println("Use pad byte");
		b = PAD_BYTE;
	      } else {
	        b = SLASH_BYTE;
		//Serial.println("Use slash");
	        sslashed = true;
	      }
  	      break;
  
	    default:
	      b = sendBuffer->read();
	      //Serial.print("Normal byte: "); Serial.write(b); Serial.println("");
	      break;
	  }
	}

	writeToStream(b);
	if(requiresCTS(bytesSent)){
	  cts = false;
	}
      }
    }

    bool StreamWithCTS::requiresCTS(unsigned long byteCount){
	if(byteCount > (uartBufferSize - 2)){
		Serial.write(ERROR_BYTE);
		Serial.write(211);
		dumpLog();
		Serial.write(211);
	}
	return byteCount == (uartBufferSize - 2);
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