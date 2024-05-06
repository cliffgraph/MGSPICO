#include "../stdafx.h"
#include "msxdef.h"
#include "CMsxDummyMain.h"
#include <memory.h>

CMsxDummyMain::CMsxDummyMain()
{
	// do nothing
	return;
}

CMsxDummyMain::~CMsxDummyMain()
{
	// do nothing
	return;
}

void CMsxDummyMain::SetSlotToPage(
	const msxpageno_t pageNo, const msxslotno_t slotNo)
{
	// do nothing;
	return;
}

msxslotno_t CMsxDummyMain::GetSlotByPage(const msxpageno_t pageNo)
{
	// do nothing
	return 0;
}

bool CMsxDummyMain::WriteMem(const z80memaddr_t addr, const uint8_t b)
{
	// do nothing
	return true;
}

uint8_t CMsxDummyMain::ReadMem(const z80memaddr_t addr) const
{
	if( addr == 0x02d )	
		return 0x02;	// 0x02 = MSX2+
	return 0xff;
}
