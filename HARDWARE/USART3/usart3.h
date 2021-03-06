#ifndef __USART3_H
#define __USART3_H
#include "sys.h"

#define USART3_MAX_RECV_LEN		5000	//最大接收缓存字节数
#define USART3_RX_EN 			1					//0,不接收;1,接收.
#define SRTING_NUM 20
#define GPS_array 10        //GPS数据存储最大个数 

typedef struct
{
    u16 year;    //年
    u8  month;   //月
    u8  day;     //日
    u8  hour;    //时
    u8 minute;   //分
    u8 second;	  //秒
    char Time[21];     //当前时间
    char Time_Old[21]; //历史时间
} Time_Pack;

__packed typedef struct     //__packed在这里表示 1、不会插入填充以对齐压缩对象 不进行对齐
{   //2、使用未对齐的访问读取或写入压缩类型的对象。
    s32 bootTimes;						//开机次数
//    s32 submitInformationErrorTimes;//上传服务器失败次数
	s32 SIM868_bootTimes;
    u32 failedTimes;	           //上一次成功之后，失败次数，用于检测到一次成功发送之后，将之前未发送的数据发送上去
    s32 postErrorTimes;				//POST失败次数
    s32 GPSErrorTimes;				//因为GPS未定位时间超时造成的SIM868重启次数
    u32 CRCData;					    //CRCData之前数据的CRC校验
    s32 isEffective;				  //记录FLASH中的数据是否有效
} _sysData;

typedef union       //共用体,里面共用一段存储空间  一个buf中存储两个字节 每两个buf存储一个数据
{
    u16 buf[sizeof(_sysData)/2];//读取或写入FLASH的数组
    _sysData data;	//自动将buf[]中的数据分离为需要的数据    buf中每两个对应一个完整的数据
} _sysData_Pack;

extern u8 USART3_RX_BUF[USART3_MAX_RECV_LEN];   //最大接收缓存是USART3_MAX_RECV_LEN字节
extern vu16 USART1_RX_STA;   						//接收数据状态
extern char *GNRMC;		//储存接收到的数据


extern char* string[SRTING_NUM];

extern u16 receive_len;			//接收计数器
extern u32 startNum;        //记录当前接收字符串在接收缓存中的开始位置
extern char* string[SRTING_NUM];
//extern char UTCTime[7];		       //UTC时间，hhmmss格式
//extern char Longitude_Str[11];	 //经度dddmm.mmmm(度分)格式
extern Time_Pack u3_time_Pack;		//时间
extern double Latitude_Temp,Baidu_Latitude_Temp;    //记录转换的经纬度
extern double Longitude_Temp,Baidu_Longitude_Temp;  //记录转换的经纬度
extern double Drift_speed;   //漂移速度
extern double distance;             //两次数据上传的距离间隔
extern double BaiduLongitude_Range[GPS_array];     //经度(百度坐标)
extern double BaiduLatitude_Range[GPS_array];	     //纬度(百度坐标)
extern double Speed_Range[GPS_array]; 
extern u8 GPS_effective;                           //GPS数据是否有效位
extern u8 Pack_length;              //数据打包长度
extern u16 GPS_unspecified_time;       //GPS连续未定位的时间
extern u8 GPS_receive;                    //串口3接收中断定时清除
 
extern _sysData_Pack sysData_Pack;
extern void getFilterLoc(double * Lat_filter,double * Lon,double * Spend_filter);   //GPS数据滤波函数
extern void GPS_Packed_Data(void);    //GPS数据打包上传函数
extern void Receive_empty(void);    //清空GPS缓存
extern void analysisGPS(void);     //GPS数据解析函数
extern void USART3_Init(u32 BaudRate); 				//串口3初始化
extern void cleanReceiveData_GPS(void);


extern char UTCTime[7];          //存放UTC时间数据 时分秒
extern char UTCTime_year[7];     //存放年月日时间
extern char Longitude_Str[11];	  //经度dddmm.mmmm(度分)格式
extern char Latitude_Str[10];	  //纬度ddmm.mmmm(度分)格式
extern char La_Position[2];     //纬度方位
extern char Lo_Position[2];     //经度方位
extern char Ground_speed[10];    //地面速度提取
extern char Azimuth[10];        //方位角
#endif



