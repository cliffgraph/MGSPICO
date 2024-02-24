#include "CUTimeCount.h"

uint32_t GetTimerCounterMS();


CUTimeCount::CUTimeCount()
{
	m_Begin = getCount();
	return;
}

CUTimeCount::~CUTimeCount()
{
	// do nothing
	return;
}

void CUTimeCount::ResetBegin()
{
	m_Begin = getCount();
	return;
}

uint64_t CUTimeCount::GetTime()
{
	return getCount() - m_Begin;
}

uint64_t CUTimeCount::getCount()	// マイクロ秒単位
{
	uint64_t usec = (uint64_t)GetTimerCounterMS() * 1000L; // msからus単位に。
	return usec;
}
