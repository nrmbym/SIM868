#include "usart3.h"
#include "usart.h"
#include "delay.h"
#include "timer.h"
#include "string.h"
#include "stdlib.h"
#include "conversion.h"
#include "stdio.h"
#include "stmflash.h"
#include "sim868.h"


Time_Pack u3_time_Pack;		   //时间
u8 USART3_RX_BUF[USART3_MAX_RECV_LEN];  //串口接收数组缓存
u16 receive_len=0;
vu16 USART1_RX_STA=0;
u32 startNum = 0;       //记录当前接收字符串在接收缓存中的开始位置
u8  stringNum=0;				//本次接收到的字符串条数
char *GNRMC;		        //储存接收到的推荐GPS定位数据
char* string[SRTING_NUM];
char UTCTime[7];          //存放UTC时间数据 时分秒
char UTCTime_year[7];     //存放年月日时间
char Longitude_Str[11];	  //经度dddmm.mmmm(度分)格式
char Latitude_Str[10];	  //纬度ddmm.mmmm(度分)格式
char La_Position[2];     //纬度方位
char Lo_Position[2];     //经度方位
char Ground_speed[10];    //地面速度提取
char Azimuth[10];        //方位角
double Latitude_Temp = 0,Baidu_Latitude_Temp = 0;     //记录转换的经纬度
double Longitude_Temp = 0,Baidu_Longitude_Temp = 0;   //记录转换的经纬度
double Drift_speed=0;    //漂移速度
double BaiduLongitude_Range[GPS_array];      //经度(百度坐标)
double BaiduLatitude_Range[GPS_array];	     //纬度(百度坐标)
double Speed_Range[GPS_array];    //地面速度信息
u8 GPS_array_Count=0;
u8 GPS_effective=0;               //GPS数据是否有效位
u8 GPS_receive=0;                 //GPS中断计时标志
//static u8 count=0;                //GPS接收次数
char *GPSState="IMEI GPS  2";
_sysData_Pack sysData_Pack;	      //保存在Flash中的相关数据的数据结构
u8 Pack_length=0;                 //数据打包长度
double distance=0;                //两次定位的距离
u16 GPS_unspecified_time=0;       //GPS连续未定位的时间
//初始化IO 串口3
//pclk1:PCLK1时钟频率(Mhz)
//bound:波特率
//用来接收GPS数据 只使能接收管脚
void USART3_Init(u32 bound)
{

    NVIC_InitTypeDef NVIC_InitStructure;
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);	// GPIOB时钟
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3,ENABLE); //串口3时钟使能

    USART_DeInit(USART3);  //复位串口3


    //USART3_RX	  PB11
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;//浮空输入
    GPIO_Init(GPIOB, &GPIO_InitStructure);  //初始化PB11

    USART_InitStructure.USART_BaudRate = bound;//波特率一般设置为9600;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;//字长为8位数据格式
    USART_InitStructure.USART_StopBits = USART_StopBits_1;//一个停止位
    USART_InitStructure.USART_Parity = USART_Parity_No;//无奇偶校验位
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;//无硬件数据流控制
    USART_InitStructure.USART_Mode = USART_Mode_Rx;	   //只设置为收模式

    USART_Init(USART3, &USART_InitStructure); //初始化串口	3

    USART_Cmd(USART3, ENABLE);                    //使能串口

    //使能接收中断
    USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);
    //开启ERR中断
    USART_ITConfig(USART3, USART_IT_ERR, ENABLE);	    //USART_IT_ERR开启错误中断（帧误差、噪声误差，误差超限),用于串口数据接收的防错
    //关闭发送中断
    USART_ITConfig(USART3, USART_IT_TXE, DISABLE);

    //设置中断优先级
    NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority=2;//抢占优先级3
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;		//子优先级3
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;			//IRQ通道使能
    NVIC_Init(&NVIC_InitStructure);	//根据指定的参数初始化VIC寄存器
}


//GPS数据缓存接收清空
void Receive_empty(void)
{
    u8 i;
    startNum = 0;
    stringNum = 0;
    receive_len = 0;
    for(i = 0; i < SRTING_NUM; i++)
    {
        string[i] = 0;
    }
}

//UTC时间转本地时间
void UTC_Localtime(void)
{
    //UTC小时加8h转为北京时间
    u3_time_Pack.hour+=8;
    if(u3_time_Pack.hour>=24)  //超过了一天
    {
        u3_time_Pack.hour-=24;
        u3_time_Pack.day+=1;  //天数加1
        switch(u3_time_Pack.month)
        {
        case 1:     //大月
        case 3:
        case 5:
        case 7:
        case 8:
        case 10:
        case 12:
            if(u3_time_Pack.day>31)
            {
                u3_time_Pack.day-=31;
                u3_time_Pack.month+=1;     //月份加1
                if(u3_time_Pack.month>12)
                {
                    u3_time_Pack.month-=12;
                    u3_time_Pack.year+=1;
                }

            }
            break;
        case 4:             //小月
        case 6:
        case 9:
        case 11:
            if(u3_time_Pack.day>30)
            {
                u3_time_Pack.day-=30;
                u3_time_Pack.month+=1;     //月份加1
                if(u3_time_Pack.month>12)
                {
                    u3_time_Pack.month-=12;
                    u3_time_Pack.year+=1;
                }

            }
            break;
        case 2:
            //先判断平年还是闰年
            if((u3_time_Pack.year%400==0)||(u3_time_Pack.year%4==0&&u3_time_Pack.year%100!=0)) //闰年
            {
                if(u3_time_Pack.day>29)
                {
                    u3_time_Pack.day-=29;
                    u3_time_Pack.month+=1;
                }
            }
            else   //平年
            {
                if(u3_time_Pack.day>28)
                {
                    u3_time_Pack.day-=28;
                    u3_time_Pack.month+=1;
                }
            }
            break;
        }
    }

    if(u3_time_Pack.year<2017||	(u3_time_Pack.month>12&&u3_time_Pack.month<1)||
            (u3_time_Pack.day>31&&u3_time_Pack.day<1)||
            u3_time_Pack.hour>24||
            u3_time_Pack.minute>60||
            u3_time_Pack.second>60)          //转化的时间出现错误
    {   //重新清0
        u3_time_Pack.year=0;
        u3_time_Pack.month=0;
        u3_time_Pack.day=0;
        u3_time_Pack.hour=0;
        u3_time_Pack.minute=0;
        u3_time_Pack.second=0;
    }

}



//推荐定位信息的GPS数据的解析
//$GNRMC,000021.262,V,,,,,0.00,0.00,060180,,,N*47
//格式:$GNRMC,<1>,<2>,<3>,<4>,<5>,<6>,<7>,<8>,<9>,<10>,<11>,<12>*<13><CR><LF>
void analysisGPS(void)
{
    u32 i=0;           //for循环变量
    u32 commaNum = 0;  //逗号个数
    u32 lenth=0;       //数据的长度
    char * start = 0;  //数据的头地址
    char * end = 0;    //数据尾地址
    char * str = 0;
	  char * str1=0;
    u8 checksum=0,recCheckSum=0;   //GPS数据校验
//    static u8  first_time= 0;      //是否第一次获取到定位数据 0:是 1:否
    char *pEnd;          //不能转换的字符的指针
    commaNum = charNum(GNRMC,',');  //先校验GPS数据完整
    if(commaNum==12)   //是一个完整的GPS数据
    {
        //计算校验和
        //strstr()函数用来检索子串在字符串中首次出现的位置
        start=strstr(GNRMC,"GNRMC");   //得到第一个地址
        end=strstr(GNRMC,"*");
        lenth=(u32)(end-start);
        for(i=0; i<lenth; i++)
        {
            checksum=checksum^start[i];   //进行异或运算得到校验和
        }
        //将字符数字类型转化为数字类型
        recCheckSum=strtol(end+1,&pEnd,16);  //$与*之间所有字符ASCII码的校验和（各字节做异或运算，得到校验和后，在转换16进制格式的ASCII字符。）

        if(recCheckSum==checksum)
        {
//            printf("数据校验成功\r\n");
            str = ReturnCommaPosition(GNRMC,2);   //查找定位状态，A=定位，V=未定位
            if(*(str+1)=='A')              //有效定位
            {
							
							
							//strncpy()会将字符串src前n个字符拷贝到字符串dest。
							
                GPS_unspecified_time=0;               //连续未定位次数清0
                //进行各数据的提取
                str = ReturnCommaPosition(GNRMC,1);  //hhmmss（时分秒) 时间的提取
                strncpy(UTCTime,str+1,6);         //将前几个数据考入数组中,strncpy函数在末尾不会添加字符结束符号
                UTCTime[6]=(u8)'\0';    //添加字符结尾

                str=ReturnCommaPosition(GNRMC,3);   //纬度的提取
                strncpy(Latitude_Str,str+1,9);
                Latitude_Str[9]='\0';
                
							  str=ReturnCommaPosition(GNRMC,4);   //纬度的方位提取
                strncpy(La_Position,str+1,1);
                La_Position[1]='\0';
							
                str=ReturnCommaPosition(GNRMC,5);  //经度的提取
                strncpy(Longitude_Str,str+1,10);
                Longitude_Str[10]='\0';

							  str=ReturnCommaPosition(GNRMC,6);  //经度的方位的提取
                strncpy(Lo_Position,str+1,1);
                Lo_Position[1]='\0';
								
                str=ReturnCommaPosition(GNRMC,7);  //地面速率的提取
								str1=ReturnCommaPosition(GNRMC,8);
                strncpy(Ground_speed,str+1,str1-str-1);
                Ground_speed[str1-str-1]='\0';
                 
								str=ReturnCommaPosition(GNRMC,8);   //方位角的提取
								str1=ReturnCommaPosition(GNRMC,9);
                strncpy(Azimuth,str+1,str1-str-1);
                Azimuth[str1-str-1]='\0';
								
                str = ReturnCommaPosition(GNRMC,9); //UTC日期 ddmmyy（日月年）格式
                strncpy(UTCTime_year,str+1,6);
                UTCTime_year[6]='\0';

                //地面速率转化
//                Drift_speed=atof(Ground_speed);  //将字符串转换为double形式的速度
//                Drift_speed=Drift_speed*1.58;      //地面速率 海里时转千米时
                //纬度转化
//                Latitude_Temp=atof(Latitude_Str);    //将字符串转换为double形式的经度
//                Latitude_Temp=(int)Latitude_Temp/100+(Latitude_Temp-((int)Latitude_Temp/100)*100)/60; //转化为度
//                //经度转化
//                Longitude_Temp=atof(Longitude_Str);
//                Longitude_Temp=(int)Longitude_Temp/100+(Longitude_Temp-((int)Longitude_Temp/100)*100)/60;

//                GPS_transformation(Latitude_Temp,Longitude_Temp,&Baidu_Latitude_Temp,&Baidu_Longitude_Temp);	//真实坐标转百度坐标

//                if((Baidu_Longitude_Temp < 72)||(Baidu_Longitude_Temp > 136)||
//                        (Baidu_Latitude_Temp < 18)||(Baidu_Latitude_Temp > 54))   //如果超出中国范围
//                {
//                    printf("超出了中国的范围");
//                    return;
//                }
//                printf("真实GPS数据%f,%f\r\n",Latitude_Temp,Longitude_Temp);  //当前的GPS数据
//                printf("百度GPS数据%f,%f\r\n",Baidu_Latitude_Temp,Baidu_Longitude_Temp);

                //10个GPS数据先填满一个数组，每来一个数据依次轮换
//                if(first_time++==0)    //是第一次
//                {   //先判断再自增
//                    for(i=0; i<GPS_array; i++)	 //填满整个数组
//                    {
//                        BaiduLatitude_Range[i] = Baidu_Latitude_Temp;
//                        BaiduLongitude_Range[i] = Baidu_Longitude_Temp;
//                        Speed_Range[i]=Drift_speed;   //速度信息
//                    }
//                    GPS_array_Count=1;
//                }

//                else  //不是第一次接收到数据
//                {
//                    BaiduLatitude_Range[GPS_array_Count]=Baidu_Latitude_Temp;
//                    BaiduLongitude_Range[GPS_array_Count]=Baidu_Longitude_Temp;
//                    Speed_Range[GPS_array_Count]=Drift_speed;
//                    GPS_array_Count++;                  //依次改变数组
//                    if(GPS_array_Count>=GPS_array)
//                    {
//                        GPS_array_Count=0;
//                    }
//                }

                //091202日月年的格式 083559.00时分秒的格式   时间的提取
//                u3_time_Pack.year=2000+(UTCTime_year[4]-'0')*10+(UTCTime_year[5]-'0');   //年的提取
//                u3_time_Pack.month=(UTCTime_year[2]-'0')*10+UTCTime_year[3]-'0';         //月的提取
//                u3_time_Pack.day=(UTCTime_year[0]-'0')*10+UTCTime_year[1]-'0';           //日
//                u3_time_Pack.hour=(UTCTime[0]-'0')*10+UTCTime[1]-'0';                    //时
//                u3_time_Pack.minute=(UTCTime[2]-'0')*10+UTCTime[3]-'0';                  //分
//                u3_time_Pack.second=(UTCTime[4]-'0')*10+UTCTime[5]-'0';                  //秒



//                UTC_Localtime();                  //UTC时间转本地时间



                GPS_effective=1;                  //标记获取的GPS数据有效
                printf("A定位有效\r\n");

            }
            else  //GPS定位信息无效
            {
                GPS_effective=0;
                printf("GPS未成功定位\r\n");
                GPS_unspecified_time++;                 //未定位次数+1
            }
        }
        else  //数据没有校验成功
        {
            printf("数据校验和失败");
            return;
        }

    }
    else
    {
        printf("没有12个逗号\r\n");
    }
}

//滤波处理算法，去掉一个最大值和最小值，其余求平均值
double GPS_filter(double data[])
{
    u8 i=0;
    double Min=0,Max=0,Average=0,Sum=0;
    Min=Max=data[1];
    for(i=0; i<GPS_array; i++)
    {
        if(data[i]<Min)
        {
            Min=data[i];
        }
        if(data[i]>Max)
        {
            Max=data[i];
        }
        Sum+=data[i];
    }
    Average=(Sum-Max-Min)/(GPS_array-2);
    return Average;
}


void getFilterLoc(double * Lat_filter,double * Lon_filter ,double * Spend_filter)     //获取的10个GPS数据进行滤波处理
{
    *Lat_filter=GPS_filter(BaiduLatitude_Range);
    *Lon_filter=GPS_filter(BaiduLongitude_Range);
	  *Spend_filter=GPS_filter(Speed_Range);
}


//Drift_speed
void GPS_Packed_Data(void)   //GPS数据打包上传
{

    sprintf(u3_time_Pack.Time,"%04d-%02d-%02d %02d:%02d:%02d",             //2016-01-14 05:24:19 时间数据放入数组
            u3_time_Pack.year,u3_time_Pack.month,u3_time_Pack.day,
            u3_time_Pack.hour,u3_time_Pack.minute,u3_time_Pack.second);


    Pack_length=sprintf(TEXT_Buffer_1,"Time=%s-Log=%s,%04d,%03d,%03d,%03d,%03f,sQ: %02d",   //一次数据打包
                        u3_time_Pack.Time,
                        u1_data_Pack.IMEI,                          //模块号数
                        sysData_Pack.data.bootTimes,      //正常开机次数
                        sysData_Pack.data.SIM868_bootTimes, //上传服务器器失败次数
                        sysData_Pack.data.postErrorTimes,  //POST失败次数
                        sysData_Pack.data.GPSErrorTimes,
                        Spend_Now,
                        signalQuality
                       );
    printf("打包数据上传大小%d\r\n",Pack_length);
    GPS_PACK=1;           //标记打包完成
}


void USART3_IRQHandler(void)
{
    u32 i;//for循环变量
    if(USART_GetITStatus(USART3, USART_IT_RXNE) == SET)//接收到数据
    {
//        USART_ClearITPendingBit(USART3,USART_IT_RXNE);       //清除中断预处理位
        USART3_RX_BUF[receive_len++] = USART_ReceiveData(USART3);//读取接收到的数据

        if((USART3_RX_BUF[receive_len - 2] == '\r')||
                (USART3_RX_BUF[receive_len - 1] == '\n'))//如果收到回车换行，代表当前字符串结束
        {
            GPS_receive=1;        //有正常数据产生;


            if(receive_len - startNum > 2)      //如果字符串长度小于等于2，证明为空字符串，不用接收
            {
                string[stringNum] = (char *)USART3_RX_BUF + startNum;//储存字符串首地址
                USART3_RX_BUF[receive_len - 2] = (u8)'\0'; //添加字符串结束符
                if((string[stringNum][0] == '$')&&
                        (string[stringNum][1] == 'G')&&
                        (string[stringNum][2] == 'N')&&
                        (string[stringNum][3] == 'R')&&
                        (string[stringNum][4] == 'M')&&
                        (string[stringNum][5] == 'C'))	      //判断是否接收到推荐GPS定位信息
                {

                    GNRMC = string[stringNum];
                    if(GNRMC != 0)
                    {
                        analysisGPS();//解析GPS数据
                    }
                }
                stringNum++;//字符串计数器加1        	//一次最多接收的字符串数量20

            }
            startNum = receive_len;
        }
        if((receive_len >= USART3_MAX_RECV_LEN)||(stringNum >= SRTING_NUM))   //超出了接收的范围
        {
            startNum = 0;
            stringNum = 0;
            receive_len = 0;
            for(i = 0; i < SRTING_NUM; i++)
            {
                string[i] = 0;
            }
        }
        USART_ClearITPendingBit(USART3,USART_IT_RXNE);       //清除中断预处理位
    }
    //溢出-如果发生溢出需要先读SR,再读DR寄存器 则可清除不断入中断的问题
    if(USART_GetFlagStatus(USART3,USART_FLAG_ORE) == SET)    //USART_FLAG_ORE溢出错误标志
    {
        USART_ClearFlag(USART3,USART_FLAG_ORE);	  //读SR
        USART_ReceiveData(USART3);				        //读DR
    }
}



//清除GPS接收器
//参数：无
//返回值 无
void cleanReceiveData_GPS(void)
{

    u32 i;
    startNum = 0;
    stringNum = 0;             //本次接收到的字符串条数清0
    receive_len = 0;           //接收计数器

    for(i = 0; i < USART3_MAX_RECV_LEN; i++)
    {
        USART3_RX_BUF[i]=0;
    }
    for(i = 0; i < SRTING_NUM; i++)
    {
        string[i] = 0;
    }
}





