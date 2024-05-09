#pragma once
#include "stdafx.h"
#include "pico/stdlib.h"
#include "HopStepZ/msxdef.h"
#include "def_gpio.h"

namespace mgspico
{

// void t_wait1us();
// void t_wait700ns();
// void t_wait100ns();

bool t_WriteMem(const z80memaddr_t addr, const uint8_t b);
uint8_t t_ReadMem(const z80memaddr_t addr);
bool t_OutPort(const z80ioaddr_t addr, const uint8_t b);
bool t_InPort(uint8_t *pB, const z80ioaddr_t addr);

void t_OutOPLL(const uint16_t addr, const uint16_t data);
void t_OutPSG(const uint16_t addr, const uint16_t data);
void t_OutSCC(const z80memaddr_t addr, const uint16_t data);


void t_MuteOPLL();
void t_MutePSG();
void t_MuteSCC();

}; // namespace mgspico