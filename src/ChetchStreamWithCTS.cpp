#include <ChetchStreamWithCTS.h>

namespace Chetch
{

	StreamWithCTS::StreamWithCTS(unsigned int uartLocalBufferSize, unsigned int uartRemoteBufferSize, unsigned int receiveBufferSize, unsigned int sendBufferSize)
	{

		this->uartLocalBufferSize = uartLocalBufferSize;
		this->uartRemoteBufferSize = uartRemoteBufferSize;

		receiveBuffer = new RingBuffer(receiveBufferSize);
		sendBuffer = new RingBuffer(sendBufferSize);

		cts = true;
	}

    StreamWithCTS::~StreamWithCTS()
	{
		if(receiveBuffer != NULL)delete receiveBuffer;
		if(sendBuffer != NULL)delete sendBuffer;
    }

    
    void StreamWithCTS::begin(Stream *stream)
	{
		this->stream = stream;
		reset();
	}

	
    void StreamWithCTS::setResetHandler(void (*callback)(StreamWithCTS*))
	{
		resetHandler = callback;
    }

	void StreamWithCTS::setEventHandler(void (*callback)(StreamWithCTS*, byte))
	{
		eventHandler = callback;
    }

    void StreamWithCTS::setReadyToReceiveHandler(bool (*callback)(StreamWithCTS*))
	{
		readyToReceiveHandler = callback;
    }

    void StreamWithCTS::setDataHandler(void (*callback)(StreamWithCTS*, bool))
	{
		dataHandler = callback;
    }

	void StreamWithCTS::setReceiveHandler(void (*callback)(StreamWithCTS*, int)){
		receiveHandler = callback;
	}

	void StreamWithCTS::setSendHandler(void (*callback)(StreamWithCTS*)){
		sendHandler = callback;
	}

    void StreamWithCTS::reset()
	{
		while(stream->available())stream->read();
		receiveBuffer->reset();
		sendBuffer->reset();
		bytesReceived = 0;
		bytesSent = 0;
		cts = true;
		rslashed = false;
		revent = false;
		sslashed = false;
		smarked = false;
		error = 0;
      
		sendEvent(Event::RESET);
    }    

    byte StreamWithCTS::readFromStream(bool count)
	{
		if(count)bytesReceived++;
		byte b = stream->read();
		return b;
    }

    void StreamWithCTS::writeToStream(byte b, bool count, bool flush)
	{
		if(count)bytesSent++;
		stream->write(b);
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
			if(revent)
			{
				b = readFromStream(false);
				if(eventHandler != NULL)eventHandler(this, b);
				revent = false;
				isData = false;
			}
			else if(rslashed)
			{
				isData = true;
			} 
			else 
			{
				switch(b){
					case RESET_BYTE:
						b = readFromStream(); //remove from buffer
						reset(); //note that this will set 'bytesReceived' to zero!!! this can cause counting problems so beware
						if(resetHandler != NULL)resetHandler(this);
						return; //cos this is a reset

					case END_BYTE:
						b = readFromStream(); //remove from buffer
						isData = false;
						receiveBuffer->setMarker();
						handleData(true);
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

					case EVENT_BYTE:
						b = readFromStream(false); //remove from buffer
						isData = false; 
						revent = true;
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
					sendEvent(Event::RECEIVE_BUFFER_FULL);
					continueReceiving = false;
				} 
				else 
				{
					b = readFromStream();
					if(rslashed)rslashed = false;
					receiveBuffer->write(b);
				}
			}
	
			//finally if we've removed enough bytes from the uart buffer AND we are 'ready to receive' then send CTS
			sendCTS();
		} //end data available loop      
    }

    void StreamWithCTS::process()
	{
		handleData(false);

		if(sendHandler != NULL && sendBuffer->isEmpty()){
			sendHandler(this);
		}
    }

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
					if(requiresCTS(bytesSent + 1, uartRemoteBufferSize)){
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
			if(requiresCTS(bytesSent, uartRemoteBufferSize)){
				cts = false;
			}
		} //end sending loop
    }


	void StreamWithCTS::handleData(bool endOfData){
		if(dataHandler != NULL){
			dataHandler(this, endOfData);
		}
		if(receiveHandler != NULL && sendBuffer->isEmpty() && bytesToRead() > 0){
			receiveHandler(this, bytesToRead());
		}
	}

    bool StreamWithCTS::requiresCTS(unsigned int byteCount, unsigned int bufferSize)
	{
		if(bufferSize == 0)return false;
		return byteCount == (bufferSize - 2);
    }

	bool StreamWithCTS::readyToReceive(){
		if(readyToReceiveHandler!= NULL){
			return readyToReceiveHandler(this);	
		} else {
			return sendBuffer->isEmpty() && bytesToRead() == 0;
		}
	}

	bool StreamWithCTS::sendCTS(){
		if(requiresCTS(bytesReceived, uartLocalBufferSize) && readyToReceive())
		{
			writeToStream(CTS_BYTE, false, true);
			bytesReceived = 0;
			return true; 
		} else {
			return false;
		}
	}

	void StreamWithCTS::sendEvent(byte e){
		writeToStream(EVENT_BYTE, false);
		writeToStream(e, false, true);
	}

    bool StreamWithCTS::isSystemByte(byte b){
		switch(b){
			case CTS_BYTE:
			case SLASH_BYTE:
			case PAD_BYTE:
			case END_BYTE:
			case RESET_BYTE:
			case EVENT_BYTE:
				return true;

			default:
				return false;
		}
    }

    bool StreamWithCTS::canRead(unsigned int byteCount){
		return receiveBuffer->used() >= byteCount;
    }

    bool StreamWithCTS::canWrite(unsigned int byteCount){
		return sendBuffer->remaining() >= byteCount;
    }

    byte StreamWithCTS::read(){
		return receiveBuffer->read();
    }

	int StreamWithCTS::bytesToRead(bool untilMarker){
		if(untilMarker){
			return receiveBuffer->readToMarkerCount();
		} else {
			return receiveBuffer->used();
		}
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

	void StreamWithCTS::endWrite(){
		sendBuffer->setMarker();
	}

    bool StreamWithCTS::isClearToSend()
	{
		return cts;
    }
} //end namespace