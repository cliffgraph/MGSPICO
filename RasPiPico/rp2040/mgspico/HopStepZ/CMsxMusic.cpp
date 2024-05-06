#include "../stdafx.h"
#include "msxdef.h"
#include "CMsxMusic.h"
#include <memory.h>
#include "../global.h"
#include "../def_gpio.h"

#include "pico/stdlib.h"	// for gpio_put

CMsxMusic::CMsxMusic()
{
	// do nothing
	return;
}

CMsxMusic::~CMsxMusic()
{
	// do nothing
	return;
}

void CMsxMusic::SetSlotToPage(const msxpageno_t pageNo, const msxslotno_t slotNo)
{
	// do nothing
	return;
}

msxslotno_t CMsxMusic::GetSlotByPage(const msxpageno_t pageNo)
{
	// do nothing
	return 0;
}

bool __time_critical_func(CMsxMusic::WriteMem)(const z80memaddr_t addr, const uint8_t b)
{
	// do nothing
	return true;
}

uint8_t __time_critical_func(CMsxMusic::ReadMem)(const z80memaddr_t addr) const
{
	const static uint8_t sign[] ={ "APRLOPLL" };
	if( 0x4018 <= addr && addr <= 0x401F ) {
		return sign[addr-0x4018];
	}
	return 0xff;
}

bool __time_critical_func(CMsxMusic::OutPort)(const z80ioaddr_t addr, const uint8_t b)
{
	// do nohing
	return false;
}

bool __time_critical_func(CMsxMusic::InPort)(uint8_t *pB, const z80ioaddr_t addr)
{
	// do nohing
	return false;
}
