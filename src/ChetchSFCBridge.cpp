#include "ChetchSFCBridge.h"

namespace Chetch
{
    StreamFlowController* SFCBridge::iStream;
    StreamFlowController* SFCBridge::xStream;
    int SFCBridge::maxDataBlockSize;
    
    void SFCBridge::init(StreamFlowController *internalStream, StreamFlowController *externalStream, int maxDataSize){
        iStream = internalStream;
        xStream = externalStream;

        iStream->setReadyToReceiveHandler(handleIReadyToReceive);
        iStream->setReceiveHandler(handleIReceive);
        iStream->setEventHandlers(NULL, handleIRemoteEvent);
        
        xStream->setReadyToReceiveHandler(handleXReadyToReceive);
        xStream->setReceiveHandler(handleXReceive);
        xStream->setCommandHandler(handleXCommand);
        
        maxDataBlockSize = maxDataSize;
    }

    //Internal stream
    bool SFCBridge::handleIReadyToReceive(StreamFlowController *stream, bool request4cts){
        if(request4cts){
            return stream->canReceive(StreamFlowController::UART_LOCAL_BUFFER_SIZE);
        } else {
            return xStream->canSend(maxDataBlockSize) && stream->bytesToRead() == 0;
        }
    }

    void SFCBridge::handleIReceive(StreamFlowController *stream, int b2r){
      if(xStream->canSend(b2r)){
        for(int i = 0; i < b2r; i++){
          xStream->write(stream->read());
        }
        xStream->endWrite();
      }
    }

    void SFCBridge::handleIRemoteEvent(StreamFlowController *stream, byte evt){
      xStream->sendEvent(200 + evt);
    }

    //External stream
    bool SFCBridge::handleXReadyToReceive(StreamFlowController *stream, bool request4cts){
      if(request4cts){
        return stream->canReceive(StreamFlowController::UART_LOCAL_BUFFER_SIZE);
      } else {
        return iStream->canSend(maxDataBlockSize) && stream->bytesToRead() == 0;
      }
    }

    void SFCBridge::handleXReceive(StreamFlowController *stream, int b2r){
      if(iStream->canSend(b2r)){
        for(int i = 0; i < b2r; i++){
          iStream->write(stream->read());
        }
        iStream->endWrite();
      }
    }

    void SFCBridge::handleXCommand(StreamFlowController *stream, byte cmd){
      switch(cmd){
        case (byte)StreamFlowController::Command::PING:
          iStream->ping();
          break;

        case (byte)StreamFlowController::Command::RESET:
          iStream->sendCommand(cmd);
          break;
      }
    }

    void SFCBridge::beginIStream(Stream *stream){
      iStream->begin(stream);
    }

    void SFCBridge::beginXStream(Stream *stream){
      xStream->begin(stream);
    }

    void SFCBridge::loop(){
      xStream->loop();
      iStream->loop();
    }    
} //end namespace