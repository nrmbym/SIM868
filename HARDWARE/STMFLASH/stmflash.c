#include "stmflash.h"
#include "delay.h"
#include "usart.h"
#include "sim868.h"

char datatemp[100];            //用于暂时缓存FLASH中读出的数据
char TEXT_Buffer_1[100];        //缓存打包数据
s32 signalQuality =98;     //信号强度,以后需要删除重新写


//读取指定地址的半字(16位数据)
//faddr:读地址(此地址必须为2的倍数!!)
//返回值:对应数据.
u16 STMFLASH_ReadHalfWord(u32 faddr)
{
    return *(vu16*)faddr;
}
#if STM32_FLASH_WREN	//如果使能了写   
//不检查的写入
//WriteAddr:起始地址
//pBuffer:数据指针
//NumToWrite:半字(16位)数  2个字节操作
void STMFLASH_Write_NoCheck(u32 WriteAddr,u16 *pBuffer,u16 NumToWrite)
{
    u16 i;
    for(i=0; i<NumToWrite; i++)
    {
        FLASH_ProgramHalfWord(WriteAddr,pBuffer[i]);
        WriteAddr+=2;//地址增加2.
    }
}
//从指定地址开始写入指定长度的数据
//WriteAddr:起始地址(此地址必须为2的倍数!!)
//pBuffer:数据指针
//NumToWrite:半字(16位)数(就是要写入的16位数据的个数.)
#if STM32_FLASH_SIZE<256
#define STM_SECTOR_SIZE 1024 //字节
#else
#define STM_SECTOR_SIZE	2048
#endif
u16 STMFLASH_BUF[STM_SECTOR_SIZE/2];//最多是2K字节
void STMFLASH_Write(u32 WriteAddr,u16 *pBuffer,u16 NumToWrite)
{
    u32 secpos;	   //扇区地址
    u16 secoff;	   //扇区内偏移地址(16位字计算)
    u16 secremain; //扇区内剩余地址(16位字计算)
    u16 i;
    u32 offaddr;   //去掉0X08000000后的地址,真实的偏移地址
    if(WriteAddr<STM32_FLASH_BASE||(WriteAddr>=(STM32_FLASH_BASE+1024*STM32_FLASH_SIZE)))return;//非法地址
    FLASH_Unlock();						//解锁
    offaddr=WriteAddr-STM32_FLASH_BASE;		//实际偏移地址.
    secpos=offaddr/STM_SECTOR_SIZE;			  //扇区地址  0~127 for STM32F103RBT6
    secoff=(offaddr%STM_SECTOR_SIZE)/2;		//在扇区内的偏移(2个字节为基本单位.)
    secremain=STM_SECTOR_SIZE/2-secoff;		//扇区剩余空间大小
    if(NumToWrite<=secremain)secremain=NumToWrite;//不大于该扇区范围，将写入的数据长度赋给剩余的空间里面
    while(1)
    {
        STMFLASH_Read(secpos*STM_SECTOR_SIZE+STM32_FLASH_BASE,STMFLASH_BUF,STM_SECTOR_SIZE/2);//读出整个扇区的内容
        for(i=0; i<secremain; i++) //校验数据
        {
            if(STMFLASH_BUF[secoff+i]!=0XFFFF)break;//需要擦除
        }
        if(i<secremain)//需要擦除
        {
            FLASH_ErasePage(secpos*STM_SECTOR_SIZE+STM32_FLASH_BASE);//擦除这个扇区
            for(i=0; i<secremain; i++) //复制
            {
                STMFLASH_BUF[i+secoff]=pBuffer[i];
            }
            STMFLASH_Write_NoCheck(secpos*STM_SECTOR_SIZE+STM32_FLASH_BASE,STMFLASH_BUF,STM_SECTOR_SIZE/2);//写入整个扇区
        } else STMFLASH_Write_NoCheck(WriteAddr,pBuffer,secremain);//写已经擦除了的,直接写入扇区剩余区间.
        if(NumToWrite==secremain)break;//写入结束了          实际的跳出擦除写入的这个循环是这个break
        else//写入未结束
        {
            secpos++;				//扇区地址增1
            secoff=0;				//偏移位置为0 	 因为是下一个新的页，所以指针的偏移的地址是0
            pBuffer+=secremain;  	//指针偏移
            WriteAddr+=secremain;	//写地址偏移
            NumToWrite-=secremain;	//字节(16位)数递减
            if(NumToWrite>(STM_SECTOR_SIZE/2))secremain=STM_SECTOR_SIZE/2;//下一个扇区还是写不完
            else secremain=NumToWrite;//下一个扇区可以写完了
        }
    };
    FLASH_Lock();//上锁
}
#endif

//从指定地址开始读出指定长度的数据
//ReadAddr:起始地址
//pBuffer:数据指针
//NumToWrite:半字(16位)数
void STMFLASH_Read(u32 ReadAddr,u16 *pBuffer,u16 NumToRead)
{
    u16 i;
    for(i=0; i<NumToRead; i++)
    {
        pBuffer[i]=STMFLASH_ReadHalfWord(ReadAddr);//读取2个字节.
        ReadAddr+=2;//偏移2个字节.
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
//WriteAddr:起始地址
//WriteData:要写入的数据
void Test_Write(u32 WriteAddr,u16 WriteData)
{
    STMFLASH_Write(WriteAddr,&WriteData,1);//写入一个字
}

void saveData(void)
{
    CRC_ResetDR();//复位CRC
    sysData_Pack.data.CRCData = CRC_CalcBlockCRC((u32*)sysData_Pack.buf, sizeof(_sysData)/4 - 2);  //计算CRC   用于下一次的CRC校验
    sysData_Pack.data.isEffective = 1;//标志位
    STMFLASH_Write(FLASH_Information,sysData_Pack.buf,sizeof(_sysData)/2);//将数据写入FLASH	 除以2的原因是STMFLASH_Write()要求是16位两字节
}


void FLASH_initialize(void)  //开机初始化FLASH中数据
{
    u32 CRC_DATA = 0;
    STMFLASH_Read(FLASH_Information,sysData_Pack.buf,sizeof(_sysData)/2);    //将FLASH中的数据读取出来
    if(sysData_Pack.data.isEffective == 1)    //FLASH中数据有效
    {
        CRC_ResetDR();         //复位CRC数据寄存器
        //参数的含义 1.指向包含要计算的数据的缓冲区的指针.  2.要计算的缓冲区的长度
        CRC_DATA = CRC_CalcBlockCRC((u32*)sysData_Pack.buf, sizeof(_sysData)/4 - 2);   //CRC数据校验 每一个都是4个字节，得到总数,减去后面的。得到校验数据长度
        if(CRC_DATA == sysData_Pack.data.CRCData)    //校验位有效
        {
            sysData_Pack.data.bootTimes++;         //记录正常开机的次数
        }
        else
        {
            sysData_Pack.data.isEffective = 0;     //标记FLASH中数据无效
        }
    }

    if(sysData_Pack.data.isEffective != 1)    //FLASH中的数据无效,初始化数据
    {
        sysData_Pack.data.bootTimes = 1;                               //开机次数
        sysData_Pack.data.SIM868_bootTimes = 0;             //上传服务器失败次数
		    sysData_Pack.data.failedTimes=0;                               //上一次成功发送后失败次数
        sysData_Pack.data.postErrorTimes = 0;                          //POST失败次数
        sysData_Pack.data.GPSErrorTimes = 0;                           //因为GPS未定位时间超时造成重启次数
        sysData_Pack.data.isEffective = 1;                             //记录FLASH中的数据是否有效
    }
    saveData();//将数据写入FLASH

    printf("设备启动次数：%d\r\n",sysData_Pack.data.bootTimes);
    printf("A7重启次数：%d\r\n",sysData_Pack.data.SIM868_bootTimes);
    printf("POST失败次数：%d\r\n",sysData_Pack.data.postErrorTimes);
    printf("GPS超时未定位次数：%d\r\n\r\n",sysData_Pack.data.GPSErrorTimes);
}



/********************************************
FLASH中 59K—63K存放GPS未上传成功的数据
1K存放6个数据
可以存放30个未成功上传的GPS数据

因为测试问题
先从65k开始存放数据
65-69k存放未上传成功的FLASH数据
*********************************************/
void FLASH_GPS_Pack(u8 m)   //未上传成功的打包数据存放到FLASH中
{ 
	  if(m>49)     //超过最大的存储范围,从头存储
		{
			 m=0;
		}
		
    if((m+1)*Pack_length<=Pack_length*10)    //59k存放    存10次   
    {                                      
        STMFLASH_Write(FLASH_SAVE_GPS+m*Pack_length,(u16*)TEXT_Buffer_1,sizeof(TEXT_Buffer_1)/2);    //写入GPS数据到FLASH
    } 
    else if(9<m&&m<=19)     //60k              
    {
        STMFLASH_Write(FLASH_SAVE_GPS+1024+(m-10)*Pack_length,(u16*)TEXT_Buffer_1,sizeof(TEXT_Buffer_1)/2);
    }
    else if(19<m&&m<=29)    //61               
    {
        STMFLASH_Write(FLASH_SAVE_GPS+1024*2+(m-20)*Pack_length,(u16*)TEXT_Buffer_1,sizeof(TEXT_Buffer_1)/2);
    }
    else if(29<m&&m<=39)    //62           
    {
        STMFLASH_Write(FLASH_SAVE_GPS+1024*3+(m-30)*Pack_length,(u16*)TEXT_Buffer_1,sizeof(TEXT_Buffer_1)/2);
    }
//    else if(39<m&&m<=49)    //63         
//    {
//        STMFLASH_Write(FLASH_SAVE_GPS+1024*4+(m-40)*Pack_length,(u16*)TEXT_Buffer_1,sizeof(TEXT_Buffer_1)/2);
//    }
		
		printf("未上传成功的GPS数据已保存FLASH\r\n\r\n");
}

void FLASH_GPS_Read(s8 j)
{
    if((j+1)*Pack_length<=(Pack_length*10))  //在一个字节内
    {
        STMFLASH_Read(FLASH_SAVE_GPS+j*Pack_length,(u16*)datatemp,sizeof(TEXT_Buffer_1)/2);
//        datatemp[Pack_length]='\0';
    
    }
    else if(9<j&&j<=19)   //2
    {
        STMFLASH_Read(FLASH_SAVE_GPS+1024+(j-10)*Pack_length,(u16*)datatemp,sizeof(TEXT_Buffer_1)/2);
//        datatemp[Pack_length]='\0';
      
    }
    else if(19<j&&j<=29)    //3
    {
        STMFLASH_Read(FLASH_SAVE_GPS+1024*2+(j-20)*Pack_length,(u16*)datatemp,sizeof(TEXT_Buffer_1)/2);
//        datatemp[Pack_length]='\0';
  
    }
    else if(29<j&&j<=39)  //4
    {
        STMFLASH_Read(FLASH_SAVE_GPS+1024*3+(j-30)*Pack_length,(u16*)datatemp,sizeof(TEXT_Buffer_1)/2);
//        datatemp[Pack_length]='\0';
    
    }
//    else if(39<j&&j<=49)        //5字节
//    {
//        STMFLASH_Read(FLASH_SAVE_GPS+1024*4+(j-40)*Pack_length,(u16*)datatemp,sizeof(TEXT_Buffer_1)/2);
////        datatemp[Pack_length]='\0';    
//    }

}

//对FLASH指定位置的擦除
//只能一页一页的擦除
//传入参数 WriteAddr      需要擦除的地址
//         NumToWrite     需要擦除的半字(16位)数
void Erase_FLASH(u32 WriteAddr,u16 NumToWrite)
{
    u32 secpos;	   //扇区地址
    u16 secoff;	   //扇区内偏移地址(16位字计算)
    u16 secremain; //扇区内剩余地址(16位字计算)
    u16 i;
    u32 offaddr;   //去掉0X08000000后的地址
    if(WriteAddr<STM32_FLASH_BASE||(WriteAddr>=(STM32_FLASH_BASE+1024*STM32_FLASH_SIZE)))return;//非法地址
    FLASH_Unlock();						//解锁
    offaddr=WriteAddr-STM32_FLASH_BASE;		//实际偏移地址.
    secpos=offaddr/STM_SECTOR_SIZE;			//扇区地址  0~127 for STM32F103RBT6
    secoff=(offaddr%STM_SECTOR_SIZE)/2;		//在扇区内的偏移(2个字节为基本单位.)
    secremain=STM_SECTOR_SIZE/2-secoff;		//扇区剩余空间大小
    if(NumToWrite<=secremain)secremain=NumToWrite;//不大于该扇区范围
    while(1)
    {
        STMFLASH_Read(secpos*STM_SECTOR_SIZE+STM32_FLASH_BASE,STMFLASH_BUF,STM_SECTOR_SIZE/2);//读出整个扇区的内容
        for(i=0; i<secremain; i++) //校验数据
        {
            if(STMFLASH_BUF[secoff+i]!=0XFFFF)break;//需要擦除
        }
				
        if(i<secremain)//需要擦除
        {
            FLASH_ErasePage(secpos*STM_SECTOR_SIZE+STM32_FLASH_BASE);//擦除这个扇区
        }
        break;
    };
    FLASH_Lock();//上锁
}


//检测FLASH中数据是否为空
//传入参数 WriteAddr     需要检测的地址
//         NumToWrite   需要检测的半字(16位)数
void Testing_FLASH(u32 WriteAddr,u16 NumToWrite)
{
    u32 secpos;	   //扇区地址
    u16 secoff;	   //扇区内偏移地址(16位字计算)
    u16 secremain; //扇区内剩余地址(16位字计算)
    u16 i;
    u32 offaddr;   //去掉0X08000000后的地址,真实的偏移地址
    if(WriteAddr<STM32_FLASH_BASE||(WriteAddr>=(STM32_FLASH_BASE+1024*STM32_FLASH_SIZE)))return;//非法地址
    FLASH_Unlock();						//解锁
    offaddr=WriteAddr-STM32_FLASH_BASE;		//实际偏移地址.
    secpos=offaddr/STM_SECTOR_SIZE;			  //扇区地址  0~127 for STM32F103RBT6
    secoff=(offaddr%STM_SECTOR_SIZE)/2;		//在扇区内的偏移(2个字节为基本单位.)
    secremain=STM_SECTOR_SIZE/2-secoff;		//扇区剩余空间大小
    if(NumToWrite<=secremain)secremain=NumToWrite;//不大于该扇区范围，将写入的数据长度赋给剩余的空间里面
    while(1)
    {
        STMFLASH_Read(secpos*STM_SECTOR_SIZE+STM32_FLASH_BASE,STMFLASH_BUF,STM_SECTOR_SIZE/2);//读出整个扇区的内容
        for(i=0; i<secremain; i++) //校验数据
        {
            if(STMFLASH_BUF[secoff+i]!=0XFFFF)break; 
        }
        if(i<secremain)  //不为空
        {
					 printf("检验的FLASH不为空\r\n"); break;
        }
        else
				{
					printf("检验的FLASH空\r\n"); break;
				}			
    };
    FLASH_Lock();//上锁
}









