#include "stdarg.h"
#include "stdio.h"
#include "string.h"
#include "stdlib.h"
#include "delay.h"
#include "timer.h"
#include "usart.h"
#include "usart_1.h"
#include "stmflash.h"
#include "sim868.h"

u8 firstsend_flag=1;           //标记是否为第一次发送  ，因为在打开AGPS的时候，会进行连接，在发送指令的时候，需要关闭这个连接
u8 Continue_Post_Errotimes=0;  //连续失败次数
u8 Post_Errotimes=0;           //本次SIM868开机一共上传失败次数
u8 Heart_error=0;              //心跳上传失败位
u8 deviceState = 0;            //设备当前状态 0:开机 1:初始化AT指令 2:检查SIM卡 3:搜索信号 4:初始化GPRS态 5:正常工作态
Data_Pack u1_data_Pack;        //SIM868相关信息
u8 GPRS_status=0;              //TCP是否正常状态,如果TCP连接没有出错，那么为
LBS_Info LBS_PACK;


char p1[300];
char POSTDATA[1000]= "*HQ,8800000999,V1,145956.000,A,2927.335567,N,10631.592282,E,0.42,196.89,190517,FFFFFBFF#";
u8 SendType=0 ;

//初始化SIM868函数

void SIM868_Init()
{
    s8 ret;
	char *res=0;
    u32 i = 0;
    deviceState = 0;  //开机态
	firstsend_flag=1;
    cleanReceiveData_GPS();
	sysData_Pack.data.SIM868_bootTimes++;
    deviceState=1;    //初始化AT指令态
    /*初始化AT指令集*/
    i = 0;
    while((ret = sendAT("AT\r\n","OK",500)) != 0)
    {
        printf("正在初始化AT指令集```%d\r\n",i++);
        if(i >= 50)
        {
            printf("\r\n初始化AT指令集失败，正在重启单片机\r\n");
			saveData();
            u1_data_Pack.Error = 1;//标记初始化Ai_A6出错，等待看门狗复位单片机
            while(1);	//等待看门狗复位
        }
    }
    if(ret != 0) {
        printf("初始化AT指令集失败\r\n");
    }
    else
    {
        printf("初始化AT指令集成功\r\n");
        /*关闭回显*/
        ret=sendAT("ATE0","OK",5000);
        if(ret == 0) {
            printf("关闭回显成功\r\n");
        }
        else {
            printf("关闭回显失败\r\n");
        }

        /*打开上报详细错误信息*/
        ret=sendAT("AT+CMEE=2","OK",5000);
        if(ret == 0) {
            printf("打开上报详细错误信息成功\r\n");
        }
        else {
            printf("打开上报详细错误信息失败\r\n");
        }
        /*查询IMEI号*/
        getIMEI();
        deviceState = 2;//检测SIM卡态
        /*查询是否检测到SIM卡*/
        i = 0;
        while((ret = sendAT("AT+CPIN?","READY",3000)) != 0)//只有插入SIM卡才进行接下来的操作
        {
            printf("正在查询SIM卡状态```%d\r\n",i++);
            if(i >= 5)
            {
                printf("\r\n检测SIM卡失败，正在重启单片机\r\n");
								saveData();
                u1_data_Pack.Error = 1;//标记初始化SIM808出错，等待看门狗复位单片机
                while(1);	//等待看门狗复位
            }
        }
        if(ret == 0) {
            printf("已插入SIM卡\r\n");
        }
        else {
            printf("未插入SIM卡\r\n");
        }
		i=0;
		deviceState = 3;  //注册网络态
        /*查询网络注册情况*/
        while(1)
		{
			sendAT("AT+CREG?","+CREG:",3000);
			res=my_strstr((char *)u1_data_Pack.USART1_RX_BUF,(char * )"+CREG:");
			if(*(res+9) =='1'||*(res+9) =='5')//搜索"0，1"或者"0,5"
			{
				printf("成功注册网络\r\n");
				break;
			}
			i++;
			if(i>=40)				
			{
				printf("注册网络失败,准备重启\r\n");
				saveData();
				u1_data_Pack.Error = 1;//标记初始化SIM808出错，等待看门狗复位单片机
                while(1);	//等待看门狗复位
			}
            printf("正在注册网络....%d\r\n",i);
			delay_ms(1500);			
        }
        deviceState = 4;//初始化GPRS态
		GPRS_Connect(); //打开GPRS承载
		SIM868_GPRS_Test();
        SIM868_GPS_Init();
        deviceState = 5;//正常工作态
    }
}
//查询模块是否附着GPRS网络
//返回是否附着GPRS网络 0：附着 -1：未附着
s8 SIM868_GPRS_Test(void)
{
    s8 ret;
    char * str = 0;
    /*查询信号质量*/
    ret = sendAT("AT+CSQ","OK",5000);
    if(ret != 0)
    {
        printf("查询信号质量失败\r\n");
        signalQuality=99;
    }
    else
    {
        if(extractParameter(my_strstr((char *)u1_data_Pack.USART1_RX_BUF,"+CSQ:"),"+CSQ: ",&signalQuality,0,0) == 0)
        {
             printf("信号质量：%d\r\n",signalQuality);
        }
    }
    /*查询模块是否附着GPRS网络*/
    ret = sendAT("AT+CGATT?","+CGATT:",1000);
    if(ret != 0) {
        return -1;
    }
    str = my_strstr((char *)u1_data_Pack.USART1_RX_BUF,"+CGATT:");
    if(str[8] == '1')//已附着GPSR网络
    {
        printf("附着GPRS网络成功\r\n");
		GPRS_status=1;
        return 0;
    }
    else//未附着GPSR网络，尝试附着GPRS网络
    {
		GPRS_status=0;
		return  -1;
    }
}

//在串口接收的字符串中搜索
//返回值		成功返回被查找字符串的首地址
//			失败返回 0
char * my_strstr(char *FirstAddr,char *searchStr)
{

    char * ret = 0;
	u16 i;
	for(i=0;i<u1_data_Pack.USART1_RX_STA;i++)
	{
		if(u1_data_Pack.USART1_RX_BUF[i]==*(searchStr))
		{
			ret= strstr((char *)u1_data_Pack.USART1_RX_BUF+i,searchStr);
			if(ret!=0)
			  break;
		}
		if((i==USART1_MAX_RECV_LEN)||ret!=0)
			break;
	}

    return ret;
}

//清除接收器
//参数：无
//返回值 无
void cleanReceiveData(void)
{
    u16 i;
    u1_data_Pack.USART1_RX_STA=0;			//接收计数器清零
    for(i = 0; i < USART1_MAX_RECV_LEN; i++)
    {
        u1_data_Pack.USART1_RX_BUF[i] = 0;
    }
}



//发送一条AT指令，并检测是否收到指定的应答
//sendStr:发送的命令字符串,当sendStr<0XFF的时候,发送数字(比如发送0X1A),大于的时候发送字符串.
//searchStr:期待的应答结果,如果为空,则表示不需要等待应答
//outTime:等待时间(单位:1ms)
//返回值:0,发送成功(得到了期待的应答结果)
//       -1,发送失败
s8 sendAT(char *sendStr,char *searchStr,u32 outTime)
{
	u16 i;
    s8 ret = 0;
    char * res =0;
//	outTime=outTime/10;
    cleanReceiveData();//清除接收器
    if((u32)sendStr < 0xFF)
    {
        while((USART1->SR&0X40)==0);//等待上一次数据发送完成
        USART1->DR=(u32)sendStr;
    }
    else
    {
        u1_printf(sendStr);//发送AT指令
        u1_printf("\r\n");//发送回车换行
    }
    if(searchStr && outTime)//当searchStr和outTime不为0时才等待应答
    {
        while((--outTime)&&(res == 0))//等待指定的应答或超时时间到来
        {
			res = my_strstr((char *)u1_data_Pack.USART1_RX_BUF,searchStr);
			if(res!=0)
				break;
			if((i==USART1_MAX_RECV_LEN)||res!=0)
				break;
			delay_ms(1);
        }
        if(outTime == 0) {
            ret = -1;    //超时
        }
        if(res != 0)//res不为0证明收到指定应答
        {
            ret = 0;
        }
    }
	delay_ms(50);
    return ret;
}


//查询模块版本号
void checkRevision(void)
{
    s8 ret;
    char * str;
    /*查询模块软件版本*/
    ret = sendAT("AT+GMR","OK",5000);
    if(ret == 0) {
        printf("查询模块软件版本成功\r\n");
    }
    else
    {
        printf("查询模块软件版本失败\r\n");
		saveData();
        u1_data_Pack.Error = 1;//标记初始化Ai_A6出错，等待看门狗复位单片机
    }
    str = my_strstr((char *)u1_data_Pack.USART1_RX_BUF,"V");
    if(str != 0)
    {
        strncpy(u1_data_Pack.revision, str , 20);
        u1_data_Pack.revision[20] = '\0';//添加字符串结束符
        printf("模块软件版本: %s  \r\n",u1_data_Pack.revision);
    }
    else
    {
        printf("查询模块软件版本失败\r\n");
				saveData();
        u1_data_Pack.Error = 1;//标记初始化Ai_A6出错，等待看门狗复位单片机
    }
}

//查询IMEI
void getIMEI(void)
{
    s8 ret;
    char * str;
    /*查询IMEI号*/
    ret = sendAT("AT+GSN","OK",5000);
    if(ret == 0) {  }
    else
    {
        printf("查询IMEI号失败\r\n");
				saveData();
        u1_data_Pack.Error = 1;//标记初始化Ai_A6出错，等待看门狗复位单片机
    }
    str = my_strstr((char *)u1_data_Pack.USART1_RX_BUF,"OK");
    if(str != 0)
    {
        strncpy(u1_data_Pack.IMEI, str - 19, 15);
        u1_data_Pack.IMEI[15] = '\0';//添加字符串结束符
        printf("IMEI号: %s\r\n",u1_data_Pack.IMEI);
    }
}






//POST /webservice.asmx/GpsSubmit HTTP/1.1
//Host: gpsws.cqutlab.cn
//Content-Type: application/x-www-form-urlencoded
//Content-Length: length

//lat=string&lon=string&IMEI=string&log=string


u8 isSendData = 0;		//是否在和服务器通信的标志位
u8 isSendDataError = 0;	//上传服务器是否失败 0:为成功 1:失败






//提取字符串中的参数(两个参数或者三个参数)
//str:源字符串，searchStr:参数前的特征字符串,data1:提取出的参数1 data2:提取出的参数2 data3:提取出的参数3
//成功返回0 失败返回-1

s8 extractParameter(char *sourceStr,char *searchStr,s32 *dataOne,s32 *dataTwo,s32 *dataThree)
{
    char *str;
    char *commaOneAddr;//第一个逗号的地址
    char *commaTwoAddr;//第二个逗号的地址
    char dataStrOne[10], dataStrTwo[10], dataStrThree[10];
    u32 commaNum = 0,lenthOne = 0,lenthTwo = 0,lenthThree = 0,lenthEigen = 0;

    str = strstr(sourceStr,searchStr);
    if(str == 0) {
        return -1;
    }
    else
    {
        commaNum = charNum(sourceStr,',');//搜索有多少逗号
        if(commaNum == 1)//有两个参数
        {
            lenthEigen = strlen(searchStr);//得到特征字符串的长度
            commaOneAddr = ReturnCommaPosition(str,1);//得到第一个逗号位置
            lenthOne = (u32)(commaOneAddr - str) - lenthEigen;//得到第一个参数的长度
            strncpy(dataStrOne, sourceStr + lenthEigen, lenthOne);
            dataStrOne[lenthOne] = '\0';//添加字符串结束符
            if(dataOne != 0) {
                *dataOne = atoi(dataStrOne);    //将字符串转换为整数
            }

            lenthTwo = strlen(commaOneAddr + 1);//得到第二个参数的长度
            strncpy(dataStrTwo, commaOneAddr + 1, lenthTwo);
            dataStrTwo[lenthTwo] = '\0';//添加字符串结束符
            if(dataTwo != 0) {
                *dataTwo = atoi(dataStrTwo);    //将字符串转换为整数
            }
            return 0;
        }
        if(commaNum >= 2)//有三个以上参数
        {
            lenthEigen = strlen(searchStr);//得到特征字符串的长度
            commaOneAddr = ReturnCommaPosition(str,1);//得到第一个逗号位置
            lenthOne = (u32)(commaOneAddr - str) - lenthEigen;//得到第一个参数的长度
            strncpy(dataStrOne, sourceStr + lenthEigen, lenthOne);
            dataStrOne[lenthOne] = '\0';//添加字符串结束符
            if(dataOne != 0) {
                *dataOne = atoi(dataStrOne);    //将字符串转换为整数
            }

            commaTwoAddr = ReturnCommaPosition(str,2);//得到第二个逗号位置
            lenthTwo = (u32)(commaTwoAddr - commaOneAddr - 1);//得到第二个参数的长度
            strncpy(dataStrTwo, commaOneAddr + 1, lenthTwo);
            dataStrOne[lenthTwo] = '\0';//添加字符串结束符
            if(dataTwo != 0) {
                *dataTwo = atoi(dataStrTwo);    //将字符串转换为整数
            }

            lenthThree = strlen(commaTwoAddr + 1);//得到第二个参数的长度
            strncpy(dataStrThree, commaTwoAddr + 1, lenthThree);
            dataStrThree[lenthTwo] = '\0';//添加字符串结束符
            if(dataThree != 0) {
                *dataThree = atoi(dataStrThree);    //将字符串转换为整数
            }
            return 0;
        }
    }
    return -1;
}


//函数名称：返回逗号位置
//输入参数：p	待检测字符串首地址
//		  num	第几个逗号
//返回参数：第num个逗号地址
//说    明：若65535个字符内没有逗号就返回字符串首地址
char * ReturnCommaPosition(char *p ,u8 num)
{
    u8 i=0;			//第i个逗号
    u16 k=0;		//地址增加值
    while(k<65535)
    {
        if( *(p+k) ==',')
        {
            i++;
            if(i==num)
            {
                return p+k;
            }
        }
        k++;
    }
    return p;
}
//函数名称：搜索字符串中有多少个指定字符
//输入参数：p	待检测字符串首地址
//		  	Char	待搜索的字符
//返回参数：字符串中有多少个指定字符

u8 charNum(char *p ,char Char)
{
    u8 i=0;			//第i个逗号
    u16 k=0;		//地址增加值
    while(1)
    {
        if( *(p+k) == Char)
        {
            i++;
        }
        else if(*(p+k) == '\0')
        {
            return i;
        }
        k++;
    }
}

void PWR_Init(void)
{
	  GPIO_InitTypeDef GPIO_InitStructure;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB,ENABLE);
    GPIO_InitStructure.GPIO_Pin=GPIO_Pin_15;
    GPIO_InitStructure.GPIO_Mode=GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed=GPIO_Speed_50MHz;
    GPIO_Init(GPIOB,&GPIO_InitStructure);
		PBout(15)=1;                              //默认为高，实际为低


    GPIO_InitStructure.GPIO_Pin=GPIO_Pin_7;   //下拉输入
    GPIO_InitStructure.GPIO_Mode=GPIO_Mode_IPD;
    GPIO_InitStructure.GPIO_Speed=GPIO_Speed_50MHz;
    GPIO_Init(GPIOB,&GPIO_InitStructure);	
		PBin(7)=0;
}
//硬件重启SIM868
//PB14 --EN 
void Restart_SIM868(void)
{
  s8 ret;
	u8 i;
	delay_ms(250);
	while(1)
	{
		PBout(15)=0;//单片机给低 PWR脚被拉高
		delay_ms(500);
		PBout(15)=1;//
		delay_ms(1500);
		PBout(15)=0;
		delay_ms(1500);
		i=0;
		while((ret = sendAT("AT\r\n","OK",500)) != 0)
    {
        printf("正在开机测试```%d\r\n",i++);
			if(i>=10)
				break;
    }
		if(i<10)
			break;
  }
	
	
	
	
}



//初始化GPS
//将GPS_EN 拉高
void SIM868_GPS_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA,ENABLE);
	GPIO_InitStructure.GPIO_Pin=GPIO_Pin_15;
  GPIO_InitStructure.GPIO_Mode=GPIO_Mode_Out_PP;
  GPIO_InitStructure.GPIO_Speed=GPIO_Speed_50MHz;
  GPIO_Init(GPIOA,&GPIO_InitStructure);
	
	
	PAout(15)=1; //将GPS_EN拉高2秒以上
	delay_ms(1500);
}

//GPRS连接
s8 GPRS_Connect(void)
{
	u8 i=0;
    s8 ret;
    ret = sendAT("AT+CIPCLOSE","OK",1000);	//关闭连接
    if(ret == 0) {
        printf("关闭连接成功\r\n");
    }
    else {
        printf("关闭连接失败\r\n");
    }

    ret = sendAT("AT+CIPSHUT","OK",1000);		//关闭移动场景
    if(ret == 0) {
        printf("关闭移动场景成功\r\n");
    }
    else {
        printf("关闭移动场景失败\r\n");
    }

    ret = sendAT("AT+CGCLASS=\"B\"","OK",1000);		//设置GPRS移动台类别为B,支持包交换和数据交换
    if(ret == 0) {
        printf("设置GPRS移动台类型为B成功\r\n");
    }
    else {
        printf("设置GPRS移动台类型为B失败\r\n");
    }
	i=0;
    while((ret=sendAT("AT+CGATT=1","OK",5000))!=0)//附着GPRS业务
	{
		i++;
		printf("附着GPRS业务失败\r\n");
		if(i>=3)
		{
			break;
		}			
	}
    if(ret == 0) 
	{
        printf("附着GPRS业务成功\r\n");
    }

    ret = sendAT("AT+CIPCSGP=1,\"IP\",\"CMNET\"","OK",5000);	//设置PDP参数
    if(ret == 0) {
        printf("设置PDP参数成功\r\n");
    }
    else {
        printf("设置PDP参数失败\r\n");
    }

    ret = sendAT("AT+CIPHEAD=1 ","OK",5000);			//激活PDP，正确激活以后就可以上网了
    if(ret == 0) {
        printf("激活PDP成功\r\n");
        GPRS_status=1;
    }
    else {
        printf("激活PDP失败\r\n");
        GPRS_status=0;
			return -1;
    }
		return 0;
}

//心跳发送函数
void SIM868_SendHeart(void)
{
    char Heart_pack[100]= {0};
    sprintf(Heart_pack,"IMEI :%s ,%04d,%03d,%03d,%03d,sQ: %02d",   //一次数据打包                
                            u1_data_Pack.IMEI,                          //模块号数
                            sysData_Pack.data.bootTimes,      //正常开机次数
                            sysData_Pack.data.SIM868_bootTimes, //上传服务器器失败次数
                            sysData_Pack.data.postErrorTimes,  //POST失败次数
                            sysData_Pack.data.GPSErrorTimes,
                            signalQuality);
	submitInformation(Heart_pack);

}





//上传GPS信息到服务器
void submitInformation(char * GPS_Info)
{
    s8 ret;
    isSendData = 1;		//在和服务器通信的标志位
    isSendDataError=0;   //错误位清零
		SIM868_GPRS_Test();
		Check_TCP();
	
    if(sendAT("AT+CIPSEND",">",5000)==0)		//发送数据
    {
        u1_printf("%s\r\n",GPS_Info);
        delay_ms(10);
        if(sendAT((char*)0X1A,"SEND OK",5000)==0)
        {
            cleanReceiveData();//清除接收器
            printf("POST成功\r\n");
            ret= 0;
        }
        else
        {
            isSendDataError = 1;//设置标志位为失败
            printf("POST失败\r\n");
            ret= -1;
            sysData_Pack.data.postErrorTimes++;
        }

    }
    else
    {
        sendAT((char*)0X1B,0,0);	//ESC,取消发送
        isSendDataError = 1;//设置标志位为失败
        printf("POST失败，取消发送");
        ret= -1;
    }
		

    isSendData = 0;//转换标志位为未在与服务器通信
}



void Send_LBS()
{
	s8 ret; 
	ret=sendAT("AT+CLBSCFG=0,3","www",3000);
	if(ret!=0)
		printf("查询LBS默认地址失败\r\n");
	else
		printf("查询LBS默认地址成功\r\n");
	ret=sendAT("AT+CLBS=1,1","+CLBS:",5000);
	if(ret!=0)
	{
		printf("获取经纬度失败\r\n");
	}
}


//解析LBS的经纬度，提取经度
char * AnalyticalLBS(void)
{
	u16 i;
	u16 Strlenth;
	char *CommaADDR1;
	char *CommaADDR2;
	char *CommaADDR3;
	
	CommaADDR1=ReturnCommaPosition((char *)u1_data_Pack.USART1_RX_BUF,1);
	CommaADDR2=ReturnCommaPosition((char *)u1_data_Pack.USART1_RX_BUF,2);
	CommaADDR3=ReturnCommaPosition((char *)u1_data_Pack.USART1_RX_BUF,3);
	//提取经度
	Strlenth=CommaADDR2-CommaADDR1;
	for(i=0;i<Strlenth-1;i++)
	{
		LBS_PACK.LBS_lonStr[i]=CommaADDR1[i+1];
	}
	LBS_PACK.LBS_lonStr[i]='\0';
	LBS_PACK.LBS_lonNum=atof(LBS_PACK.LBS_lonStr);
	
	//提取纬度
	Strlenth=CommaADDR3-CommaADDR2;
	for(i=0;i<Strlenth-1;i++)
	{
		LBS_PACK.LBS_latStr[i]=CommaADDR2[i+1];
	}
	LBS_PACK.LBS_latStr[i]='\0';
	LBS_PACK.LBS_latNum=atof(LBS_PACK.LBS_latStr);
	return 0;
}
//解析函数
//+HTTPACTION: 1,200,94
//+HTTPREAD: 94
//<?xml version="1.0" encoding="utf-8"?>
//<string xmlns="http://tempuri.org/">Submit OK</string>
char * AnalyticalData(char *searchStr_ack)
{
    u16 i;
	char *res=0;
	u16 outTime=200;
	if(searchStr_ack && outTime)//当searchStr和outTime不为0时才等待应答
    {
        while((--outTime)&&(res == 0))//等待指定的应答或超时时间到来
        {
			for(i=0;i<u1_data_Pack.USART1_RX_STA;i++)
			{
				if(u1_data_Pack.USART1_RX_BUF[i]==*(searchStr_ack))
				{
					res = strstr((char *)u1_data_Pack.USART1_RX_BUF+i,searchStr_ack);
					if(res!=0)
					{
						delay_ms(50);
						break;
					}
				}
				if((i==USART1_MAX_RECV_LEN)||res!=0)
					break;
			}
            delay_ms(100);
        }
    }
	return res;
}
//2017/5/12 :

void Check_AT(void)
{
	u8 i=0;
     while(sendAT("AT\r\n","OK",500) != 0)
    {
        printf("AT检查模块是否死机```%d\r\n",i++);
        if(i >= 10)
        {
            printf("\r\n模块无响应，准备重启\r\n");
						saveData();
            u1_data_Pack.Error = 1;//标记检查AT出错，等待看门狗复位单片机
            while(1);	//等待看门狗复位
        }
    }
	
}



const char ADDR[] = "\"58.17.235.65\",\"7700\"";
s8 TCP_Connect(void)
{
    s8 ret;
    char p[100];
    sprintf((char*)p,"AT+CIPSTART=\"TCP\",%s",ADDR);
    ret = sendAT(p,"CONNECT OK",5000);	//建立TCP连接
    if(ret == 0)
    {
        printf("建立TCP的连接成功\r\n");
    }
    else
    {
        printf("建立TCP的连接失败\r\n");
				
    }
    return ret;
}
//检查当前连接状态，根据当前状态进行操作
//CONNECK OK:TCP已连接
//IP CLOSE :TCP连接被关闭
//GPRSACT:GPRS已连接，TCP未连接
//INITIAL:GPRS未连接
//其他状态就再配置一次
s8 Check_TCP(void)
{
    s8 ret;
    if((ret=sendAT("AT+CIPSTATUS","CONNECT OK",3000))==0)
    {
        printf("当前连接状态:CONNECT OK\r\n");
    }
    else
    {
			if(TCP_Connect()!=0)//如果建立失败就重新设置GPRS网络，并且重建建立连接
			{
				GPRS_Connect();
				TCP_Connect();
			}
		}

    return ret;
}









/*********************************************历史废弃函数,不参与编译******************************/

#if 0
#define

//此函数用于发送并且校验服务器返回来的信息
//发送流程为，先发送最新数据，如果成功，紧接着发送flash里面未成功的数据，
//如果失败，就将failedTimes++,failedTimes最大为30次
void SIM868_SendPost(char * TEXT_Buffer )  //发送并校验
{
    u8 yl;
    s8 ret=0;
    Upload_Time=0;     			//心跳上传服务器时间清0
    SIM868_GPRS_Test();    			//GPRS测试函数
	if(firstsend_flag==1)
	{
		firstsend_flag=0;
		GPRS_Connect();
	}
    HTTP_POST(TEXT_Buffer);      //POST打包发送
    if(ret ==0)
        printf("发送成功\r\n");
    else
        printf("发送失败\r\n");

    if(!(isSendDataError || isSendData) )
    {
        waitservice_flag=0;          //等待标志清零，开始等待服务器应答
        while(!(waitservice_flag ||(u1_data_Pack.USART1_RX_STA&(1<<15))));//等待服务器回应,如果大于

        if(AnalyticalData()!=0)
        {
            isSendDataError = 0;//设置标志位为成功
            printf("收到正确应答\r\n");
            Continue_Post_Errotimes=0 ;  //连续失败次数清零
        }
        else
        {
            isSendDataError = 1;                 
            printf("未收到正确应答\r\n");
        }
    }
    ret = sendAT("AT+CIPCLOSE","OK",1000);	//关闭连接
    if(ret == 0) {
        printf("关闭连接成功\r\n");
    }
    else {
        printf("关闭连接失败\r\n");
    }


}


char SubWebService[]= {"WebService.asmx/GpsSubmit"};
char keystr[20]= {"2-409"};
//POST方式提交一次数据
//返回值 0 成功 ；-1失败
//入口参数  GPS_Info 发送的字符串  SendType 发送类型 心跳/GPS数据

s8 HTTP_POST(char * GPS_Info)
{
    u8 i=0;
    s8 ret;

    u16 length;
    isSendData = 1;		//在和服务器通信的标志位
    isSendDataError=0;   //错误位清零
    while((ret=Check_TCP())!=0)
    {
        i++;
//        printf("TCP连接出现异常...%d\r\n",i);
        if(i>=5)
        {
			i=0;
            printf("TCP连接多次失败...准备重启\r\n");
			saveData();
            u1_data_Pack.Error=1;//
        }
        delay_ms(50);
    }
    if(SendType)
        sprintf(p1,"lat=%.6lf&lon=%0.6lf&IMEI=%s&log=%s",LatNow,LonNow,(u8 *) GPS_IMEI,GPS_Info);// 修改时间2017/3/24 17:21
    else
        sprintf(p1,"lat=%s&lon=%s&IMEI=%s&log=%s","heart","heart",(u8 *) GPS_IMEI,GPS_Info);
    printf("%s\r\n",p1);
    length=strlen(p1);
    printf("%d\r\n",length);
    sprintf(POSTDATA,"POST /%s HTTP/1.1\r\nHost: %s\r\n\
Content-Type: application/x-www-form-urlencoded\r\nContent-Length: %d\r\n\r\n%s\r\n\r\n",
            SubWebService,ipaddr,length,p1	);

    if(sendAT("AT+CIPSEND",">",5000)==0)		//发送数据
    {
        printf("CIPSEND DATA:%s\r\n",p1);	 			//发送数据打印到串口

        u1_printf("%s\r\n",POSTDATA);
        delay_ms(10);
        if(sendAT((char*)0X1A,"OK",5000)==0)
        {
            cleanReceiveData();//清除接收器
            printf("POST成功\r\n");
            ret= 0;
        }
        else
        {
            isSendDataError = 1;//设置标志位为失败
            printf("POST失败\r\n");
            ret= -1;
            sysData_Pack.data.postErrorTimes++;
        }

    }
    else
    {
        sendAT((char*)0X1B,0,0);	//ESC,取消发送
        isSendDataError = 1;//设置标志位为失败
        printf("POST失败，取消发送");
        ret= -1;
    }

    isSendData = 0;//转换标志位为未在与服务器通信
    return ret;
}


//HTTP/1.1 200 OK
//Content-Type: text/xml; charset=utf-8
//Content-Length: length

//<?xml version="1.0" encoding="utf-8"?>
//<string xmlns="http://tempuri.org/">string</string>
//解析返回参数
char * AnalyticalData(void)
{
    u16 i;
    char * retHead = 0;
    char * retTail = 0;
    if(my_strstr((char *)u1_data_Pack.USART1_RX_BUF,"HTTP/1.1 200 OK")!=0)
    {
        retTail=my_strstr((char *)u1_data_Pack.USART1_RX_BUF,"</string>");
        if(retTail!=0)
        {
            for(i=0; i<USART1_MAX_RECV_LEN; i++)
            {
                if( *(retTail-i)=='>' )
                {
                    retHead=retTail-i+1;
                    * retTail=0;
                    delay_ms(20);
//                    LCD_SString(10,100,300,200,12,(u8 *)retHead);		//显示一个字符串,12/16字体
                    return retHead;
                }
            }
        }


    }
    return 0;
}




char ipaddr[]={"gpsws.cqutlab.cn"};	//IP地址
char port[]={"80"};				//端口号

s8 TCP_Connect(void)
{
    s8 ret;
    char p[100];
    sprintf((char*)p,"AT+CIPSTART=\"TCP\",\"%s\",%s",ipaddr,port);
    ret = sendAT(p,"OK",5000);	//建立TCP连接
    if(ret == 0)
    {
        printf("建立TCP的连接成功\r\n");
    }
    else
    {
        printf("建立TCP的连接失败\r\n");
    }
    return ret;
}
//检查当前连接状态，根据当前状态进行操作
//CONNECK OK:TCP已连接
//IP CLOSE :TCP连接被关闭
//GPRSACT:GPRS已连接，TCP未连接
//INITIAL:GPRS未连接
//其他状态就再配置一次
s8 Check_TCP(void)
{
    s8 ret;
    u8 state=0;

    if((ret=sendAT("AT+CIPSTATUS","0,IP CLOSE",3000))==0)
    {
        printf("当前连接状态:IP CLOSE\r\n");
        state=5;
    }
    else
    {
        if(my_strstr((char *)u1_data_Pack.USART1_RX_BUF,"0,CONNECT OK")!=0)
        {
            printf("当前连接状态:CONNECT OK\r\n");
            state=4;
        }
        else if(my_strstr((char *)u1_data_Pack.USART1_RX_BUF,"0,IP GPRSACT")!=0)
        {
            printf("当前连接状态:GPRSACT\r\n");
            state=3;
        }
        else if(my_strstr((char *)u1_data_Pack.USART1_RX_BUF,"0,IP INITIAL")!=0)
        {
            printf("当前连接状态:INITIAL\r\n");
            state=2;
        }
        else
        {
            printf("当前连接状态:未知\r\n");
            state=1;
        }
    }
    switch(state)
    {
    case 1:
    case 2:
        GPRS_Connect();
    case 3:case 5:
        ret=TCP_Connect();break;
	case 4:ret=0;break;
	
        
    }
    return ret;
}
#endif
