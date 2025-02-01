#include <Arduino.h>
#include "ChetchMessageFrame.h"

namespace Chetch{

    void MessageFrame::intToBytes(byte *bytes, int n, int offset, int padToLength){
      for(int i = 0; i < padToLength; i++){
        bytes[offset + i] = (byte)(n >> 8*i);
      }
    }

    int MessageFrame::bytesToInt(byte *bytes, int offset, int numberOfBytes){
      int retVal = 0;
        for(int i = 0; i < numberOfBytes; i++){
            ((byte *)&retVal)[i] = bytes[i + offset];
        }
        return retVal;
    }

    bool MessageFrame::isValidSchema(byte b){
        return b > 0 && b < 3;
    }

    bool MessageFrame::isValidEncoding(byte b){
      return b > 0 && b < 6;
    }

    byte MessageFrame::simpleChecksum(byte *bytes, int startIdx, int numberOfBytes){
      byte v = 0;
      for(int i = startIdx; i < startIdx + numberOfBytes; i++){
        v += bytes[i];
      }
      return v;
    }

    
    MessageFrame::MessageFrame(){

    }

    MessageFrame::MessageFrame(FrameSchema schema, int maxPayload){
      this->schema = schema;
      dimensions = new Dimensions(schema);

      this->maxPayload = maxPayload;
      bytes = new byte[dimensions->getFrameSize() + this->maxPayload];

      header = &bytes[0];
      payload = &bytes[dimensions->getPayloadIndex()];

      header[0] = (byte)schema;
    }

    MessageFrame::~MessageFrame(){
      reset();
      if(dimensions != NULL)delete dimensions;
      if(bytes != NULL)delete[] bytes;
    }

    void MessageFrame::setEncoding(MessageEncoding encoding){
      this->encoding = encoding;
      header[1] = (byte)encoding;
    }

    MessageFrame::MessageEncoding MessageFrame::getEncoding(){
      return this->encoding;
    }

    void MessageFrame::updatePayload(int payloadSize){
	    if(payloadSize > dimensions->payload){
	        dimensions->payload = payloadSize;
      	    intToBytes(bytes, dimensions->payload, dimensions->getPayloadSizeIndex(), dimensions->payloadSize);
	    }
    }

    void MessageFrame::setPayload(byte *payload, int payloadSize){
      if(dimensions == NULL || bytes == NULL){
        return;
      }

      updatePayload(payloadSize);
      
      for(int i = 0; i < payloadSize; i++){
        this->payload[i] = payload[i];
      }
    }

    void MessageFrame::add2payload(byte val, int index){
	    this->payload[index] = val;
	    updatePayload(index + sizeof(val));
    }

    

    byte *MessageFrame::getBytes(bool addChecksum){
      if(dimensions->checksum != 0 && addChecksum){
        switch(schema){
          case SMALL_SIMPLE_CHECKSUM:
            bytes[dimensions->getChecksumIndex()] = simpleChecksum(bytes, 0, dimensions->getChecksumIndex());;
            break;
        }
      }
      return bytes;
    }

    bool MessageFrame::add(byte b){
      if(complete)return true;

      if(addPosition == 0){
        //Check the first byte is valid as this determines the dimensions of frame we are dealing with
        if(b != (byte)schema){
          error = MessageFrame::FrameError::NON_VALID_SCHEMA;
          complete = true;
          return true;
        }

      } else if(addPosition == dimensions->getPayloadIndex()){
        //here we have the header so make some assignments
        encoding = (MessageFrame::MessageEncoding)header[dimensions->getEncodingIndex()];

        //specify payload size
        dimensions->payload = bytesToInt(header, dimensions->getPayloadSizeIndex(), dimensions->payloadSize);

        //specify checksum index
        if(dimensions->checksum > 0){
          checksum = &bytes[dimensions->getChecksumIndex()];
        }
      }

      bytes[addPosition] = b;
      addPosition++;

      if(dimensions->payload > 0 && addPosition == dimensions->getSize()){
        complete = true;
      }

      return complete;
    }

    bool MessageFrame::add(byte *bytes, int byteCount){
        if(byteCount <= 0){
            error = MessageFrame::FrameError::INCOMPLETE_DATA;
            return true;
        }

        bool complete = false;
        for(int i = 0; i < byteCount; i++){
            complete = add(bytes[i]);
        }
        return complete;
    }

    bool MessageFrame::validate(){
      if(error != FrameError::NO_ERROR){
        return false;
      }
      if(!complete){
        error = MessageFrame::FrameError::INCOMPLETE_DATA;
        return false;
      }

      if(dimensions == NULL){
        error = FrameError::NO_DIMENSIONS;
        return false;
      }
      if(payload == NULL || dimensions->payload == 0){
        error = FrameError::NO_PAYLOAD;
        return false;
      }
      if(header == NULL){
        error = FrameError::NO_HEADER;
        return false;
      }
      if(!isValidSchema(header[0]) || header[0] != (byte)schema){
        error = FrameError::NON_VALID_SCHEMA;
        return false;
      }
      if(!isValidEncoding(header[1])){
        error = FrameError::NON_VALID_ENCODING;
        return false;
      }

      //confirm checksum
      if(dimensions->checksum == 0){
        return true;
      } else {
        switch(schema){
          case SMALL_SIMPLE_CHECKSUM:
            byte csum = simpleChecksum(bytes, 0, dimensions->getChecksumIndex());
            if(checksum[0] != csum)error = FrameError::CHECKSUM_FAILED;
            break;
        }
      }

      return error == FrameError::NO_ERROR;
    }

    byte *MessageFrame::getPayload(){ return payload; }
    int MessageFrame::getPayloadSize(){ return dimensions == NULL ? -1 : dimensions->payload; }
    int MessageFrame::getSize(){ return dimensions == NULL ? -1 : dimensions->getSize(); }
    int MessageFrame::getFrameSize(){ return dimensions == NULL ? -1 : dimensions->getFrameSize(); }
    int MessageFrame::getMaxSize(){ return maxPayload + getFrameSize(); }

    void MessageFrame::reset(){
      addPosition = 0;
      complete = false;
      error = FrameError::NO_ERROR;
      dimensions->payload = 0;
    }

    void MessageFrame::write(Stream* stream){
      byte* bytes = getBytes();
      for(int i = 0; i < getSize(); i++){
        //stream->print(bytes[i]);
        stream->write(bytes[i]);
      }
      //stream->print(getSize());
    }

    bool MessageFrame::read(Stream* stream){
      while(stream->available()){
        byte b = stream->read();
        if(add(b))break;
      }
      return complete;
    }
} //end namespace