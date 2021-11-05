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

    StreamFlowController::~StreamFlowController()
	{
		if(receiveBuffer != NULL)delete receiveBuffer;
		if(sendBuffer != NULL)delete sendBuffer;
    }

    
    void StreamFlowController::begin(Stream *stream)
	{
		this->stream = stream;
		localReset = false;
		remoteReset = false;
		reset(true); //send command for remote to reset
	}

	bool StreamFlowController::hasBegun(){
		return stream != NULL && localReset == true;
	}

	void StreamFlowController::end(){
		reset(false); 
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

    void StreamFlowController::setReadyToReceiveHandler(bool (*handler)(StreamFlowController*, bool))
	{
		readyToReceiveHandler = handler;
    }

    void StreamFlowController::setDataHandler(void (*handler)(StreamFlowController*, bool))
	{
		dataHandler = handler;
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

	void StreamFlowController::reset(bool sendCommandByte)
	{
		while(stream->available())stream->read();
		receiveBuffer->reset();
		sendBuffer->reset();
		bytesReceived = 0;
		bytesSent = 0;
		cts = true;
		remoteRequestedCTS = false;
		localRequestedCTS = false;
		lastRemoteCTSRequest = 0;
		lastCTSrequired = 0;
		rslashed = false;
		rcommand = false;
		revent = false;
		sslashed = false;
		smarked = false;
		error = 0;
		
		if(sendCommandByte){
			remoteReset = false;
			sendCommand(Command::RESET);
		}
		sendEvent(Event::RESET);
		localReset = true;
    }    

    byte StreamFlowController::readFromStream(bool count)
	{
		if(count){
			bytesReceived++;
		}
		byte b = stream->read();

		//Flow control
		if(count)sendCTS();

		return b;
    }

    void StreamFlowController::writeToStream(byte b, bool count, bool flush)
	{
		if(count){
			bytesSent++;
		}
		stream->write(b);
		if(flush)stream->flush();

		//Flow control by byte counting
		if(requiresCTS(bytesSent, uartRemoteBufferSize)){
			cts = false;
			lastCTSrequired = millis();
		}
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

    void StreamFlowController::dumpLog()
	{
		
    }

    void StreamFlowController::receive()
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
				b = readFromStream(false); //read the event
				switch(b){
					case (byte)Event::RESET:
						remoteReset = true;
						break;

					case (byte)Event::CTS_TIMEOUT:
						remoteRequestedCTS = true;
						lastRemoteCTSRequest = millis(); //for a timeout
						break;

					case (byte)Event::CTS_REQUEST_TIMEOUT:
                        localRequestedCTS = false; //remote request has timedout so reset this flag so the local can try and request again
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
							localRequestedCTS = false; //request has been granted
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
			if(isData)
			{
				if(isReady()){ //both this and the remote are in sync with reset
					b = readFromStream();
					if(receiveBuffer->isFull())
					{
						//TODO: we have to take action here (e.g. reset receive buffer) otherwise there is a risk of this 
						//event firing too many times
						sendEvent(Event::RECEIVE_BUFFER_FULL);
						continueReceiving = false;
					} 
					else 
					{
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
					readFromStream(false);
				}
			}		
		} //end data available loop      
    }

    void StreamFlowController::process()
	{
		if(!isReady())return;

		handleData(false);

		if(remoteRequestedCTS){
			if(readyToReceive(true)){
				sendCTS(true);
			} else {
				if(millis() - lastRemoteCTSRequest > 2000){
					remoteRequestedCTS = false;
					sendEvent(Event::CTS_REQUEST_TIMEOUT);
				}
			}
		}

		if(sendHandler != NULL && !sendBuffer->isFull()){
			sendHandler(this, sendBuffer->remaining());
		}
    }

    void StreamFlowController::send()
	{
		if(!isReady())return;

		//check if timeout has been exceeded
		if(!cts && ctsTimeout > 0 && !localRequestedCTS){
			if((millis() - lastCTSrequired > ctsTimeout)) {
				sendEvent(Event::CTS_TIMEOUT);
				localRequestedCTS = true;
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

			//we write to the underlying stream
			writeToStream(b);
		} //end sending loop
    }

	void StreamFlowController::loop(){
		receive();
		process();
		send();
	}

	void StreamFlowController::handleData(bool endOfData){
		if(dataHandler != NULL){
			dataHandler(this, endOfData);
		}
		if(receiveHandler != NULL && bytesToRead() > 0){
			receiveHandler(this, bytesToRead());
		}
	}

    bool StreamFlowController::requiresCTS(unsigned int byteCount, unsigned int bufferSize)
	{
		if(bufferSize <= 0)return false;
		int limit = bufferSize - RESERVED_BUFFER_SIZE;
		return byteCount == limit;
    }

	bool StreamFlowController::readyToReceive(bool request4cts){
		if(readyToReceiveHandler!= NULL){
			return readyToReceiveHandler(this, request4cts);	
		} else if(request4cts){ //if the remoe has requested a cts to be sent
			return canReceive(StreamFlowController::UART_LOCAL_BUFFER_SIZE);
		} else { //Normal flow control}
			return bytesToRead() == 0;
		}
	}

	bool StreamFlowController::sendCTS(bool overrideFlowControl){
		if(overrideFlowControl || (requiresCTS(bytesReceived, uartLocalBufferSize) && readyToReceive(false)))
		{
			writeToStream(CTS_BYTE, false, true);
			bytesReceived = 0;
			remoteRequestedCTS = false; //close the request
			return true; 
		} else {
			return false;
		}
	}


	void StreamFlowController::sendCommand(Command c){
		sendCommand((byte)c);
	}

	void StreamFlowController::sendCommand(byte b){
		writeToStream(COMMAND_BYTE, false);
		writeToStream(b, false, true);
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
			writeToStream(EVENT_BYTE, false);
			writeToStream(b, false, true);
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

	unsigned int StreamFlowController::getBytesReceived(){ return bytesReceived; }
    unsigned int StreamFlowController::getBytesSent(){ return bytesSent; }

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