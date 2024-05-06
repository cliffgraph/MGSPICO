#include "../stdafx.h"
#include "msxdef.h"
#include "CMsxIoSystem.h"
#include <assert.h>
#include <vector>
#include "pico/stdlib.h"	// for __time_critical_func

CMsxIoSystem::CMsxIoSystem()
{
	m_SystemTimer.ResetBegin();
	m_SystemTimeCount = 0;
	return;
}

CMsxIoSystem::~CMsxIoSystem()
{
	// do nothing
	return;

}
void CMsxIoSystem::JoinObject(IZ80IoDevice *pIoObj)
{
	assert(pIoObj != nullptr);
	m_Objs.push_back(pIoObj);
	return;
}
void __time_critical_func(CMsxIoSystem::Out)(const z80ioaddr_t addr, const uint8_t b)
{
	for( auto &p : m_Objs )
		p->OutPort(addr, b);
	return;
}

uint8_t __time_critical_func(CMsxIoSystem::In)(const z80ioaddr_t addr)
{
	uint8_t b = 0xFF;
	for( auto &p : m_Objs ){
		if( p->InPort(&b, addr) )
			break;
	}
	return b;
}

bool __time_critical_func(CMsxIoSystem::OutPort)(const z80ioaddr_t addr, const uint8_t b)
{
	if (addr == 0xe6) {
		m_SystemTimeCount = 0;
		m_SystemTimer.ResetBegin();
		return true;
	}
	return false;
}

bool __time_critical_func(CMsxIoSystem::InPort)(uint8_t *pB, const z80ioaddr_t addr)
{
	if( addr == 0xe6 ){
		updateSystemTimer();
		m_SystemTimeCount++;
		*pB = static_cast<uint8_t>(m_SystemTimeCount & 0xff);
		return true;
	}
	else if( addr == 0xe7 ){
		updateSystemTimer();
		*pB = static_cast<uint8_t>((m_SystemTimeCount>>8) & 0xff);
		return true;
	}
	return false;
}

void __time_critical_func(CMsxIoSystem::updateSystemTimer)()
{
	uint64_t temp = m_SystemTimer.GetTime();
	if( 4 <= temp ){
		m_SystemTimeCount += static_cast<uint16_t>(temp / 4);		// 4は本来は3.911us
		m_SystemTimer.ResetBegin();
	}
	return;
}
