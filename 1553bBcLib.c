/*********************************************************************
�ļ�����:   1553BBcLib.c

�ļ�����:   ʵ��Vxworks 61580оƬ����

�ļ�˵��:   Դ�ļ�

��ǰ�汾:   V1.0

�޸ļ�¼:   2016-08-22  V1.0    ������  ����
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
��������:   InitBcMode

��������:   ��ʼ��Bc����

��������        ����                ����/���           ����
sa              unsigned short      input               Bc��ַ

����ֵ  :   0

����˵��:   ��ʼ��Bc����

�޸ļ�¼:   2016-10-18  ������  ����
********************************************************************/
int InitBcMode(void)
{	
	pMsg = (MsgType_t *)malloc(sizeof(MsgType_t));
    bzero((char *)pMsg, sizeof(MsgType_t));
	
	/* �����ź�*/
    pBu61580BcDev->BuSemID   = semBCreate(SEM_Q_FIFO, SEM_EMPTY);
	
	WRITE_REGISTER_USH(pBu61580BcDev->bu61580RegBase, START_REG, 0x0001);    /* ��λоƬ */
	taskDelay(10);
	WRITE_REGISTER_USH(pBu61580BcDev->bu61580RegBase, INT_MASK_REG, 0x0000);    /* �ж����� */
	
	WRITE_REGISTER_USH(pBu61580BcDev->bu61580RegBase, CONF_REG_3, 0xffff);
	WRITE_REGISTER_USH(pBu61580BcDev->bu61580RegBase, CONF_REG_3, 0x8000);    /* ����չģʽ */
	DEBUG_PRINTF("CONF_REG_3(at InitBcMode) = 0x%x\n", READ_REGISTER_USH(pBu61580BcDev->bu61580RegBase, CONF_REG_3), 0,0,0,0,0);

	WRITE_REGISTER_USH(pBu61580BcDev->bu61580RegBase, CONF_REG_2, 0x0418);
	DEBUG_PRINTF("CONF_REG_2(at InitBcMode) = 0x%x\n", READ_REGISTER_USH(pBu61580BcDev->bu61580RegBase, CONF_REG_2), 0,0,0,0,0);

	WRITE_MEM_USH(pBu61580BcDev->bu61580MemBase, 0x100, 0x0000);    /* ��ʼ���C����0x0000 */
	DEBUG_PRINTF("stkPtr(at InitBcMode) = 0x%x\n", READ_MEM_USH(pBu61580BcDev->bu61580MemBase, 0x100),0,0,0,0,0);
	WRITE_MEM_USH(pBu61580BcDev->bu61580MemBase, 0x104, 0x0F00);    /* ��ʼ���C����0x0000 */
	/* ʹ��Bcģʽ, ֮ǰ����ʱ�������д�Ĵ���������*/
	WRITE_REGISTER_USH(pBu61580BcDev->bu61580RegBase, CONF_REG_1, 0x0000);
	DEBUG_PRINTF("CONF_REG_1(at InitBcMode) = 0x%x\n", READ_REGISTER_USH(pBu61580BcDev->bu61580RegBase, CONF_REG_1),0,0,0,0,0);
	
	pBu61580BcDev->currStk = READ_REGISTER_USH(pBu61580BcDev->bu61580MemBase,STACK_POINTER_A);
	pBu61580BcDev->lastStk = pBu61580BcDev->currStk;
	DEBUG_PRINTF("InitMtMode: currStk = %x, lastStk = %x\n", pBu61580BcDev->currStk, pBu61580BcDev->lastStk,0,0,0,0);
	
	pBu61580BcDev->stkRngID = rngCreate(8192); /* ջ������� */
	pBu61580BcDev->dataRngID = rngCreate(10240); /* ����������� */
	
	/* ������������ */
    pBu61580BcDev->BcTaskID = taskSpawn("tBu61580BcRecv", 60, 0, 8192, (FUNCPTR)tBu61580BcRecv, (int)pBu61580BcDev,0,0,0,0,0,0,0,0,0);
	return 0;
}

/********************************************************************
��������:   WriteBcMesg

��������:   ����Bc���ͻ���

��������        ����                ����/���           ����
bufData         unsigned short *    input               ����ָ��
count           unsigned short      input               �������ݳ���
sa              unsigned short      input               Bc�ӵ�ַ

����ֵ  :   ��

����˵��:   ����Bc���ͻ��棬�ȴ�BC�ϴ���Ϣָ��

�޸ļ�¼:   2016-10-18  ������  ����
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
		WRITE_MEM_USH(pBu61580BcDev->bu61580MemBase, 0x0101, 0xFFFE); /*��һ֡ */
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
��������:   ReadBcMesg

��������:   ����Bc��Ϣ

��������        ����                ����/���           ����
BUFFMODE        unsigned short      input               ��ģʽ
buffer          unsigned short *    input               ����ָ��

����ֵ  :   ��

����˵��:   ����Bc��Ϣ���������жϵ��ã�Ҳ����������ѯ����

�޸ļ�¼:   2016-10-18  ������  ����
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
				WRITE_MEM_USH(pBu61580BcDev->bu61580MemBase, (DataPtr + i + 4), 0x0000);		/*����ȡ���Ŀռ�����*/
			}
			
			Message->Status1 = READ_MEM_USH(pBu61580BcDev->bu61580MemBase, (DataPtr + 3));
			WRITE_MEM_USH(pBu61580BcDev->bu61580MemBase, (DataPtr + 3), 0x0000);
			
			if((Message->CmdWord1 & 0xf800) == Message->Status1)		/*Rt���ص�״̬����ȷ*/
			{
				BcPutMessege(Message);    /* ֻ����RT-BC���� */
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
��������:   Hi6130Int

��������:   Bc�����жϷ������

��������        ����                ����/���           ����
��

����ֵ  :   ��

����˵��:   ���ж��д������ʱ�����û�յ����ݣ������������FIFO�Ĳ�����
            �˴�����Ҫ�жϷ���ֵ��������ѯģʽ��Buffer��ڲ�����

�޸ļ�¼:   2016-10-18  ������  ����
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
		semGive(pBu61580BcDev->BuSemID);     /* �ͷ��ź��� */
	}
	
	
	DEBUG_PRINTF("Leave Irq!\n", 0,0,0,0,0,0);
}

void tBu61580BcRecv(BU61580BC_DEV *pBu61580BcDev)
{
    for (;;)
    {
        /* �����ȴ��жϷ�����򴥷��ź��� */
        if (semTake(pBu61580BcDev->BuSemID, WAIT_FOREVER) == ERROR)
        {
            return ;  /* �ź�����ɾ��ʱ ���򲻻������ѭ�� */
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
	
	/* ����vx�ṩ�Ļ����Ϊ8λ���Ƚ�����תΪ8Ϊ�ٴ浽����� */
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
	
	/* ����vx�ṩ�Ļ����Ϊ8λ��������תΪ16λ */
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
	
	/* ����PCI���� */
    ret = pciFindDevice(BU61580_VENDOR_ID, BU61580_DEV_ID, index, &BusNo, &DeviceNo, &FuncNo);
    if (ret == ERROR)
    {
        printf("Can not find 1553B(index = %d) in pci bus!\n", index);
        return ret;
    }

    /* ��ȡ�ڴ�ӳ���ַ */
    pciConfigInLong(BusNo, DeviceNo, FuncNo, PCI_CFG_BASE_ADDRESS_1, (UINT32 *)&ul);
    ul &= 0xFFFFFFF0;

    /* ӳ���ڴ��ַ ����&PCI_DEV_MMU_MSK ��Ϊ����̫С ʹ�ö��� */
    ret = sysMmuMapAdd((void *)ul, 4096, VM_STATE_MASK_FOR_ALL, VM_STATE_FOR_PCI);
    if (ret == -1)
    {
        printf("sysMmuMapAdd faild!ul = 0x%08X\n", ul);
        return ret;
    }
	
    /* ���� */
    pciConfigOutWord (BusNo, DeviceNo, FuncNo, PCI_CFG_COMMAND, PCI_CMD_IO_ENABLE | PCI_CMD_MEM_ENABLE | PCI_CMD_MASTER_ENABLE);

    /* Get Interrupt line on the Client PCI card */
    pciConfigInByte(BusNo, DeviceNo, FuncNo, PCI_CFG_DEV_INT_LINE, &uch);
	
    /* ��ʼ���ṹ�� */
    pBu61580BcDev = (BU61580BC_DEV *)malloc(sizeof(BU61580BC_DEV));
    bzero((char *)pBu61580BcDev, sizeof(BU61580BC_DEV));
    pBu61580BcDev->channel = channel;
    pBu61580BcDev->bu61580RegBase = ul;
    pBu61580BcDev->bu61580MemBase = ul + 0x4000;
    pBu61580BcDev->BuIrq = uch;

	DEBUG_PRINTF("Bu61580BcCreate: regAddr = %x, memAddr = %x, Irq = %x\n", pBu61580BcDev->bu61580RegBase,pBu61580BcDev->bu61580MemBase,uch,0,0,0);

    WRITE_REGISTER_USH(pBu61580BcDev->bu61580RegBase, START_REG, 0x0001);    /* ��λоƬ */

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
