/*********************************************************************
文件名称:   1553BBcLib.c

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
#include "semLib.h"
#include "sysLib.h"
#include "vmLib.h"
#include "arch\I86\ivI86.h"
#include "1553bBcLib.h"
#include "sysLib.h"

BU61580BC_DEV *pBu61580BcDev;

int g_stack_a = 0;
int g_data_a = 0x108;

IMPORT STATUS sysMmuMapAdd
(
void * address,           /* memory region base address */
UINT   length,            /* memory region length in bytes*/
UINT   initialStateMask,  /* PHYS_MEM_DESC state mask */
UINT   initialState       /* PHYS_MEM_DESC state */
);
MsgType_t *pMsg;
static void Bu61580BcInterrupt(void);
void tBu61580BcRecv(BU61580BC_DEV *pBu61580BcDev);
int BcPutBuf(FAST RING_ID rngId,unsigned short *buffer, int nbytes);
int BcGetBuf(FAST RING_ID rngId, unsigned short *buffer, int nbytes);
int _ReadBcMesg(MsgType_t *Message);
void BcPutMessege(MsgType_t *Message);
/********************************************************************
函数名称:   InitBcMode

函数功能:   初始化Bc功能

参数名称        类型                输入/输出           含义
sa              unsigned short      input               Bc地址

返回值  :   0

函数说明:   初始化Bc功能

修改记录:   2016-10-18  李世杰  创建
********************************************************************/
int InitBcMode(void)
{	
	pMsg = (MsgType_t *)malloc(sizeof(MsgType_t));
    bzero((char *)pMsg, sizeof(MsgType_t));
	
	/* 创建信号*/
    pBu61580BcDev->BuSemID   = semBCreate(SEM_Q_FIFO, SEM_EMPTY);
	
	WRITE_REGISTER_USH(pBu61580BcDev->bu61580RegBase, START_REG, 0x0001);    /* 复位芯片 */
	taskDelay(10);
	WRITE_REGISTER_USH(pBu61580BcDev->bu61580RegBase, INT_MASK_REG, 0x0000);    /* 中断屏蔽 */
	
	WRITE_REGISTER_USH(pBu61580BcDev->bu61580RegBase, CONF_REG_3, 0xffff);
	WRITE_REGISTER_USH(pBu61580BcDev->bu61580RegBase, CONF_REG_3, 0x8000);    /* 非扩展模式 */
	DEBUG_PRINTF("CONF_REG_3(at InitBcMode) = 0x%x\n", READ_REGISTER_USH(pBu61580BcDev->bu61580RegBase, CONF_REG_3), 0,0,0,0,0);

	WRITE_REGISTER_USH(pBu61580BcDev->bu61580RegBase, CONF_REG_2, 0x0418);
	DEBUG_PRINTF("CONF_REG_2(at InitBcMode) = 0x%x\n", READ_REGISTER_USH(pBu61580BcDev->bu61580RegBase, CONF_REG_2), 0,0,0,0,0);

	WRITE_MEM_USH(pBu61580BcDev->bu61580MemBase, 0x100, 0x0000);    /* 初始化桟顶到0x0000 */
	DEBUG_PRINTF("stkPtr(at InitBcMode) = 0x%x\n", READ_MEM_USH(pBu61580BcDev->bu61580MemBase, 0x100),0,0,0,0,0);
	WRITE_MEM_USH(pBu61580BcDev->bu61580MemBase, 0x104, 0x0F00);    /* 初始化桟顶到0x0000 */
	/* 使能Bc模式, 之前加延时，否则读写寄存器不正常*/
	WRITE_REGISTER_USH(pBu61580BcDev->bu61580RegBase, CONF_REG_1, 0x0000);
	DEBUG_PRINTF("CONF_REG_1(at InitBcMode) = 0x%x\n", READ_REGISTER_USH(pBu61580BcDev->bu61580RegBase, CONF_REG_1),0,0,0,0,0);
	
	pBu61580BcDev->currStk = READ_REGISTER_USH(pBu61580BcDev->bu61580MemBase,STACK_POINTER_A);
	pBu61580BcDev->lastStk = pBu61580BcDev->currStk;
	DEBUG_PRINTF("InitMtMode: currStk = %x, lastStk = %x\n", pBu61580BcDev->currStk, pBu61580BcDev->lastStk,0,0,0,0);
	
	pBu61580BcDev->stkRngID = rngCreate(8192); /* 栈软件缓存 */
	pBu61580BcDev->dataRngID = rngCreate(10240); /* 数据软件缓存 */
	
	/* 创建接收任务 */
    pBu61580BcDev->BcTaskID = taskSpawn("tBu61580BcRecv", 60, 0, 8192, (FUNCPTR)tBu61580BcRecv, (int)pBu61580BcDev,0,0,0,0,0,0,0,0,0);
	return 0;
}

/********************************************************************
函数名称:   WriteBcMesg

函数功能:   更新Bc发送缓存

参数名称        类型                输入/输出           含义
bufData         unsigned short *    input               数据指针
count           unsigned short      input               发送数据长度
sa              unsigned short      input               Bc子地址

返回值  :   空

函数说明:   更新Bc发送缓存，等待BC上传消息指令

修改记录:   2016-10-18  李世杰  创建
********************************************************************/
int WriteBcMesg(MsgType_t *Message)
{
    unsigned short ush;
	int i = 0;
	int j = 0;
    unsigned short StackPtr,DataPtr;
	
	for(i = 0; i < 5000; i++)
	{
		ush = READ_REGISTER_USH(pBu61580BcDev->bu61580RegBase, CONF_REG_1);
		if((ush & 0x0002) != 0x0002)
		{
			break;
		}
	}
	if(i == 5000)
	{
		return -1;
	}
	
	WRITE_REGISTER_USH(pBu61580BcDev->bu61580RegBase, CONF_REG_1, 0x0000);

	if((ush & 0x2000) == 0x0000)
	{
		WRITE_MEM_USH(pBu61580BcDev->bu61580MemBase, 0x0101, 0xFFFE); /*发一帧 */
		DEBUG_PRINTF("WriteBcMesg: count = %x\n", READ_REGISTER_USH(pBu61580BcDev->bu61580RegBase, CONF_REG_1), 0,0,0,0,0);
		StackPtr = g_stack_a;
		DEBUG_PRINTF("WriteBcMesg: StackPtr = %x\n", StackPtr, 0,0,0,0,0);
		WRITE_MEM_USH(pBu61580BcDev->bu61580MemBase, StackPtr, 0x0000);
		
		WRITE_MEM_USH(pBu61580BcDev->bu61580MemBase, (StackPtr + 1), 0x0000);
		
		WRITE_MEM_USH(pBu61580BcDev->bu61580MemBase, (StackPtr + 2), 0x0320);
		
		WRITE_MEM_USH(pBu61580BcDev->bu61580MemBase, (StackPtr + 3), g_data_a);
		DEBUG_PRINTF("WriteBcMesg: g_data_a = %x\n", g_data_a, 0,0,0,0,0);
		WRITE_MEM_USH(pBu61580BcDev->bu61580MemBase, g_data_a, Message->ControlWord);
		
		WRITE_MEM_USH(pBu61580BcDev->bu61580MemBase, g_data_a + 1, Message->CmdWord1);
		
		if((Message->CmdWord1 & 0x0400) == 0x0000)   
		{
			Message->DataLength = Message->CmdWord1 & 0x001F; 
			if(Message->DataLength == 0)
			{
				Message->DataLength = 32;
			}
			for(i = 0; i < Message->DataLength; i++)
			{
				WRITE_MEM_USH(pBu61580BcDev->bu61580MemBase, (g_data_a + 2 + i), Message->Data[i]);
			}
			
			WRITE_MEM_USH(pBu61580BcDev->bu61580MemBase, (g_data_a + 2 + i + 1), 0x0000);
			
			WRITE_MEM_USH(pBu61580BcDev->bu61580MemBase, (g_data_a + 2 + i + 2), 0x0000);
		}

		if(g_stack_a == 0xfc)
		{
			g_stack_a = 0x00;
		}
		else
		{
			g_stack_a = g_stack_a + 4;
		}
		
		if(g_data_a == 0x3EC)
		{
			g_data_a = 0x108;
		}
		else
		{
			g_data_a = g_data_a + 37;
		}
	}
	
	WRITE_REGISTER_USH(pBu61580BcDev->bu61580RegBase, START_REG, 0x0002);
	for(i = 0; i < 0xFFFF; i++)
	{
		ush = READ_REGISTER_USH(pBu61580BcDev->bu61580RegBase, CONF_REG_1);
		if((ush & 0x0002) != 0x0002)
		{
			break;
		}
	}
	if(i == 0xFFFF)
	{
		return -1;
	}
	if(Message->CmdWord1 & 0x0400)
	{
		_ReadBcMesg(pMsg);
	}
    else
    {
        StackPtr = READ_MEM_USH(pBu61580BcDev->bu61580RegBase, COMMAND_STACK_POINTER_REG);
        DEBUG_PRINTF("_ReadBcMesg: StackPtr = %x\n", StackPtr, 0,0,0,0,0);
        
        Message->BlockStatus=READ_MEM_USH(pBu61580BcDev->bu61580MemBase, StackPtr);
		DEBUG_PRINTF("_ReadBcMesg: Message->BlockStatus = %x\n", Message->BlockStatus, 0,0,0,0,0);
		
		Message->TimeTag =READ_MEM_USH(pBu61580BcDev->bu61580MemBase, (StackPtr+1));
		DEBUG_PRINTF("_ReadBcMesg: Message->TimeTag = %x\n", Message->TimeTag, 0,0,0,0,0);
		
		Message->GapTime = READ_MEM_USH(pBu61580BcDev->bu61580MemBase, (StackPtr+2));
		DEBUG_PRINTF("_ReadBcMesg: Message->GapTime = %x\n", Message->CmdWord1, 0,0,0,0,0);
		
		DataPtr = READ_MEM_USH(pBu61580BcDev->bu61580MemBase, (StackPtr+3));
		DEBUG_PRINTF("_ReadBcMesg: DataPtr = %x\n", DataPtr, 0,0,0,0,0);	
		
		Message->ControlWord = READ_MEM_USH(pBu61580BcDev->bu61580MemBase, DataPtr);
		DEBUG_PRINTF("_ReadBcMesg: Message->ControlWord = %x\n", Message->ControlWord, 0,0,0,0,0);
		
		Message->CmdWord1 = READ_MEM_USH(pBu61580BcDev->bu61580MemBase, (DataPtr + 1));
		DEBUG_PRINTF("_ReadBcMesg: Message->CmdWord1 = %x\n", Message->CmdWord1, 0,0,0,0,0);
        
        Message->Status1 = READ_MEM_USH(pBu61580BcDev->bu61580MemBase, (DataPtr + Message->DataLength + 3));
        DEBUG_PRINTF("_ReadBcMesg: Message->Status1 = %x\n", Message->Status1, 0,0,0,0,0);
        
    }
	return 0;
}

/********************************************************************
函数名称:   ReadBcMesg

函数功能:   处理Bc消息

参数名称        类型                输入/输出           含义
BUFFMODE        unsigned short      input               读模式
buffer          unsigned short *    input               数据指针

返回值  :   空

函数说明:   处理Bc消息，可以在中断调用，也可以用作轮询处理

修改记录:   2016-10-18  李世杰  创建
********************************************************************/
int _ReadBcMesg(MsgType_t *Message)
{
    unsigned short StackPtr,DataPtr;
	int i = 0;
	
	StackPtr = READ_MEM_USH(pBu61580BcDev->bu61580MemBase, STACK_POINTER_A);
	DEBUG_PRINTF("_ReadBcMesg: StackPtr = %x\n", StackPtr, 0,0,0,0,0);
	pBu61580BcDev->lastStk = StackPtr;
	
	while(pBu61580BcDev->currStk != pBu61580BcDev->lastStk)
	{
		StackPtr = pBu61580BcDev->currStk;
		DEBUG_PRINTF("_ReadBcMesg: currStk = %x, lastStk = %x\n", StackPtr, pBu61580BcDev->lastStk,0,0,0,0);

		Message->BlockStatus=READ_MEM_USH(pBu61580BcDev->bu61580MemBase, StackPtr);
		DEBUG_PRINTF("_ReadBcMesg: Message->BlockStatus = %x\n", Message->BlockStatus, 0,0,0,0,0);
		
		Message->TimeTag =READ_MEM_USH(pBu61580BcDev->bu61580MemBase, (StackPtr+1));
		DEBUG_PRINTF("_ReadBcMesg: Message->TimeTag = %x\n", Message->TimeTag, 0,0,0,0,0);
		
		Message->GapTime = READ_MEM_USH(pBu61580BcDev->bu61580MemBase, (StackPtr+2));
		DEBUG_PRINTF("_ReadBcMesg: Message->GapTime = %x\n", Message->CmdWord1, 0,0,0,0,0);
		
		DataPtr = READ_MEM_USH(pBu61580BcDev->bu61580MemBase, (StackPtr+3));
		DEBUG_PRINTF("_ReadBcMesg: DataPtr = %x\n", DataPtr, 0,0,0,0,0);	
		
		Message->ControlWord = READ_MEM_USH(pBu61580BcDev->bu61580MemBase, DataPtr);
		DEBUG_PRINTF("_ReadBcMesg: Message->ControlWord = %x\n", Message->ControlWord, 0,0,0,0,0);
		
		Message->CmdWord1 = READ_MEM_USH(pBu61580BcDev->bu61580MemBase, (DataPtr + 1));
		DEBUG_PRINTF("_ReadBcMesg: Message->CmdWord1 = %x\n", Message->CmdWord1, 0,0,0,0,0);
		
		if((Message->CmdWord1 & 0x0400) == 0x0400)   
		{
			Message->DataLength = Message->CmdWord1 & 0x001F; 
			if(Message->DataLength == 0)
			{
				Message->DataLength = 32;	   	
			}
			DEBUG_PRINTF("_ReadBcMesg: Message->DataLength = %x\n", Message->DataLength, 0,0,0,0,0);
			for(i = 0; i < Message->DataLength; i++)
			{
				Message->Data[i] = READ_MEM_USH(pBu61580BcDev->bu61580MemBase, (DataPtr + i + 4));
				WRITE_MEM_USH(pBu61580BcDev->bu61580MemBase, (DataPtr + i + 4), 0x0000);		/*将读取过的空间清零*/
			}
			
			Message->Status1 = READ_MEM_USH(pBu61580BcDev->bu61580MemBase, (DataPtr + 3));
			WRITE_MEM_USH(pBu61580BcDev->bu61580MemBase, (DataPtr + 3), 0x0000);
			
			if((Message->CmdWord1 & 0xf800) == Message->Status1)		/*Rt返回的状态字正确*/
			{
				BcPutMessege(Message);    /* 只缓存RT-BC数据 */
			}
		}
		else
		{
			Message->DataLength = Message->CmdWord1 & 0x001F; 
			if(Message->DataLength == 0)
			{
				Message->DataLength = 32;	   	
			}
			DEBUG_PRINTF("_ReadBcMesg: Message->DataLength = %x\n", Message->DataLength, 0,0,0,0,0);
			for(i = 0; i < Message->DataLength; i++)
			{
				Message->Data[i] = READ_MEM_USH(pBu61580BcDev->bu61580MemBase, (DataPtr + i + 2));
			}
			
			Message->Status1 = READ_MEM_USH(pBu61580BcDev->bu61580MemBase, (DataPtr + Message->DataLength + 3));
            DEBUG_PRINTF("_ReadBcMesg: Message->Status1 = %x\n", Message->Status1, 0,0,0,0,0);
			WRITE_MEM_USH(pBu61580BcDev->bu61580MemBase, (DataPtr + Message->DataLength + 3), 0x0000);
		}
		if(pBu61580BcDev->currStk == 0xfc)
		{
			pBu61580BcDev->currStk = 0x00;
		}
		else
		{
			pBu61580BcDev->currStk = pBu61580BcDev->currStk + 4;
		}

	}
  return 0;
}

/********************************************************************
函数名称:   Hi6130Int

函数功能:   Bc接收中断服务程序

参数名称        类型                输入/输出           含义
空

返回值  :   空

函数说明:   在中断中处理接收时，如果没收到数据，不会产生接收FIFO的操作，
            此处不需要判断返回值，保留查询模式下Buffer入口参数。

修改记录:   2016-10-18  李世杰  创建
********************************************************************/
static void Bu61580BcInterrupt(void)
{
    unsigned short ush = 0;
    DEBUG_PRINTF("Entet Irq!\n", 0,0,0,0,0,0);
	DEBUG_PRINTF("CONF_REG_1(at InitBcMode) = 0x%x\n", READ_REGISTER_USH(pBu61580BcDev->bu61580RegBase, CONF_REG_1),0,0,0,0,0);
	ush = READ_REGISTER_USH(pBu61580BcDev->bu61580RegBase, INT_STATE_REG);
	DEBUG_PRINTF("Bu61580BcInterrupt: ush = %x\n", ush,0,0,0,0,0);
	if((ush & 0x0001) == 0x0001)
	{
		semGive(pBu61580BcDev->BuSemID);     /* 释放信号量 */
	}
	
	
	DEBUG_PRINTF("Leave Irq!\n", 0,0,0,0,0,0);
}

void tBu61580BcRecv(BU61580BC_DEV *pBu61580BcDev)
{
    for (;;)
    {
        /* 阻塞等待中断服务程序触发信号量 */
        if (semTake(pBu61580BcDev->BuSemID, WAIT_FOREVER) == ERROR)
        {
            return ;  /* 信号量被删除时 程序不会进入死循环 */
        }
		_ReadBcMesg(pMsg);
    }
}

int ReadBcMesg(MsgType_t *Message)
{
	int ret = 0;
	int i = 0;
	unsigned short buffer[32];
	
	memset(buffer, 0, 64);
	
	i = rngNBytes(pBu61580BcDev->stkRngID);
	if(i < 10)
	{
		return -1;
	}

	ret = BcGetBuf(pBu61580BcDev->stkRngID, buffer, 5);
	if(ret != 5)
	{
		return -1;
	}
	
	Message->BlockStatus = buffer[0];
	
	Message->TimeTag = buffer[1];
	
	Message->GapTime = buffer[2];
	
	Message->CmdWord1 = buffer[3];
	
	Message->Status1  = buffer[4];
	
	Message->DataLength = Message->CmdWord1 & 0x001F; 	
	if(Message->DataLength == 0)
	{
		Message->DataLength = 32;	   	
	}
	
	ret = BcGetBuf(pBu61580BcDev->dataRngID, buffer, Message->DataLength);
	/*
	if(ret != Message->DataLength)
	{
		return -1;
	}
	*/
	for(i = 0; i < Message->DataLength; i++)
	{
		Message->Data[i] = buffer[i];
	}

	return 0;
}

void BcPutMessege(MsgType_t *Message)
{
	int  i = 0;
	BcPutBuf(pBu61580BcDev->stkRngID, &Message->BlockStatus, 1);
	
	BcPutBuf(pBu61580BcDev->stkRngID, &Message->TimeTag, 1);
	
	BcPutBuf(pBu61580BcDev->stkRngID, &Message->GapTime, 1);	
	
	BcPutBuf(pBu61580BcDev->stkRngID, &Message->CmdWord1, 1);
	
	BcPutBuf(pBu61580BcDev->stkRngID, &Message->Status1, 1);
	
	for(i = 0; i < Message->DataLength; i++)
	{
		BcPutBuf(pBu61580BcDev->dataRngID, &Message->Data[i], 1);
	}
}

int BcPutBuf(FAST RING_ID rngId,unsigned short *buffer, int nbytes)
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

int BcGetBuf(FAST RING_ID rngId, unsigned short *buffer, int nbytes)
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

STATUS Bu61580BcCreate(int index, int channel)
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
    pBu61580BcDev = (BU61580BC_DEV *)malloc(sizeof(BU61580BC_DEV));
    bzero((char *)pBu61580BcDev, sizeof(BU61580BC_DEV));
    pBu61580BcDev->channel = channel;
    pBu61580BcDev->bu61580RegBase = ul;
    pBu61580BcDev->bu61580MemBase = ul + 0x4000;
    pBu61580BcDev->BuIrq = uch;

	DEBUG_PRINTF("Bu61580BcCreate: regAddr = %x, memAddr = %x, Irq = %x\n", pBu61580BcDev->bu61580RegBase,pBu61580BcDev->bu61580MemBase,uch,0,0,0);

    WRITE_REGISTER_USH(pBu61580BcDev->bu61580RegBase, START_REG, 0x0001);    /* 复位芯片 */

    pciIntConnect(INUM_TO_IVEC(INT_NUM_GET(pBu61580BcDev->BuIrq)), Bu61580BcInterrupt, (int)pBu61580BcDev);

    sysIntEnablePIC(pBu61580BcDev->BuIrq);
	
    return ret;
}

int CloseBcMode(void)
{
	int ret = 0;
	ret = pciIntDisconnect2(INUM_TO_IVEC(INT_NUM_GET(pBu61580BcDev->BuIrq)), Bu61580BcInterrupt, (int)pBu61580BcDev);
	if(ret != OK)
	{
		return ret;
	}
	ret = semDelete(pBu61580BcDev->BuSemID);
	if(ret != OK)
	{
		return ret;
	}

	rngDelete(pBu61580BcDev->dataRngID);
	
	rngDelete(pBu61580BcDev->stkRngID);
	
	ret = taskDelete(pBu61580BcDev->BcTaskID);
	if(ret != OK)
	{
		return ret;
	}
	
	free(pMsg);
	return 0;
} 
