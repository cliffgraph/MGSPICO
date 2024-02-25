#pragma once
#include "msxdef.h"
#include "IPhysicalSlotDevice.h"

class CPhysicalSlotDeviceKIN5 : public IPhysicalSlotDevice
{
public:
	CPhysicalSlotDeviceKIN5();
	virtual ~CPhysicalSlotDeviceKIN5();

private:
	uint8_t	m_ExtReg;
	bool	m_bExt;
	void init(uint8_t v);
	bool enableFMPAC();

public:
	void Clear(uint8_t v);

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


