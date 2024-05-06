#include "../stdafx.h"
#include "msxdef.h"
#include "CScc.h"
#include <memory.h>
//#include "RmmChipMuse.h"

CScc::CScc()
{
	m_M9000 = 0;
	return;
}
CScc::~CScc()
{
	return;
}

void CScc::SetupHardware()
{
	return;
}

void CScc::SetSlotToPage(
	const msxpageno_t pageNo, const msxslotno_t slotNo)
{
	// do nothing;
	return;
}

msxslotno_t CScc::GetSlotByPage(const msxpageno_t pageNo)
{
	// do nothing
	return 0;
}

bool CScc::WriteMem(const z80memaddr_t addr, const uint8_t b)
{
	bool bRetc = false;
	if( addr == 0x9000 ){
		m_M9000 = b;
		bRetc = true;
	}
	else if(m_M9000 == 0x3f && ADDR_START <= addr && addr <= ADDR_END ){
		m_M9800[addr-ADDR_START] = b;
		bRetc = true;
	}
	return bRetc;
}

uint8_t CScc::ReadMem(const z80memaddr_t addr) const
{
	uint8_t b = 0xff;
	if( addr == 0x9000 ){
		b = m_M9000;
	}
	else if(m_M9000 == 0x3f && ADDR_START <= addr && addr <= ADDR_END ){
		b = m_M9800[addr-ADDR_START];
	}
	return b;
}
