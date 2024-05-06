#include "../stdafx.h"
#include "pico/stdlib.h"
#include "msxdef.h"
#include <vector>
#include <assert.h>
#include "CMsxMemSlotSystem.h"

CMsxMemSlotSystem::CMsxMemSlotSystem()
{
	// ひとまず全基本スロットにVoidMemをセットする
	for( msxslotno_t slotNo = 0; slotNo < BASESLOTNO_NUM; ++slotNo){
		m_BaseSlotObjs[slotNo] = &m_VoidMem;
	}
	// Z80メモリ空間の全ページはスロット#3を割り付けておく
	for( msxpageno_t pageNo = 0; pageNo < MEMPAGENO_NUM; ++pageNo)
		m_SlotNoToPage[pageNo] = 3;
	return;
}

CMsxMemSlotSystem::~CMsxMemSlotSystem()
{
	// do nothing;
	return;
}

/* 指定の基本スロットにメモリデバイスをセットする
*/
void CMsxMemSlotSystem::JoinObject(
	const msxslotno_t slotNo, IZ80MemoryDevice *pObj)
{
	const msxslotno_t baseNo = slotNo & 0x03;
	m_BaseSlotObjs[baseNo] = pObj;
	return;
}

/** 指定ページに指定スロット(基本/拡張)を割りつける
 * （指定ページを指定スロットに切り替える） 
 */
void CMsxMemSlotSystem::SetSlotToPage(
	const msxpageno_t pageNo, const msxslotno_t slotNo)
{
	const msxslotno_t baseNo = slotNo & 0x03;
	const msxslotno_t extNo = (slotNo>>2) & 0x03;
	m_SlotNoToPage[pageNo] = baseNo;
	auto *pObj = m_BaseSlotObjs[baseNo];
	pObj->SetSlotToPage(pageNo, extNo);
	return;
}

/** 指定ページに割り付けられているスロット(基本/拡張)の番号を返す
 */
msxslotno_t CMsxMemSlotSystem::GetSlotByPage(const msxpageno_t pageNo)
{
	const msxslotno_t baseNo = m_SlotNoToPage[pageNo];
	auto *pObj = m_BaseSlotObjs[baseNo];
	const msxslotno_t slotNo = (pObj->GetSlotByPage(pageNo) << 2) | baseNo;
	return slotNo;
}

/** 指定メモリにバイナリブロックを書き込む
 */
void CMsxMemSlotSystem::BinaryTo(
	const z80memaddr_t dest, const uint8_t *pSrc, const uint32_t bsize)
{
	z80memaddr_t p = dest;
	for(uint32_t t = 0; t < bsize; ++t){
		writeByte(p++, *(pSrc++));
	}
	return;
}

/** 指定メモリに１バイト書き込む
 */
void __time_critical_func(CMsxMemSlotSystem::writeByte)(const z80memaddr_t addr, const uint8_t b)
{
	auto pageNo = static_cast<msxpageno_t>(addr / Z80_PAGE_SIZE);
	auto slotNo = m_SlotNoToPage[pageNo];
	auto *pBase = m_BaseSlotObjs[slotNo&0x3];
	pBase->WriteMem(addr, b);
	return;
}

/** 指定メモリから１バイト読み込む
 */
uint8_t __time_critical_func(CMsxMemSlotSystem::readByte)(const z80memaddr_t addr) const
{
	auto pageNo = addr / Z80_PAGE_SIZE;
	auto slotNo = m_SlotNoToPage[pageNo];
	auto *pBase = m_BaseSlotObjs[slotNo];
	const uint8_t b = pBase->ReadMem(addr);
	return b;
}

void __time_critical_func(CMsxMemSlotSystem::Write)(const z80memaddr_t addr, const uint8_t b)
{
	writeByte(addr, b);
	return;
}

uint8_t __time_critical_func(CMsxMemSlotSystem::Read)(const z80memaddr_t addr) const
{
	const uint8_t b = readByte(addr);
	return b;
}

int8_t CMsxMemSlotSystem::ReadInt8(const z80memaddr_t addr) const
{
	return static_cast<int8_t>(readByte(addr));
}

uint16_t CMsxMemSlotSystem::ReadWord(const z80memaddr_t addr) const
{
	uint16_t v;
	v = Read(addr + 0);
	v |= Read(addr + 1) << 8;
	return v;
}

void __time_critical_func(CMsxMemSlotSystem::WriteWord)(const z80memaddr_t addr, uint16_t v)
{
	Write(addr + 0, (v>>0) & 0xff);
	Write(addr + 1, (v>>8) & 0xff);
	return;
}

void CMsxMemSlotSystem::ReadString(std::string *pStr, z80memaddr_t srcAddr)
{
	for(;;){
		uint8_t v = readByte(srcAddr++);
		if( v == '\0' )
			break;
		*pStr += static_cast<char>(v);
	}
	return;
}
bool __time_critical_func(CMsxMemSlotSystem::OutPort)(const z80ioaddr_t addr, const uint8_t b)
{
	if( addr == 0xa8 ) {
		// A8Hへのアクセスに従い、基本スロットを切り替える
		m_SlotNoToPage[0] = static_cast<msxslotno_t>((b >> 0) & 0x3);
		m_SlotNoToPage[1] = static_cast<msxslotno_t>((b >> 2) & 0x3);
		m_SlotNoToPage[2] = static_cast<msxslotno_t>((b >> 4) & 0x3);
		m_SlotNoToPage[3] = static_cast<msxslotno_t>((b >> 6) & 0x3);
		return true;
	}
	return false;
}
bool __time_critical_func(CMsxMemSlotSystem::InPort)(uint8_t *pB, const z80ioaddr_t addr)
{
	if( addr == 0xa8 ) {
		*pB =
			((m_SlotNoToPage[0] & 0x03) << 0) |
			((m_SlotNoToPage[1] & 0x03) << 2) | 
			((m_SlotNoToPage[2] & 0x03) << 4) | 
			((m_SlotNoToPage[3] & 0x03) << 6); 
		return true;
	}
	return false;
}
