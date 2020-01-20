/*********************************************************************
�ļ�����:   1553BLib.h

�ļ�����:   DSP 6130оƬ���� ͷ�ļ�

�ļ�˵��:   Դ�ļ�

��ǰ�汾:   V1.0

�޸ļ�¼:   2016-08-22  V1.0    ������  ����
***********************************************************************/
#ifndef _1553BBCLIB_H_
#define _1553BBCLIB_H_

#include "1553bReg.h"

typedef struct bu61580bc_dev
{
    DEV_HDR        pDevHdr;
    unsigned char  channel;
    unsigned int   bu61580RegBase;
    unsigned int   bu61580MemBase;
    SEM_ID         BuSemID;
    int            BusMode;
    int            BuIrq;
	int            BcTaskID;
	unsigned short currStk;
	unsigned short lastStk;
	RING_ID 	   stkRngID;
	RING_ID 	   dataRngID;
}BU61580BC_DEV;

#define INVALID_DATA 0xFFEF

#define BLOCK_ERROR 0x1000

STATUS Bu61580BcCreate(int index, int channel);
int InitBcMode(void);
int ReadBcMesg(MsgType_t *Message);
int WriteBcMesg(MsgType_t *Message);
int CloseBcMode(void);
#endif
