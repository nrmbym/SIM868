#ifndef __USART_1_H
#define __USART_1_H
#include "sys.h"
#include "stdio.h"

#define USART1_MAX_RECV_LEN		5000	//最大接收缓存字节数
#define USART1_MAX_SEND_LEN		2000	//最大发送缓存字节数
#define USART1_RX_EN 			1		//0,不接收;1,接收.
#define SRTING_NUM	 			20		//一次最多接收的字符串数量

extern void USART1_Init(u32 bound); //串口1初始化函数
void u1_printf(char* fmt,...);    //串口1打印函数
#endif



