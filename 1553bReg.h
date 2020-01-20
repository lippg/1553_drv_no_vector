#ifndef _1553BREG_H_
#define _1553BREG_H_

#include "semLib.h"
#include "taskLib.h"
#include "rngLib.h"

#define VM_STATE_MASK_FOR_ALL   VM_STATE_MASK_VALID | VM_STATE_MASK_WRITABLE | VM_STATE_MASK_CACHEABLE
#define VM_STATE_FOR_PCI        VM_STATE_VALID | VM_STATE_WRITABLE | VM_STATE_CACHEABLE_NOT
#define VM_PAGE_SIZE            8192
#define PCI_DEV_MMU_MSK         (~(VM_PAGE_SIZE - 1))

/* PCI总线映射模式为MEM模式或IO模式，ISA为IO模式，在这里定义读写方式方便扩展 */
#define FOR_IO_SPACE            1
#define FOR_MEM_SPASE           2
#define MAPPING_TYPE            FOR_MEM_SPASE    /* 现在驱动为ISA总线 */

/* 芯片寄存器读写是16位 */
#if 0
#define WRITE_REGISTER_USH(baseaddress, offset, value)    (*(volatile unsigned short *)(baseaddress + (offset << 2)) = value)
#define WRITE_MEM_USH(baseaddress, offset, value)    (*(volatile unsigned short *)(baseaddress + (offset<<2)) = value)
#endif
#define READ_REGISTER_USH(baseaddress, offset)            (*(volatile unsigned short *)(baseaddress + (offset << 2)))

void WRITE_REGISTER_USH(unsigned int baseaddress, int offset, unsigned short value);
void WRITE_MEM_USH(unsigned int baseaddress, int offset, unsigned short value);
/* 1553b芯片使用的数据内存读写函数 */

#define READ_MEM_USH(baseaddress, offset)            (*(volatile unsigned short *)(baseaddress + (offset<<2)))

#ifdef _WRS_CONFIG_SMP
IMPORT UINT8 *sysInumTbl;
#undef INT_NUM_GET
#define INT_NUM_GET(irq) sysInumTbl[irq]
#else
#define INT_NUM_IRQ0        0x20
#undef INT_NUM_GET
#define INT_NUM_GET(irq)    (INT_NUM_IRQ0 + irq)
#endif

void PrintDebug(void);
void InitCount(unsigned long count);

IMPORT STATUS sysMmuMapAdd
(
void * address,           /* memory region base address */
UINT   length,            /* memory region length in bytes*/
UINT   initialStateMask,  /* PHYS_MEM_DESC state mask */
UINT   initialState       /* PHYS_MEM_DESC state */
);

typedef struct MsgStruct
{
    unsigned short Type;
    unsigned short BlockStatus;
    unsigned short GapTime;
    unsigned short TimeTag;
    unsigned short CmdWord1;
    unsigned short CmdWord2;    /* RT TO RT模式会用 */
    unsigned short Status1;
    unsigned short Status2;
    unsigned short LoopBack1;
    unsigned short LoopBack2;
    unsigned short ControlWord;
    unsigned short WordCount;
    unsigned short Data[32];
    unsigned short DataLength;
    unsigned char RtAddress;
    unsigned char SubAddress;
    int WaitForever;
} MsgType_t;

#undef DEBUG
#ifdef DEBUG
#define    DEBUG_PRINTF(fmt, a1, a2, a3, a4, a5, a6)    logMsg (fmt, (int)(a1), (int)(a2), (int)(a3), (int)(a4),    \
                        (int)(a5), (int)(a6));
#else
#define    DEBUG_PRINTF(fmt, a1, a2, a3, a4, a5, a6)
#endif

#define BU61580_VENDOR_ID    		 0x9058
#define BU61580_DEV_ID   			 0x1553
#define ILLEGAL_CMD_DETECT 1
#define STACK_POINTER_A        0x0100

#define T_R                0x0400 

#define INT_MASK_REG 				 0x00
#define CONF_REG_1   				 0x01
#define CONF_REG_2   				 0x02

#define START_REG                    0x03
#define COMMAND_STACK_POINTER_REG    0x03

#define CONTROL_WORD_REG		     0x04
#define TIME_TAG_REG			     0x05
#define INT_STATE_REG			     0x06
#define CONF_REG_3   				 0x07
#define CONF_REG_4   				 0x08
#define CONF_REG_5   			     0x09
#define DATA_STACK_ADDR_REG          0x0A
#define BC_FRAME_TIME_REMAIN_REG     0x0B
#define BC_TIME_REMAIN_NEXT_REG      0x0C

#define BC_FRAME_TIME_REG		     0x0D
#define RT_LAST_COMMAND_REG		     0x0D
#define MT_TRIGGER_WORD_REG		     0x0D

#define RT_STATUS_WORD_REG           0x0E
#define RT_BIT_WORD_REG              0x0F

#define CURRENT_SA SA7
#define SA1 1
#define SA2 2
#define SA3 3
#define SA4 4
#define SA5 5
#define SA6 6
#define SA7 7
#define SA8 8
#define SA9 9
#define SA10 10
#define SA11 11
#define SA12 12
#define SA13 13
#define SA14 14
#define SA15 15
#define SA16 16
#define SA17 17
#define SA18 18
#define SA19 19
#define SA20 20
#define SA21 21
#define SA22 22
#define SA23 23
#define SA24 24
#define SA25 25
#define SA26 26
#define SA27 27
#define SA28 28
#define SA29 29
#define SA30 30

#endif
