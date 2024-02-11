#pragma once
#include <stdint.h>

typedef uint8_t		z80ioaddr_t;
typedef uint16_t	z80memaddr_t;
typedef uint8_t		dosfuncno_t;
static const int	 Z80_PAGE_SIZE = 16*1024;
static const int 	Z80_MEMORY_SIZE = 64*1024;

typedef uint8_t		msxslotno_t;			// 0000yyxxb	yy=拡張スロットNo、xx基本スロット番号
typedef int			msxpageno_t;			// ページ番号 ０～
static const int 	BASESLOTNO_NUM = 4;		// 基本スロット数
static const int 	EXTSLOTNO_NUM = 4;		// 基本スロット内の最大拡張スロット数
static const int 	MEMPAGENO_NUM = 4;		// ページ数

static const z80memaddr_t PAGE0_END = 0x3FFF;
static const z80memaddr_t PAGE1_END = 0x8FFF;
static const z80memaddr_t PAGE2_END = 0xBFFF;
static const z80memaddr_t PAGE3_END = 0xFFFF;

//----------------------------------------------------------------------------
class IZ80MemoryDevice
{
public:
	virtual ~IZ80MemoryDevice(){return;}
	virtual void SetSlotToPage(const msxpageno_t pageNo, const msxslotno_t slotNo) = 0;
	virtual msxslotno_t GetSlotByPage(const msxpageno_t pageNo) = 0;
public:
	virtual bool WriteMem(const z80memaddr_t addr, const uint8_t b) = 0;
	virtual uint8_t ReadMem(const z80memaddr_t addr) const = 0;
};

//----------------------------------------------------------------------------
class IZ80IoDevice
{
public:
	virtual ~IZ80IoDevice(){return;}
public:
	virtual bool OutPort(const z80ioaddr_t addr, const uint8_t b) = 0;
	virtual bool InPort(uint8_t *pB, const z80ioaddr_t addr) = 0;
};

