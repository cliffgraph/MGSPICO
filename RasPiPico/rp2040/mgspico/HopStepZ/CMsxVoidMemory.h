#pragma once
#include "msxdef.h"
#include "msxdef.h"

class CMsxVoidMemory : public IZ80MemoryDevice
{
public:
	CMsxVoidMemory();
	virtual ~CMsxVoidMemory();

public:
/*IZ80MemoryDevice*/
	void SetSlotToPage(const msxpageno_t pageNo, const msxslotno_t slotNo);
	msxslotno_t GetSlotByPage(const msxpageno_t pageNo);
	bool WriteMem(const z80memaddr_t addr, const uint8_t b);
	uint8_t ReadMem(const z80memaddr_t addr) const;
};


