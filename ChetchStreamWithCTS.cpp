#include <ChetchStreamWithCTS.h>

namespace Chetch{

    StreamWithCTS::StreamWithCTS(unsigned int uartBufferSize, unsigned int receiveBufferSize, unsigned int sendBufferSize){

      this->uartBufferSize = uartBufferSize;

      receiveBuffer = new RingBuffer(receiveBufferSize);
      sendBuffer = new RingBuffer(sendBufferSize);

      //receiveBuffer = RingBuffer(receiveBufferSize);
      //sendBuffer = RingBuffer(sendBufferSize);

      //receiveBuffer = RingBuffer(rbuffer, 4);
      //sendBuffer = RingBuffer(sbuffer, 4);

      cts = true;

      commandCallback = NULL;
      //dataHandler = NULL;
    }

    StreamWithCTS::~StreamWithCTS(){
      if(receiveBuffer != NULL)delete receiveBuffer;
      if(sendBuffer != NULL)delete sendBuffer;
      //if(dataBuffer != NULL)delete dataBuffer;  
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
      receiveBuffer->reset();
      sendBuffer->reset();
      //if(dataBuffer != NULL)dataBuffer->reset();
      bytesReceived = 0;
      bytesSent = 0;
      cts = true;
      error = 0;
    }


    /*bool StreamWithCTS::setDataHandler(void (*handler)(StreamWithCTS*, RingBuffer*, bool), int size){
      if(dataHandler != NULL)return false;

      dataHandler = handler;
      dataBuffer = new RingBuffer(size);
    }*/

    byte StreamWithCTS::readFromStream(){
	return stream->read();
    }

    void StreamWithCTS::writeToStream(byte b, bool flush){
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
	Serial.print(sendBuffer->remaining());
	Serial.println("");*/
	
    }
    void StreamWithCTS::receive(){
      bool isData = true;
      byte b;
      while(dataAvailable() > 0){
	b = peekAtStream();
	switch(b){
	  case RESET_BYTE:
	    readFromStream(); //remove byte from stream		
            reset();
	    if(commandCallback != NULL)commandCallback(this, b);
	    return; //cos this is a reset

	  case END_BYTE:
	    isData = false;
	    readFromStream(); //remove byte from stream
	      /*if(dataBuffer != NULL){
	  	while(!receiveBuffer->isEmpty()){
	    	  dataBuffer->write(receiveBuffer->read());
	      	}
	      	if(dataHandler != NULL){
		  //Serial.println("END BYTE");
		  dataHandler(this, dataBuffer, true);
		}
	      }*/
	    break; //allow break so we count this

   	  case ERROR_BYTE:
	    readFromStream(); //remove byte from stream
	    if(commandCallback != NULL)commandCallback(this, b);
	    isData = false;
	    break; //allow break so we count this

	  case SLASH_BYTE:
	    //Serial.print(">>>>>> Received Slashed"); printVitals();
	    if(receiveBuffer->isFull() || dataAvailable() <= 1)return; //wait for good receive conditions
	    readFromStream(); //remove slashbyte from stream (note we don't count this in received bytes)
	    b = readFromStream(); //get the data
	    isData = true;
	    break; //allow break cos this is really data
	   
	  case CTS_BYTE:
	    //Serial.print(">>>>>> Received CTS "); printVitals();
	    cts = true;
	    bytesSent = 0;
     	    readFromStream(); //get rid of the byte
	    continue; //continue so as not to count

	  default:
	    b = readFromStream();
	    isData = true;
	    break;
	}
	
	if(isData){
	  if(receiveBuffer->isFull()){
	    break;
	  } else {
  	    receiveBuffer->write(b);
	  }
	}
	bytesReceived++;
	
	//Serial.print("Received: "); Serial.print(b); printVitals();

	if(bytesReceived >= (uartBufferSize - 2)){
	  writeToStream(CTS_BYTE, true);
	  //Serial.print("<<<<<<< Sending CTS"); printVitals();
	  bytesReceived = 0;
	}
      } //end data available loop      
    }

    /*void StreamWithCTS::process(){
	if(dataBuffer != NULL){
	  bool dataReceived = false;
	    
	  while(!receiveBuffer->isEmpty()){
	    byte b = receiveBuffer->read();
	    dataBuffer->write(b);
	    dataReceived = true;
	  }

	  if(dataHandler != NULL && dataReceived){
	    dataHandler(this, dataBuffer, false);	
	  }
	}
    }*/

    void StreamWithCTS::send(){
      byte b;
      while(!sendBuffer->isEmpty() && cts){
	b = sendBuffer->peek();
	switch(b){
	  case CTS_BYTE:
	  case SLASH_BYTE:
	  case END_BYTE:
	  case RESET_BYTE:
	  case ERROR_BYTE:
	    //Serial.print("Adding slash byte for: "); Serial.println(b);
   	    writeToStream(SLASH_BYTE);
	    b = sendBuffer->read();
	    break;

	  default:
	    b = sendBuffer->read();
	    break;
	}
	writeToStream(b);
	bytesSent++;
	if(bytesSent >= (uartBufferSize - 2)){
	  cts = false;
	}
	//Serial.print("Sent: "); Serial.print(b); printVitals();
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