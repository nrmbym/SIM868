#include "delay.h"
#include "usart_1.h"
#include "stdarg.h"	 	  	 
#include "string.h"	 
#include "timer.h"
#include "usart3.h"
#include "sim868.h"

void USART1_Init(u32 bound)
{
	NVIC_InitTypeDef NVIC_InitStructure;
	GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);	// GPIOA时钟
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1,ENABLE); //串口2时钟使能

	USART_DeInit(USART1);  //复位串口2
	//USART1_TX   PA9
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9; //
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;	//复用推挽输出
	GPIO_Init(GPIOA, &GPIO_InitStructure); //初始化

	//USART1_RX	  PA10
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;//
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;//浮空输入
	GPIO_Init(GPIOA, &GPIO_InitStructure);  //初始化

	USART_InitStructure.USART_BaudRate = bound;//波特率一般设置为115200;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;//字长为8位数据格式
	USART_InitStructure.USART_StopBits = USART_StopBits_1;//一个停止位
	USART_InitStructure.USART_Parity = USART_Parity_No;//无奇偶校验位
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;//无硬件数据流控制
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;	//收发模式

	USART_Init(USART1, &USART_InitStructure); //初始化串口1


	USART_Cmd(USART1, ENABLE);                    //使能串口 

	//使能接收中断
	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE); 
	//开启ERR中断
	USART_ITConfig(USART1, USART_IT_ERR, ENABLE);	
	//关闭发送中断
	USART_ITConfig(USART1, USART_IT_TXE, DISABLE); 

	//设置中断优先级
	NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority=0 ;//抢占优先级3
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;		//子优先级3
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;			//IRQ通道使能
	NVIC_Init(&NVIC_InitStructure);	//根据指定的参数初始化VIC寄存器
	
	
	
//	u1_data_Pack.USART1_RX_STA=0;		//清零
}

//串口1中断服务函数
//将接收到的数据存入接收缓存中，并将字符串分离
//可以判读是否收到GPS数据
void USART1_IRQHandler(void)
{
	u8 res;	      
	if(USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)//接收到数据
	{
        USART_ClearITPendingBit(USART1,USART_IT_RXNE);		
		res =USART_ReceiveData(USART1);		 
		if(u1_data_Pack.USART1_RX_STA<USART1_MAX_RECV_LEN)	//还可以接收数据
		{			//计数器清空
			   u1_data_Pack.USART1_RX_BUF[u1_data_Pack.USART1_RX_STA++]=res;	//记录接收到的值	 
		}
	} 	
	
	//溢出-如果发生溢出需要先读SR,再读DR寄存器 则可清除不断入中断的问题
    if(USART_GetFlagStatus(USART1,USART_FLAG_ORE) == SET)
    {
        USART_ClearFlag(USART1,USART_FLAG_ORE);	//读SR
        USART_ReceiveData(USART1);				//读DR
    }
}  
//串口1,printf 函数
//确保一次发送数据不超过USART1_MAX_SEND_LEN字节
void u1_printf(char* fmt,...)
{  
	u16 i,j; 
	va_list ap; 
	va_start(ap,fmt);
	vsprintf((char*)u1_data_Pack.USART1_TX_BUF,fmt,ap);
	va_end(ap);
	i=strlen((const char*)u1_data_Pack.USART1_TX_BUF);		//此次发送数据的长度
	for(j=0;j<i;j++)							//循环发送数据
	{
		while(USART_GetFlagStatus(USART1,USART_FLAG_TC)==RESET); //循环发送,直到发送完毕   
		USART_SendData(USART1,u1_data_Pack.USART1_TX_BUF[j]); 
	} 
}
