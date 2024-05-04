#pragma once
#include "msxdef.h"
#include "IPhysicalSlotDevice.h"

class CPhysicalSlotDevice : public IPhysicalSlotDevice
{
public:
	CPhysicalSlotDevice();
	virtual ~CPhysicalSlotDevice();

private:
	uint8_t	m_ExtReg;
	bool	m_bExt;

	bool (CPhysicalSlotDevice::*m_pWriteMem)(const z80memaddr_t addr, const uint8_t b);
	uint8_t (CPhysicalSlotDevice::*m_pReadMem)(const z80memaddr_t addr) const;
	bool (CPhysicalSlotDevice::*m_pOutPort)(const z80ioaddr_t addr, const uint8_t b);
	bool (CPhysicalSlotDevice::*m_pInPort)(uint8_t *pB, const z80ioaddr_t addr);

private:
	void init(uint8_t v);
	bool enableFMPAC();

private:
	bool writeMemMGS(const z80memaddr_t addr, const uint8_t b);
	uint8_t readMemMGS(const z80memaddr_t addr) const;
	bool outPortMGS(const z80ioaddr_t addr, const uint8_t b);
	bool inPortMGS(uint8_t *pB, const z80ioaddr_t addr);

public:
	bool Setup();

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


