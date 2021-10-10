#include "ChetchSFCBridge.h"

namespace Chetch
{
    StreamFlowController* SFCBridge::iStream;
    StreamFlowController* SFCBridge::xStream;
    int SFCBridge::maxDataBlockSize;
    bool SFCBridge::forwarding; //used to supress default handler behaviour (see)
    
    void SFCBridge::init(StreamFlowController *internalStream, StreamFlowController *externalStream, int maxDataSize){
        iStream = internalStream;
        xStream = externalStream;

        iStream->setReadyToReceiveHandler(handleIReadyToReceive);
        iStream->setReceiveHandler(handleIReceive);
        iStream->setEventHandlers(NULL, handleIRemoteEvent);
        
        xStream->setReadyToReceiveHandler(handleXReadyToReceive);
        xStream->setReceiveHandler(handleXReceive);
        xStream->setCommandHandler(handleXCommand);
        xStream->setEventHandlers(handleXLocalEvent, NULL);
        
        maxDataBlockSize = maxDataSize;
        forwarding = false;
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
        switch(evt){
            case (byte)StreamFlowController::Event::RESET: 
                forwarding = true;
                xStream->sendEvent(evt); //forward the event on;
                forwarding = false;
                break;

            default:
                xStream->sendEvent(200 + evt); //forward the event on with a bump
                break;
        }
        
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
        case (byte)StreamFlowController::Command::PING_REMOTE:
          iStream->ping();
          break;

        case (byte)StreamFlowController::Command::RESET:
          iStream->sendCommand(cmd); //forward the reset command
          break;
      }
    }

    bool SFCBridge::handleXLocalEvent(StreamFlowController *stream, byte evt){
        switch(evt){
            //cancel sending event back to remote. Instead we use handleXCommand to forward reset command to Internal stream remote.
            //Once the internal stream remote has reset and sent event bak to internal stream local we catch it with handleIEvent and
            //then forward it on to X remote.
            case (byte)StreamFlowController::Event::RESET: 
                return forwarding;

            default:
                return true;
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