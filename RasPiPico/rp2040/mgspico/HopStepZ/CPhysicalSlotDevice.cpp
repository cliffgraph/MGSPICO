#include "pico/stdlib.h"
#include <stdio.h>
#include <memory.h>
#include "CPhysicalSlotDevice.h"
#include "../def_gpio.h"
#include "../t_mgspico.h"


CPhysicalSlotDevice::CPhysicalSlotDevice()
{
	m_portCarnivore2 = 0x00;
	m_M9000 = 0;
	return;
}

CPhysicalSlotDevice::~CPhysicalSlotDevice()
{
	// do nothing
	return;
}

bool CPhysicalSlotDevice::enableCARNIVORE2()
{
	m_portCarnivore2 = 0x00;
	ReadMem(0x4000); // ← 一度メモリ読み込みを行わないとCarnivore2が機能しない
	const z80ioaddr_t c2ports[] = {0xf0, 0xf1, 0xf2, };
	const int NUM_C2PORTS = static_cast<int>(sizeof(c2ports));
	for( int t = NUM_C2PORTS-1; 0 <= t; --t) {
		const auto pt = c2ports[t];
		OutPort(pt, 'C');
		uint8_t inDt;
		InPort(&inDt, pt);
		if( inDt == '2' ){
			OutPort(pt, 'S');
			InPort(&inDt, pt);
			printf("Found CARNIVORE2 in slot %d(%c)\n",t,inDt);
			m_portCarnivore2 = pt;
			// OutPort(pt, 'M');
			// busy_wait_ms(500);
			break;
		}
	}

	if( m_portCarnivore2 != 0x00 )
	{
		const z80memaddr_t baseAddr = 0x8F80;
		const auto pt = m_portCarnivore2;
		OutPort(pt, 'R');
		OutPort(pt, '2');	// 0x8F80
		// --------------------------------------------------------------------
		printf("CARNIVORE2 firmware ver%c.%c%c\n",
			ReadMem(baseAddr+0x2C), ReadMem(baseAddr+0x2D), ReadMem(baseAddr+0x2E));
		// --------------------------------------------------------------------
		// CARNIVORE2 - registor initial values
		static uint8_t g_CAR2_INIT_CFG[] = 
		{
			0xF8,0x50,0x00,0x85,0x03,0x40,
			0,0,0,0,0,0,
			0,0,0,0,0,0,
			0,0,0,0,0,0,
			0xFF,0x50,			// MGSPICOは/M1信号線はH固定なので、"遅延設定"は使用できない
		};
		const uint8_t *pTable = g_CAR2_INIT_CFG;
		const int NUM_REGS = static_cast<int>(sizeof(g_CAR2_INIT_CFG));
		// --------------------------------------------------------------------
		// // CARNIVORE2 - registor SCC+ values
		// static uint8_t g_CAR2_SCCPLUS_CFG[] = 
		// {
		// 	0xF8,0x50,0x00,0xB4,0xFF,0x40,
		// 	0xF8,0x70,0x01,0xB4,0xFF,0x60,
		// 	0xF8,0x90,0x02,0xB4,0xFF,0x80,
		// 	0xF8,0xB0,0x03,0xB1,0x01,0xA0,
		// 	0x01,0x50,// Mconf &  CardMDR
		// };
		// const uint8_t *pTable = g_CAR2_SCCPLUS_CFG;
		// const int NUM_REGS = static_cast<int>(sizeof(g_CAR2_SCCPLUS_CFG));
		// --------------------------------------------------------------------
		// CARNIVORE2 - registor initial values
		// static uint8_t g_CAR2_SCC_CFG[] = 
		// {
		// 	0xF8,0x50,0x00,0x8C,0x3F,0x40,
		// 	0xF8,0x70,0x01,0x8C,0x3F,0x60,
		// 	0xF8,0x90,0x02,0x8C,0x3F,0x80,
		// 	0xF8,0xB0,0x03,0x8C,0x3F,0xA0,
		// 	0x01,0xD0,
		// };
		// const uint8_t *pTable = g_CAR2_SCC_CFG;
		// const int NUM_REGS = static_cast<int>(sizeof(g_CAR2_SCC_CFG));
		// --------------------------------------------------------------------
		for( int t = 0; t < NUM_REGS; ++t)  {
			WriteMem(baseAddr+0x06+t, pTable[t]);
			 busy_wait_us(2);
		}
		// --------------------------------------------------------------------
		WriteMem(baseAddr+0x22, 0x80);	// Mono FM.
		WriteMem(baseAddr+0x24, 0x9b);	// Enable CARNIVORE2's PSG.
		// --------------------------------------------------------------------
		OutPort(pt, 'H');
	}
	return true;
}

bool CPhysicalSlotDevice::enableYAMANOOTO()
{
	// For Yamanooto cartridge, enable PSG echo on standard ports #A0-#A3
	WriteMem(0x7fff, ReadMem(0x7fff) | 0x01);
	WriteMem(0x7ffd, ReadMem(0x7ffd) | 0x02);
	return true;
}

bool CPhysicalSlotDevice::enableFMPAC()
{
	bool bRec = false;
	static const char *pMark = "OPLL";
	static const int LEN_MARK = 8;
	char sample[LEN_MARK+1] = "\0\0\0\0\0\0\0\0";	// '\0' x LEN_MARK
	for( int cnt = 0; cnt < LEN_MARK; ++cnt) {
		sample[cnt] = (char)ReadMem(0x4018 + cnt);
	}
	if( memcmp(sample+4, pMark, LEN_MARK-4) == 0) {
		printf("found OPLL: %s\n", sample);
		uint8_t v = ReadMem(0x7ff6);
		WriteMem(0x7ff6, v|0x01);
		bRec = true;;
	}
	return bRec;
}

bool CPhysicalSlotDevice::Setup()
{
	// CARNIVORE2を見つけて有効化する
	enableCARNIVORE2();

	// 拡張スロットの有無をチェックする -> m_bExt
	WriteMem(0xffff, 0x55);
	const uint8_t extFlag = ReadMem(0xffff);
	m_bExt = (extFlag==0xaa)?true:false;
	WriteMem(0xffff, 0x00);
	m_ExtReg = ReadMem(0xffff) ^ 0xff;
	printf("Ext:%d %02x\n", m_bExt, extFlag);

	// for YAMANOOTO.
	if( m_portCarnivore2 == 0x00) {
		enableYAMANOOTO();
	}

	// FMPACKがいればFMPACKのIOアクセスを有効化する
	if( !m_bExt ) {
		enableFMPAC();
	}
	else {
		const msxslotno_t tempSlotNo = GetSlotByPage(1);
		for(int t = 0; t < EXTSLOTNO_NUM; ++t) {
			SetSlotToPage(1/*0x4000*/, t);
			if( enableFMPAC() )
				break;
		}
		SetSlotToPage(1, tempSlotNo);
	}

	// // SCC RAMアクセステスト(無限ループ）
	// if( m_portCarnivore2 != 0x00 ){
	// 	WriteMem(0xBFFE, 0x00);
	// 	WriteMem(0x9000, 0x3F);		// bank.63(0x3f)はSCCレジスタのあるバンク
	// 	uint16_t addd = 0x9801;
	// 	busy_wait_ms(1);
	// 	WriteMem(addd, 0xaa);
	// 	for(;;) {
	// 		for( int t = 0; t <= 0xff; ++t){
	// 			const z80memaddr_t ad = addd;//+t;
	// 			WriteMem(ad, 0xaa);
	// 			auto v = ReadMem(ad);
	// 			printf(">> %04x: %02x:%02x\n", ad, t, v);
	// 			// if( v != t )
	// 			// 	break;
	// 		}
	// 	}
	// }

#ifdef MGS_MUSE_MACHINA
	mgspico::t_MuteSCC();
#endif
	return true;
}

RAM_FUNC void CPhysicalSlotDevice::SetSlotToPage(const msxpageno_t pageNo, const msxslotno_t slotNo)
{
	m_ExtReg &= (0x03 << (pageNo*2)) ^ 0xff;
	m_ExtReg |= slotNo << (pageNo*2);
	WriteMem(0xffff, m_ExtReg);
	return;
}

RAM_FUNC msxslotno_t CPhysicalSlotDevice::GetSlotByPage(const msxpageno_t pageNo)
{
	m_ExtReg = ReadMem(0xffff) ^ 0xff;
	const msxslotno_t extSlotNo = ( m_ExtReg >> (pageNo*2)) & 0x03; 
	return extSlotNo;
}

// ------------------------------------------------------------------------------------------------
RAM_FUNC bool CPhysicalSlotDevice::WriteMem(const z80memaddr_t addr, const uint8_t b)
{
	bool bRetc = false;
#ifdef MGS_MUSE_MACHINA
	if( addr == 0x9000 ){
		m_M9000 = b;
		mgspico::t_OutSCC(addr, b);
		bRetc = true;
	}
	else if(m_M9000 == 0x3f && ADDR_START <= addr && addr <= ADDR_END ){
		m_M9800[addr-ADDR_START] = b;
		mgspico::t_OutSCC(addr, b);
		bRetc = true;
	}
	else {
		bRetc = mgspico::t_WriteMem(addr, b);
	}
#else
	if( m_portCarnivore2 != 0x00 ) {
		if( addr == 0x9000 ){
			m_M9000 = b;
			bRetc = true;
		}
		else if(m_M9000 == 0x3f && ADDR_START <= addr && addr <= ADDR_END ){
			m_M9800[addr-ADDR_START] = b;
			bRetc = true;
		}
	}
	else {
		bRetc = mgspico::t_WriteMem(addr, b);
	}
#endif
	return bRetc;
}

RAM_FUNC uint8_t CPhysicalSlotDevice::ReadMem(const z80memaddr_t addr) const
{
	uint8_t b = 0xff;
#ifdef MGS_MUSE_MACHINA
	if( addr == 0x9000 ){
		b = m_M9000;
	}
	else if(m_M9000 == 0x3f && ADDR_START <= addr && addr <= ADDR_END ){
		b = m_M9800[addr-ADDR_START];
	}
	else {
		b = mgspico::t_ReadMem(addr);
	}
#else
	if( m_portCarnivore2 != 0x00 ) {
		if( addr == 0x9000 ){
			b = m_M9000;
		}
		else if(m_M9000 == 0x3f && ADDR_START <= addr && addr <= ADDR_END ){
			b = m_M9800[addr-ADDR_START];
		}
		else {
			b = mgspico::t_ReadMem(addr);
		}
	}
	else {
		b = mgspico::t_ReadMem(addr);
	}
#endif
	return b;
}
RAM_FUNC bool CPhysicalSlotDevice::OutPort(const z80ioaddr_t addr, const uint8_t b)
{
	return mgspico::t_OutPort(addr, b);
}

RAM_FUNC bool CPhysicalSlotDevice::InPort(uint8_t *pB, const z80ioaddr_t addr)
{
	return mgspico::t_InPort(pB, addr);
}

