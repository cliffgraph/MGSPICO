#pragma once
#include "stdafx.h"
#include "pico/stdlib.h"
//#include "HopStepZ/msxdef.h"
#include "def_gpio.h"

#ifdef MGSPICO_3RD
namespace mmmspi
{
bool Init();
void ClearBUff();
void PushBuff(const uint32_t cmd, const uint32_t addr, const uint32_t data);
void Present();
}; // namespace mmmspi
#endif // MGSPICO_3RD