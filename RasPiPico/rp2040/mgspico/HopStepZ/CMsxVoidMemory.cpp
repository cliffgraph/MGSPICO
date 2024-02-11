#include "stdafx.h"
#include "msxdef.h"
#include "CMsxVoidMemory.h"
#include <memory.h>

CMsxVoidMemory::CMsxVoidMemory()
{
	// do nothing
	return;
}

CMsxVoidMemory::~CMsxVoidMemory()
{
	// do nothing
	return;
}

void CMsxVoidMemory::SetSlotToPage(
	const msxpageno_t pageNo, const msxslotno_t slotNo)
{
	// do nothing;
	return;
}

msxslotno_t CMsxVoidMemory::GetSlotByPage(const msxpageno_t pageNo)
{
	// do nothing
	return 0;
}

bool CMsxVoidMemory::WriteMem(const z80memaddr_t addr, const uint8_t b)
{
	// do nothing
	return true;
}

uint8_t CMsxVoidMemory::ReadMem(const z80memaddr_t addr) const
{
	return 0xff;
}
