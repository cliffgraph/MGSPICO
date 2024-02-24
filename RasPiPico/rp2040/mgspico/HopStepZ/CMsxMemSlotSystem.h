#pragma once
#include "msxdef.h"
#include "msxdef.h"
#include "CMsxVoidMemory.h"
#include <vector>
#include <string>

class CMsxMemSlotSystem : public IZ80IoDevice
{
private:
	//
	CMsxVoidMemory m_VoidMem;
	// 各基本スロットにセットされたメモリ装置オブジェクトへのポインタを保持する
	IZ80MemoryDevice* m_BaseSlotObjs[BASESLOTNO_NUM/*基本スロット*/];
	// 各ページそれぞれの基本スロット番号を保持する
	msxslotno_t m_SlotNoToPage[MEMPAGENO_NUM];

public:
	CMsxMemSlotSystem();
	virtual ~CMsxMemSlotSystem();

public:
	void JoinObject(const msxslotno_t baseSlotNo, IZ80MemoryDevice *pObj);
	void SetSlotToPage(const msxpageno_t pageNo, const msxslotno_t slotNo);
	msxslotno_t GetSlotByPage(const msxpageno_t pageNo);
public:
	void BinaryTo(const z80memaddr_t dest, const uint8_t *pSrc, const uint32_t bsize);

private:
	void writeByte(const z80memaddr_t addr, const uint8_t b);
	uint8_t readByte(const z80memaddr_t addr) const;

public:
	void Write(const z80memaddr_t addr, const uint8_t b);
	uint8_t Read(const z80memaddr_t addr) const;
	int8_t ReadInt8(const z80memaddr_t addr) const;
	void Push16(const uint16_t w);
	void ReadString(std::string *pStr, z80memaddr_t srcAddr);
	uint16_t ReadWord(const z80memaddr_t addr) const;
	void WriteWord(const z80memaddr_t addr, uint16_t v);

/*IZ80IoDevice*/
public:
	bool OutPort(const z80ioaddr_t addr, const uint8_t b);
	bool InPort(uint8_t *pB, const z80ioaddr_t addr);
};
