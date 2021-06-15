#include <ChetchStreamWithCTS.h>

namespace Chetch
{

	StreamWithCTS::StreamWithCTS(unsigned int uartBufferSize, unsigned int receiveBufferSize, unsigned int sendBufferSize)
	{

		this->uartBufferSize = uartBufferSize;

		receiveBuffer = new RingBuffer(receiveBufferSize);
		sendBuffer = new RingBuffer(sendBufferSize);

		cts = true;

		commandCallback = NULL;
		//dataHandler = NULL;
	}

    StreamWithCTS::~StreamWithCTS()
	{
		if(receiveBuffer != NULL)delete receiveBuffer;
		if(sendBuffer != NULL)delete sendBuffer;
    }

    unsigned int StreamWithCTS::getUartBufferSize()
	{ 
		return uartBufferSize; 
    }

    void StreamWithCTS::begin(Stream *stream, void (*callback)(StreamWithCTS*, byte))
	{
		this->stream = stream;
		reset();
		setCommandCallback(callback);
    }

    void StreamWithCTS::setCommandCallback(void (*callback)(StreamWithCTS*, byte))
	{
		commandCallback = callback;
    }

    void StreamWithCTS::setReadyToReceive(bool (*callback)(StreamWithCTS*))
	{
		readyToReceive = callback;
    }

    void StreamWithCTS::setDataHandler(void (*handler)(StreamWithCTS*, bool))
	{
		dataHandler = handler;
    }

    void StreamWithCTS::reset()
	{
		while(stream->available())stream->read();
		receiveBuffer->reset();
		sendBuffer->reset();
		bytesReceived = 0;
		bytesSent = 0;
		cts = true;
		rtr = true;
		rslashed = false;
		sslashed = false;
		smarked = false;
		error = 0;
      
      
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

    byte StreamWithCTS::readFromStream(bool count)
	{
		if(count)bytesReceived++;
		byte b = stream->read();
		readHistory[readHistoryIndex] = b;
		readHistoryIndex = (readHistoryIndex + 1) % 64;
		return b;
    }

    void StreamWithCTS::writeToStream(byte b, bool count, bool flush)
	{
		if(count)bytesSent++;
		stream->write(b);
		//stream->println(b);
		if(flush)stream->flush();
    }

    byte StreamWithCTS::peekAtStream()
	{
		return stream->peek();
    }

    int StreamWithCTS::dataAvailable()
	{
		return stream->available();
    }


    void StreamWithCTS::printVitals()
	{
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

    void StreamWithCTS::dumpLog()
	{
		Serial.write(ctsCount);
		Serial.write((byte)bytesReceived);
		for(int i = 0; i < 64; i++){
			int idx = (readHistoryIndex + i) % 64;
			Serial.write(readHistory[idx]);
		}
    }

    void StreamWithCTS::receive()
	{
		//As part of the loop (receive, process send) this will be called in situations where there is no data available.
		//Given that sendCTS is dependent not only on bytes received but also potentially a user-defined method (canReceive)
		//we allow for the posibiity that the conditions for sending CTS have changed even if there are no bytes to receive.
		if(dataAvailable() == 0)
		{
			sendCTS();
			return;
		}


		//here we can already start receiving data
		bool isData = true;
		bool continueReceiving = true;
		byte b;
		while(dataAvailable() > 0 && continueReceiving)
		{
			b = peekAtStream(); //we peek in case this is a data byte but the receive buffer is full
			if(rslashed)
			{
				isData = true;
			} 
			else 
			{
				//Serial.print("Peaking at: "); Serial.write(b); Serial.println("");
				switch(b){
					case RESET_BYTE:
						b = readFromStream(); //remove from buffer
						reset(); //note that this will set 'bytesReceived' to zero!!! this can cause counting problems so beware
						if(commandCallback != NULL)commandCallback(this, b);
						return; //cos this is a reset

					case END_BYTE:
						b = readFromStream(); //remove from buffer
						isData = false;
						if(dataHandler!= NULL){
							dataHandler(this, true);	
						}
						break;

   					case ERROR_BYTE:
						b = readFromStream(); //remove from uart buffer
						Serial.write(ERROR_BYTE);
						Serial.write(248);
						dumpLog();
						Serial.write(248);
						if(commandCallback != NULL)commandCallback(this, b); //TODO: maybe make this return a bool for continueReceiving flag
						isData = false;
						break; 

  					case SLASH_BYTE:
						b = readFromStream(); //remove from uart buffer
						isData = false;
						rslashed = true;
						break;

					case PAD_BYTE:
						b = readFromStream(); //remove from buffer (and therfore count)
						isData = false; //so we jus discard this byte afer counting
						break;

					case PING_BYTE:
						Serial.write(ERROR_BYTE);
						Serial.write(249);
						b = readFromStream(); //remove from uart buffer
						isData = false;
						dumpLog();
						Serial.write(249);
						break;

					case CTS_BYTE:
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
	
			//If it's data then we add to the receive buffer
			if(isData)
			{
				if(receiveBuffer->isFull())
				{
					//Serial.println("RB buffer full");
					Serial.write(242);
					continueReceiving = false;
				} 
				else 
				{
					b = readFromStream();
					if(rslashed)rslashed = false;
					receiveBuffer->write(b);
				}
			}
	
			//finally if we've removed enough bytes from the uart buffer AND '
			sendCTS();

		} //end data available loop      
    }

    void StreamWithCTS::process()
	{
		if(dataHandler!= NULL){
			dataHandler(this, false);	
		}	
    }

    /*void StreamWithCTS::send(){
      byte b;
      while(!sendBuffer->isEmpty() && cts){
	b = sendBuffer->peek();
	if(sslashed){
	  sslashed = false;
	  b = sendBuffer->read();
	} else if(b == SLASH_BYTE) {
	  if(requiresCTS(bytesSent + 1)){
	      //Serial.println("Use pad byte");
	    b = PAD_BYTE;
	  } else {
	    sslashed = true;
	    b = sendBuffer->read();
	  }
	} else {
	  b = sendBuffer->read();
  	}

	writeToStream(b);
	if(requiresCTS(bytesSent)){
	  cts = false;
	}
      }
    }*/

    void StreamWithCTS::send()
	{
		byte b;
		while((!sendBuffer->isEmpty() || smarked) && cts)
		{
			if(smarked)
			{
				smarked = false;
				//Serial.println("End byte");
				b = END_BYTE;
			} 
			else if(sslashed)
			{
				sslashed = false;
				smarked = sendBuffer->hasMarker();
				//if(smarked)Serial.println("MARKER");
				b = sendBuffer->read();
			} 
			else 
			{
				b = sendBuffer->peek();
				if(isSystemByte(b))
				{
					if(requiresCTS(bytesSent + 1)){
						//Serial.println("Use pad byte");
						b = PAD_BYTE;
					} 
					else 
					{
						b = SLASH_BYTE;
						//Serial.println("Use slash");
						sslashed = true;
					}
  				} else {
					smarked = sendBuffer->hasMarker();
					//if(smarked)Serial.println("MARKER");
					b = sendBuffer->read();
					//Serial.print("Normal byte: "); Serial.write(b); Serial.println("");
				}
			}

			writeToStream(b);
			if(requiresCTS(bytesSent)){
				cts = false;
			}
		} //end sending loop
    }

    bool StreamWithCTS::requiresCTS(unsigned long byteCount)
	{
		if(byteCount > (uartBufferSize - 2)){
			Serial.write(ERROR_BYTE);
			Serial.write(211);
			dumpLog();
			Serial.write(211);
		}
		return byteCount == (uartBufferSize - 2);
    }

	bool StreamWithCTS::sendCTS(){
		if(requiresCTS(bytesReceived) && (readyToReceive == NULL || readyToReceive(this)))
		{
			writeToStream(CTS_BYTE, false, true);
			bytesReceived = 0;
			return true; 
		} else {
			return false;
		}
	}

    bool StreamWithCTS::isSystemByte(byte b){
		switch(b){
			case CTS_BYTE:
			case SLASH_BYTE:
			case PAD_BYTE:
			case END_BYTE:
			case RESET_BYTE:
			case ERROR_BYTE:
			case PING_BYTE:
				return true;

			default:
				return false;
		}
    }

    bool StreamWithCTS::canRead(){
		return !receiveBuffer->isEmpty();
    }

    bool StreamWithCTS::canWrite(){
		return sendBuffer->remaining() >= 1;
    }

    byte StreamWithCTS::read(){
		return receiveBuffer->read();
    }

    bool StreamWithCTS::write(byte b, bool addMarker)
	{
		bool retVal = sendBuffer->write(b);
		if(retVal && addMarker)sendBuffer->setMarker();
		return retVal;
    }

	bool StreamWithCTS::write(int n, bool addMarker)
	{
		return write<int>(n, addMarker);
	}

	bool StreamWithCTS::write(long n, bool addMarker)
	{
		return write<long>(n, addMarker);
	}

    bool StreamWithCTS::write(byte *bytes, int size, bool addEndMarker)
	{
		for(int i = 0; i < size; i++)
		{
			bool lastByte = (i == size - 1);
			if(!write(bytes[i], addEndMarker && lastByte)){
				return false;
			}
		}
		return true;
    }

    bool StreamWithCTS::isClearToSend()
	{
		return cts;
    }
} //end namespace