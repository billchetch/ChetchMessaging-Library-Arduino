#ifndef CHETCH_MESSAGE_FRAME_H
#define CHETCH_MESSAGE_FRAME_H

#define MESSAGE_FRAME_ADD_TIMEOUT 1000

namespace Chetch{

class MessageFrame{

  public:
      enum FrameSchema{
        SMALL_NO_CHECKSUM = 1,      //FrameSchema = 1 byte, Encoding = 1 byte, Payload size = 1 byte, Payload = max 255 bytes
        SMALL_SIMPLE_CHECKSUM,      //FrameSchema = 1 byte, Encoding = 1 byte, Payload size = 1 byte, Payload = max 255 bytes, Checksum = 1 byte
      };

      enum MessageEncoding{
        SYSTEM_DEFINED = 1,
        XML,
        QUERY_STRING,
        POSITIONAL,
        BYTES_ARRAY,
        JSON
      };

      enum FrameError{
        NO_ERROR = 0,
        NO_DIMENSIONS,
        NO_HEADER,
        NO_PAYLOAD,
        INCOMPLETE_DATA,
        NON_VALID_SCHEMA,
        NON_VALID_ENCODING,
        CHECKSUM_FAILED,
        ADD_TIMEOUT
      };

    class Dimensions{
      public:
        byte schema;
        byte encoding;
        byte payloadSize = 0;
        int payload = 0;
        byte checksum;

        Dimensions(FrameSchema frameSchema){
          schema = 1;
          encoding = 1;
          switch(frameSchema){
            case SMALL_NO_CHECKSUM:
              payloadSize = 1;
              checksum = 0;
              break;

            case SMALL_SIMPLE_CHECKSUM:
              payloadSize = 1;
              checksum = 1;
              break;
          }
        }

        int getSchemaIndex(){ return 0; }
        int getEncodingIndex(){ return getSchemaIndex() + schema; }
        int getPayloadSizeIndex() { return getEncodingIndex() + encoding; }
        int getPayloadIndex() { return getPayloadSizeIndex() + payloadSize; }
        int getChecksumIndex(){ return payload > 0 ? getPayloadIndex() + payload : -1; }
        int getSize(){ return payload > 0 && payloadSize > 0 ? getChecksumIndex() + checksum : -1; }
    };

    static long bytesToLong(byte *bytes, int offset, int numberOfBytes);
    static int bytesToInt(byte *bytes, int offset, int numberOfBytes);
    static void longToBytes(byte *bytes, long n, int offset, int padToLength);
    static void intToBytes(byte *bytes, int n, int offset, int padToLength);
    static bool isValidSchema(byte b);
    static bool isValidEncoding(byte b);
    static byte simpleChecksum(byte *bytes, int startIdx, int numberOfBytes);

    FrameSchema schema;
    MessageEncoding encoding;
    Dimensions *dimensions = NULL;
    byte *header = NULL;
    byte *payload = NULL;
    byte *checksum = NULL;
    byte *bytes = NULL; //will point to header or payload or checkum

    long startedAdding = -1; //time in millis where we started addinig bytes
    int position = 0;
    bool complete = false;

    FrameError error = FrameError::NO_ERROR;

    MessageFrame();
    MessageFrame(FrameSchema schema, MessageEncoding encoding = MessageEncoding::BYTES_ARRAY);
    ~MessageFrame();

    void setEncoding(MessageEncoding encoding);
    void setPayload(byte *payload, int payloadSize);
    byte *getBytes(bool addChecksum = false);
    bool add(byte b);
    bool validate();

    byte *getPayload();
    int getPayloadSize();
    int getSize();

    void reset();

    bool write(Stream *stream);
};

} //end namespace

#endif