/*********************************************************************
�ļ�����:   1553BLib.h

�ļ�����:   DSP 6130оƬ���� ͷ�ļ�

�ļ�˵��:   Դ�ļ�

��ǰ�汾:   V1.0

�޸ļ�¼:   2016-08-22  V1.0    ������  ����
***********************************************************************/
#ifndef _1553BRTLIB_H_
#define _1553BRTLIB_H_

#include "1553bReg.h"
#include "iosLib.h"

typedef struct bu61580rt_dev
{
    DEV_HDR        pDevHdr;
    unsigned char  channel;
    unsigned int   bu61580RegBase;
    unsigned int   bu61580MemBase;
    SEM_ID         BuSemID;
    int            BusMode;
    int            BuIrq;
	int            RtTaskID;
	unsigned short currStk;
	unsigned short lastStk;
	RING_ID 	   stkRngID;
	RING_ID 	   dataRngID;
	int 		   init_subaddr;
	int 		   subaddr;
}BU61580RT_DEV;

#define INVALID_DATA 0xFFEF

#define BLOCK_ERROR 0x1000

STATUS Bu61580RtCreate(int index, int channel);
int InitRtMode(unsigned short sa);
int ReadRtMesg(MsgType_t *Message);
int WriteRtMesg(unsigned short *bufData, unsigned short count, unsigned short sa);
int CloseRtMode(void);
#endif
