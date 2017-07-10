#ifndef __SIM868_H
#define __SIM868_H
#include "sys.h"
#include "usart_1.h"
#include "usart3.h"


#define Continue_Post_Errotimes_MAX    8  //连续失败次数
#define Post_Errotimes_MAX             50   //一次开机最大允许出错次数
#define SendHeart    0
#define SendGPS      1
#define GPS_EN    PBout(13)
#define SIM868_EN PBout(14)
#define GPS_IMEI "006"

typedef struct
{
    vu16 USART1_RX_STA;			//接收到的数据状态//[15]:0,没有接收到数据;1,接收到了一批数据.//[14:0]:接收到的数据长度
    u8 Error;								//操作A6是否出错。0:未出错 1:出错
    u8 USART1_RX_BUF[USART1_MAX_RECV_LEN];//接收缓冲,最大USART2_MAX_RECV_LEN字节.
    u8 USART1_TX_BUF[USART1_MAX_SEND_LEN];//发送缓冲,最大USART2_MAX_SEND_LEN字节
    u8 signalQuality;			  //GSM信号质量
    char revision[50];			//模块固件版本信息
    char IMEI[16];				  //模块IMEI号
    char Operator[15];			//运营商
} Data_Pack;


typedef struct
{
	char LBS_latStr[20];
	char LBS_lonStr[20];
	double  LBS_latNum;
	double  LBS_lonNum;
}LBS_Info;



extern u8 Heart_error;              //心跳上传失败位
extern char p1[300];
extern char POSTDATA[1000];
extern u8 Continue_Post_Errotimes ; //
extern u8 Post_Errotimes;       //SIM868一次开机最大次数
extern Data_Pack u1_data_Pack;  //SIM868相关结构体
extern u8 isSendDataError;       //发送错误标志位
extern u8 isSendData;           //通信标志位
extern u8 deviceState;          //SIM868工作状态标志
extern u8 GPRS_status;
extern u8 SendType;
extern u8 firstsend_flag;


extern s8 sendAT(char *sendStr,char *searchStr,u32 outTime);//发送AT指令函数
extern s8 Check_TCP(void);    //检查当前连接状态
extern s8 GPRS_Connect(void);   //GPRS连接
extern s8 TCP_Connect(void);   //TCP连接
//extern s8 HTTP_POST(char * GPS_Info ); //POST发送
extern s8 extractParameter(char *sourceStr,char *searchStr,s32 *dataOne,s32 *dataTwo,s32 *dataThree);
extern s8 SIM868_GPRS_Test(void);  //检测GPRS连接状态

extern u8 charNum(char *p ,char Char);


extern void cleanReceiveData(void);    //清楚接收器
extern void checkRevision(void);//查询模块版本信息
extern void getIMEI(void); //获取IMEI
extern void SIM868_Init(void);   //初始化SIM868
extern void SIM868_GPS_Init(void);  //初始化SIM868
extern void submitInformation(char * GPS_Info);  //发送并校验
extern void SIM868_SendHeart(void);//发送心跳包
extern void Restart_SIM868(void);   //硬件重启SIM868
extern void Check_AT(void);
extern void PWR_Init(void);

extern char * my_strstr(char *FirstAddr,char *searchStr);  //搜索字符串函数
extern char * AnalyticalLBS(void);//解析函数
extern char * ReturnCommaPosition(char *p ,u8 num);
extern char * AnalyticalData(char *searchStr_ack);

#endif
