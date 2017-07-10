#include "timer.h"
#include "sim868.h"
#include "stdio.h"
#include "stmflash.h"
#include "iwdg.h"
#include "conversion.h"
#include "sim868.h"
#include "led.h"


u32  MeanwhileHeart = MeanwhileHeart_MAX;    //主循环心跳
u32 GPS_Upload=0;    //GPS数据打包标志
double 	LatNow = 0,LonNow = 0,LatOld=0, LonOld=0 ,Spend_Now;//记录上传时的纬度 经度  和速度
u32 GPS_invalidtimes=0;
u32 Upload_Time=0;      //上传服务器时间间隔
u32 Heartbeat_Upload=0;  //心跳上传标志位
u8 submit_info_flag=0;   //发送GPS标志位
u8 GPS_PACK=0;           //两次打包完成标志位
u8 waitservice_flag=0;   //等待服务器超时标志
u8 LEDshine_flag=0;
u8 dabao=0;
//通用定时器3中断初始化
//这里时钟选择为APB1的2倍，而APB1为36M
//arr：自动重装值。
//psc：时钟预分频数


void TIM3_Int_Init(u16 arr,u16 psc)
{
    TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE); //时钟使能

    //定时器TIM3初始化
    TIM_TimeBaseStructure.TIM_Period = arr; //设置在下一个更新事件装入活动的自动重装载寄存器周期的值
    TIM_TimeBaseStructure.TIM_Prescaler =psc; //设置用来作为TIMx时钟频率除数的预分频值
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1; //设置时钟分割:TDTS = Tck_tim
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;  //TIM向上计数模式
    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure); //根据指定的参数初始化TIMx的时间基数单位

    TIM_ITConfig(TIM3,TIM_IT_Update,ENABLE ); //使能指定的TIM3中断,允许更新中断

    //中断优先级NVIC设置
    NVIC_InitStructure.NVIC_IRQChannel = TIM3_IRQn;  //TIM3中断
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;  //先占优先级0级
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;  //从优先级3级
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE; //IRQ通道被使能
    NVIC_Init(&NVIC_InitStructure);  //初始化NVIC寄存器

    TIM_Cmd(TIM3, ENABLE);  //使能TIMx
}

//定时器3中断服务程序
//每100ms进一次中断服务函数
void TIM3_IRQHandler(void)
{
    static u32 T=0;
    static u32 times=0;              //正常工作定时心跳
    static u8 tt=0;                  //总的定时心跳
    static u32 waitservicetime = 0;  //服务器等待的计时
    if(TIM_GetITStatus(TIM3, TIM_IT_Update) != RESET)    //是更新中断
    {
        TIM_ClearITPendingBit(TIM3,TIM_IT_Update);  //清除TIMx更新中断标志

        T++;
        if(T%10==0)    //每1s  心跳减一
        {
            if(MeanwhileHeart!=0)   //当心跳为0的时候不再继续减
                MeanwhileHeart --;   //主循环心跳
        }
        if(u1_data_Pack.Error==0)   //没有出错
        {
            if(MeanwhileHeart!=0 )   //看门狗复位
                IWDG_Feed(); //喂狗
            else
                printf("达到心跳\r\n");
        }
        switch(deviceState)
        {
        case 0:LED1=1;break;
        case 1:
        case 2:
				case 3:
        case 4:if(T%5==0)
			     LED1=!LED1;
            break;
        case 5:    //正常工作状态 gps打开
        {
						LED1=0;
            times++; //用于计时间
            if(times%5== 0)  //每0.5秒闪一次灯
            {
                if(GPS_effective&GPRS_status)
                    LED0=!LED0;
                else if((GPS_effective==1)&&(GPRS_status!=1))   //只有GPS正常
                    LED0=1;
                else
                    LED0=0;                //GPS不正常
            }

            if(submit_info_flag==0)     //优化，距离上次上传的时间的间隔
            {
                tt++;
                if(tt%150==0)   //每30秒上传一次
                {
                    submit_info_flag=1;
                }
            }
            if(times%10==0)       //每正常工作1s
            {

                if(GPS_effective==1)    //GPS数据有效
                {
                    Upload_Time=0;      //上传心跳计数清零
                    GPS_effective=0;    //GPS数据标记为无效
                    if(times%150==0)     //每15s输出一次滤波信息
                    {
                        //只有前后两次的距离超过40m才保证打包数据
                        getFilterLoc(&LatNow, &LonNow,&Spend_Now);   //得到滤波后的经纬度    把两个存储变量的地址传入，赋给一个指针变量
                        printf("滤波后的百度地图纬度%f,经度%f\r\n",LatNow,LonNow);
                        distance=Cal_Distance(LatOld,LonOld,LatNow,LonNow)*1000;       //计算两次定位的   (第一次定位的距离差距很大)
                        printf("\r\n两次上传的距离为%lfm\r\n",distance);
                        LatOld=LatNow;           //便于下一次计算距离
                        LonOld=LonNow;
                        dabao=1;

                    }
                    GPS_invalidtimes=0;   //GPS未定位的时间清0
                }
                else
                {
                    GPS_effective=0;
                }
                Upload_Time++;   //上传时间间隔

                if(Upload_Time>=Upload_Time_MAX)    //超过2分钟没有上传GPS数据
                {
                    Heartbeat_Upload=1;   //心跳上传标志
                }
            }

            if(waitservice_flag==0)// 标志位如果被标记为等待，进行计时等待
            {
                waitservicetime++;
                if((waitservicetime%100)==0)
                {
                    waitservice_flag=1;
                    waitservicetime=0;
                }
            }
            break;
        }

        }
    }
}









