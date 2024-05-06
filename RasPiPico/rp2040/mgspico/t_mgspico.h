#pragma once
#include "stdafx.h"
#include "pico/stdlib.h"
#include "HopStepZ/msxdef.h"
#include "def_gpio.h"

namespace mgspico
{

// 1us wait.
inline void t_wait1us()
{
	// no good
	// sleep_us(1), busy_wait_us(1)
	// no good
	// { uint32_t t = time_us_32(); while(t == time_us_32()); }

	// 125MHz動作時、下記のコードでbusy_wait_usを使用するより安定した1usのウェイトを生成できた
	// ループ回数は実機で動作させ実測して決定した
	// この方法の情報源：https://forums.raspberrypi.com/viewtopic.php?t=304922
	for(int t = 0; t < 25; t++) {
	    __asm volatile ("nop":);
	}
	return;
}

inline void t_wait700ns()
{
	for(int t = 0; t < 16; t++) {
	    __asm volatile ("nop":);
	}
	return;
}

inline void t_wait100ns()
{
	for(int t = 0; t < 7; t++) {
	    __asm volatile ("nop":);
	}
	return;
}


bool t_WriteMem(const z80memaddr_t addr, const uint8_t b);
uint8_t t_ReadMem(const z80memaddr_t addr);
bool t_OutPort(const z80ioaddr_t addr, const uint8_t b);
bool t_InPort(uint8_t *pB, const z80ioaddr_t addr);

};