#pragma once
#include "msxdef.h"
#include <vector>
#include "../CUTimeCount.h"

class CMsxIoSystem : public IZ80IoDevice
{
private:
	std::vector<IZ80IoDevice*> m_Objs;
	CUTimeCount m_SystemTimer;
	uint16_t	m_SystemTimeCount;

public:
	CMsxIoSystem();
	virtual ~CMsxIoSystem();
	void JoinObject(IZ80IoDevice *pIoObj);

public:
	void Out(const z80ioaddr_t addr, const uint8_t b);
	uint8_t In(const z80ioaddr_t addr);

public:
/*IZ80IoDevice*/
	bool OutPort(const z80ioaddr_t addr, const uint8_t b);
	bool InPort(uint8_t *pB, const z80ioaddr_t addr);

private:
	void updateSystemTimer();
};