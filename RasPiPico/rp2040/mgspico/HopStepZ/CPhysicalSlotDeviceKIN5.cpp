#include "CPhysicalSlotDeviceKIN5.h"
#include "../def_gpio.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <memory.h>

// 1us wait.
inline void wait1us()
{
	// no good
	// sleep_us(1), busy_wait_us(1)
	// no good
	// { uint32_t t = time_us_32(); while(t == time_us_32()); }

	// 125MHz動作時、下記のコードでbusy_wait_usを使用するより安定した1usのウェイトを生成できた
	// ループ回数は実機で動作させ実測して決定した
	// この方法の情報源：https://forums.raspberrypi.com/viewtopic.php?t=304922
	for(int t = 0; t < 25; t++) {
	    __asm volatile ("nop":);
	}
	return;
}

inline void wait700ns()
{
	for(int t = 0; t < 16; t++) {
	    __asm volatile ("nop":);
	}
	return;
}

inline void wait100ns()
{
	for(int t = 0; t < 7; t++) {
	    __asm volatile ("nop":);
	}
	return;
}

CPhysicalSlotDeviceKIN5::CPhysicalSlotDeviceKIN5()
{
	// 拡張スロットの有無をチェックする -> m_bExt
	WriteMem(0xffff, 0x55);
	m_bExt = (ReadMem(0xffff)==0xaa)?true:false;
	WriteMem(0xffff, 0x00);
	m_ExtReg = ReadMem(0xffff) ^ 0xff;

	// Activation for YAMANOOTO PSG echo mode.
	// WriteMem(0x7fff, ReadMem(0x7fff) | 0x01);
	// WriteMem(0x7ffd, ReadMem(0x7ffd) | 0x02);

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
	return;
}

CPhysicalSlotDeviceKIN5::~CPhysicalSlotDeviceKIN5()
{
	// do nothing
	return;
}

bool CPhysicalSlotDeviceKIN5::enableFMPAC()
{
	bool bRec = false;
	static const char *pMark = "PAC2OPLL";
	static const int MARKLEN = 8;
	char sample[MARKLEN+1] = {'\0','\0','\0','\0','\0','\0','\0','\0','\0',};
	for( int cnt = 0; cnt < MARKLEN; ++cnt) {
		sample[cnt] = (char)ReadMem(0x4018 + cnt);
	}
	if( memcmp(sample, pMark, MARKLEN) == 0) {
		uint8_t v = ReadMem(0x7ff6);
		WriteMem(0x7ff6, v|0x01);
		bRec = true;;
	}
	return bRec;
}

void __time_critical_func(CPhysicalSlotDeviceKIN5::SetSlotToPage)(const msxpageno_t pageNo, const msxslotno_t slotNo)
{
	m_ExtReg &= (0x03 << (pageNo*2)) ^ 0xff;
	m_ExtReg |= slotNo << (pageNo*2);
	WriteMem(0xffff, m_ExtReg);
	return;
}

msxslotno_t __time_critical_func(CPhysicalSlotDeviceKIN5::GetSlotByPage)(const msxpageno_t pageNo)
{
	m_ExtReg = ReadMem(0xffff) ^ 0xff;
	const msxslotno_t extSlotNo = ( m_ExtReg >> (pageNo*2)) & 0x03; 
	return extSlotNo;
}

bool __time_critical_func(CPhysicalSlotDeviceKIN5::WriteMem)(const z80memaddr_t addr, const uint8_t b)
{
	gpio_put(MSX_LATCH_A, 1);
	// 	・アドレスバス0-15(GPIO_0-15) <= メモリアドレス
	for(int t = 0; t < 16; ++t) {
		gpio_put(MSX_A0_D0 +t, (addr>>t)&0x01);
	}
	wait100ns();
	gpio_put(MSX_LATCH_A, 0);
	wait100ns();

	// 	・データバス0-7(GPIO_0-7) <= 1Byteデータ 
	for(int t = 0; t < 8; ++t) {
		gpio_put(MSX_A0_D0 +t, (b>>t)&0x01);
	}

	gpio_put(MSX_A8_IORQ,	 1);
	gpio_put(MSX_A9_MREQ,	 0);
	gpio_put(MSX_A10_RW,	 0);
	gpio_put(MSX_A11_RD,	 1);
	gpio_put(MSX_A12_SLTSL,	 0);
	gpio_put(MSX_A13_C1,	 1);
	gpio_put(MSX_A14_C12,	 1);
	gpio_put(MSX_A15_RESET,	 1);
	gpio_put(MSX_LATCH_C,	 1);
	wait1us();

	gpio_put(MSX_A9_MREQ,	 1);
	gpio_put(MSX_A10_RW,	 1);
	gpio_put(MSX_A12_SLTSL,	 1);
	wait100ns();
	gpio_put(MSX_LATCH_C,	 0);

	return true;
}

uint8_t __time_critical_func(CPhysicalSlotDeviceKIN5::ReadMem)(const z80memaddr_t addr) const
{
	gpio_put(MSX_LATCH_A, 1);
	// 	・アドレスバス0-15(GPIO_0-15) <= メモリアドレス
	for(int t = 0; t < 16; ++t) {
		gpio_put(MSX_A0_D0 +t, (addr>>t)&0x01);
	}
	wait100ns();
	gpio_put(MSX_LATCH_A, 0);

	// 	・GPIO_0-7 をINに設定する
	for(int t = 0; t < 8; ++t) {
		gpio_set_dir(MSX_A0_D0 +t, GPIO_IN);
	}
	// 	・DDIR <= H(A->B)
	gpio_put(MSX_DDIR,		 1);

	gpio_put(MSX_A8_IORQ,	 1);
	gpio_put(MSX_A9_MREQ,	 0);
	gpio_put(MSX_A10_RW,	 1);
	gpio_put(MSX_A11_RD,	 0);
	gpio_put(MSX_A12_SLTSL,	 0);
	gpio_put(MSX_A13_C1,	 (0x4000<=addr&&addr<=0x7fff)?0:1 );
	gpio_put(MSX_A14_C12,	 (0x4000<=addr&&addr<=0xbfff)?0:1 );
	gpio_put(MSX_A15_RESET,	 1);
	gpio_put(MSX_LATCH_C,	 1);
	wait1us();

	// 	・データバス0-7(GPIO_0-7) => データ 読み出し
	uint8_t dt8 = 0x00;
	for(int t = 0; t < 8; ++t) {
		dt8 |= (gpio_get(MSX_A0_D0+t)&0x01) << t;
	}

	gpio_put(MSX_A9_MREQ,	 1);
	gpio_put(MSX_A11_RD,	 1);
	gpio_put(MSX_A12_SLTSL,	 1);
	gpio_put(MSX_A13_C1,	 1);
	gpio_put(MSX_A14_C12,	 1);
	wait100ns();
	gpio_put(MSX_LATCH_C, 0);

	// 	・GPIO_0-7 をOUTに設定する
	for(int t = 0; t < 8; ++t) {
		gpio_set_dir(MSX_A0_D0 +t, GPIO_OUT);
	}
	// 	・DDIR <= L(B->A)
	gpio_put(MSX_DDIR,	 0);

	return dt8;
}

bool __time_critical_func(CPhysicalSlotDeviceKIN5::OutPort)(const z80ioaddr_t addr, const uint8_t b)
{
	gpio_put(MSX_LATCH_A, 1);
	// 	・アドレスバス0-7(GPIO_0-7) <= ポート番号
	for(int t = 0; t < 8; ++t) {
		gpio_put(MSX_A0_D0 +t, (addr>>t)&0x01);
	}
	gpio_put(MSX_LATCH_A, 0);
	wait100ns();

	// 	・データバス0-7(GPIO_0-7) <= 1Byteデータ 
	for(int t = 0; t < 8; ++t) {
		gpio_put(MSX_A0_D0 +t, (b>>t)&0x01);
	}
	gpio_put(MSX_A8_IORQ,	 0);
	gpio_put(MSX_A9_MREQ,	 1);
	gpio_put(MSX_A10_RW,	 1);
	gpio_put(MSX_A11_RD,	 1);
	gpio_put(MSX_A12_SLTSL,	 1);
	gpio_put(MSX_A13_C1,	 1);
	gpio_put(MSX_A14_C12,	 1);
	gpio_put(MSX_A15_RESET,	 1);
	gpio_put(MSX_LATCH_C,	 1);
	wait100ns();
	gpio_put(MSX_A10_RW,	 0);
	wait700ns();
	gpio_put(MSX_A10_RW,	 1);
	gpio_put(MSX_A8_IORQ,	 1);
	wait100ns();
	gpio_put(MSX_LATCH_C,	 0);

	// // ここのウェイトはいるだろうか
	// if( addr == 0x7c )
	// 	busy_wait_us(3);
	// else if( addr == 0x7d )
	// 	busy_wait_us(23);

	return true;
}

bool __time_critical_func(CPhysicalSlotDeviceKIN5::InPort)(uint8_t *pB, const z80ioaddr_t addr)
{
	gpio_put(MSX_LATCH_A,	 1);
	// 	・アドレスバス0-7(GPIO_0-7) <= ポート番号
	// 	・アドレスバス8-15(GPIO_8-15)  <= 0x00
	for(int t = 0; t < 8; ++t) {
		gpio_put(MSX_A0_D0 +t, (addr>>t)&0x01);
	}
	gpio_put(MSX_LATCH_A,	 0);

	// 	・GPIO_0-7 をINに設定する
	for(int t = 0; t < 8; ++t) {
		gpio_set_dir(MSX_A0_D0 +t, GPIO_IN);
	}
	// 	・DDIR <= H(A->B)
	gpio_put(MSX_DDIR,		 1);

	gpio_put(MSX_A8_IORQ,	 0);
	gpio_put(MSX_A9_MREQ,	 1);
	gpio_put(MSX_A10_RW,	 1);
	gpio_put(MSX_A11_RD,	 0);
	gpio_put(MSX_A12_SLTSL,	 1);
	gpio_put(MSX_A13_C1,	 1);
	gpio_put(MSX_A14_C12,	 1);
	gpio_put(MSX_A15_RESET,	 1);
	gpio_put(MSX_LATCH_C,	 1);
	wait1us();

	// 	・データバス0-7(GPIO_0-7) => データ 読み出し
	uint8_t dt8 = 0x00;
	for(int t = 0; t < 8; ++t) {
		dt8 |= (gpio_get(MSX_A0_D0+t)&0x01) << t;
	}
	gpio_put(MSX_A8_IORQ,	 1);
	gpio_put(MSX_A11_RD,	 1);
	wait100ns();
	gpio_put(MSX_LATCH_C,	 0);

	// 	・GPIO_0-7 をOUTに設定する
	for(int t = 0; t < 8; ++t) {
		gpio_set_dir(MSX_A0_D0 +t, GPIO_OUT);
	}
	// 	・DDIR <= L(B->A)
	gpio_put(MSX_DDIR,	 0);

	return true;
}



