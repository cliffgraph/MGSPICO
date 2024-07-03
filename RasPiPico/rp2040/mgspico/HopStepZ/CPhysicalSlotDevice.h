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

	// for Carnivore2
	z80ioaddr_t m_portCarnivore2;
#ifdef MGS_MUSE_MACHINA
	static const z80memaddr_t ADDR_START = 0x9800;
	static const z80memaddr_t ADDR_END = 0x98FF;
#else
	static const z80memaddr_t ADDR_START = 0x9800;
	static const z80memaddr_t ADDR_END = 0x987F;
#endif
	static const z80memaddr_t MEM_SIZE = (ADDR_END-ADDR_START+1);
	uint8_t	m_M9000;
	uint8_t	m_M9800[MEM_SIZE];

private:
	bool enableCARNIVORE2();
	bool enableYAMANOOTO();
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


