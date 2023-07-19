#include <ChetchStreamFlowController.h>

namespace Chetch
{

	StreamFlowController::StreamFlowController(unsigned int uartLocalBufferSize, unsigned int uartRemoteBufferSize, unsigned int receiveBufferSize, unsigned int sendBufferSize)
	{
		this->uartLocalBufferSize = uartLocalBufferSize;
		this->uartRemoteBufferSize = uartRemoteBufferSize;

		receiveBuffer = new RingBuffer(receiveBufferSize);
		sendBuffer = new RingBuffer(sendBufferSize);

		cts = true;
	}

	StreamFlowController::StreamFlowController(unsigned int receiveBufferSize, unsigned int sendBufferSize)
	{
		this->uartLocalBufferSize = 0;
		this->uartRemoteBufferSize = 0;

		receiveBuffer = new RingBuffer(receiveBufferSize);
		sendBuffer = new RingBuffer(sendBufferSize);

		cts = true;
	}

    StreamFlowController::~StreamFlowController()
	{
		if(receiveBuffer != NULL)delete receiveBuffer;
		if(sendBuffer != NULL)delete sendBuffer;
    }

    
    void StreamFlowController::begin(Stream *stream, bool clearUartBuffer)
	{
		this->stream = stream;
		localReset = false;
		remoteReset = false;
		reset(false, false, clearUartBuffer); //send command for remote to reset
	}

	bool StreamFlowController::hasBegun(){
		return stream != NULL && localReset == true;
	}

	void StreamFlowController::end(){
		reset(false, false); 
		localReset = false;
		remoteReset = false;
	}
	
    void StreamFlowController::setCommandHandler(void (*handler)(StreamFlowController*, byte))
	{
		commandHandler = handler;
    }

	void StreamFlowController::setEventHandlers(bool (*handler1)(StreamFlowController*, byte), void (*handler2)(StreamFlowController*, byte))
	{
		localEventHandler = handler1;
		remoteEventHandler = handler2;
    }

    void StreamFlowController::setReadyToReceiveHandler(bool (*handler)(StreamFlowController*))
	{
		readyToReceiveHandler = handler;
    }

    void StreamFlowController::setReceiveHandler(void (*handler)(StreamFlowController*, int)){
		receiveHandler = handler;
	}

	void StreamFlowController::setSendHandler(void (*handler)(StreamFlowController*, int)){
		sendHandler = handler;
	}

	void StreamFlowController::setCTSTimeout(int ms){
		ctsTimeout = ms;
	}

	void StreamFlowController::setMaxDatablockSize(int max){
		maxDatablockSize = max;
	}

	
	void StreamFlowController::reset(bool sendCommandByte, bool sendEventByte, bool clearUartBuffer)
	{
		if(clearUartBuffer){
			while(dataAvailable())stream->read();
		}
		receiveBuffer->reset();
		sendBuffer->reset();
		bytesReceived = 0;
		bytesReceivedSinceCTS = 0;
		bytesSent = 0;
		bytesSentSinceCTS = 0;
		cts = true;
		sentCTSTimeout = false;
		lastCTSrequired = 0;
		rslashed = false;
		rcommand = false;
		revent = false;
		sslashed = false;
		smarked = false;
		error = 0;
		
		if(sendCommandByte){
			//Serial.print("Sending RESET command ");
			//printVitals();
			sendCommand(Command::RESET);
		}
		if(sendEventByte){
			//Serial.print("Sending RESET Event ");
			//printVitals();
			sendEvent(Event::RESET);
		}
		localReset = true;
    }    

    byte StreamFlowController::readFromStream()
	{
		bytesReceived++;
		if(uartLocalBufferSize > 0)bytesReceivedSinceCTS++;
		byte b = stream->read();
		return b;
    }

    void StreamFlowController::writeToStream(byte b, bool flush)
	{
		bytesSent++;
		if(uartRemoteBufferSize > 0)bytesSentSinceCTS++;
		stream->write(b);
		if(flush)stream->flush();
    }

    byte StreamFlowController::peekAtStream()
	{
		return stream->peek();
    }

    int StreamFlowController::dataAvailable()
	{
		return stream->available();
    }

	bool StreamFlowController::isReady(){
		return localReset && remoteReset;
	}


    void StreamFlowController::printVitals()
	{
		Serial.print("bytes sent / received since CTS ");
		Serial.print(bytesSentSinceCTS);
		Serial.print(" / ");
		Serial.print(bytesReceivedSinceCTS);
		Serial.println("");
	
    }

    void StreamFlowController::dumpLog()
	{
		
    }

	//returns number of bytes
    int StreamFlowController::receive()
	{
		//here we can already start receiving data
		int bytesAvailable = dataAvailable(); //we take a value now as this can change independently (i.e works like an interrupt)
		if(bytesAvailable == 0){
			return 0;
		}


		byte b;
		bool isData = true;
		int bytes2read = max(1, min(bytesAvailable, receiveBuffer->remaining())); //we must read atleast one byte
		int bytesRead = 0;
		for(bytesRead = 0; bytesRead < bytes2read; bytesRead++)
		{
			b = peekAtStream(); //we peek in case this is a data byte but the receive buffer is full
			
			if(rcommand)
			{
				b = readFromStream(); //read the command
				switch(b){
					case (byte)Command::RESET:
						//Serial.print("Received RESET command calling reset(false, false) ");
						//printVitals();
						remoteReset = false; //wait for event to come
						reset(false, true); //do NOT send command for remote to reset but do send event
						//Serial.println("Remote commanded local to reset");
						break;

					case (byte)Command::RESET_RECEIVE_BUFFER:
						receiveBuffer->reset();
						break;
					case (byte)Command::RESET_SEND_BUFFER:
						sendBuffer->reset();
						break;

					case (byte)Command::PING_REMOTE:
						sendEvent(Event::PING_RECEIVED);
						break;
				}
				if(commandHandler != NULL)commandHandler(this, b);
				rcommand = false;
				isData = false;
			}
			else if(revent)
			{
				b = readFromStream(); //read the event (NOTE: received byte will be counted here))
				switch(b){
					case (byte)Event::RESET:
						//Serial.print("Received RESET Event ");
						//printVitals();
						bool ready = isReady();
						remoteReset = true;
						if(!ready && isReady()){
							//Serial.print("READY!!!!");
							bytesSentSinceCTS = 0;
                            bytesReceivedSinceCTS = 0;
						}
						break;

					case (byte)Event::CTS_TIMEOUT:
						//Serial.println("Received CTS timeout");
						break;

					case (byte)Event::CTS_REQUEST_TIMEOUT:
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
						b = readFromStream(); //remove from buffer
						isData = false; 
						rcommand = true;
						break;

					case EVENT_BYTE:
						b = readFromStream(); //remove from buffer
						isData = false; 
						revent = true;
						break;

					case CTS_BYTE:
						b = readFromStream(); //remove from uart buffer
						cts = true;
						bytesSentSinceCTS = 0;
						sentCTSTimeout = false; 
						//Serial.println("<----- Received CTS byte...");
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
				if(isReady()){ //both this and the remote are in sync with reset
					if(receiveBuffer->isFull())
					{
						sendEvent(Event::RECEIVE_BUFFER_FULL);
						return bytesRead;
					} 
					else 
					{
						b = readFromStream();
					
						if(rslashed)rslashed = false;
						receiveBuffer->write(b);

						if(maxDatablockSize > 0){
							int n = receiveBuffer->lastSetMarkerCount();
							if(n < 0)n = receiveBuffer->used();
							if(n > maxDatablockSize){
								//TODO: we have to take action here (e.g. reset receive buffer) otherwise there is a risk of this 
								//event firing too many times
								sendEvent(Event::MAX_DATABLOCK_SIZE_EXCEEDED);
							}
						}	
					}
				} else {
					//otherwise remove from buffer
					readFromStream();
				}
			}		

			//Serial.print("-- End of data available loop "); Serial.print(bytesReceived); Serial.println("");
		} //end data available loop 
		
		return bytesRead;
    }

    void StreamFlowController::process()
	{
		if(!isReady()){
			return;
		}

		//remove data from reeive buffer
		if(receiveHandler != NULL && bytesToRead() > 0){
			receiveHandler(this, bytesToRead());
		}
		

		//add data to send buffer
		if(sendHandler != NULL && !sendBuffer->isFull()){
			sendHandler(this, sendBuffer->remaining());
		}
    }

    void StreamFlowController::send()
	{
		if(!isReady())return;

		//check if timeout has been exceeded
		if(!cts && ctsTimeout > 0 && !sentCTSTimeout){
			if((millis() - lastCTSrequired > ctsTimeout)) {
				//Serial.print("Sent CTS timeout... rb / sb remaining / bytesToRead ");
				//printVitals();
				
				sentCTSTimeout = true;
				sendEvent(Event::CTS_TIMEOUT);
				
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
					//will the next byte sent result in reaching the remote buffer limit and therefore we need to wait for the remote to send a CTS byte
					if(requiresCTS(bytesSentSinceCTS + 1, uartRemoteBufferSize)){
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

			//we write to the underlying stream
			writeToStream(b);

			//are we clear to send or do we need to halt until the remote has sent a CTS byte
			if(requiresCTS(bytesSentSinceCTS, uartRemoteBufferSize)){
				//Serial.print("Waiting for CTS byte... ");
				//printVitals();
				cts = false;
				lastCTSrequired = millis();
			}
		} //end sending loop
    }

	
	void StreamFlowController::loop(){
		receive();
	
		process();
	
		if(((uartLocalBufferSize > 0 && requiresCTS(bytesReceivedSinceCTS, uartLocalBufferSize)))){
			if(readyToReceive()){
				//Serial.print("---> Sent CTS  ");
				//printVitals();
				sendCTS();
				bytesReceivedSinceCTS = 0;
			} else {
				//Serial.print("!!!!Cannot send CTS as not ready to receive: rb remaining : ");
				//printVitals();
			}
		}

		send();
	}

	//a basic way to check if the buffer can recevie more data or this process must stop and wait for a CTS byte
    bool StreamFlowController::requiresCTS(unsigned int byteCount, unsigned int bufferSize)
	{
		if(bufferSize == 0)return false;
		int limit = bufferSize - RESERVED_BUFFER_SIZE;
		return byteCount >= limit;
    }


	//used to determine whether to send a CTS byte to the remote (see loop method)
	bool StreamFlowController::readyToReceive(){
		if(readyToReceiveHandler!= NULL){
			return readyToReceiveHandler(this);	
		} else {
			return canReceive(StreamFlowController::UART_LOCAL_BUFFER_SIZE);
		} 
	}

	void StreamFlowController::sendCTS(){
		writeToStream(CTS_BYTE, true);
	}


	void StreamFlowController::sendCommand(Command c){
		sendCommand((byte)c);
	}

	void StreamFlowController::sendCommand(byte b){
		writeToStream(COMMAND_BYTE);
		writeToStream(b, true);
	}

	void StreamFlowController::ping(){
		sendCommand(Command::PING_REMOTE);
	}

	void StreamFlowController::sendEvent(Event e){
		sendEvent((byte)e);
	}

	void StreamFlowController::sendEvent(byte b){
		bool doSend = true;
		if(localEventHandler != NULL)doSend = localEventHandler(this, b);
		
		if(doSend){
			writeToStream(EVENT_BYTE);
			writeToStream(b, true);
		}
	}

    bool StreamFlowController::isSystemByte(byte b){
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

	int StreamFlowController::getLimit(int limit){
		switch(limit){
			case UART_LOCAL_BUFFER_SIZE:
				return uartLocalBufferSize;

			case UART_REMOTE_BUFFER_SIZE:
				return uartRemoteBufferSize;

			case RECEIVE_BUFFER_SIZE:
				return receiveBuffer->getSize();

			case SEND_BUFFER_SIZE:
				return sendBuffer->getSize();

			case MAX_DATABLOCK_SIZE:
				return maxDatablockSize;

			case CTS_LOCAL_LIMIT:
				return uartLocalBufferSize - RESERVED_BUFFER_SIZE;

			case CTS_REMOTE_LIMIT:
				return uartRemoteBufferSize - RESERVED_BUFFER_SIZE;

			default:
				return limit;
		}
	}

	unsigned long StreamFlowController::getBytesReceived(){ return bytesReceived; }
    unsigned long StreamFlowController::getBytesSent(){ return bytesSent; }

    bool StreamFlowController::canReceive(int byteCount){
		if(byteCount == NO_LIMIT){
			return  receiveBuffer->isEmpty();
		} else {
			return receiveBuffer->remaining() >= getLimit(byteCount);
		}
    }

    bool StreamFlowController::canSend(int byteCount){
		if(byteCount == NO_LIMIT){
			return  sendBuffer->isEmpty();
		} else {
			return sendBuffer->remaining() >= getLimit(byteCount);
		}
    }

    byte StreamFlowController::read(){
		return receiveBuffer->read();
    }

	int StreamFlowController::bytesToRead(bool untilMarker){
		if(untilMarker){
			return receiveBuffer->readToMarkerCount();
		} else {
			return receiveBuffer->used();
		}
	}

    bool StreamFlowController::write(byte b, bool addMarker)
	{
		bool retVal = sendBuffer->write(b);
		if(retVal && addMarker)sendBuffer->setMarker();
		return retVal;
    }

	bool StreamFlowController::write(int n, bool addMarker)
	{
		return write<int>(n, addMarker);
	}

	bool StreamFlowController::write(long n, bool addMarker)
	{
		return write<long>(n, addMarker);
	}

    bool StreamFlowController::write(byte *bytes, int size, bool addEndMarker)
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

	void StreamFlowController::endWrite(){
		sendBuffer->setMarker();
	}

    bool StreamFlowController::isClearToSend()
	{
		return cts;
    }
} //end namespace