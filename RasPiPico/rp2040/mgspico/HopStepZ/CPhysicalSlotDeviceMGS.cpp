#include "CPhysicalSlotDeviceMGS.h"
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

CPhysicalSlotDeviceMGS::CPhysicalSlotDeviceMGS()
{
	WriteMem(0xffff, 0x55);
	m_bExt = (ReadMem(0xffff)==0xaa)?true:false;
	WriteMem(0xffff, 0x00);
	m_ExtReg = ReadMem(0xffff) ^ 0xff;

	// FMPACKがいればIOアクセスを有効化する
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

CPhysicalSlotDeviceMGS::~CPhysicalSlotDeviceMGS()
{
	// do nothing
	return;
}

bool CPhysicalSlotDeviceMGS::enableFMPAC()
{
	bool bRec = false;
	static const char *pMark = "OPLL";
	char sample[5] = {'\0','\0','\0','\0','\0',};
	int cnt = 0;
	for( ; cnt < 4; ++cnt) {
		sample[cnt] = (char)ReadMem(0x401C + cnt);
	}
	if( memcmp(sample, pMark, 4) == 0) {
		uint8_t v = ReadMem(0x7ff6);
		WriteMem(0x7ff6, v|0x01);
		bRec = true;;
	}
	return bRec;
}

void __time_critical_func(CPhysicalSlotDeviceMGS::SetSlotToPage)(const msxpageno_t pageNo, const msxslotno_t slotNo)
{
	m_ExtReg &= (0x03 << (pageNo*2)) ^ 0xff;
	m_ExtReg |= slotNo << (pageNo*2);
	WriteMem(0xffff, m_ExtReg);
	return;
}

msxslotno_t __time_critical_func(CPhysicalSlotDeviceMGS::GetSlotByPage)(const msxpageno_t pageNo)
{
	m_ExtReg = ReadMem(0xffff) ^ 0xff;
	const msxslotno_t extSlotNo = ( m_ExtReg >> (pageNo*2)) & 0x03; 
	return extSlotNo;
}

bool __time_critical_func(CPhysicalSlotDeviceMGS::WriteMem)(const z80memaddr_t addr, const uint8_t b)
{
// 	(・GPIO_8-15 == H)
// 	(・LATCH_C == L)
// 	(・DDIR <= L(B->A))

// 	・LATCH_A <= H
	gpio_put(MSX_LATCH_A, 1);
// 	・アドレスバス0-15(GPIO_0-15) <= メモリアドレス
	for(int t = 0; t < 16; ++t) {
		gpio_put(MSX_A0_D0 +t, (addr>>t)&0x01);
	}
// 	・ウェイト 2.5ns～10ns
	wait1us();
// 	・LATCH_A <= L
	gpio_put(MSX_LATCH_A, 0);
// 	・データバス0-7(GPIO_0-7) <= 1Byteデータ 
	for(int t = 0; t < 8; ++t) {
		gpio_put(MSX_A0_D0 +t, (b>>t)&0x01);
	}
// 	・ウェイト 2.5cyc(279ns*2.5 = 698ns)
	wait1us();
// 	・IORQ(GPIO_8)	<= H
// 	・MREQ(GPIO_9)	<= L
// 	・RW(GPIO_10)	<= L
// 	・RD(GPIO_11)	<= H
// 	・SLTSL(GPIO_12)<= L
// 	・C1(GPIO_13)	<= H
// 	・C12(GPIO_14)	<= H
// 	・GPIO_15) 		<= H
// 	・LATCH_C 		<= H
	gpio_put(MSX_A8_IORQ,	 1);
	gpio_put(MSX_A9_MREQ,	 0);
	gpio_put(MSX_A10_RW,	 0);
	gpio_put(MSX_A11_RD,	 1);
	gpio_put(MSX_A12_SLTSL,	 0);
	gpio_put(MSX_A13_C1,	 1);
	gpio_put(MSX_A14_C12,	 1);
	gpio_put(MSX_A15_RESET,	 1);
	gpio_put(MSX_LATCH_C,	 1);
// 	・ウェイト 1us
	wait1us();
// 	・IORQ(GPIO_8)	<= H
// 	・MREQ(GPIO_9)	<= H
// 	・RW(GPIO_10)	<= H
// 	・RD(GPIO_11)	<= H
// 	・SLTSL(GPIO_12)<= H
// 	・C1(GPIO_13)	<= H
// 	・C12(GPIO_14)	<= H
// 	・GPIO_15 		<= H
// 	・LATCH_C 		<= L
	gpio_put(MSX_A8_IORQ,	 1);
	gpio_put(MSX_A9_MREQ,	 1);
	gpio_put(MSX_A10_RW,	 1);
	gpio_put(MSX_A11_RD,	 1);
	gpio_put(MSX_A12_SLTSL,	 1);
	gpio_put(MSX_A13_C1,	 1);
	gpio_put(MSX_A14_C12,	 1);
	gpio_put(MSX_A15_RESET,	 1);
	wait1us();
	gpio_put(MSX_LATCH_C,	 0);
	wait1us();

	return true;
}

uint8_t __time_critical_func(CPhysicalSlotDeviceMGS::ReadMem)(const z80memaddr_t addr) const
{
// 	(・GPIO_8-15 == H)
// 	(・LATCH_C == L)
// 	(・DDIR <= L(B->A))

// 	・LATCH_A <= H
	gpio_put(MSX_LATCH_A,	 1);
// 	・アドレスバス0-15(GPIO_0-15) <= メモリアドレス
	for(int t = 0; t < 16; ++t) {
		gpio_put(MSX_A0_D0 +t, (addr>>t)&0x01);
	}
// 	・ウェイト 2.5ns～10ns
	wait1us();
// 	・LATCH_A <= L
	gpio_put(MSX_LATCH_A,	 0);

// 	・GPIO_0-7 をINに設定する
	for(int t = 0; t < 8; ++t) {
		gpio_set_dir(MSX_A0_D0 +t, GPIO_IN);
	}

// 	・DDIR <= H(A->B)
	gpio_put(MSX_DDIR,		 1);

// 	・IORQ(GPIO_8)	<= H
// 	・MREQ(GPIO_9)	<= L
// 	・RW(GPIO_10)	<= H
// 	・RD(GPIO_11)	<= L
// 	・SLTSL(GPIO_12)<= L
	gpio_put(MSX_A8_IORQ,	 1);
	gpio_put(MSX_A9_MREQ,	 0);
	gpio_put(MSX_A10_RW,	 1);
	gpio_put(MSX_A11_RD,	 0);
	gpio_put(MSX_A12_SLTSL,	 0);
// 	・C1(GPIO_13)	<= * アドレス 0x4000-0x7FFFFの範囲は L
// 	・C12(GPIO_14)	<= * アドレス 0x4000-0xBFFFFの範囲は L
// 	・GPIO_15 		<= H
	gpio_put(MSX_A13_C1,	 (0x4000<=addr&&addr<=0x7fff)?0:1 );
	gpio_put(MSX_A14_C12,	 (0x4000<=addr&&addr<=0xbfff)?0:1 );
	gpio_put(MSX_A15_RESET,	 1);
	gpio_put(MSX_LATCH_C,	 1);
// 	・ウェイト 1us
	wait1us();

// 	・データバス0-7(GPIO_0-7) => データ 読み出し
	uint8_t dt8 = 0x00;
	for(int t = 0; t < 8; ++t) {
		dt8 |= (gpio_get(MSX_A0_D0+t)&0x01) << t;
	}

// 	・IORQ(GPIO_8)	<= H
// 	・MREQ(GPIO_9)	<= H
// 	・RW(GPIO_10)	<= H
// 	・RD(GPIO_11)	<= H
// 	・SLTSL(GPIO_12)<= H
// 	・C1(GPIO_13)	<= H
// 	・C12(GPIO_14)	<= H
// 	・GPIO_15 		<= H
	gpio_put(MSX_A8_IORQ,	 1);
	gpio_put(MSX_A9_MREQ,	 1);
	gpio_put(MSX_A10_RW,	 1);
	gpio_put(MSX_A11_RD,	 1);
	gpio_put(MSX_A12_SLTSL,	 1);
	gpio_put(MSX_A13_C1,	 1);
	gpio_put(MSX_A14_C12,	 1);
	gpio_put(MSX_A15_RESET,	 1);
	wait1us();
	gpio_put(MSX_LATCH_C,	 0);

// 	・GPIO_0-7 をOUTに設定する
	for(int t = 0; t < 8; ++t) {
		gpio_set_dir(MSX_A0_D0 +t, GPIO_OUT);
	}
// 	・DDIR <= L(B->A)
	gpio_put(MSX_DDIR,	 0);

	return dt8;
}


bool __time_critical_func(CPhysicalSlotDeviceMGS::OutPort)(const z80ioaddr_t addr, const uint8_t b)
{
//	printf("%02x %02x\n", addr, b);
// 	(・GPIO_8-15 == H)
// 	(・LATCH_C == L)
// 	(・DDIR <= L(B->A))

// 	・アドレスバス0-7(GPIO_0-7) <= ポート番号
// 	・アドレスバス8-15(GPIO_8-15)  <= 0x00
	for(int t = 0; t < 8; ++t) {
		gpio_put(MSX_A0_D0 +t, (addr>>t)&0x01);
	}
	// for(int t = 0; t < 8; ++t) {
	// 	gpio_put(MSX_A8_IORQ +t, 0);
	// }
// 	・LATCH_A <= H
	gpio_put(MSX_LATCH_A,	 1);
// 	・ウェイト 2.5ns～10ns
	wait1us();
// 	・LATCH_A <= L
	gpio_put(MSX_LATCH_A,	 0);

// 	・データバス0-7(GPIO_0-7) <= 1Byteデータ 
	for(int t = 0; t < 8; ++t) {
		gpio_put(MSX_A0_D0 +t, (b>>t)&0x01);
	}
// 	・ウェイト 2.5cyc(279ns*2.5 = 698ns)
//	wait1us();	// ←これは必要ない
// 	・IORQ(GPIO_8)	<= L
// 	・MREQ(GPIO_9)	<= H
// 	・RW(GPIO_10)	<= L
// 	・RD(GPIO_11)	<= H
// 	・SLTSL(GPIO_12)<= H
// 	・C1(GPIO_13)	<= H
// 	・C12(GPIO_14)	<= H
// 	・GPIO_15) 		<= H
// 	・LATCH_C 		<= H
	gpio_put(MSX_A11_RD,	 1);
	gpio_put(MSX_A8_IORQ,	 0);
	gpio_put(MSX_A9_MREQ,	 1);
	gpio_put(MSX_A12_SLTSL,	 1);
	gpio_put(MSX_A13_C1,	 1);
	gpio_put(MSX_A14_C12,	 1);
	gpio_put(MSX_A15_RESET,	 1);
	gpio_put(MSX_LATCH_C,	 1);
	wait100ns();
	gpio_put(MSX_A10_RW,	 0);
	wait700ns();
// 	・IORQ(GPIO_8)	<= H
// 	・MREQ(GPIO_9)	<= H
// 	・RW(GPIO_10)	<= H
// 	・RD(GPIO_11)	<= H
// 	・SLTSL(GPIO_12)<= H
// 	・C1(GPIO_13)	<= H
// 	・C12(GPIO_14)	<= H
// 	・GPIO_15 		<= H
// 	・LATCH_C 		<= L
	gpio_put(MSX_A10_RW,	 1);
	gpio_put(MSX_A11_RD,	 1);
	gpio_put(MSX_A8_IORQ,	 1);
	gpio_put(MSX_A9_MREQ,	 1);
	gpio_put(MSX_A12_SLTSL,	 1);
	gpio_put(MSX_A13_C1,	 1);
	gpio_put(MSX_A14_C12,	 1);
	gpio_put(MSX_A15_RESET,	 1);
	wait100ns();
	gpio_put(MSX_LATCH_C,	 0);

	// // ここのウェイトはいるだろうか
	// if( addr == 0x7c )
	// 	busy_wait_us(3);
	// else if( addr == 0x7d )
	// 	busy_wait_us(23);

	return true;
}

bool __time_critical_func(CPhysicalSlotDeviceMGS::InPort)(uint8_t *pB, const z80ioaddr_t addr)
{
// 	(・GPIO_8-15 == H)
// 	(・LATCH_C == L)
// 	(・DDIR <= L(B->A))

// 	・アドレスバス0-7(GPIO_0-7) <= ポート番号
// 	・アドレスバス8-15(GPIO_8-15)  <= 0x00
	for(int t = 0; t < 8; ++t) {
		gpio_put(MSX_A0_D0 +t, (addr>>t)&0x01);
	}
	// for(int t = 0; t < 8; ++t) {
	// 	gpio_put(MSX_A8_IORQ +t, 0);
	// }
// 	・LATCH_A <= H
	gpio_put(MSX_LATCH_A,	 1);
// 	・ウェイト 2.5ns～10ns
	wait1us();
// 	・LATCH_A <= L
	gpio_put(MSX_LATCH_A,	 0);

// 	・GPIO_0-7 をINに設定する
	for(int t = 0; t < 8; ++t) {
		gpio_set_dir(MSX_A0_D0 +t, GPIO_IN);
	}
// 	・DDIR <= H(A->B)
	gpio_put(MSX_DDIR,		 1);

// 	・MREQ(GPIO_8)	<= H
// 	・IORQ(GPIO_9)	<= L
// 	・RW(GPIO_10)	<= H
// 	・RD(GPIO_11)	<= L
// 	・SLTSL(GPIO_12)<= H
// 	・C1(GPIO_13)	<= H
// 	・C12(GPIO_14)	<= H
// 	・GPIO_15 		<= H
	gpio_put(MSX_A8_IORQ,	 0);
	gpio_put(MSX_A9_MREQ,	 1);
	gpio_put(MSX_A10_RW,	 1);
	gpio_put(MSX_A11_RD,	 0);
	gpio_put(MSX_A12_SLTSL,	 1);
	gpio_put(MSX_A13_C1,	 1);
	gpio_put(MSX_A14_C12,	 1);
	gpio_put(MSX_A15_RESET,	 1);
	gpio_put(MSX_LATCH_C,	 1);
// 	・ウェイト 1us
	wait1us();

// 	・データバス0-7(GPIO_0-7) => データ 読み出し
	uint8_t dt8 = 0x00;
	for(int t = 0; t < 8; ++t) {
		dt8 |= (gpio_get(MSX_A0_D0+t)&0x01) << t;
	}
// 	・MREQ(GPIO_8)	<= H
// 	・IORQ(GPIO_9)	<= H
// 	・RW(GPIO_10)	<= H
// 	・RD(GPIO_11)	<= H
// 	・SLTSL(GPIO_12)<= H
// 	・C1(GPIO_13)	<= H
// 	・C12(GPIO_14)	<= H
// 	・GPIO_15 		<= H
	gpio_put(MSX_A8_IORQ,	 1);
	gpio_put(MSX_A9_MREQ,	 1);
	gpio_put(MSX_A10_RW,	 1);
	gpio_put(MSX_A11_RD,	 1);
	gpio_put(MSX_A12_SLTSL,	 1);
	gpio_put(MSX_A13_C1,	 1);
	gpio_put(MSX_A14_C12,	 1);
	gpio_put(MSX_A15_RESET,	 1);
// 	・ウェイト 1us
//	wait1us();
	gpio_put(MSX_LATCH_C,	 0);

// 	・GPIO_0-7 をOUTに設定する
	for(int t = 0; t < 8; ++t) {
		gpio_set_dir(MSX_A0_D0 +t, GPIO_OUT);
	}
// 	・DDIR <= L(B->A)
	gpio_put(MSX_DDIR,	 0);
	return true;
}



