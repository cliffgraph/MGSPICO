#pragma once
#include "msxdef.h"
class RmmChipMuse;

class CMsxMusic : public IZ80MemoryDevice, public IZ80IoDevice
{
private:
	RmmChipMuse *m_pOpll;
	RmmChipMuse *m_pPsg;

public:
	CMsxMusic();
	virtual ~CMsxMusic();

public:
/*IZ80MemoryDevice*/
	void SetSlotToPage(const msxpageno_t pageNo, const msxslotno_t slotNo);
	msxslotno_t GetSlotByPage(const msxpageno_t pageNo);
	bool WriteMem(const z80memaddr_t addr, const uint8_t b);
	uint8_t ReadMem(const z80memaddr_t addr) const;
/*IZ80IoDevice*/
	bool OutPort(const z80ioaddr_t addr, const uint8_t b);
	bool InPort(uint8_t *pB, const z80ioaddr_t addr);
};
