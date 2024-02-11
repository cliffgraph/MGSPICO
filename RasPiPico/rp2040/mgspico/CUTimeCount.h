
#pragma once
#include <stdint.h>

class CUTimeCount
{
private:
	uint64_t m_Begin;

private:
	uint64_t getCount();
public:
	CUTimeCount();
	virtual ~CUTimeCount();
public:
	void ResetBegin();
	uint64_t GetTime();
};

