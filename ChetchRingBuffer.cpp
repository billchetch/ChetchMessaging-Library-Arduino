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

      Marker *m = markers;
      Marker *pm;
      while(m != NULL){
	pm = m;
      	m = m->nextMarker;
	delete pm;
      }
    }

    void RingBuffer::setBuffer(byte *buf, int size){
      if(deleteBuffer && buffer != NULL)delete[] buffer;
      buffer = buf;
      deleteBuffer = false;
      this->size = size;
    }

    byte *RingBuffer::getBuffer(){
      return buffer;
    }

    void RingBuffer::reset(){
      readPosition = 0;
      writePosition = 0;
      lastWritePosition = -1;
      full = false;
      upcomingMarker = NULL;
      Marker* m = markers;
      while(m != NULL){
        m->position = -1;
        m = m->nextMarker;
      }
    }

    bool RingBuffer::write(byte b){
      if(isFull()){
        return false;
      } else {
	lastWritePosition = writePosition;
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
        if(upcomingMarker != NULL && upcomingMarker->position == readPosition){
	        upcomingMarker->position = -1;
	        updateMarkers();
        }
        if(!isEmpty()){
	        readPosition = (readPosition + 1) % size;
        }
        full = false;
        return b;
    }

    int RingBuffer::readToMarker(byte *bytes, int startAt){
        if(!hasUpcomingMarker())return 0;

        int n = 0;
        while(true){
            bool hm = hasMarker();
            byte b = read();
            bytes[startAt + n] = b;
            n++;
            if(hm)break;
        }
        return n;
    }

    int RingBuffer::readToMarkerCount(){
        if(!hasUpcomingMarker())return 0;

        int r = upcomingMarker->position - readPosition;
        if(r < 0)r += size;
        return r + 1;
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

    int RingBuffer::setMarker(){
      if(isEmpty())return -1;

      int markerPosition = lastWritePosition;
      
      Marker *m = NULL;
      if(markers == NULL){
	m = new Marker();
	markers = m;
	markerCount++;
      } else {
	Marker *m1 = markers;
	while(m1->nextMarker != NULL && m1->position >= 0 && m1->position != markerPosition){
	  m1 = m1->nextMarker;
	}
	if(m1->position < 0 || m1->position == markerPosition){
	  m = m1;
	} else {
	  m = new Marker();
	  m1->nextMarker = m;
	  markerCount++;
	}
      }
      m->position = markerPosition;
      updateMarkers();
      return m->position;
    }

    bool RingBuffer::hasMarker(){
      if(upcomingMarker != NULL && upcomingMarker->position == readPosition){
     	return true;
      } else {
        return false;
      }
    }

    bool RingBuffer::hasUpcomingMarker(){
        if(hasMarker()){
            return true;
        } else if(upcomingMarker != NULL) {
            return upcomingMarker->position != -1;
        } else {
            return false;
        }
    }

    void RingBuffer::updateMarkers(){
      if(markers == NULL)return;
      Marker* m = markers;
      Marker* closestMarker = NULL;
      while(m != NULL){
	if(m->position >= 0){
	  if(closestMarker == NULL){
	    closestMarker = m;
	  } else {
	    int diff1 = closestMarker->position - readPosition;
	    if(diff1 < 0)diff1 += size;
	    int diff2 = m->position - readPosition;
	    if(diff2 < 0)diff2 += size;

	    if(diff2 < diff1)closestMarker = m;
	  }
	}
	m = m->nextMarker;
      }
      upcomingMarker = closestMarker;
    }

} //end namespace