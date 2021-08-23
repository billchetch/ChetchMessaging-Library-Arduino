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
		localReset = false;
		remoteReset = false;
		reset(true); //send command for remote to reset
	}

	
    void StreamWithCTS::setCommandHandler(void (*handler)(StreamWithCTS*, byte))
	{
		commandHandler = handler;
    }

	void StreamWithCTS::setEventHandlers(void (*handler1)(StreamWithCTS*, byte), void (*handler2)(StreamWithCTS*, byte))
	{
		localEventHandler = handler1;
		remoteEventHandler = handler2;
    }

    void StreamWithCTS::setReadyToReceiveHandler(bool (*handler)(StreamWithCTS*))
	{
		readyToReceiveHandler = handler;
    }

    void StreamWithCTS::setDataHandler(void (*handler)(StreamWithCTS*, bool))
	{
		dataHandler = handler;
    }

	void StreamWithCTS::setReceiveHandler(void (*handler)(StreamWithCTS*, int)){
		receiveHandler = handler;
	}

	void StreamWithCTS::setSendHandler(void (*handler)(StreamWithCTS*, int)){
		sendHandler = handler;
	}

	void StreamWithCTS::setCTSTimeout(int ms){
		ctsTimeout = ms;
	}

	void StreamWithCTS::setMaxDatablockSize(int max){
		maxDatablockSize = max;
	}

	void StreamWithCTS::reset(bool sendCommandByte)
	{
		while(stream->available())stream->read();
		receiveBuffer->reset();
		sendBuffer->reset();
		bytesReceived = 0;
		bytesSent = 0;
		cts = true;
		rslashed = false;
		rcommand = false;
		revent = false;
		sslashed = false;
		smarked = false;
		error = 0;
		
		if(sendCommandByte)sendCommand(Command::RESET);
		sendEvent(Event::RESET);
		localReset = true;
    }    

    byte StreamWithCTS::readFromStream(bool count)
	{
		if(count)bytesReceived++;
		byte b = stream->read();

		//Flow control
		if(count)sendCTS();

		return b;
    }

    void StreamWithCTS::writeToStream(byte b, bool count, bool flush)
	{
		if(count)bytesSent++;
		stream->write(b);
		if(flush)stream->flush();

		//Flow control by byte counting
		if(requiresCTS(bytesSent, uartRemoteBufferSize)){
			cts = false;
			lastCTSrequired = millis();
		}
    }

    byte StreamWithCTS::peekAtStream()
	{
		return stream->peek();
    }

    int StreamWithCTS::dataAvailable()
	{
		return stream->available();
    }

	bool StreamWithCTS::isReady(){
		return localReset && remoteReset;
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
		//Given that sendCTS is dependent not only on bytes received but also potentially a user-defined method
		//we allow for the posibiity that the conditions for sending CTS have changed even if there are no bytes to receive.
		if(dataAvailable() == 0)
		{
			sendCTS();
			return;
		}


		//here we can already start receiving data
		byte b;
		bool isData = true;
		bool continueReceiving = true;
		while(dataAvailable() > 0 && continueReceiving)
		{
			b = peekAtStream(); //we peek in case this is a data byte but the receive buffer is full
			
			if(rcommand)
			{
				b = readFromStream(false); //read the command
				switch(b){
					case (byte)Command::RESET:
						reset(false); //do NOT send command for remote to reset
						break;
				}
				if(commandHandler != NULL)commandHandler(this, b);
				rcommand = false;
				isData = false;
			}
			else if(revent)
			{
				b = readFromStream(false); //read the event
				switch(b){
					case (byte)Event::RESET:
						remoteReset = true;
						break;
				}
				if(remoteEventHandler != NULL)remoteEventHandler(this, b);
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

					case COMMAND_BYTE:
						b = readFromStream(false); //remove from buffer
						isData = false; 
						rcommand = true;
						break;

					case EVENT_BYTE:
						b = readFromStream(false); //remove from buffer
						isData = false; 
						revent = true;
						break;

					case CTS_BYTE:
						b = readFromStream(false); //remove from uart buffer
						if(!cts){ // we check so as to avoid unnecessary reseting of bytesSent
							cts = true;
							bytesSent = 0;
     						continueReceiving = false; //we exit the loop so as we can dispatch anything in the send buffer
						}
						isData = false;
						break;

					default:
						isData = true;
						break;
				}
			} //end slashed test
	
			//If it's data then we add to the receive buffer
			if(isData && isReady())
			{
				b = readFromStream();
				if(receiveBuffer->isFull())
				{
					//Serial.println("RB buffer full");
					sendEvent(Event::RECEIVE_BUFFER_FULL);
					continueReceiving = false;
				} 
				else 
				{
					if(rslashed)rslashed = false;
					receiveBuffer->write(b);
				}
			}		
		} //end data available loop      
    }

    void StreamWithCTS::process()
	{
		if(!isReady())return;

		handleData(false);

		if(sendHandler != NULL && !sendBuffer->isFull()){
			sendHandler(this, sendBuffer->remaining());
		}
    }

    void StreamWithCTS::send()
	{
		if(!isReady())return;

		//check if timeout has been exceeded
		if(ctsTimeout > 0){
			if(!cts && (millis() - lastCTSrequired > ctsTimeout)) {
				sendEvent(Event::CTS_TIMEOUT);
				lastCTSrequired = millis(); //this is so it doesn't fire every call but every ctsTimeout interval instead'
				return;
			}
		}

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
			
		} //end sending loop
    }

	void StreamWithCTS::loop(){
		receive();
		process();
		send();
	}

	void StreamWithCTS::handleData(bool endOfData){
		if(dataHandler != NULL){
			dataHandler(this, endOfData);
		}
		if(receiveHandler != NULL && bytesToRead() > 0){
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
			return bytesToRead() == 0;
		}
	}

	bool StreamWithCTS::sendCTS(bool overrideFlowControl){
		if((requiresCTS(bytesReceived, uartLocalBufferSize) && readyToReceive()) || overrideFlowControl)
		{
			writeToStream(CTS_BYTE, false, true);
			bytesReceived = 0;
			return true; 
		} else {
			return false;
		}
	}


	void StreamWithCTS::sendCommand(Command c){
		sendCommand((byte)c);
	}

	void StreamWithCTS::sendCommand(byte b){
		writeToStream(COMMAND_BYTE, false);
		writeToStream(b, false, true);
	}

	void StreamWithCTS::sendEvent(Event e){
		sendEvent((byte)e);
	}

	void StreamWithCTS::sendEvent(byte b){
		if(localEventHandler != NULL)localEventHandler(this, b);
		
		writeToStream(EVENT_BYTE, false);
		writeToStream(b, false, true);
	}

    bool StreamWithCTS::isSystemByte(byte b){
		switch(b){
			case CTS_BYTE:
			case SLASH_BYTE:
			case PAD_BYTE:
			case END_BYTE:
			case COMMAND_BYTE:
			case EVENT_BYTE:
				return true;

			default:
				return false;
		}
    }

    bool StreamWithCTS::canReceive(int byteCount){
		if(byteCount == NO_LIMIT){
			return  receiveBuffer->isEmpty();
		} else if(byteCount == UART_BUFFER_SIZE){
			return  receiveBuffer->remaining() >= uartLocalBufferSize; 
		} else {
			return receiveBuffer->remaining() >= byteCount;
		}
    }

    bool StreamWithCTS::canSend(int byteCount){
		if(byteCount == NO_LIMIT){
			return  sendBuffer->isEmpty();
		} else if(byteCount == UART_BUFFER_SIZE){
			return  sendBuffer->remaining() >= uartLocalBufferSize; 
		} else {
			return sendBuffer->remaining() >= byteCount;
		}
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