#include "timerobj.h"

TimerObj::TimerObj()
	: m_TargetCnt(0), m_BeginCnt(0), m_TimeOut(false)
{
	return;
}

TimerObj::~TimerObj()
{
	return;
}

void TimerObj::Start(const uint32_t cnt)
{
	m_BeginCnt = to_ms_since_boot(get_absolute_time());
	m_TargetCnt = cnt;
	m_TimeOut = false;
	return;
}

bool TimerObj::IsTimeout()
{
	if (m_TimeOut)
		return true;
	if (m_BeginCnt==0) 
		return false;
	auto now = to_ms_since_boot(get_absolute_time());
	if (m_TargetCnt <= (now - m_BeginCnt))
		m_TimeOut = true;
	return m_TimeOut;
}

bool TimerObj::IsEffective()
{
	if (m_BeginCnt == 0) 
		return false;
	return true;
}

void TimerObj::Cancel()
{
	m_TimeOut = false;
	m_BeginCnt = 0;
	m_TargetCnt = 0;
	return;
}

// -----------------------------------------------------------------------------
TimerUsObj::TimerUsObj()
	: m_TargetCnt(0), m_BeginCnt(0), m_TimeOut(false)
{
	return;
}

TimerUsObj::~TimerUsObj()
{
	return;
}

void TimerUsObj::Start(const uint64_t cnt)
{
	m_BeginCnt = to_us_since_boot(get_absolute_time());
	m_TargetCnt = cnt;
	m_TimeOut = false;
	return;
}

bool TimerUsObj::IsTimeout()
{
	if (m_TimeOut)
		return true;
	if (m_BeginCnt==0) 
		return false;
	auto now = to_us_since_boot(get_absolute_time());
	if (m_TargetCnt <= (now - m_BeginCnt))
		m_TimeOut = true;
	return m_TimeOut;
}

bool TimerUsObj::IsEffective()
{
	if (m_BeginCnt == 0) 
		return false;
	return true;
}

void TimerUsObj::Cancel()
{
	m_TimeOut = false;
	m_BeginCnt = 0;
	m_TargetCnt = 0;
	return;
}

