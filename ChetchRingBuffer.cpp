#include <ChetchRingBuffer.h>

namespace Chetch{

    RingBuffer::RingBuffer(){
       deleteBuffer = false;
    }

    RingBuffer::RingBuffer(int size){
      this->buffer = new byte[size];
      deleteBuffer = true;
      this->size = size;
      reset();
    }

    RingBuffer::RingBuffer(byte *buffer, int size){
      this->buffer = buffer;
      deleteBuffer = false;
      this->size = size;
      reset();
    }

    RingBuffer::~RingBuffer(){
      if(deleteBuffer && buffer != NULL)delete[] buffer;
    }

    void RingBuffer::setBuffer(byte *buf, int size){
      if(deleteBuffer && buffer != NULL)delete[] buffer;
      buffer = buf;
      deleteBuffer = false;
      this->size = size;
    }

    void RingBuffer::reset(){
      readPosition = 0;
      writePosition = 0;
      full = false;
    }

    bool RingBuffer::write(byte b){
      if(isFull()){
        return false;
      } else {
        buffer[writePosition] = b;
        writePosition = (writePosition + 1) % size;
        full = writePosition == readPosition;
        return true;
      }
    }

    bool RingBuffer::write(byte *bytes, int size){
	if(remaining() < size){
		return false;
	} else {
		for(int i = 0; i < size; i++){
			if(!write(bytes[i]))return false;
		}
		return true;
	}
    }

    byte RingBuffer::read(){
      byte b = buffer[readPosition];
      if(!isEmpty()){
        readPosition = (readPosition + 1) % size;
      }
      full = false;
      return b;
    }

    byte RingBuffer::peek(){
        return buffer[readPosition];
    }

    bool RingBuffer::isFull(){
      return full;
    }

    bool RingBuffer::isEmpty(){
      return readPosition == writePosition && !isFull();
    }

    int RingBuffer::used(){
      if(isFull()){
        return size;
      } else if(isEmpty()){
        return 0;
      } else if(writePosition > readPosition){
        return writePosition - readPosition;
      } else {
        return (size - readPosition) + writePosition;
      }
    }

    int RingBuffer::remaining(){
      return size - used(); 
    }

    int RingBuffer::getSize(){
	return size;
    }
} //end namespace