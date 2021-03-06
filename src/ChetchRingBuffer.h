#ifndef CHETCH_RING_BUFFER_H
#define CHETCH_RING_BUFFER_H

#include <Arduino.h>

namespace Chetch{

class RingBuffer{

    public:
    
        struct Marker{
            int position = -1; //a negative position indicates non-use
            Marker *nextMarker = NULL;
        };

    private:
    
    public:
        byte *buffer;
        int size;
        bool deleteBuffer = false;
        int readPosition = 0;
        int lastWritePosition = -1;
        int writePosition = 0;
        bool full = false;
        Marker *markers = NULL;
        Marker *upcomingMarker = NULL;
        int markerCount = 0;
    
        RingBuffer();
        RingBuffer(int size);
        RingBuffer(byte *buffer, int size);
        ~RingBuffer();

        void setBuffer(byte *buf, int size);
        byte *getBuffer();
        void reset();
        bool write(byte b);
        bool write(byte *bytes, int size);
        byte read();
        int readToMarker(byte *bytes, int startAt = 0);
        int readToMarkerCount();
        byte peek();
        bool isFull();
        bool isEmpty();
        int used();
        int remaining();
        int getSize();
        Marker *setMarker();
        bool hasMarker();
        bool hasUpcomingMarker();
        Marker *getLastSetMarker();
        int lastSetMarkerCount();
        void updateMarkers();
        int getMarkerCount(bool active = true);
        Marker *getClosestMarker(int position, bool forward = true);
}; //end class
} //end namespace
#endif