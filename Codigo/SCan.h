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

int write_flag = 0;
int read_flag = 0;
int sock_can_on = 1;
UCHAR canid, candlc;
UCHAR candata[BUFSIZE];

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

void sock_can()
{
  struct ifreq ifr;
  struct sockaddr_can addr;
  struct can_frame frame;
  int s;
  int count=0;
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
      canid = frame.can_id;
      candlc = frame.can_dlc;
      for(count=0;count<candlc;count++)
	{
	  candata[count]=frame.data[count];
	}
      read_flag = 1;
      if(write_flag)
	{
	  frame.can_id = canid;
	  frame.can_dlc = candlc;
	  for(count=0;count<candlc;count++)
	    {
	      frame.data[count]=candata[count];
	    }
	  //strcpy((char *)frame.data, candata);
	  write(s,&frame,sizeof(frame));
	  write_flag=0;
	}
    }
  close(s);
}
