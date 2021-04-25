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
    }

    byte StreamWithCTS::readFromStream(){
	return stream->read();
    }

    void StreamWithCTS::writeToStream(byte b){
	stream->write(b);
    }

    bool StreamWithCTS::streamIsAvailable(){
	return stream->available();
    }

    bool StreamWithCTS::receive(){
      bool sentCTS = false;
      while(streamIsAvailable()){
        byte b = readFromStream();
	if(!cts && b == CTS_BYTE){
          cts = true;
          bytesSent = 0;
        } else {
	  bytesReceived++;
          receiveBuffer.write(b);
          if(bytesReceived >= uartBufferSize - 1){ //this is 1 less so we guaranteed to have space for sending a CTS_BYTE
              writeToStream(CTS_BYTE);
              bytesReceived = 0;
              sentCTS = true;
              break;
          }
        }
      }
      return sentCTS;
    }

    bool StreamWithCTS::send(){
      while(!sendBuffer.isEmpty() && cts){
        writeToStream(sendBuffer.read());
        bytesSent++;
        if(bytesSent >= uartBufferSize - 1){ //this is 1 less so we guaranteed to have space for sending a CTS_BYTE
          cts = false;
          break;
        }
      }
      return cts;
    }

    bool StreamWithCTS::isEmpty(){
      return receiveBuffer.isEmpty();
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

} //end namespace