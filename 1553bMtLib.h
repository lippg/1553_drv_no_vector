/*********************************************************************
�ļ�����:   1553BLib.h

�ļ�����:   DSP 6130оƬ���� ͷ�ļ�

�ļ�˵��:   Դ�ļ�

��ǰ�汾:   V1.0

�޸ļ�¼:   2016-08-22  V1.0    ������  ����
***********************************************************************/
#ifndef _1553BMTLIB_H_
#define _1553BMTLIB_H_

#include "1553bReg.h"

typedef struct bu61580mt_dev
{
    DEV_HDR        pDevHdr;
    unsigned char  channel;
    unsigned int   bu61580RegBase;
    unsigned int   bu61580MemBase;
    SEM_ID         BuSemID;
	RING_ID 	   stkRngID;
	RING_ID 	   dataRngID;
    int            BusMode;
    int            BuIrq;
	int            MtTaskID;
	unsigned short currStk;
	unsigned short lastStk;
	int 		   mtOpenFlag;
} BU61580MT_DEV;

#define MT_START     0x0002

STATUS Bu61580MtCreate(int index, int channel);
int InitMtMode(void);
int ReadMtMesg(MsgType_t *Message);
int CloseMtMode(void);
#endif
