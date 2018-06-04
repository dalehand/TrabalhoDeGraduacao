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
#define WAITING_FOR 0x02
#define ESTABLISHED 0x03
#define TIMED_OUT 0x04
#define DEFERRED 0x05
#define CLOSING 0x06

// Define Instance IDs and TIMER numbers
#define EXPLICIT 0x01
#define IO_POLL 0x02
#define BIT_STROBE 0x03
#define COS_CYCLIC 0x04
#define ACK_WAIT 0x05
#define UPDATE 0x06

// define message tags which can be used to identify message
#define RECEIVED_ACK 0x01
#define SEND_ACK 0x02
#define ACK_TIMEOUT 0x03
#define	ACK_ERROR 0x04

// define bit positions for global event word
#define IO_POLL_REQUEST 0x0001
#define EXPLICIT_REQUEST 0x0002
#define DUP_MAC_REQUEST 0x0004
#define UNC_PORT_REQUEST 0x0008
#define EXPLICIT_TIMEOUT 0x0040
#define IO_POLL_TIMEOUT 0x0080
#define DEVICE_UPDATE 0x0200
#define ACK_WAIT_TIMEOUT 0x2000
#define FULL_RESET 0x8000

// Define bit positions in Allocation Choice attribute
#define	EXPLICIT_CONXN 	0x01
#define IO_POLL_CONXN 0x02
#define	BIT_STROBE_CONXN 0x04
#define COS_CONXN 0x10
#define CYCLIC_CONXN 0x20

// Service codes (CIP v1-appendixA)
#define GET_REQUEST 0x0E
#define SET_REQUEST 0x10

// Error codes
#define SERVICE_NOT_SUPPORTED 0x08
#define ATTRIB_NOT_SUPPORTED 0x14

//Define message fragment values
#define FIRST_FRAG 0x00
#define MIDDLE_FRAG 0x40
#define LAST_FRAG 0x80
#define ACK_FRAG 0xC0

// define bit positions for global status word
#define OWNED 0x0001
#define ON_LINE 0x0002
#define CONFIGURED 0x0004
#define SELF_TESTING 0x0008
#define OPERATIONAL 0x0010
#define DEVICE_FAULT 0x0800
#define DUP_MAC_FAULT 0x1000
#define BUS_OFF	0x2000
#define NETWORK_FAULT 0x3000
#define LONELY_NODE 0x4000

// DeviceNet error codes
#define RESOURCE_UNAVAILABLE 0x02
#define SERVICE_NOT_SUPPORTED 0x08
#define INVALID_ATTRIB_VALUE 0x09
#define ALREADY_IN_STATE 0x0B
#define OBJECT_STATE_CONFLICT 0x0C
#define ATTRIB_NOT_SETTABLE 0x0E
#define PRIVILEGE_VIOLATION 0x0F
#define DEVICE_STATE_CONFLICT 0x10
#define REPLY_DATA_TOO_LARGE 0x11
#define NOT_ENOUGH_DATA 0x13
#define ATTRIB_NOT_SUPPORTED 0x14
#define TOO_MUCH_DATA 0x15
#define OBJECT_DOES_NOT_EXIST 0x16
#define NO_STORED_ATTRIB_DATA 0x18
#define STORE_OP_FAILURE 0x19
#define INVALID_PARAMETER 0x20
#define INVALID_MEMBER_ID 0x28
#define MEMBER_NOT_SETTABLE 0x29

// DeviceNet additional error codes (object specific)
#define ALLOCATION_CONFLICT 0x01
#define INVALID_ALLOC_CHOICE 0x02
#define INVALID_UNC_REQUEST 0x03

// DeviceNet service codes
#define RESET_REQUEST 0x05
#define START_REQUEST 0x06
#define STOP_REQUEST 0x07
#define CREATE_REQUEST 0x08
#define DELETE_REQUEST 0x09
#define GET_REQUEST 0x0E
#define SET_REQUEST 0x10
#define RESTORE_REQUEST0x15
#define SAVE_REQUEST 0x16
#define	ALLOCATE_CONNECTIONS 0x4B
#define RELEASE_CONNECTIONS 0x4C

// Class IDs
#define IDENTITY_CLASS 0x01
#define ROUTER_CLASS 0x02
#define DEVICENET_CLASS 0x03
#define ASSEMBLY_CLASS 0x04
#define CONNECTION_CLASS 0x05
#define DISCRETE_INPUT_POINT_CLASS 0x08
#define DISCRETE_OUTPUT_POINT_CLASS 0x09

// Connection instance type (CIP v1-3.15)
//#define EXPLICIT 0x00
//#define IO_POLL 0x01

//
#define SUCCESS_RESPONSE 0x80
#define NON_FRAGMENTED 0x7F
#define ERROR_RESPONSE 0x94
#define NO_ADDITIONAL_CODE 0xFF
#define DEFAULT_MASTER_MAC_ID 0xFF
#define	EXPLICIT_TYPE 0x00
#define IO_TYPE 0x01

void strcpy_UCHAR (UCHAR *dst, char *src)
{
  int i=0;
  while(src[i]!='\0')
    {
      dst[i] = src[i];
      i++;
    }
  dst[i]='\0';
}
