#pragma once

#include <stdio.h>
#include <memory.h>
#include "pico/stdlib.h"

class TimerObj
{
private:
	uint32_t m_TargetCnt;
	uint32_t m_BeginCnt;
	bool m_TimeOut;
public:
	TimerObj();
	virtual ~TimerObj();
	void Start(const uint32_t cnt);
	bool IsEffective();
	bool IsTimeout();
	void Cancel();
};

class TimerUsObj
{
private:
	uint64_t m_TargetCnt;
	uint64_t m_BeginCnt;
	bool m_TimeOut;
public:
	TimerUsObj();
	virtual ~TimerUsObj();
	void Start(const uint64_t cnt);
	bool IsEffective();
	bool IsTimeout();
	void Cancel();
};

