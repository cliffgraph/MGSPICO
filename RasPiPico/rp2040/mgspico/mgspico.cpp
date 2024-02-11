/**
 * MGSPICO (RaspberryPiPico firmware)
 * Copyright (c) 2024 Harumakkin.
 * SPDX-License-Identifier: MIT
 */
// https://spdx.org/licenses/

//#define FOR_DEGUG

#include <stdio.h>
#include <memory.h>
#include <stdint.h>
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include <hardware/flash.h>

#ifdef FOR_DEGUG
#include <hardware/clocks.h>
#endif

#include "global.h"
#include "ff.h"
#include "diskio.h"
#include "def_gpio.h"
#include "sdfat.h"		// for sd_fatReadFileFrom
#include "if_player.h"	// for IF_PLAYER_PICO
#include "CUTimeCount.h"
#include "HopStepZ/msxdef.h"
#include "HopStepZ/CHopStepZ.h"
#include "HopStepZ/CMsxMemSlotSystem.h"
#include "HopStepZ/CZ80MsxDos.h"
#include "mgs/mgs_tools.h"
#include "oled/oledssd1306.h"


struct INITGPTABLE {
	int gpno;
	int direction;
	bool bPullup;
	int	init_value;
};
 static const INITGPTABLE g_CartridgeMode_GpioTable[] = {
	{ MSX_A0_D0,	GPIO_OUT,	false, 0, },
	{ MSX_A1_D1,	GPIO_OUT,	false, 1, },
	{ MSX_A2_D2,	GPIO_OUT,	false, 1, },
	{ MSX_A3_D3,	GPIO_OUT,	false, 1, },
	{ MSX_A4_D4,	GPIO_OUT,	false, 1, },
	{ MSX_A5_D5,	GPIO_OUT,	false, 1, },
	{ MSX_A6_D6,	GPIO_OUT,	false, 1, },
	{ MSX_A7_D7,	GPIO_OUT,	false, 1, },
	{ MSX_A8_IORQ,	GPIO_OUT,	false, 1, },
	{ MSX_A9_MREQ,	GPIO_OUT,	false, 1, },
	{ MSX_A10_RW,	GPIO_OUT,	false, 1, },
	{ MSX_A11_RD,	GPIO_OUT,	false, 1, },
	{ MSX_A12_SLTSL,GPIO_OUT,	false, 1, },
	{ MSX_A13_C1,	GPIO_OUT,	false, 1, },
	{ MSX_A14_C12,	GPIO_OUT,	false, 1, },
	{ MSX_A15_RESET,GPIO_OUT,	false, 0, },	// RESET = L
	{ MSX_DDIR,		GPIO_OUT,	false, 0, },	// DDIR  = L(B->A)
	{ MSX_LATCH_A,	GPIO_OUT,	false, 1, },
	{ MSX_LATCH_C,	GPIO_OUT,	false, 1, },
	{ MSX_SW1,		GPIO_OUT,	false, 1, },
	{ MSX_SW2,		GPIO_IN,	true,  0, },
	{ MSX_SW3,		GPIO_IN,	true,  0, },
	{ MSX_SW3,		GPIO_IN,	true,  0, },
	{ GP_PICO_LED,	GPIO_OUT,	false, 1, },
 	{ -1,			0,			false, 0, },	// eot
};


static char tempWorkPath[255+1];
static bool g_bDiskAcc = true;


static void setupGpio(const INITGPTABLE pTable[] )
{
	for (int t = 0; pTable[t].gpno != -1; ++t) {
		const int no = pTable[t].gpno;
		gpio_init(no);
		//gpio_set_irq_enabled(no, 0xf, false);
		gpio_put(no, pTable[t].init_value);			// PIN方向を決める前に値をセットする
		gpio_set_dir(no, pTable[t].direction);
		if (pTable[t].bPullup)
		 	gpio_pull_up(no);
		else
		 	gpio_disable_pulls(no);
	}
	return;
}

static int g_PlayNo = 1;
static int g_CurNo = 1;
static int g_PageTopNo = 1;
static void changeCurPos(const int step)
{
	const int numFiles = g_MgsFiles.GetNumFiles();
	int old = g_CurNo;
	g_CurNo += step;
	if( g_CurNo <= 0 ) {
		g_CurNo = 1;
	}
	else if (numFiles < g_CurNo ) {
		g_CurNo = numFiles;
	}
	if( g_CurNo == 0 || old == g_CurNo )
		return;

	if( g_CurNo < g_PageTopNo )
		g_PageTopNo = g_CurNo;
	if( g_PageTopNo+2 < g_CurNo )
		g_PageTopNo = g_CurNo-2;

	return;
}

static void reloadPlay(CHopStepZ *pMsx, CMsxMemSlotSystem *pSlot, CZ80MsxDos *pCpu)
{

	pSlot->Write(0x4800+7, 0x00);			// .request_res
	pSlot->Write(0x4800+6, 0x01/*停止*/);	// .request_from_pico

	// 停止が受理されるまで待つ
	while( pSlot->Read(0x4800+7) == 0x00 );

	auto *p = g_WorkRam.GetPtrPage(0, 0);
	UINT readSize = 0;
	auto *pF = g_MgsFiles.GetFileSpec(g_PlayNo);
	if(sd_fatReadFileFrom(pF->name, MEM_16K_SIZE, p, &readSize) ) {
#ifdef FOR_DEGUG
		printf("%s\n", pF->name);
#endif
		pMsx->WriteMemory(0x8000, p, readSize);
	}

	pSlot->Write(0x4800+7, 0x00);			// .request_res
	pSlot->Write(0x4800+6, 0x02/*再生*/);

	g_bDiskAcc = true;
	return;
}

static void stopPlay(CHopStepZ *pMsx, CMsxMemSlotSystem *pSlot, CZ80MsxDos *pCpu)
{

	pSlot->Write(0x4800+7, 0x00);			// .request_res
	pSlot->Write(0x4800+6, 0x01/*停止*/);	// .request_from_pico

	// 待ち
	while( pSlot->Read(0x4800+7) == 0x00 );

	return;
}

static uint32_t timercnt = 0;
bool __time_critical_func(timerproc_fot_ff)(repeating_timer_t *rt)
{
	++timercnt;
	return true;
}
uint32_t __time_critical_func(GetTimerCounterMS)()
{
	return timercnt;
}

static void displayPlayFileName(
	CSsd1306I2c &oled, CHopStepZ *pMsx,
	CMsxMemSlotSystem *pSlot, CZ80MsxDos *pCpu)
{
	oled.Clear();

	if( g_MgsFiles.GetNumFiles() == 0 ) {
		const static char *pNo = " No MGS file.";
		oled.Strings(0, 1, pNo, strlen(pNo), true);
	}
	else {
		// ファイル名リスト
		for( int t = 0; t < 3; ++t) {
			int index = g_PageTopNo + t;
			auto *pF = g_MgsFiles.GetFileSpec(index);
			if( pF == nullptr )
				return;
			sprintf(tempWorkPath, "%03d:%s", index, pF->name);
			oled.Strings(0, 1+t, tempWorkPath, strlen(tempWorkPath), (g_CurNo==index)?true:false);
		}
	}
	return;
}

static bool displaySoundIndicator(
	CSsd1306I2c &oled, CHopStepZ *pMsx,
	CMsxMemSlotSystem *pSlot, CZ80MsxDos *pCpu,
	bool bForce)
{
	static const int NUM_TRACKS = 17;
	static const int HEIGHT = 9;	// バーの高さ
	static const int FLAME_W = 7;	// 領域の幅
	static const int BAR_W = 5;		// バーの幅
	bool bUpdatetd = bForce;
	static int waitc = 0;

	z80memaddr_t workAddr = pSlot->ReadWord(0x4800+11);	// .mgs_track_top
	uint16_t workSize = pSlot->ReadWord(0x4800+13);		// .mgs_track_size
	
	static int16_t oldCnt[NUM_TRACKS];
	static int8_t lvl[NUM_TRACKS] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	static int8_t oldLvl[NUM_TRACKS] = {7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7};
	int16_t cnt[NUM_TRACKS];
	for( int trk = 0; trk < NUM_TRACKS; ++trk ) { 
		cnt[trk] = pSlot->ReadWord(workAddr+workSize*trk+0x01);
		if( oldCnt[trk] < cnt[trk] ) {
			lvl[trk] = HEIGHT;
		}
		else if( cnt[trk] < 0 ) {
			lvl[trk] = 0;
		}
		oldCnt[trk] = cnt[trk];
		if( oldLvl[trk] != lvl[trk] || bForce){
			oldLvl[trk] = lvl[trk];
			bUpdatetd = true;
			auto x = trk*FLAME_W;
			int y;
			for(y = 0; y < HEIGHT; ++y ){
				oled.Line(x, y, x+BAR_W, y, ((HEIGHT-y)<=lvl[trk])?true:false);
			}
			oled.Line(x, y, x+BAR_W, y, true);
		}
		if( 0 < lvl[trk] && 2 < waitc++ ) {
			lvl[trk]--;
			waitc = 0;
		}
	}
	return bUpdatetd;
}

static CHopStepZ *g_pMsx;

static void Core1Task()
{
	for(;;) {
		g_pMsx->RunStage2();
		static uint8_t lamp = 0;
		gpio_put(GP_PICO_LED, (((++lamp)>>7)&0x01));
	}
	return;
}

enum SWINDEX : int 
{
	SWINDEX_SW1,
	SWINDEX_SW2,
	SWINDEX_SW3,
	SWINDEX_SW_NUM,
};
static bool checkSw(SWINDEX swIndex)
{
	static const uint32_t swtable[SWINDEX_SW_NUM] = {MSX_SW1, MSX_SW2, MSX_SW3};
	static uint32_t timerCnts[SWINDEX_SW_NUM] = {0,0,0};
	static bool swSts[SWINDEX_SW_NUM] = {false, false, false};
	if( 10 < GetTimerCounterMS() - timerCnts[swIndex] ) {
		bool sts = gpio_get(swtable[swIndex]);
		if( swSts[swIndex] != sts ) {
			swSts[swIndex] = sts;
			timerCnts[swIndex] = GetTimerCounterMS();
		}
	}
	return swSts[swIndex];
}

int main()
{
	setupGpio(g_CartridgeMode_GpioTable);
	sleep_ms(1);
	gpio_put(MSX_A15_RESET, 1);	// RESET = H
	sleep_us(1);
	gpio_put(MSX_LATCH_C, 0);	// LATCH_C = L	// 制御ラインを現状でラッチする

	static repeating_timer_t tim;
	add_repeating_timer_ms (1/*ms*/, timerproc_fot_ff, nullptr, &tim);

#ifdef FOR_DEGUG
	stdio_init_all();
#endif

	CSsd1306I2c oled;
	oled.Start();
	oled.Clear();
	oled.Strings(1, 1, "MGSPICO v1.0", 12, false);
	oled.Strings(1, 2, "by hrumakkin", 12, false);
	oled.Box(4, 14, 100, 16, true);
	oled.Present();
	
	disk_initialize(0);
	sleep_ms(1000);
#ifdef FOR_DEGUG
	printf("MGSPICO by harumakkin.2024\n");
#endif
	g_pMsx = GCC_NEW CHopStepZ();
	g_pMsx->Setup();
	CMsxMemSlotSystem *pSlot;
	CZ80MsxDos *pCpu;
	g_pMsx->GetSubSystems(&pSlot, &pCpu);
	for(size_t t = 0; t < sizeof(IF_PLAYER_PICO); ++t) {
		pSlot->Write(0x4800+t, 0x00);
	}

	g_MgsFiles.ReadFileNames();

	UINT readSize = 0;
	auto *p = g_WorkRam.GetPtrPage(0, 0);

	// MGSDRV の本体部分を、0x6000 へ読み込む
	sprintf(tempWorkPath, "%s", "MGSDRV.COM");
	if(!sd_fatReadFileFrom(tempWorkPath, MEM_16K_SIZE, p, &readSize) ) {
		oled.Start();
		oled.Clear();
		oled.Strings(0, 1, "Not found", 9, false);
		oled.Strings(0, 2, tempWorkPath, strlen(tempWorkPath), false);
		oled.Present();
#ifdef FOR_DEGUG
		printf("not found %s\n", tempWorkPath);
#endif
		return -1;
	}
	printf("loaded %s, size = %d bytes\n", tempWorkPath, readSize);
	const uint8_t *pBody;
	uint16_t bodySize;
	if( t_Mgs_GetPtrBodyAndSize(reinterpret_cast<const STR_MGSDRVCOM*>(p), &pBody, &bodySize) ) {
		g_pMsx->WriteMemory(0x6000, pBody, bodySize);
	}

	// PLAYERS.COMを0x4000へ読み込んで実行する
	sprintf(tempWorkPath, "%s", "PLAYERS.COM");
	if( !sd_fatReadFileFrom(tempWorkPath, MEM_16K_SIZE, p, &readSize) ) {
		oled.Start();
		oled.Clear();
		oled.Strings(0, 1, "Not found", 9, false);
		oled.Strings(0, 2, tempWorkPath, strlen(tempWorkPath), false);
		oled.Present();
#ifdef FOR_DEGUG
		printf("loaded %s, size = %d bytes\n", tempWorkPath, readSize);
#endif
		return -1;
	}
	printf("loaded %s, size = %d bytes\n", tempWorkPath, readSize);
	g_pMsx->WriteMemory(0x4000, p, readSize);
	g_pMsx->RunStage1(0x4000, 0x5FFF);

	multicore_launch_core1(Core1Task);

	bool sw[3] = {true,true,true,};
	bool bPlaying = false;
	for(;;) {
		bool sw1 = checkSw(SWINDEX_SW1);
		bool sw2 = checkSw(SWINDEX_SW2);
		bool sw3 = checkSw(SWINDEX_SW3);
		if( sw[0] != sw1) {
			sw[0] = sw1;
			if( !sw1 && 0 < g_MgsFiles.GetNumFiles()) {
				if( g_PlayNo != g_CurNo ){
					g_PlayNo = g_CurNo;
					reloadPlay(g_pMsx, pSlot, pCpu);
					bPlaying = true;
				}
				else{
					bPlaying ^= true;
					if( bPlaying ) {
						reloadPlay(g_pMsx, pSlot, pCpu);
					}
					else {
						stopPlay(g_pMsx, pSlot, pCpu);
					}
				}
			}
		}
		if( sw[1] != sw2) {
			sw[1] = sw2;
			if( !sw2 ) {
				changeCurPos(+1);
				bPlaying = true;
			}
		}
		if( sw[2] != sw3) {
			sw[2] = sw3;
			if( !sw3 ) {
				changeCurPos(-1);
				bPlaying = true;
			}
		}

		bool bUpdatetd = false;
		static int oldCurNo = -1;
		if( oldCurNo != g_CurNo ) {
			oldCurNo = g_CurNo;
			displayPlayFileName(oled, g_pMsx, pSlot, pCpu);
			bUpdatetd = true;
		}
		bUpdatetd = displaySoundIndicator(oled, g_pMsx, pSlot, pCpu, bUpdatetd);
		if( bUpdatetd ) {
			if( g_bDiskAcc ) {
				oled.Start();
				g_bDiskAcc = false;
			}
			oled.Present();
		}

#ifdef FOR_DEGUG
		z80memaddr_t mibAddr = pSlot->ReadWord(0x4800+9);	// .mgs_mib_addr
		static uint8_t oldCnt[2] = {255, 255};
		uint8_t cnt1 = pSlot->Read(mibAddr+5);
		uint8_t cnt2 = pSlot->Read(mibAddr+8);
		if( oldCnt[0] != cnt1 || oldCnt[1] != cnt2){
			oldCnt[0] = cnt1;
			oldCnt[1] = cnt2;
			for( int t = 0; t < 16; ++t){
				printf("%02X ", pSlot->Read(mibAddr+t));
			}
			printf("\n");
		}
#endif
	}
	return 0;
}




