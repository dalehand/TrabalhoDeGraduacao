#include<linux/can.h>
#include<linux/can/raw.h>
#include<endian.h>
#include<net/if.h>
#include<sys/ioctl.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<unistd.h>
#include<iostream>
#include<thread>
#include<cerrno>
#include<csignal>
#include<cstdint>
#include<cstdio>
#include<cstring>
#include "DeviceNet.h"

int write_flag = 0;
int read_flag = 0;
int sock_can_on = 1;
UINT can_id_write;

UCHAR global_CAN_write[BUFSIZE];

UCHAR global_CAN_buf[BUFSIZE];
UINT global_timer[10];
UINT global_status;
UINT global_event;

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

void sock_can_read()
{
  struct ifreq ifr;
  struct sockaddr_can addr;
  struct can_frame frame;
  int s;
  int valid_message;
  int count = 0;
  memset(&ifr, 0x0, sizeof(ifr));
  memset(&addr, 0x0, sizeof(addr));
  memset(&frame, 0x0, sizeof(frame));

  s = socket(PF_CAN, SOCK_RAW, CAN_RAW);
  strcpy(ifr.ifr_name, "can0");
  ioctl(s, SIOCGIFINDEX, &ifr);
  
  addr.can_ifindex = ifr.ifr_ifindex;
  addr.can_family = PF_CAN;

  bind(s, (struct sockaddr *)&addr, sizeof(addr));

  while(sock_can_on)
    {

      read(s, &frame, sizeof(frame));
      valid_message = 0;

      switch(frame.can_id)
        {
            case 0b10111100101: // Masters I/O Poll Command/Change of State/Cyclic Message (CIPv3c3.7): Group 2 MACID = 60 Message ID = 5
                global_event |= IO_POLL_REQUEST;
		valid_message = 1;
                break;
            case 0b10111100100: // Masters Explicit Request Messages (CIPv3c3.7): Group 2 MACID = 60 Message ID = 4
                global_event |= EXPLICIT_REQUEST;
		valid_message = 1;
                break;
            case 0b10111100111: // Duplicate MAC ID Check Messages: Group 2 MACID = 60 Message ID = 7
                global_event |= DUP_MAC_REQUEST;
		valid_message = 1;
                break;
            case 0b10111100110: // Unconnected Explicit Request Messages (CIPv3c3.7): Group 2 MACID = 60 Message ID = 6
                global_event |= UNC_PORT_REQUEST;
		valid_message = 1;
                break;
            case 0b01111111100: // Slaves I/O Poll Response or Change of State/Cyclic Acknowledge Message (CIPv3c3.7): Group 1 MACID = 60 Message ID = 15
                global_event |= 0x0010;
		valid_message = 1;
                break;
            case 0b10111100011: // Slaves Explicit/ Unconnected Response Messages (CIPv3c3.7): Group 2 MACID = 60 Message ID = 3
                global_event |= 0x0020;
		valid_message = 1;
                break;
		//case 0b10111100111: // Duplicate MAC ID Check Messages: Group 2 MACID = 60 Message ID = 7
                //global_event |= 0x0040;
		//valid_message = 1;
                //break;
	    default:
	        valid_message = 0;
		break;
        }
      if(valid_message)
      {
      global_CAN_buf[LENGTH] = frame.can_dlc;
      printf("%X\t",frame.can_id);
      for(count=0;count<global_CAN_buf[LENGTH];count++)
	{
	  global_CAN_buf[count]=frame.data[count];
	  printf("%02X ",global_CAN_buf[count]);
	}
      }
    }
  close(s);
}

void sock_can_write()
{

  struct ifreq ifr;
  struct sockaddr_can addr;
  struct can_frame frame;
  int s;
  int count = 0;
  memset(&ifr, 0x0, sizeof(ifr));
  memset(&addr, 0x0, sizeof(addr));
  memset(&frame, 0x0, sizeof(frame));

  s = socket(PF_CAN, SOCK_RAW, CAN_RAW);
  strcpy(ifr.ifr_name, "can0");
  ioctl(s, SIOCGIFINDEX, &ifr);
  
  addr.can_ifindex = ifr.ifr_ifindex;
  addr.can_family = PF_CAN;

  bind(s, (struct sockaddr *)&addr, sizeof(addr));
  while(1)
    {
      if(write_flag)
	{
	  frame.can_id = can_id_write;
	  frame.can_dlc = global_CAN_write[LENGTH];
	  for(count=0;count<global_CAN_write[LENGTH];count++)
	    {
	      frame.data[count]=global_CAN_write[count];
	    }
	  //strcpy((char *)frame.data, candata);
	  write(s,&frame,sizeof(frame));
	  write_flag=0;
	}
    }
  close(s);
}

void timer_func()
{
  if (global_timer[EXPLICIT])            // Explicit Connection timer
    {
      global_timer[EXPLICIT]--;           // decrement until zero
      if (global_timer[EXPLICIT] == 0) global_event |= EXPLICIT_TIMEOUT;
    }
  if (global_timer[IO_POLL])             // I/O Poll Connection timer
    {
      global_timer[IO_POLL]--;            // decrement until zero
      if (global_timer[IO_POLL] == 0) global_event |= IO_POLL_TIMEOUT;
    }
  if (global_timer[ACK_WAIT])          	// Acknowlege message timer
    {
      global_timer[ACK_WAIT]--;   	      // decrement until zero
      if (global_timer[ACK_WAIT] == 0) global_event |= ACK_WAIT_TIMEOUT;
    }
  if (global_timer[UPDATE])              // Timer for updating the device
    {
      global_timer[UPDATE]--;   	         // decrement until zero
      if (global_timer[UPDATE] == 0) global_event |= DEVICE_UPDATE;
    }
}

void reset (void)
{
  ((void (code *) (void)) 0x0000) ();
}
