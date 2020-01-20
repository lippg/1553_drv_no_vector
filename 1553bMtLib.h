/*********************************************************************
文件名称:   1553BLib.h

文件功能:   DSP 6130芯片驱动 头文件

文件说明:   源文件

当前版本:   V1.0

修改记录:   2016-08-22  V1.0    李世杰  创建
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
