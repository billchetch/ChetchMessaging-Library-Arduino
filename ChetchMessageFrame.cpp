#include <Arduino.h>
#include "ChetchMessageFrame.h"


namespace Chetch{


    long MessageFrame::bytesToLong(byte *bytes, int offset, int numberOfBytes){
      long retVal = 0L;
      for(int i = offset; i < offset + numberOfBytes; i++){
        retVal += (long)bytes[i] << (8*(i - offset));
      }
      return retVal;
    }

    int MessageFrame::bytesToInt(byte *bytes, int offset, int numberOfBytes){
        return (int)bytesToLong(bytes, offset, numberOfBytes);
    }

    void MessageFrame::longToBytes(byte *bytes, long n, int offset, int padToLength){
      for(int i = 0; i < padToLength; i++){
        bytes[offset + i] = (byte)(n >> 8*i);
      }
    }

    void MessageFrame::intToBytes(byte *bytes, int n, int offset, int padToLength){
      longToBytes(bytes, (long)n, offset, padToLength);
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

    MessageFrame::MessageFrame(FrameSchema schema, MessageEncoding encoding){
      this->schema = schema;
      dimensions = new Dimensions(schema);

      bytes = new byte[dimensions->getPayloadIndex() + 256 + dimensions->checksum];

      header = &bytes[0];
      payload = &bytes[dimensions->getPayloadIndex()];

      header[0] = (byte)schema;
      setEncoding(encoding);
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

    void MessageFrame::setPayload(byte *payload, int payloadSize){
      if(dimensions == NULL || bytes == NULL){
        return;
      }

      dimensions->payload = payloadSize;
      intToBytes(bytes, payloadSize, dimensions->getPayloadSizeIndex(), dimensions->payloadSize);

      for(int i = 0; i < payloadSize; i++){
        this->payload[i] = payload[i];
      }
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

      if(startedAdding > 0 && millis() - startedAdding > MESSAGE_FRAME_ADD_TIMEOUT){
         complete = true;
         error = MessageFrame::FrameError::ADD_TIMEOUT;
         return true;
      }

      if(position == 0){
        startedAdding = millis();

        //Check the first byte is valid as this determines the dimensions of frame we are dealing with
        if(b != (byte)schema){
          error = MessageFrame::FrameError::NON_VALID_SCHEMA;
          complete = true;
          return true;
        }

      } else if(position == dimensions->getPayloadIndex()){
        //here we have the header so make some assignments
        encoding = (MessageFrame::MessageEncoding)header[dimensions->getEncodingIndex()];

        //specify payload size
        dimensions->payload = bytesToInt(header, dimensions->getPayloadSizeIndex(), dimensions->payloadSize);

        //specify checksum index
        if(dimensions->checksum > 0){
          checksum = &bytes[dimensions->getChecksumIndex()];
        }
      }

      bytes[position] = b;
      position++;

      if(dimensions->payload > 0 && position == dimensions->getSize()){
        complete = true;
      }

      return complete;
    }

    bool MessageFrame::validate(){
      if(error != FrameError::NO_ERROR){
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

    void MessageFrame::reset(){
      startedAdding = -1;
      position = 0;
      complete = false;
      error = FrameError::NO_ERROR;
      dimensions->payload = 0;
    }


    bool MessageFrame::write(Stream *stream){
      if(dimensions == NULL){
        error = FrameError::NO_DIMENSIONS;
        return false;
      }
      if(dimensions->payload == 0){
        error = FrameError::NO_PAYLOAD;
        return false;
      }

      int n = dimensions->getSize();
      if(n == 0)return false;

      stream->write(bytes, n);

      return true;
    }
} //end namespace