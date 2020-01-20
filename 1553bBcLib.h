/*********************************************************************
文件名称:   1553BLib.h

文件功能:   DSP 6130芯片驱动 头文件

文件说明:   源文件

当前版本:   V1.0

修改记录:   2016-08-22  V1.0    李世杰  创建
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
