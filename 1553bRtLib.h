/*********************************************************************
文件名称:   1553BLib.h

文件功能:   DSP 6130芯片驱动 头文件

文件说明:   源文件

当前版本:   V1.0

修改记录:   2016-08-22  V1.0    李世杰  创建
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
