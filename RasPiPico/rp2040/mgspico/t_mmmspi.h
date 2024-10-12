#pragma once
#include "stdafx.h"
#include "pico/stdlib.h"
//#include "HopStepZ/msxdef.h"
#include "def_gpio.h"

#ifdef MGSPICO_3RD
namespace mmmspi
{
enum class CMD : uint32_t
{
	NONE			= 0x00,
	VSYNC			= 0x02,
	PSG				= 0x11,
	OPLL			= 0x12,
	SCC				= 0x13,
	IKAOPLL_MOVOL	= 0x18,
	IKAOPLL_ROVOL	= 0x19,
	SEL_SCC_MODULE	= 0x1A,
};

bool Init();
void ClearBUff();
void PushBuff(const CMD cmd, const uint32_t addr, const uint32_t data);
void Present();
}; // namespace mmmspi
#endif // MGSPICO_3RD