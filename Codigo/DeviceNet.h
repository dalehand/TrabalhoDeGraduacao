typedef int BOOL;
typedef unsigned char UCHAR;
typedef unsigned int UINT;
typedef unsigned long ULONG;

#define LOBYTE(w) ((UCHAR)(w))
#define HIBYTE(w) ((UCHAR)((int)(w) >> 8))

#define MAC_ID 60 // Between 0-62 (63 reserved by devicenet)
#define BAUD_RATE 0 // 0=125k, 1=250k, 2=250k
#define NUM_SENSORS 45 // Number of sensors (Max 64)
#define NUM_TRAINS 2 // Number of trains (Max 4)


#define FALSE 0
#define TRUE 1
#define OK 1
#define NO_RESPONSE 0
#define BUFSIZE 80
#define LENGTH BUFSIZE-1
#define MESSAGE_TAG BUFSIZE-2
#define PATH_SIZE 10

// Connection state atribbutes (CIP v1-3.14)
#define NON_EXISTENT 0x00
#define CONFIGURING 0x01
#define WAITING_FOR_CONNECTION_ID 0x02
#define ESTABLISHED 0x03
#define TIMED_OUT 0x04
#define DEFERRED_DELETE 0x05
#define CLOSING 0x06

// Service codes (CIP v1-appendixA)
#define GET_REQUEST 0x0E
#define SET_REQUEST 0x10

// Error codes
#define SERVICE_NOT_SUPPORTED 0x08
#define ATTRIB_NOT_SUPPORTED 0x14

// Class IDs
#define IDENTITY_CLASS 0x01
#define ROUTER_CLASS 0x02
#define DEVICENET_CLASS 0x03
#define ASSEMBLY_CLASS 0x04
#define CONNECTION_CLASS 0x05
#define DISCRETE_INPUT_POINT_CLASS 0x08
#define DISCRETE_OUTPUT_POINT_CLASS 0x09

// Connection instance type (CIP v1-3.15)
#define EXPLICIT 0x00
#define IO_POLL 0x01

//
#define SUCCESS_RESPONSE 0x80
#define NON_FRAGMENTED 0x7F
#define ERROR_RESPONSE 0x94
#define NO_ADDITIONAL_CODE 0xFF

