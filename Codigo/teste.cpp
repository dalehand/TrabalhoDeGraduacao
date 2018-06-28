#include<iostream>
#include<thread>
#include "SCan.h"

int main ()
{
	std::thread t1(&sock_can_read);
	std::thread t2(&sock_can_write);
	can_id_write = 0b10111100101;
	global_CAN_write[0] = 0x11;
	global_CAN_write[1] = 0x22;
	global_CAN_write[LENGTH] = 2;
	write_flag = 1;
	while(1);
	return 0;
}

