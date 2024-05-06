#pragma once
#include "../stdafx.h"
#include <vector>

class CMsxMemSlotSystem;
class CMsxIoSystem;
class CZ80MsxDos;
class CRam64k;
class CMsxMusic;
class CMsxDummyMain;
//class CScc;
class IPhysicalSlotDevice;
class CZ80Regs;

class CHopStepZ
{
private:
	CMsxMemSlotSystem	*m_pSlot;
	CMsxIoSystem 		*m_pIo;
	CZ80MsxDos			*m_pCpu;
	CRam64k				*m_pRam64;
	CMsxMusic			*m_pFm;
//	CScc				*m_pScc;
	IPhysicalSlotDevice	*m_pPhy;
	CMsxDummyMain		*m_pDumyMain;

public:
	CHopStepZ();
	virtual ~CHopStepZ();
public:
	void Setup(const bool bForceOpll, const bool bKINROU5);
	void GetSubSystems(CMsxMemSlotSystem **pSlot, CZ80MsxDos **pCpu);
	void Run(const z80memaddr_t startAddr, const z80memaddr_t stackAddr, bool *pStop);
public:
	void RunStage1(const z80memaddr_t startAddr, const z80memaddr_t stackAddr);
	bool RunStage2();

public:
	uint8_t ReadMemory(const z80memaddr_t addr) const;
	uint16_t ReadMemoryWord(const z80memaddr_t addr) const;
	void WriteMemory(const z80memaddr_t addr, const uint8_t b);
	void WriteMemory(const z80memaddr_t addr, const uint8_t *pSrc, const uint32_t bsize);

public:
	// for debug
	const CZ80Regs &GetCurrentRegs() const;
};

