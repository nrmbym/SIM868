#ifndef __STMFLASH_H__
#define __STMFLASH_H__
#include "sys.h"

//59-63k 用来存放未上传成功的打包数据
//64K    存放开机次数,错误信息次数

//#define FLASH_SAVE_GPS     0X0800EC00       //从第59k开始。用于存放未能成功上传的GPS信息，便于下一次上传
#define FLASH_SAVE_GPS       0X0800F000       //60开始存放
#define FLASH_SAVE_GPS_DEV   0X0800EC00+164   //每次偏移固定的地址用strnlen()函数进行计算

#define FLASH_Information    0X0800F000       //60k
//#define FLASH_Information  0X08010400       //64K起始地址
//#define FLASH_Information    0X800E800       //58K起始地址   存放开机次数等

//////////////////////////////////////////////////////////////////////////////////////////////////////
//用户根据自己的需要设置
#define STM32_FLASH_SIZE 512 	 		//所选STM32的FLASH容量大小(单位为K)
#define STM32_FLASH_WREN 1              //使能FLASH写入(0，不是能;1，使能)
//////////////////////////////////////////////////////////////////////////////////////////////////////

//FLASH起始地址
#define STM32_FLASH_BASE 0x08000000 	//STM32 FLASH的起始地址
//FLASH解锁键值
u16 STMFLASH_ReadHalfWord(u32 faddr);		  //读出半字
void STMFLASH_WriteLenByte(u32 WriteAddr,u32 DataToWrite,u16 Len);	//指定地址开始写入指定长度的数据
u32 STMFLASH_ReadLenByte(u32 ReadAddr,u16 Len);						//指定地址开始读取指定长度数据
void STMFLASH_Write(u32 WriteAddr,u16 *pBuffer,u16 NumToWrite);		//从指定地址开始写入指定长度的数据
void STMFLASH_Read(u32 ReadAddr,u16 *pBuffer,u16 NumToRead);   		//从指定地址开始读出指定长度的数据

//测试写入
void Test_Write(u32 WriteAddr,u16 WriteData);
extern char TEXT_Buffer_1[100];               //第1次缓存打包数据
//extern char TEXT_Buffer_2[85];               //第2次缓存打包数据
extern char datatemp[100];                  //缓存读取打包数据
extern void FLASH_initialize(void);         //FLASH中数据的初始化
extern s32 signalQuality;
extern void FLASH_GPS_Pack(u8 m);    //未上传成功的打包数据存放到FLASH中
extern void FLASH_GPS_Read(s8 j);    //FLASH中存放的GPS数据的读取
extern void Erase_FLASH(u32 WriteAddr,u16 NumToWrite);     //擦除指定地址的FLASH
extern void Testing_FLASH(u32 WriteAddr,u16 NumToWrite);   //检验FLASH中是否为空
extern void saveData(void);                                //数据写入FLASH
#endif

















