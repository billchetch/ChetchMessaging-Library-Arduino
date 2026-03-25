#ifndef CHETCH_MESSAGE_IO
#define CHETCH_MESSAGE_IO

#include <Arduino.h>

namespace Chetch{

    class MessageIO{
        public:
            void* owner;

        public:
            virtual void begin(void* owner = NULL){ this->owner = owner; }
            virtual bool enqueueMessageToSend(void* sender, byte messageID, byte messageTag = 0) = 0;
            virtual void loop() = 0;
    };
}
#endif