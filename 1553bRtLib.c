/*********************************************************************
�ļ�����:   1553BRtLib.c

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
#include "1553bRtLib.h"
#include "sysLib.h"

int g_count = 0;
int g_count1 = 0;
BU61580RT_DEV *pBu61580RtDev;

IMPORT STATUS sysMmuMapAdd
(
void * address,           /* memory region base address */
UINT   length,            /* memory region length in bytes*/
UINT   initialStateMask,  /* PHYS_MEM_DESC state mask */
UINT   initialState       /* PHYS_MEM_DESC state */
);
MsgType_t *pMsg;
static void Bu61580RtInterrupt(void);
void tBu61580RtRecv(BU61580RT_DEV *pBu61580RtDev);
int RtPutBuf(FAST RING_ID rngId,unsigned short *buffer, int nbytes);
int RtGetBuf(FAST RING_ID rngId, unsigned short *buffer, int nbytes);
int _ReadRtMesg(MsgType_t *Message);
/********************************************************************
��������:   InitRtMode

��������:   ��ʼ��RT����

��������        ����                ����/���           ����
sa              unsigned short      input               RT��ַ

����ֵ  :   0

����˵��:   ��ʼ��RT����

�޸ļ�¼:   2016-10-18  ������  ����
********************************************************************/
int InitRtMode(unsigned short sa)
{
    int i = 0;
	
	pMsg = (MsgType_t *)malloc(sizeof(MsgType_t));
    bzero((char *)pMsg, sizeof(MsgType_t));
	
	/* �����ź�*/
    pBu61580RtDev->BuSemID = semBCreate(SEM_Q_FIFO, SEM_EMPTY);
    
    for(i=0;i<0x20;i++)
    {
        WRITE_REGISTER_USH(pBu61580RtDev->bu61580RegBase, i, 0x0000);
    }
    
    for(i=0;i<4096;i++)
    {
        WRITE_MEM_USH(pBu61580RtDev->bu61580MemBase, i, 0x0000);
    }
	
	WRITE_REGISTER_USH(pBu61580RtDev->bu61580RegBase, START_REG, 0x0001);    /* ��λоƬ */
	taskDelay(10);
	WRITE_REGISTER_USH(pBu61580RtDev->bu61580RegBase, INT_MASK_REG, 0x0001);    /* �ж����� */
	
	WRITE_REGISTER_USH(pBu61580RtDev->bu61580RegBase, CONF_REG_3, 0x8000);
	WRITE_REGISTER_USH(pBu61580RtDev->bu61580RegBase, CONF_REG_3, 0x8001);
	DEBUG_PRINTF("CONF_REG_3(at InitRtMode) = 0x%x\n", READ_REGISTER_USH(pBu61580RtDev->bu61580RegBase, CONF_REG_3), 0,0,0,0,0);

	WRITE_REGISTER_USH(pBu61580RtDev->bu61580RegBase, CONF_REG_4, 0x4000);
	DEBUG_PRINTF("CONF_REG_4(at InitRtMode) = 0x%x\n", READ_REGISTER_USH(pBu61580RtDev->bu61580RegBase, CONF_REG_4), 0,0,0,0,0);

	WRITE_REGISTER_USH(pBu61580RtDev->bu61580RegBase, CONF_REG_2, 0x801C);
	DEBUG_PRINTF("CONF_REG_2(at InitRtMode) = 0x%x\n", READ_REGISTER_USH(pBu61580RtDev->bu61580RegBase, CONF_REG_2), 0,0,0,0,0);

	/* ʱ��λ����ʹ�� */
	WRITE_REGISTER_USH(pBu61580RtDev->bu61580RegBase, CONF_REG_5, 0x0800);
	DEBUG_PRINTF("CONF_REG_5(at InitRtMode) = 0x%x\n", READ_REGISTER_USH(pBu61580RtDev->bu61580RegBase, CONF_REG_5),0,0,0,0,0);

	WRITE_MEM_USH(pBu61580RtDev->bu61580MemBase, 0x100, 0x0000);    /* ��ʼ���C����0x0000 */
	DEBUG_PRINTF("stkPtr(at InitRtMode) = 0x%x\n", READ_MEM_USH(pBu61580RtDev->bu61580MemBase, 0x100),0,0,0,0,0);
	
	for(i = 0; i < 8; i++)
	{
		WRITE_MEM_USH(pBu61580RtDev->bu61580MemBase, (0x108 + i), 0x0000);    /* ����ʹ�ܷ�ʽ���ж�*/
	}
	for(i = 0; i < 0x100; i++)
	{
		WRITE_MEM_USH(pBu61580RtDev->bu61580MemBase, (0x300 + i), 0x0000);    /* ȫ��ʹ�ܺϷ� */
	}

	/* ����looptable */
    
    for(i=1;i<=30;i++)
    {																		/*4370->RT buffer = 512word*/
    	WRITE_MEM_USH(pBu61580RtDev->bu61580MemBase, (0x1A0 + i), 0x4210);    /* 0x4370 �жϴ�����ѭ����ַ 0x4210*/
    	DEBUG_PRINTF("InitRtMode: loop table addr = 0x%x\n", READ_MEM_USH(pBu61580RtDev->bu61580MemBase, (0x1A0 + sa)), 0,0,0,0,0);
    }
    /* ���÷��͵�ַӳ�� */
    for(i=1;i<=30;i++)
	{
		WRITE_MEM_USH(pBu61580RtDev->bu61580MemBase, (0x160 + i), 0x0800+(0x20*(i)));
		DEBUG_PRINTF("InitRtMode: transmit table addr = 0x%x\n", READ_MEM_USH(pBu61580RtDev->bu61580MemBase, (0x160 + sa)), 0,0,0,0,0);
	}
    /* ���ý��յ�ַӳ�� */
    for(i=1;i<=30;i++)
	{
		WRITE_MEM_USH(pBu61580RtDev->bu61580MemBase, (0x140 + i), 0x0400+(0x20*(i)));
		DEBUG_PRINTF("InitRtMode: recv table addr = 0x%x\n", READ_MEM_USH(pBu61580RtDev->bu61580MemBase, (0x140 + sa)), 0,0,0,0,0);
	}

    /* ���ù㲥��ַӳ�� */
    for(i=1;i<=30;i++)
    {
    	WRITE_MEM_USH(pBu61580RtDev->bu61580MemBase, (0x180 + i), 0x0c00+(0x20*i));
    }
    
	pBu61580RtDev->init_subaddr = (0x0640 + sa * 0x60);
	pBu61580RtDev->subaddr = (0x0640 + sa * 0x60);

	/* ʹ��RTģʽ, ֮ǰ����ʱ�������д�Ĵ���������*/
	WRITE_REGISTER_USH(pBu61580RtDev->bu61580RegBase, CONF_REG_1, 0x8f80);
	DEBUG_PRINTF("CONF_REG_1(at InitRtMode) = 0x%x\n", READ_REGISTER_USH(pBu61580RtDev->bu61580RegBase, CONF_REG_1),0,0,0,0,0);

	pBu61580RtDev->currStk = READ_REGISTER_USH(pBu61580RtDev->bu61580MemBase,STACK_POINTER_A);
	pBu61580RtDev->lastStk = pBu61580RtDev->currStk;
	DEBUG_PRINTF("InitMtMode: currStk = %x, lastStk = %x\n", pBu61580RtDev->currStk, pBu61580RtDev->lastStk,0,0,0,0);
	
	pBu61580RtDev->stkRngID = rngCreate(8192); /* ջ������� */
	pBu61580RtDev->dataRngID = rngCreate(10240); /* ����������� */
	
	/* ������������ */
    /*pBu61580RtDev->RtTaskID = taskSpawn("tBu61580RtRecv", 60, 0, 8192, (FUNCPTR)tBu61580RtRecv, (int)pBu61580RtDev,0,0,0,0,0,0,0,0,0);*/
	return 0;
}

/********************************************************************
��������:   WriteRtMesg

��������:   ����RT���ͻ���

��������        ����                ����/���           ����
bufData         unsigned short *    input               ����ָ��
count           unsigned short      input               �������ݳ���
sa              unsigned short      input               RT�ӵ�ַ

����ֵ  :   ��

����˵��:   ����RT���ͻ��棬�ȴ�BC�ϴ���Ϣָ��

�޸ļ�¼:   2016-10-18  ������  ����
********************************************************************/
int WriteRtMesg(unsigned short *bufData, unsigned short count, unsigned short sa)
{
    unsigned short ush;
	int i = 0;
	int j = 0;
	
	/* ���·��ͻ��� */
	ush = READ_MEM_USH(pBu61580RtDev->bu61580MemBase, (0x0160 + sa));
	DEBUG_PRINTF("WriteRtMesg: transmit addr = %x, base =%x\n", ush, pBu61580RtDev->bu61580MemBase,0,0,0,0);
    if((count < 33) && (sa > 0) && (sa < 31))
    {
        for(i = 0; i < count; i++)
		{
			WRITE_MEM_USH(pBu61580RtDev->bu61580MemBase, (ush + i), bufData[i]);
			/*logMsg("WriteRtMesg: addr = %x, data =%x, buf=%x\n", (ush + i), READ_MEM_USH(pBu61580RtDev->bu61580MemBase, (ush + i)),bufData[i],0,0,0);*/
		}
    }

	return 0;
}

/********************************************************************
��������:   ReadRtMesg

��������:   ����RT��Ϣ

��������        ����                ����/���           ����
BUFFMODE        unsigned short      input               ��ģʽ
buffer          unsigned short *    input               ����ָ��

����ֵ  :   ��

����˵��:   ����RT��Ϣ���������жϵ��ã�Ҳ����������ѯ����

�޸ļ�¼:   2016-10-18  ������  ����
********************************************************************/
int _ReadRtMesg(MsgType_t *Message)
{
    unsigned short StackPtr,DataPtr;
	int i = 0;
	
	StackPtr = READ_MEM_USH(pBu61580RtDev->bu61580RegBase, COMMAND_STACK_POINTER_REG);
	DEBUG_PRINTF("_ReadRtMesg: StackPtr = %x\n", StackPtr, 0,0,0,0,0);
    Message->CmdWord1 = READ_MEM_USH(pBu61580RtDev->bu61580MemBase, (StackPtr+3));
    DEBUG_PRINTF("_ReadRtMesg: CmdWord1 = %x Base:%x \n", Message->CmdWord1, pBu61580RtDev->bu61580MemBase + ((StackPtr+3)<<2),0,0,0,0);
	
    if((((Message->CmdWord1)>>5) & 0x001f) == 0x0000)		/*ģʽ�룬�����ж�(4c10)*/
	{
        return 0;
    }
    
    if(((Message->CmdWord1) & 0x0400) == 0x0400)			/*�����жϣ������ֱ�־Ϊ���ͣ���ʸ����*/
    {
        return 0;
    }
    
    Message->BlockStatus=READ_MEM_USH(pBu61580RtDev->bu61580MemBase, StackPtr);
	DEBUG_PRINTF("ReadRtMesg: Message->BlockStatus = %x\n", Message->BlockStatus, 0,0,0,0,0);

	Message->TimeTag=READ_MEM_USH(pBu61580RtDev->bu61580MemBase, (StackPtr+1));
	DEBUG_PRINTF("ReadRtMesg: Message->TimeTag = %x\n", Message->TimeTag, 0,0,0,0,0);
	
	DataPtr = READ_MEM_USH(pBu61580RtDev->bu61580MemBase, (StackPtr+2));
	DEBUG_PRINTF("ReadRtMesg: DataPtr = %x\n", DataPtr, 0,0,0,0,0);	
	
	Message->DataLength = Message->CmdWord1 & 0x001F;
	if(Message->DataLength == 0)
	{
		Message->DataLength = 32;
	}

	for(i = 0; i < Message->DataLength; i++)
	{
		Message->Data[i] = READ_MEM_USH(pBu61580RtDev->bu61580MemBase, (DataPtr + i));	/*������λ��*/
		DEBUG_PRINTF("ReadRtMesg: addr = %x, Message->DataAddr = %x\n", (DataPtr + i), Message->Data[i],0,0,0,0);
	}
	
	if(rngFreeBytes(pBu61580RtDev->stkRngID)<6 || rngFreeBytes(pBu61580RtDev->dataRngID)<(Message->DataLength*2))
	{
		rngFlush(pBu61580RtDev->stkRngID);
		rngFlush(pBu61580RtDev->dataRngID);
		logMsg("\nFlush 1553 Buffer!\n",0,0,0,0,0,0);
	}
	RtPutBuf(pBu61580RtDev->stkRngID, &Message->BlockStatus, 1);
	RtPutBuf(pBu61580RtDev->stkRngID, &Message->TimeTag, 1);
	RtPutBuf(pBu61580RtDev->stkRngID, &Message->CmdWord1, 1);
    
	for(i = 0; i < Message->DataLength; i++)
	{
		RtPutBuf(pBu61580RtDev->dataRngID, &Message->Data[i], 1);
	}
    
  return 0;
}

/********************************************************************
��������:   Hi6130Int

��������:   RT�����жϷ������

��������        ����                ����/���           ����
��

����ֵ  :   ��

����˵��:   ���ж��д������ʱ�����û�յ����ݣ������������FIFO�Ĳ�����
            �˴�����Ҫ�жϷ���ֵ��������ѯģʽ��Buffer��ڲ�����

�޸ļ�¼:   2016-10-18  ������  ����
********************************************************************/
static void Bu61580RtInterrupt(void)
{
    unsigned short ush = 0;
    DEBUG_PRINTF("Entet Irq!\n", 0,0,0,0,0,0);
	DEBUG_PRINTF("CONF_REG_1(at InitRtMode) = 0x%x\n", READ_REGISTER_USH(pBu61580RtDev->bu61580RegBase, CONF_REG_1),0,0,0,0,0);
	ush = READ_REGISTER_USH(pBu61580RtDev->bu61580RegBase, INT_STATE_REG);
	DEBUG_PRINTF("Bu61580RtInterrupt: ush = %x\n", ush,0,0,0,0,0);
    
    if((ush & 0x0001) == 0x0001)				/*�������ж�*/
    {
        DEBUG_PRINTF("InitRtMode: recv table addr = 0x%x\n", READ_MEM_USH(pBu61580RtDev->bu61580MemBase, (0x140 + 1)), 0,0,0,0,0);
        _ReadRtMesg(pMsg);
    }
	
	DEBUG_PRINTF("Leave Irq!\n", 0,0,0,0,0,0);
}

void tBu61580RtRecv(BU61580RT_DEV *pBu61580RtDev)
{
    for (;;)
    {
        /* �����ȴ��жϷ�����򴥷��ź��� */
        if (semTake(pBu61580RtDev->BuSemID, WAIT_FOREVER) == ERROR)
        {
            return ;  /* �ź�����ɾ��ʱ ���򲻻������ѭ�� */
        }
		_ReadRtMesg(pMsg);
    }
}

int ReadRtMesg(MsgType_t *Message)
{
	int ret = 0;
	int i = 0;
	unsigned short buffer[32];
	
	memset(buffer, 0, 64);
	
	ret = RtGetBuf(pBu61580RtDev->stkRngID, buffer, 3);
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
	
	ret = RtGetBuf(pBu61580RtDev->dataRngID, buffer, Message->DataLength);
	if(ret != Message->DataLength)
	{
		return -1;
	}
	for(i = 0; i < Message->DataLength; i++)
	{
		Message->Data[i] = buffer[i];
	}
	/*printf("int = %d, read = %d \n", g_count,g_count1);*/
	return 0;
}

int RtPutBuf(FAST RING_ID rngId,unsigned short *buffer, int nbytes)
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

int RtGetBuf(FAST RING_ID rngId, unsigned short *buffer, int nbytes)
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

STATUS Bu61580RtCreate(int index, int channel)
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
    /*ul &= 0xFFFFFFF0;*/
    logMsg("PCI Addr : 0x%x\n", ul,0,0,0,0,0);

#if 0
    /* ӳ���ڴ��ַ ����&PCI_DEV_MMU_MSK ��Ϊ����̫С ʹ�ö��� */
    ret = sysMmuMapAdd((void *)ul, 4096, VM_STATE_MASK_FOR_ALL, VM_STATE_FOR_PCI);
    if (ret == -1)
    {
        printf("sysMmuMapAdd faild!ul = 0x%08X\n", ul);
        return ret;
    }
#endif
	
    /* ���� */
    pciConfigOutWord (BusNo, DeviceNo, FuncNo, PCI_CFG_COMMAND, PCI_CMD_IO_ENABLE | PCI_CMD_MEM_ENABLE | PCI_CMD_MASTER_ENABLE);

    /* Get Interrupt line on the Client PCI card */
    pciConfigInByte(BusNo, DeviceNo, FuncNo, PCI_CFG_DEV_INT_LINE, &uch);
	
    /* ��ʼ���ṹ�� */
    pBu61580RtDev = (BU61580RT_DEV *)malloc(sizeof(BU61580RT_DEV));
    bzero((char *)pBu61580RtDev, sizeof(BU61580RT_DEV));
    pBu61580RtDev->channel = channel;
    pBu61580RtDev->bu61580RegBase = ul;
    pBu61580RtDev->bu61580MemBase = ul + 0x4000;
    pBu61580RtDev->BuIrq = uch;

	DEBUG_PRINTF("Bu61580RtCreate: regAddr = %x, memAddr = %x, Irq = %x\n", pBu61580RtDev->bu61580RegBase,pBu61580RtDev->bu61580MemBase,uch,0,0,0);
    logMsg("Bu61580RtCreate: regAddr = 0x%x, memAddr = 0x%x, Irq = %x\n", pBu61580RtDev->bu61580RegBase,pBu61580RtDev->bu61580MemBase,uch,0,0,0);

    WRITE_REGISTER_USH(pBu61580RtDev->bu61580RegBase, START_REG, 0x0001);    /* ��λоƬ */

    pciIntConnect(INUM_TO_IVEC(INT_NUM_GET(pBu61580RtDev->BuIrq)), Bu61580RtInterrupt, (int)pBu61580RtDev);

    sysIntEnablePIC(pBu61580RtDev->BuIrq);
	
    return ret;
}

int CloseRtMode(void)
{
	int ret = 0;
	ret = pciIntDisconnect2(INUM_TO_IVEC(INT_NUM_GET(pBu61580RtDev->BuIrq)), Bu61580RtInterrupt, (int)pBu61580RtDev);
	if(ret != OK)
	{
		return ret;
	}
	ret = semDelete(pBu61580RtDev->BuSemID);
	if(ret != OK)
	{
		return ret;
	}
	ret = taskDelete(pBu61580RtDev->RtTaskID);
	if(ret != OK)
	{
		return ret;
	}
	
	rngDelete(pBu61580RtDev->stkRngID);
	
	rngDelete(pBu61580RtDev->dataRngID);
	
	free(pMsg);
	return 0;
} 
