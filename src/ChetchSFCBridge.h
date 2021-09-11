
#ifndef CHETCH_SFC_BRIDGE_H
#define CHETCH_SFC_BRIDGE_H

#include "ChetchStreamFlowController.h"

namespace Chetch
{

    class SFCBridge{
        public:
            static StreamFlowController *xStream;
            static StreamFlowController *iStream;
            static int maxDataBlockSize;

            static void init(StreamFlowController *internalStream, StreamFlowController *externalStream, int maxDataSize);
    
            static bool handleXReadyToReceive(StreamFlowController *stream, bool request4cts);
            static void handleXReceive(StreamFlowController *stream, int b2r);
            static void handleXCommand(StreamFlowController *stream, byte cmd);
  
            static bool handleIReadyToReceive(StreamFlowController *stream, bool request4cts);
            static void handleIReceive(StreamFlowController *stream, int b2r);
            static void handleIRemoteEvent(StreamFlowController *stream, byte evt);

            static void beginIStream(Stream *stream);
            static void beginXStream(Stream *stream);
            static void loop();
    };

}
#endif