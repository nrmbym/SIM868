#include "led.h"
#include "delay.h"
#include "key.h"
#include "sys.h"
#include "usart.h"
#include "usart3.h"
#include "timer.h"
#include "stmflash.h"
#include "iwdg.h"
#include "usart_1.h"
#include "sim868.h"


/****************************************************
实验室stm32开发板按键备注
开发板原理图上的按键备注是错的  其余的标注按键都没有
红色按键        系统复位按键
中间按键        KEY0_PRES 对应管脚PE4  KEY0
最后的一个按键  WKUP_PRES 对应管脚PA0  WKUP

使用芯片 stm32F103xb  使用的是中等容量芯片
1.CORE中的启动文件 需要更改为中等容量的启动文件
2.Define 中的设置需要更改
3.FLASH的操作 芯片一页是1k
hd(High Density )是大容量，
md(Medium Density ) 是中容量
ld (Low Density ) 是小容量
1,2次信号强度是一样的

基本功能实现
****************************************************/

/****************************************************************
本程序由A6到	A7最后确定为sim868三种芯片，程序备注当中存在少量A6 A7字眼皆为遗漏备注修改处，
本程序一共使用3个串口，1个定时器
程序功能，获取GPS/北斗定位数据后上传至固定服务器

DATE: 2017/5/28

*******************************************************************/


#define BaudRate_usart3 115200        //串口3的波特率(获取A7返回的GPS数据)
#define BaudRate_usart1 115200	    //串口AT指令发送和接收




//超过

int main(void)
{
    u8 i=0;
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2); 	 //设置NVIC中断分组2:2位抢占优先级，2位响应优先级
    delay_init();	    	      //延时函数初始化
    IWDG_Init(6,156);         //初始化看看门狗 溢出时间为1s
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_CRC,ENABLE);//开启CRC时钟CRC校验
    uart_init(115200);	      //串口初始化为115200
    TIM3_Int_Init(999,7199);  //初始化定时器 100ms
    USART1_Init(BaudRate_usart1);
    FLASH_initialize();       //初始化FLASH中第60k的数据记录
    LED_Init();		  	        //初始化与LED连接的硬件接口
		PWR_Init();
    Restart_SIM868();               //重启SIM868
    SIM868_Init();                //SIM868模块初始化
    USART3_Init(BaudRate_usart3);    //初始化串口3 用来接收GPS数据

    while(1)
    {
        MeanwhileHeart = MeanwhileHeart_MAX;
			if(submit_info_flag==1&&GPS_effective)
			{
            submit_info_flag=0;
				//*HQ,8800000999,V1,145956.000,A,2927.335567,N,10631.592282,E,0.42,196.89,190517,FFFFFFFF#
				sprintf(TEXT_Buffer_1,"*HQ,8800000998,V1,%s,A,%s,N,%s,E,%s,%s,%s,FFFFFFFF#",UTCTime,Latitude_Str,Longitude_Str,
				Ground_speed,	Azimuth,UTCTime_year);
				printf("%s\r\n%s\r\n%s\r\n",Ground_speed,Azimuth,UTCTime_year);
				printf("%s\r\n",TEXT_Buffer_1);
			
            submitInformation(TEXT_Buffer_1);          //数据上传服务器

            if(isSendDataError==1)  //没有上传成功
            {
				if(signalQuality>=5)  //未大于5说明是因为信号不好造成的发送失败，不进行记录
				{
					Continue_Post_Errotimes++;
					Post_Errotimes++;                  //未用
				}
				sysData_Pack.data.failedTimes++;       //提交服务器失败次数
				Check_AT();                        //进入模块死机检测
          }
            else
            {
                Continue_Post_Errotimes=0;
            }
				
        if(Continue_Post_Errotimes>Continue_Post_Errotimes_MAX)//连续发送失败大于预定次数之后，重启SIM868
        {
            printf("连续失败次数过多，模块重启\r\n");
            saveData();
            u1_data_Pack.Error=1;
            Continue_Post_Errotimes=0;       //重启后清零
        }
				
        if(Heartbeat_Upload==1)   //上传心跳标志
        {
            Heartbeat_Upload=0;
            SendType=0;
            Upload_Time=0;        //上传服务器时间间隔清0
            SIM868_SendHeart();	         //传送心跳包
            i++;
            if(Heart_error!=0)       //上传心跳成功
            {
                if(i>=2)
                {
                    i=0;
                    saveData();
                    u1_data_Pack.Error=1;            //重启模块
                }
            }
        }
			}//end of if(submit_info_flag==1)

    }
}

//修改备注：
//***************************************************************
//2017/3/27 12：49  ：将打开AGPS后的GPRS_Init()取消改到重启后第一次发送时重新建立 正在跑的
//2017/3/27 17: 18  : 将GPRS连接改为最大为3次
//2017/3/28 20: 24  : 将连续5次发送失败之后重启改为连续发送2次失败就重启，并且打开了主循环心跳
//2017/3/29 12: 57  : 发送对象为百度地图,并且设置为3次激活PDP
//2017/3/30 19: 44  : 将指示灯改为闪烁（正常），长灭（GPRS不正常），常亮（GPS不正常）
//2017/3/30 20: 34  : 增加1分钟未收到GPS的输出就重启
//2017/3/30 22: 53  : 更换查询SIM卡的指令为AT+CCID
//2017/3/31 19: 29  : 加入GPSErrorTimes
//2017/3/31 19: 39  : 将submitInformationErrorTimes变量修改为SIM868_bootTimes A7重启时间
//2017/3/31 21: 48  : 将printf("A定位有效\r\n")改变位置，改到标志位之后
//2017/3/31 21: 59  : 修复多次TCP连接失败，计数未清零
//2017/4/4  13: 39  : 将串口3中断的清除放在后面，防止一次中断函数未执行完，下一次中断来了
//2017/4/4  18: 16  : 将重启模块全部变为重启单片机,加入savedata()
//2017/4/12 21: 21  : 将未定位重启允许时间改为3分钟
//2017/4/13 11: 07  : 修正了flash对模块信息储存不对的bug
//2017/4/13 11: 08  : 增加多次激活PDP失败就重启
//2017/4/13 11: 09  : 增加心跳发送时，同时发送当前SIM868信息
//2017/4/25 21: 58  : 程序大改，改为SIM868
//2017/4/26 18: 15  : 发送流程更改完毕.
//2017/4/28 17: 23  : http协议修改完成，串口接收方式，等待应答方式大改，依旧存在串口未接收完的情况（未解决）
//2017/4/29 17: 47  : 修改GPS解析函数，将注册网络最大次数改为40次
//2017/4/30 14: 54  : 修改解析函数,发现并不是程序问题，模块的确是没有返回
//2017/4/30 18: 33  : 加入GPRS_Test(),发现delay_ms()参数最大为1800 ,最大应答等待时间20s
//2017/5/09 17: 51  : 添加速率运算，加入惯性滤波
//2017/5/09 17: 55  : 添加Check_AT() 
//2017/5/11 00: 15  : 测试自己做的板子，改写开关机函数。成功实现功能。
//2017/5/12 23: 32  : 重写开机函数，加入将POST失败次数加入发送之中，解析函数改为100ms轮询一次，轮询200次，大约20s的时间。
//2017/5/19 14: 27  : 修改开机方式，修复SIM868重启次数与单片机重启次数不对等
//2017/5/19 23: 11  : 
//2017/5/20 22：45  : 修改开关机函数
//2017/5/21 10: 35  : 重新将IP修改为公司IP 
//2017/5/22 23: 06  : tcp如果建立失败，重新设置GPRS并且重新建立TCP防止这种情况发生。
//2017/5/23 7:  40  : 测试成功修改为公司服务器。将最大允许失败次数改为8次
//2017/5/24 14: 59  : 重新修改开关机方式，加入AT指令检验  盒子998 不带盒子999

