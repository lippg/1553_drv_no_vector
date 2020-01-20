/*********************************************************************
文件名称:   1553BRtLib.c

文件功能:   实现Vxworks 61580芯片驱动

文件说明:   源文件

当前版本:   V1.0

修改记录:   2016-08-22  V1.0    李世杰  创建
***********************************************************************/
#include "vxWorks.h"
#include "stdlib.h"
#include "stdio.h"
#include "string.h"
#include "ioLib.h"
#include "drv\pci\pciConfigLib.h"
#include "drv\pci\pciIntLib.h"
#include "errnoLib.h"
#include "errno.h"
#include "logLib.h"
#include "iosLib.h"
#include "sysLib.h"
#include "vmLib.h"
#include "arch\I86\ivI86.h"
#include "1553bMtLib.h"
#include "1553bReg.h"
#include "sysLib.h"

MsgType_t *pMsg;
BU61580MT_DEV *pBu61580MtDev;
static void Bu61580MtInterrupt(void);
void tBu61580MtRecv(BU61580MT_DEV *pBu61580MtDev);
int MtPutBuf(FAST RING_ID rngId,unsigned short *buffer, int nbytes);
int MtGetBuf(FAST RING_ID rngId, unsigned short *buffer, int nbytes);
int _ReadMtMesg(MsgType_t *Message);
/********************************************************************
函数名称:   InitMtMode

函数功能:   初始化RT功能

参数名称        类型                输入/输出           含义
sa              unsigned short      input               RT地址

返回值  :   0

函数说明:   初始化RT功能

修改记录:   2016-10-18  李世杰  创建
********************************************************************/
int InitMtMode(void)
{
    int i = 0;
	
	pMsg = (MsgType_t *)malloc(sizeof(MsgType_t));
    bzero((char *)pMsg, sizeof(MsgType_t));
			
	WRITE_REGISTER_USH(pBu61580MtDev->bu61580RegBase, START_REG, 0x0001);    /* 复位芯片 */
	taskDelay(10);
	WRITE_REGISTER_USH(pBu61580MtDev->bu61580RegBase, CONF_REG_3, 0xffff);
	WRITE_REGISTER_USH(pBu61580MtDev->bu61580RegBase, CONF_REG_3, 0x8700);
	DEBUG_PRINTF("CONF_REG_3(at InitMtMode) = 0x%x\n", READ_REGISTER_USH(pBu61580MtDev->bu61580RegBase, CONF_REG_3), 0,0,0,0,0);

	WRITE_MEM_USH(pBu61580MtDev->bu61580MemBase, 0x102, 0x0000);    /* 初始化桟顶到0x0400 */
	DEBUG_PRINTF("_ReadMtMesg: StackPtr = %x\n", READ_REGISTER_USH(pBu61580MtDev->bu61580MemBase,0x102), 0,0,0,0,0);

	WRITE_MEM_USH(pBu61580MtDev->bu61580MemBase, 0x103, 0x0800);    /* 初始化桟顶到0x0800 */
	DEBUG_PRINTF("_ReadMtMesg: dataPtr = %x\n", READ_REGISTER_USH(pBu61580MtDev->bu61580MemBase,0x103), 0,0,0,0,0);

	for(i = 0; i < 0x128; i++)
	{
		WRITE_MEM_USH(pBu61580MtDev->bu61580MemBase, (0x280 + i), 0xffff);    /* 全部使能合法 */
	}

	WRITE_REGISTER_USH(pBu61580MtDev->bu61580RegBase, CONF_REG_2, 0x0418);
	DEBUG_PRINTF("CONF_REG_2(at InitMtMode) = 0x%x\n", READ_REGISTER_USH(pBu61580MtDev->bu61580RegBase, CONF_REG_2), 0,0,0,0,0);

	WRITE_REGISTER_USH(pBu61580MtDev->bu61580RegBase, INT_MASK_REG, 0x0001);   /*  中断屏蔽 */
	DEBUG_PRINTF("INT_MASK_REG(at InitMtMode) = 0x%x\n", READ_REGISTER_USH(pBu61580MtDev->bu61580RegBase, INT_MASK_REG), 0,0,0,0,0);

	/* 使能MT模式, 之前加延时，否则读写寄存器不正常*/
	WRITE_REGISTER_USH(pBu61580MtDev->bu61580RegBase, CONF_REG_1, 0x5000);
	DEBUG_PRINTF("CONF_REG_1(at InitMtMode) = 0x%x\n", READ_REGISTER_USH(pBu61580MtDev->bu61580RegBase, CONF_REG_1),0,0,0,0,0);

	/* START MT MODE */
	WRITE_REGISTER_USH(pBu61580MtDev->bu61580RegBase, START_REG, MT_START);    
	DEBUG_PRINTF("InitMtMode: START_REG = 0x%x\n", READ_REGISTER_USH(pBu61580MtDev->bu61580RegBase, START_REG), 0,0,0,0,0);
	DEBUG_PRINTF("CONF_REG_1(at InitMtMode) = 0x%x\n", READ_REGISTER_USH(pBu61580MtDev->bu61580RegBase, CONF_REG_1),0,0,0,0,0);

	if(READ_REGISTER_USH(pBu61580MtDev->bu61580RegBase, CONF_REG_1) != 0x5004)
	{
		return -1;
	}

	pBu61580MtDev->currStk = READ_REGISTER_USH(pBu61580MtDev->bu61580MemBase,0x102);
	pBu61580MtDev->lastStk = pBu61580MtDev->currStk;
	DEBUG_PRINTF("InitMtMode: currStk = %x, lastStk = %x\n", pBu61580MtDev->currStk, pBu61580MtDev->lastStk,0,0,0,0);

	/* 创建信号*/
    pBu61580MtDev->BuSemID = semBCreate(SEM_Q_FIFO, SEM_EMPTY);
	
	pBu61580MtDev->stkRngID = rngCreate(8192); /* 栈软件缓存 */
	pBu61580MtDev->dataRngID = rngCreate(10240); /* 数据软件缓存 */
	
	/* 创建接收任务 */
    pBu61580MtDev->MtTaskID = taskSpawn("tBu61580MtRecv", 60, 0, 8192, (FUNCPTR)tBu61580MtRecv, (int)pBu61580MtDev,0,0,0,0,0,0,0,0,0);
	
	return 0;
}

/********************************************************************
函数名称:   _ReadMtMesg

函数功能:   处理RT消息

参数名称        类型                输入/输出           含义
BUFFMODE        unsigned short      input               读模式
buffer          unsigned short *    input               数据指针

返回值  :   空

函数说明:   处理RT消息，可以在中断调用，也可以用作轮询处理

修改记录:   2016-10-18  李世杰  创建
********************************************************************/
int _ReadMtMesg(MsgType_t *Message)
{
    unsigned short StackPtr,DataPtr;
	int i = 0;
	
	if(rngIsFull(pBu61580MtDev->stkRngID) | rngIsFull(pBu61580MtDev->dataRngID))
	{
	}
	
	StackPtr = READ_MEM_USH(pBu61580MtDev->bu61580MemBase, 0x102);
	DEBUG_PRINTF("_ReadMtMesg: StackPtr = %x\n", StackPtr, 0,0,0,0,0);	
	pBu61580MtDev->lastStk = StackPtr;
	#if 0
	if(pBu61580MtDev->lastStk >pBu61580MtDev->currStk)
	{
		if((pBu61580MtDev->lastStk - pBu61580MtDev->currStk) % 4 != 0)
		{
			pBu61580MtDev->currStk = pBu61580MtDev->lastStk;
			StackPtr = READ_MEM_USH(pBu61580MtDev->bu61580MemBase, 0x102);
			DEBUG_PRINTF("_ReadMtMesg: StackPtr1 = %x\n", StackPtr, 0,0,0,0,0);
		}
	}
	else
	{
		if((pBu61580MtDev->currStk - pBu61580MtDev->lastStk) % 4 != 0)
		{
			pBu61580MtDev->currStk = pBu61580MtDev->lastStk;
			StackPtr = READ_MEM_USH(pBu61580MtDev->bu61580MemBase, 0x102);
			DEBUG_PRINTF("_ReadMtMesg: StackPtr2 = %x\n", StackPtr, 0,0,0,0,0);
		}
	}
	#endif

	while(pBu61580MtDev->currStk != pBu61580MtDev->lastStk)
	{	
		StackPtr = pBu61580MtDev->currStk;
		DEBUG_PRINTF("_ReadMtMesg: currStk = %x, lastStk = %x\n", StackPtr, pBu61580MtDev->lastStk,0,0,0,0);
		Message->BlockStatus = READ_MEM_USH(pBu61580MtDev->bu61580MemBase, StackPtr);
		MtPutBuf(pBu61580MtDev->stkRngID, &Message->BlockStatus, 1);
		DEBUG_PRINTF("_ReadMtMesg: Message->BlockStatus = %x\n", Message->BlockStatus, 0,0,0,0,0);
		
		Message->TimeTag=READ_MEM_USH(pBu61580MtDev->bu61580MemBase, (StackPtr+1));
		MtPutBuf(pBu61580MtDev->stkRngID, &Message->TimeTag, 1);
		DEBUG_PRINTF("_ReadMtMesg: Message->TimeTag = %x\n", Message->TimeTag, 0,0,0,0,0);
		
		DataPtr = READ_MEM_USH(pBu61580MtDev->bu61580MemBase, (StackPtr+2));
		DEBUG_PRINTF("_ReadMtMesg: DataPtr = %x\n", DataPtr, 0,0,0,0,0);
		
		Message->CmdWord1=READ_MEM_USH(pBu61580MtDev->bu61580MemBase, (StackPtr+3));
		MtPutBuf(pBu61580MtDev->stkRngID, &Message->CmdWord1, 1);
		DEBUG_PRINTF("_ReadMtMesg: Message->CmdWord1 = %x\n", Message->CmdWord1, 0,0,0,0,0);
		
		Message->DataLength = Message->CmdWord1 & 0x001F; 	
		if(Message->DataLength == 0)
		{
			Message->DataLength = 32;	   	
		}
		DEBUG_PRINTF("_ReadMtMesg: Message->DataLength = %d\n", Message->DataLength, 0,0,0,0,0);
		
		for(i = 0; i < Message->DataLength; i++)
		{
			if((DataPtr + i) <= 0x9ff) /*9ff*/
			{
				Message->Data[i] = READ_MEM_USH(pBu61580MtDev->bu61580MemBase, (DataPtr + i));
				MtPutBuf(pBu61580MtDev->dataRngID, &Message->Data[i], 1);
				/*DEBUG_PRINTF("_ReadMtMesg: addr = %x, Message->DataAddr = %x\n", (DataPtr + i), Message->Data[i],0,0,0,0);*/
			}
			else
			{
				Message->Data[i] = READ_MEM_USH(pBu61580MtDev->bu61580MemBase, (0x800 + i - (0x0A00 - DataPtr)));
				MtPutBuf(pBu61580MtDev->dataRngID, &Message->Data[i], 1);
				/*DEBUG_PRINTF("_ReadMtMesg: addr = %x, Message->DataAddr = %x,\n%x %x\n", 0x800 + i - (0x0A00 - DataPtr), Message->Data[i],0x0A00 - DataPtr,DataPtr,0,0);*/
			}
		}
		
		if(pBu61580MtDev->currStk == 0x0fc)
		{
			pBu61580MtDev->currStk = 0x0000;
		}
		else
		{
			pBu61580MtDev->currStk = pBu61580MtDev->currStk + 4;
		}
	}
  return 0;
}

/********************************************************************
函数名称:   Bu61580MtInterrupt

函数功能:   RT接收中断服务程序

参数名称        类型                输入/输出           含义
空

返回值  :   空

函数说明:   在中断中处理接收时，如果没收到数据，不会产生接收FIFO的操作，
            此处不需要判断返回值，保留查询模式下Buffer入口参数。

修改记录:   2016-10-18  李世杰  创建
********************************************************************/
static void Bu61580MtInterrupt(void)
{
    unsigned short ush = 0;
    DEBUG_PRINTF("Entet Irq!\n", 0,0,0,0,0,0);
	
	ush = READ_REGISTER_USH(pBu61580MtDev->bu61580RegBase, INT_STATE_REG);
	DEBUG_PRINTF("Bu61580MtInterrupt: ush = %x\n", ush,0,0,0,0,0);

	if((ush & 0x0001) == 0x0001)
	{
		WRITE_REGISTER_USH(pBu61580MtDev->bu61580RegBase, INT_MASK_REG, 0x0001);   /*  中断屏蔽 */
		semGive(pBu61580MtDev->BuSemID);     /* 释放信号量，执行回调函数 */
	}
	DEBUG_PRINTF("Leave Irq!\n", 0,0,0,0,0,0);
}

void tBu61580MtRecv(BU61580MT_DEV *pBu61580MtDev)
{
    for (;;)
    {
        /* 阻塞等待中断服务程序触发信号量 */
        if (semTake(pBu61580MtDev->BuSemID, WAIT_FOREVER) == ERROR)
        {
            return ;  /* 信号量被删除时 程序不会进入死循环 */
        }
		DEBUG_PRINTF("SemTake BuSemID\n", 0,0,0,0,0,0);
		_ReadMtMesg(pMsg);
    }
}

int ReadMtMesg(MsgType_t *Message)
{
	int ret = 0;
	int i = 0;
	int j = 0;
	unsigned short buffer[32];
	
	memset(buffer, 0, 64);

	i = rngNBytes(pBu61580MtDev->stkRngID);
	if(i < 6)
	{
		return -1;
	}

	ret = MtGetBuf(pBu61580MtDev->stkRngID, buffer, 3);
	if(ret != 3)
	{
		return -1;
	}
	Message->BlockStatus = buffer[0];
	
	Message->TimeTag = buffer[1];
	
	Message->CmdWord1 = buffer[2];
	
	Message->DataLength = Message->CmdWord1 & 0x001F; 	
	if(Message->DataLength == 0)
	{
		Message->DataLength = 32;	   	
	}
	
	ret = MtGetBuf(pBu61580MtDev->dataRngID, buffer, Message->DataLength);
	/*
	if(ret != Message->DataLength)
	{
		return -1;
	}*/
	for(i = 0; i < Message->DataLength; i++)
	{
		Message->Data[i] = buffer[i];
	}

	return 0;
}

STATUS Bu61580MtCreate(int index, int channel)
{
	int BusNo;
    int DeviceNo;
    int FuncNo;
    int ret = 0;
    unsigned int ul = 0;
    unsigned char uch = 0;
	
	/* 搜索PCI总线 */
    ret = pciFindDevice(BU61580_VENDOR_ID, BU61580_DEV_ID, index, &BusNo, &DeviceNo, &FuncNo);
    if (ret == ERROR)
    {
        printf("Can not find 1553B(index = %d) in pci bus!\n", index);
        return ret;
    }

    /* 获取内存映射地址 */
    pciConfigInLong(BusNo, DeviceNo, FuncNo, PCI_CFG_BASE_ADDRESS_1, (UINT32 *)&ul);
    ul &= 0xFFFFFFF0;

    /* 映射内存地址 不能&PCI_DEV_MMU_MSK 因为长度太小 使用定长 */
    ret = sysMmuMapAdd((void *)ul, 4096, VM_STATE_MASK_FOR_ALL, VM_STATE_FOR_PCI);
    if (ret == -1)
    {
        printf("sysMmuMapAdd faild!ul = 0x%08X\n", ul);
        return ret;
    }
	
    /* 功能 */
    pciConfigOutWord (BusNo, DeviceNo, FuncNo, PCI_CFG_COMMAND, PCI_CMD_IO_ENABLE | PCI_CMD_MEM_ENABLE | PCI_CMD_MASTER_ENABLE);

    /* Get Interrupt line on the Client PCI card */
    pciConfigInByte(BusNo, DeviceNo, FuncNo, PCI_CFG_DEV_INT_LINE, &uch);
	
    /* 初始化结构体 */
    pBu61580MtDev = (BU61580MT_DEV *)malloc(sizeof(BU61580MT_DEV));
    bzero((char *)pBu61580MtDev, sizeof(BU61580MT_DEV));
    pBu61580MtDev->channel = channel;
    pBu61580MtDev->bu61580RegBase = ul;
    pBu61580MtDev->bu61580MemBase = ul + 0x4000;
    pBu61580MtDev->BuIrq = uch;

	DEBUG_PRINTF("Bu61580RtCreate: regAddr = %x, memAddr = %x, Irq = %x\n", pBu61580MtDev->bu61580RegBase,pBu61580MtDev->bu61580MemBase,uch,0,0,0);

    pciIntConnect(INUM_TO_IVEC(INT_NUM_GET(pBu61580MtDev->BuIrq)), Bu61580MtInterrupt, (int)pBu61580MtDev);

    sysIntEnablePIC(pBu61580MtDev->BuIrq);
	
    return ret;
}

int CloseMtMode(void)
{
	int ret = 0;
	ret = pciIntDisconnect2(INUM_TO_IVEC(INT_NUM_GET(pBu61580MtDev->BuIrq)), Bu61580MtInterrupt, (int)pBu61580MtDev);
	if(ret != OK)
	{
		return ret;
	}
	ret = semDelete(pBu61580MtDev->BuSemID);
	if(ret != OK)
	{
		return ret;
	}
	ret = taskDelete(pBu61580MtDev->MtTaskID);
	if(ret != OK)
	{
		return ret;
	}
	
	rngDelete(pBu61580MtDev->stkRngID);
	
	rngDelete(pBu61580MtDev->dataRngID);

	
	free(pMsg);
	return 0;
} 

int MtPutBuf(FAST RING_ID rngId,unsigned short *buffer, int nbytes)
{
	int i = 0;
	unsigned char tmpBuf[128];
	
	/* 由于vx提供的缓冲池为8位，先将数据转为8为再存到缓冲池 */
	for(i = 0; i < nbytes; i ++)
	{
		tmpBuf[i << 1] = (unsigned char)buffer[i];
		tmpBuf[(i << 1) + 1] = (unsigned char)(buffer[i] >> 8);
	}
	
	rngBufPut(rngId, (char *)tmpBuf, 2 * nbytes);

	return 0;
}

int MtGetBuf(FAST RING_ID rngId, unsigned short *buffer, int nbytes)
{
	int i = 0;
	int ret = 0;
	unsigned char tmpBuf[128];
	
	/* 由于vx提供的缓冲池为8位，读数据转为16位 */
	ret = rngBufGet(rngId, (char *)tmpBuf, 2 * nbytes);
	if(ret == 2 * nbytes)
	{
		for(i = 0; i < nbytes; i ++)
		{
			buffer[i] = tmpBuf[i << 1] | ((unsigned short)tmpBuf[(i << 1) + 1] << 8) ;
		}
	}

	return (ret / 2);
}

