#include "stdafx.h"
#include <stdio.h>
#include "t_mgspico.h"
#include "t_mmmspi.h"


namespace mgspico
{
inline void t_wait1us()
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

inline void t_wait700ns()
{
	for(int t = 0; t < 16; t++) {
	    __asm volatile ("nop":);
	}
	return;
}

inline void t_wait100ns()
{
	for(int t = 0; t < 7; t++) {
	    __asm volatile ("nop":);
	}
	return;
}

RAM_FUNC bool t_WriteMem(const z80memaddr_t addr, const uint8_t b)
{
#ifdef MGSPICO_1ST
	// 	・アドレスバス0-15(GPIO_0-15) <= メモリアドレス
	gpio_put(MSX_LATCH_A, 1);
	for(int t = 0; t < 16; ++t) {
		gpio_put(MSX_A0_D0 +t, (addr>>t)&0x01);
	}
// 	・ウェイト 要:2.5ns～10ns（nop*2 ->約20ns)
    __asm volatile ("nop":);
    __asm volatile ("nop":);
	gpio_put(MSX_LATCH_A, 0);	// 	・LATCH_A <= L

	// 	・データバス0-7(GPIO_0-7) <= 1Byteデータ 
	for(int t = 0; t < 8; ++t) {
		gpio_put(MSX_A0_D0 +t, (b>>t)&0x01);
	}

	gpio_put(MSX_A8_IORQ,	 1);
	gpio_put(MSX_A9_MREQ,	 0);
	gpio_put(MSX_A11_RD,	 1);
	gpio_put(MSX_A12_SLTSL,	 0);
	gpio_put(MSX_A13_C1,	 1);
	gpio_put(MSX_A14_C12,	 1);
	gpio_put(MSX_A15_RESET,	 1);
	gpio_put(MSX_LATCH_C,	 1);
    __asm volatile ("nop":);
    __asm volatile ("nop":);
	gpio_put(MSX_A10_RW,	 0);
	busy_wait_us(3);
	// t_wait1us();
	// t_wait1us();
	// t_wait1us();
	t_wait100ns();

	gpio_put(MSX_A10_RW,	 1);
    __asm volatile ("nop":);
    __asm volatile ("nop":);
	gpio_put(MSX_A9_MREQ,	 1);
	gpio_put(MSX_A12_SLTSL,	 1);

    __asm volatile ("nop":);
    __asm volatile ("nop":);
	gpio_put(MSX_LATCH_C,	 0);
#endif
	return true;
}

RAM_FUNC uint8_t t_ReadMem(const z80memaddr_t addr)
{
#ifdef MGSPICO_1ST
	// 	・アドレスバス0-15(GPIO_0-15) <= メモリアドレス
	gpio_put(MSX_LATCH_A,	 1);
	for(int t = 0; t < 16; ++t) {
		gpio_put(MSX_A0_D0 +t, (addr>>t)&0x01);
	}
    __asm volatile ("nop":);
    __asm volatile ("nop":);
	gpio_put(MSX_LATCH_A,	 0);	// 	・LATCH_A <= L

	// 	・GPIO_0-7 をINに設定する
	for(int t = 0; t < 8; ++t) {
		gpio_set_dir(MSX_A0_D0 +t, GPIO_IN);
	}
	// 	・DDIR <= H(A->B)
	gpio_put(MSX_DDIR,		 1);

	const uint32_t c1 = (0x4000<=addr&&addr<=0x7fff)?0:1;	// CS1
	const uint32_t c12= (0x8000<=addr&&addr<=0xbfff)?0:1;	// CS2

	// RDだけLにするのではなく、その他は1にしておく必要あり
	gpio_put(MSX_A8_IORQ,	 1);
	gpio_put(MSX_A9_MREQ,	 1);
	gpio_put(MSX_A10_RW,	 1);
	gpio_put(MSX_A11_RD,	 0);
	gpio_put(MSX_A12_SLTSL,	 1);
	gpio_put(MSX_A13_C1,	 1);
	gpio_put(MSX_A14_C12,	 1);
	gpio_put(MSX_A15_RESET,	 1);
	gpio_put(MSX_LATCH_C,	 1);
    __asm volatile ("nop":);
    __asm volatile ("nop":);
	gpio_put(MSX_A9_MREQ,	 0);
	gpio_put(MSX_A12_SLTSL,	 0);
	gpio_put(MSX_A13_C1,	 c1);
	gpio_put(MSX_A14_C12,	 c12);
	// 	・ウェイト 1us
	busy_wait_us(3);
	// t_wait1us();
	// t_wait1us();
	// t_wait1us();
	t_wait100ns();
	// 	・データバス0-7(GPIO_0-7) => データ 読み出し
	uint8_t dt8 = 0x00;
	for(int t = 0; t < 8; ++t) {
		dt8 |= (gpio_get(MSX_A0_D0+t)&0x01) << t;
	}

	gpio_put(MSX_A8_IORQ,	 1);
	gpio_put(MSX_A9_MREQ,	 1);
	gpio_put(MSX_A10_RW,	 1);
	gpio_put(MSX_A11_RD,	 1);
	gpio_put(MSX_A12_SLTSL,	 1);
	gpio_put(MSX_A13_C1,	 1);
	gpio_put(MSX_A14_C12,	 1);
	gpio_put(MSX_A15_RESET,	 1);
    __asm volatile ("nop":);
    __asm volatile ("nop":);
	gpio_put(MSX_LATCH_C,	 0);

	t_wait700ns();

	// 	・GPIO_0-7 をOUTに設定する
	for(int t = 0; t < 8; ++t) {
		gpio_set_dir(MSX_A0_D0 +t, GPIO_OUT);
	}
	// 	・DDIR <= L(B->A)
	gpio_put(MSX_DDIR,	 0);
	return dt8;
#else
	return 0x00;
#endif
}

RAM_FUNC bool t_OutPort(const z80ioaddr_t addr, const uint8_t b)
{
#if defined(MGSPICO_2ND)	// MGS,MuSICA
	switch(addr)
	{
		// OPLL
		case 0x7C:
			gpio_put(MMM_AEX0, 0);
			for(int t = 0; t < 8; ++t) {
				gpio_put(MMM_D0 +t, (b>>t)&0x01);
			}
			gpio_put(MMM_CSWR_FM, 0);
			busy_wait_us(1);
			gpio_put(MMM_CSWR_FM, 1);
			busy_wait_us(12);
			break;
		case 0x7D:
			gpio_put(MMM_AEX0, 1);
			for(int t = 0; t < 8; ++t) {
				gpio_put(MMM_D0 +t, (b>>t)&0x01);
			}
			gpio_put(MMM_CSWR_FM, 0);
			busy_wait_us(1);
			gpio_put(MMM_CSWR_FM, 1);
			busy_wait_us(84);
			break;

		// PSG
		case 0xA0:
			gpio_put(MMM_AEX0, 0);
			for(int t = 0; t < 8; ++t) {
				gpio_put(MMM_D0 +t, (b>>t)&0x01);
			}
			gpio_put(MMM_CSWR_PSG, 0);
			busy_wait_us(1);
			gpio_put(MMM_CSWR_PSG, 1);
			busy_wait_us(1);
			break;
		case 0xA1:
			gpio_put(MMM_AEX0, 1);
			for(int t = 0; t < 8; ++t) {
				gpio_put(MMM_D0 +t, (b>>t)&0x01);
			}
			gpio_put(MMM_CSWR_PSG, 0);
			busy_wait_us(1);
			gpio_put(MMM_CSWR_PSG, 1);
			busy_wait_us(1);
			break;
		default:
			break;
	}
#elif defined(MGSPICO_3RD)
	static uint8_t opll_addr;
	static uint8_t psg_addr;
	switch(addr)
	{
		// OPLL
		case 0x7C:
			opll_addr = b;
			break;
		case 0x7D:
			mmmspi::PushBuff(mmmspi::CMD::OPLL, opll_addr, b);
			break;

		// PSG
		case 0xA0:
			psg_addr = b;
			break;
		case 0xA1:
			mmmspi::PushBuff(mmmspi::CMD::PSG, psg_addr, b);
			break;
		default:
			break;
	}
#elif defined(MGSPICO_1ST)
	// 	・アドレスバス0-7(GPIO_0-7) <= ポート番号
	// 	・アドレスバス8-15(GPIO_8-15)  <= 0x00
	for(int t = 0; t < 8; ++t) {
		gpio_put(MSX_A0_D0 +t, (addr>>t)&0x01);
	}
	// 	・LATCH_A <= H
	gpio_put(MSX_LATCH_A,	 1);
// 	・ウェイト 要:2.5ns～10ns（nop*2 ->約20ns)
    __asm volatile ("nop":);
    __asm volatile ("nop":);
	// 	・LATCH_A <= L
	gpio_put(MSX_LATCH_A,	 0);

	// 	・データバス0-7(GPIO_0-7) <= 1Byteデータ 
	for(int t = 0; t < 8; ++t) {
		gpio_put(MSX_A0_D0 +t, (b>>t)&0x01);
	}
	// 	・ウェイト 2.5cyc(279ns*2.5 = 698ns)
	//	t_wait1us();	// ←これは必要ない
	gpio_put(MSX_A11_RD,	 1);
	gpio_put(MSX_A8_IORQ,	 0);
	gpio_put(MSX_A9_MREQ,	 1);
	gpio_put(MSX_A12_SLTSL,	 1);
	gpio_put(MSX_A13_C1,	 1);
	gpio_put(MSX_A14_C12,	 1);
	gpio_put(MSX_A15_RESET,	 1);
	gpio_put(MSX_LATCH_C,	 1);
    __asm volatile ("nop":);
    __asm volatile ("nop":);
	gpio_put(MSX_A10_RW,	 0);
	busy_wait_us(6);
	t_wait1us();
	t_wait1us();
	t_wait1us();
	t_wait1us();
	t_wait1us();
	t_wait1us();
	t_wait100ns();	
	gpio_put(MSX_A10_RW,	 1);
	gpio_put(MSX_A11_RD,	 1);
	gpio_put(MSX_A8_IORQ,	 1);
	gpio_put(MSX_A9_MREQ,	 1);
	gpio_put(MSX_A12_SLTSL,	 1);
	gpio_put(MSX_A13_C1,	 1);
	gpio_put(MSX_A14_C12,	 1);
	gpio_put(MSX_A15_RESET,	 1);
    __asm volatile ("nop":);
    __asm volatile ("nop":);
	gpio_put(MSX_LATCH_C,	 0);
#endif
	return true;
}

RAM_FUNC bool t_InPort(uint8_t *pB, const z80ioaddr_t addr)
{
#if defined(MGSPICO_1ST)
	// 	・アドレスバス0-7(GPIO_0-7) <= ポート番号
	// 	・アドレスバス8-15(GPIO_8-15)  <= (不定価)
	for(int t = 0; t < 8; ++t) {
		gpio_put(MSX_A0_D0 +t, (addr>>t)&0x01);
	}
	gpio_put(MSX_LATCH_A,	 1);
// 	・ウェイト 要:2.5ns～10ns（nop*2 ->約20ns)
    __asm volatile ("nop":);
    __asm volatile ("nop":);
	// 	・LATCH_A <= L
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
	busy_wait_us(3);
	t_wait1us();
	t_wait1us();
	t_wait1us();
	t_wait1us();
	t_wait1us();
	t_wait1us();
	t_wait100ns();
	// 	・データバス0-7(GPIO_0-7) => データ 読み出し
	uint8_t dt8 = 0x00;
	for(int t = 0; t < 8; ++t) {
		dt8 |= (gpio_get(MSX_A0_D0+t)&0x01) << t;
	}
	gpio_put(MSX_A8_IORQ,	 1);
	gpio_put(MSX_A9_MREQ,	 1);
	gpio_put(MSX_A10_RW,	 1);
	gpio_put(MSX_A11_RD,	 1);
	gpio_put(MSX_A12_SLTSL,	 1);
	gpio_put(MSX_A13_C1,	 1);
	gpio_put(MSX_A14_C12,	 1);
	gpio_put(MSX_A15_RESET,	 1);
	gpio_put(MSX_LATCH_C,	 0);

	// 	・GPIO_0-7 をOUTに設定する
	for(int t = 0; t < 8; ++t) {
		gpio_set_dir(MSX_A0_D0 +t, GPIO_OUT);
	}
	// 	・DDIR <= L(B->A)
	gpio_put(MSX_DDIR,	 0);

	*pB = dt8;
#endif
	return true;
}

RAM_FUNC void t_OutOPLL(const uint16_t addr, const uint16_t data)
{
#ifdef MGSPICO_2ND
	gpio_put(MMM_AEX0, 0);
	for(int t = 0; t < 8; ++t) {
		gpio_put(MMM_D0 +t, (addr>>t)&0x01);
	}
	gpio_put(MMM_CSWR_FM, 0);
	busy_wait_us(1);
	gpio_put(MMM_CSWR_FM, 1);
	busy_wait_us(12);

	gpio_put(MMM_AEX0, 1);
	for(int t = 0; t < 8; ++t) {
		gpio_put(MMM_D0 +t, (data>>t)&0x01);
	}
	gpio_put(MMM_CSWR_FM, 0);
	busy_wait_us(1);
	gpio_put(MMM_CSWR_FM, 1);
	busy_wait_us(84);
#elif defined(MGSPICO_3RD)
	mmmspi::PushBuff(mmmspi::CMD::OPLL, addr, data);
#elif defined(MGSPICO_1ST)
	mgspico::t_OutPort(0x7C, (uint8_t)addr);
	busy_wait_us(4);
	mgspico::t_OutPort(0x7D, (uint8_t)data);
	busy_wait_us(24);
#endif
	return;
}

RAM_FUNC void t_OutPSG(const uint16_t addr, const uint16_t data)
{
#ifdef MGSPICO_2ND
	gpio_put(MMM_AEX0, 0);
	for(int t = 0; t < 8; ++t) {
		gpio_put(MMM_D0 +t, (addr>>t)&0x01);
	}
	gpio_put(MMM_CSWR_PSG, 0);
	busy_wait_us(1);
	gpio_put(MMM_CSWR_PSG, 1);
	busy_wait_us(1);

	gpio_put(MMM_AEX0, 1);
	for(int t = 0; t < 8; ++t) {
		gpio_put(MMM_D0 +t, (data>>t)&0x01);
	}
	gpio_put(MMM_CSWR_PSG, 0);
	busy_wait_us(1);
	gpio_put(MMM_CSWR_PSG, 1);
	busy_wait_us(1);

#elif defined(MGSPICO_3RD)
	mmmspi::PushBuff(mmmspi::CMD::PSG, addr, data);
#elif defined(MGSPICO_1ST)
	mgspico::t_OutPort(0xA0, (uint8_t)addr);
	busy_wait_us(1);
	mgspico::t_OutPort(0xA1, (uint8_t)data);
	busy_wait_us(1);
#endif
	return;
}

#include <stdio.h>
RAM_FUNC void t_OutSCC(const z80memaddr_t addrOrg, const uint16_t data)
{
#ifdef MGSPICO_2ND
	const uint32_t seg = addrOrg & 0xff00;
	gpio_put(MMM_ADDT_SCC, 1);	// ADDRESS
	uint32_t addr = 0x0000;
	switch(seg)
	{
		case 0x9000: gpio_put(MMM_AEX1, 0); gpio_put(MMM_AEX0, 0); addr = addrOrg; break;
		case 0x9800: gpio_put(MMM_AEX1, 0); gpio_put(MMM_AEX0, 1); addr = addrOrg; break;
		case 0xb800: gpio_put(MMM_AEX1, 1); gpio_put(MMM_AEX0, 0); addr = addrOrg; break;
		case 0xbf00: gpio_put(MMM_AEX1, 1); gpio_put(MMM_AEX0, 1); addr = addrOrg; break;
		case 0xb000: gpio_put(MMM_AEX1, 1); gpio_put(MMM_AEX0, 1); addr = 0xfd;	   break;
	}
	for(int t = 0; t < 8; ++t) {
		gpio_put(MMM_D0 +t, (addr>>t)&0x01);
	}
	busy_wait_us(1);

	gpio_put(MMM_ADDT_SCC, 0);	// DATA
	for(int t = 0; t < 8; ++t) {
		gpio_put(MMM_D0 +t, (data>>t)&0x01);
	}
	gpio_put(MMM_CSWR_SCC, 0);
	busy_wait_us(1);
	gpio_put(MMM_CSWR_SCC, 1);
	busy_wait_us(1);
#elif defined(MGSPICO_3RD)
	uint32_t addr = addrOrg & 0x00ff;
	switch(addrOrg & 0xff00)
	{
		case 0x9000: addr |= 0x0000; break;
		case 0x9800: addr |= 0x0100; break;
		case 0xb800: addr |= 0x0200; break;
		case 0xbf00: addr |= 0x0300; break;
		case 0xb000: addr |= 0x0400; break;
	}
	mmmspi::PushBuff(mmmspi::CMD::SCC, addr, data);
#elif defined(MGSPICO_1ST)
	mgspico::t_WriteMem(addrOrg, (uint8_t)data);
#endif
	return;
}

RAM_FUNC void t_OutVSYNC(const uint32_t cnt)
{
#ifdef MGSPICO_3RD
	mmmspi::PushBuff(mmmspi::CMD::VSYNC, 0x00, 0x00);
	mmmspi::Present();
#endif
	return;
}

RAM_FUNC void t_OutSelSccMod(const uint32_t mod)
{
#ifdef MGSPICO_3RD
	mmmspi::PushBuff(mmmspi::CMD::SEL_SCC_MODULE, 0x00, mod);
	mmmspi::Present();
#endif
	return;
}

RAM_FUNC void t_MuteOPLL()
{
	// 音量を0にする
	// それ以外のレジスタはいじらない
	// OPLL
	t_OutOPLL(0x30, 0x0F);	// Vol = 0
	t_OutOPLL(0x31, 0x0F);
	t_OutOPLL(0x32, 0x0F);
	t_OutOPLL(0x33, 0x0F);
	t_OutOPLL(0x34, 0x0F);
	t_OutOPLL(0x35, 0x0F);
	t_OutOPLL(0x36, 0x0F);
	t_OutOPLL(0x37, 0xFF);
	t_OutOPLL(0x38, 0xFF);
	return;
}

RAM_FUNC void t_MutePSG()
{
	t_OutPSG(0x08, 0x00);
	t_OutPSG(0x09, 0x00);
	t_OutPSG(0x0A, 0x00);
	return;
}

RAM_FUNC void t_MuteSCC()
{
	// SCC+
	t_OutSCC(0xbffe, 0x20);
	t_OutSCC(0xb000, 0x80);
	// 音量 vol=0
	for( z80memaddr_t addr = 0xb8aa; addr <= 0xb8ae; ++addr)
		t_OutSCC(addr, 0x00);
	// チャンネルイネーブルビット
	t_OutSCC(0xb8af, 0x00);	// turn off, CH.A-E
	// wave table data 再生速度
	for( z80memaddr_t addr = 0xb8a0; addr <= 0xb8a8; ++addr)
		t_OutSCC(addr, 0x00);
	// wave table data A,B,C,D/E
	for( z80memaddr_t addr = 0xb800; addr <= 0xb8bf; ++addr)
		t_OutSCC(addr, 0x00);

	// SCC
	t_OutSCC(0xbffe, 0x00);
	t_OutSCC(0x9000, 0x3f);
	// 音量 vol=0
	for( z80memaddr_t addr = 0x988a; addr <= 0x988e; ++addr)
		t_OutSCC(addr, 0x00);
	// チャンネルイネーブルビット
	t_OutSCC(0x988f, 0x00);	// turn off, CH.A-E
	// wave table data 再生速度
	for( z80memaddr_t addr = 0x9880; addr <= 0x9889; ++addr)
		t_OutSCC(addr, 0x00);
	// wave table data A,B,C,D/E
	for( z80memaddr_t addr = 0x9800; addr <= 0x987f; ++addr)
		t_OutSCC(addr, 0x00);

	return;
}

}; // namespace mgspico

