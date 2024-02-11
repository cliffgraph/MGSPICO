#pragma once
#include "msxdef.h"
#include "msxdef.h"

class CRam64k : public IZ80MemoryDevice, public IZ80IoDevice
{
private:
	static const int NUM_SEGMENTS = 6;				// 4 * 16[KBytes] = 64[KBytes]
	int m_AssignedSegmentToPage[MEMPAGENO_NUM];		// 各ページに割り付けているセグメントの番号０～
	uint8_t *m_pPage[MEMPAGENO_NUM];

public:
	static const int TOTAL_SIZE = (Z80_PAGE_SIZE*NUM_SEGMENTS);
	//uint8_t	m_Memory[TOTAL_SIZE];

public:
	CRam64k();
	explicit CRam64k(uint8_t v);
	virtual ~CRam64k();

private:
	void init(uint8_t v);

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


