#include "1553bReg.h"

void WRITE_REGISTER_USH(unsigned int baseaddress, int offset, unsigned short value)
{
	unsigned long i = 0;
	unsigned int ui = 0;
	(*(volatile unsigned short *)(baseaddress + (offset << 2)) = value);

	/*while((*(volatile unsigned short *)(0xeffe800c)) != 0x0001);*/
	
	for(i = 0; i < 1000; i ++);
}

void WRITE_MEM_USH(unsigned int baseaddress, int offset, unsigned short value)
{
	unsigned long i = 0;
		unsigned int ui = 0;
	(*(volatile unsigned short *)(baseaddress + (offset << 2)) = value);

	/*while((*(volatile unsigned short *)(0xeffe800c)) != 0x0001);*/

	for(i = 0; i < 1000; i ++);
}
