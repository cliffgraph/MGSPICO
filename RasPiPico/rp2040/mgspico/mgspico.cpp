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
#include "oled/SOUNDLOGO.h"
 #include "MgsFiles.h"


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


static uint8_t g_WorkRam[Z80_PAGE_SIZE];
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

static void changeCurPos(const int numFiles, int *pPageTopNo, int *pCurNo, const int step)
{
	int old = *pCurNo;
	*pCurNo += step;
	if( *pCurNo <= 0 ) {
		*pCurNo = 1;
	}
	else if (numFiles < *pCurNo ) {
		*pCurNo = numFiles;
	}
	if( *pCurNo == 0 || old == *pCurNo )
		return;

	if( *pCurNo < *pPageTopNo )
		*pPageTopNo = *pCurNo;
	if( *pPageTopNo+2 < *pCurNo )
		*pPageTopNo = *pCurNo-2;

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

static void stopPlay(CHopStepZ &msx)
{
	// MGS再生を停止指示し、停止するまで待つ

	msx.WriteMemory(0x4800+7, 0x00);			// .request_res
	msx.WriteMemory(0x4800+6, 0x01/*停止*/);	// .request_from_pico
	// 待ち
	while( msx.ReadMemory(0x4800+7) == 0x00 );

	return;
}

static void reloadPlay(CHopStepZ &msx, const MgsFiles::FILESPEC &f)
{
	// MGS再生を停止指示し、停止するまで待つ
	stopPlay(msx);

	// メモリ8000hにMGSファイルを読み込む
	auto *p = g_WorkRam;
	UINT readSize = 0;
	if(sd_fatReadFileFrom(f.name, Z80_PAGE_SIZE, p, &readSize) ) {
		msx.WriteMemory(0x8000, p, readSize);
	}

	// 再生を指示する
	msx.WriteMemory(0x4800+7, 0x00);			// .request_res
	msx.WriteMemory(0x4800+6, 0x02/*再生*/);

	g_bDiskAcc = true;
	return;
}


static bool displayPlayFileName(
	CSsd1306I2c &oled,
	const MgsFiles &mgsf,
	const int pageTopNo, const int seleFileNo)
{
	if( mgsf.IsEmpty() ) {
		const static char *pNo = " No MGS file.";
		oled.Strings8x16(0, 1*16, pNo, true);
	}
	else {
		// ファイル名リスト
		for( int t = 0; t < 3; ++t) {
			int index = pageTopNo + t;
			auto *pF = mgsf.GetFileSpec(index);
			if( pF == nullptr )
				continue;
			sprintf(tempWorkPath, "%03d:%s", index, pF->name);
			oled.Strings8x16(0, (1+t)*16, tempWorkPath, (seleFileNo==index)?true:false);
		}
	}
	return true;
}

static bool displayPlayTime(
	CSsd1306I2c &oled,
	const MgsFiles &mgsf,
	const uint32_t timev, const int playingFileNo, const bool bForce)
{
	static uint32_t oldTime = 0;
	bool bUpdated = false;
	const uint32_t maxv = (99*60+59)*1000;
	const uint32_t pt = ((maxv < timev) ? maxv : timev ) / 1000;
	if( oldTime != pt || bForce ) {
		oldTime = pt;
		const int s = pt % 60;
		const int m = pt / 60;
		sprintf(tempWorkPath, "%02d:%02d", m, s);
		oled.Strings16x16(40, 5*8, tempWorkPath);
		bUpdated = true;
	}
	if( bUpdated && !mgsf.IsEmpty() ) {
		const int index = playingFileNo;
		auto *pF = mgsf.GetFileSpec(index);
		sprintf(tempWorkPath, "%03d:%*s", index, LEN_FILE_NAME, pF->name);
		oled.Strings8x16(0, 1*16, tempWorkPath, false);
	}

	return bUpdated;
}

static bool displaySoundIndicator(
	CSsd1306I2c &oled, CHopStepZ &msx, bool bForce)
{
	static const int NUM_TRACKS = 17;
	static const int HEIGHT = 9;	// バーの高さ
	static const int FLAME_W = 7;	// 領域の幅
	static const int BAR_W = 5;		// バーの幅
	bool bUpdatetd = false;
	static int waitc = 0;

	z80memaddr_t workAddr = msx.ReadMemoryWord(0x4800+11);	// .mgs_track_top
	uint16_t workSize = msx.ReadMemoryWord(0x4800+13);		// .mgs_track_size
	
	static int16_t oldCnt[NUM_TRACKS];
	static int8_t lvl[NUM_TRACKS] =
		{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	static int8_t oldLvl[NUM_TRACKS] =
		{HEIGHT,HEIGHT,HEIGHT,HEIGHT,HEIGHT,HEIGHT,HEIGHT,HEIGHT,
		HEIGHT,HEIGHT,HEIGHT,HEIGHT,HEIGHT,HEIGHT,HEIGHT,HEIGHT,HEIGHT};
	int16_t cnt[NUM_TRACKS];
	for( int trk = 0; trk < NUM_TRACKS; ++trk ) { 
		cnt[trk] = msx.ReadMemoryWord(workAddr+workSize*trk+0x01);
		// key-on/offを判断してレベル値を決める
		if( oldCnt[trk] < cnt[trk] ) {
			lvl[trk] = HEIGHT;	// key-on
		}
		else if( cnt[trk] < 0 ) {
			lvl[trk] = 0;		// key-off
		}
		oldCnt[trk] = cnt[trk];
		// レベル値が変化していたらバーを描画する
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
		// レベル値を下降させる
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

static void displayNotFound(CSsd1306I2c &oled, const char *pPathName)
{
	oled.Start();
	oled.Clear();
	oled.Strings8x16(0*8, 1*16, "Not found", false);
	oled.Strings8x16(0*8, 2*16, pPathName, false);
	oled.Present();
	#ifdef FOR_DEGUG
		printf("not found %s\n", pPathName);
	#endif
	return;
}

static void displayMusicLogo(CSsd1306I2c &oled, CHopStepZ &msx)
{
	z80memaddr_t mibAddr = msx.ReadMemoryWord(0x4800+9);	// .mgs_mib_addr
	bool bOPLL = (msx.ReadMemory(mibAddr+0)==0xFF)?false:true;
	bool bSCC = (msx.ReadMemory(mibAddr+1)==0xFF)?false:true;
	if( bOPLL || bSCC ){
		oled.Start();
		oled.Clear();
		if( bOPLL )
			oled.Bitmap(4, 8, MSX_MUSIC_LOGO_32x32_BITMAP, MSX_MUSIC_LOGO_LX, MSX_MUSIC_LOGO_LY);
		if( bSCC )
			oled.Bitmap(40, 8, SCC_80x32_BITMAP, SCC_LOGO_LX, SCC_LOGO_LY);
		oled.Present();
		sleep_ms(1500);
	}
}

static void printMIB()
{
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
};


// 画面遷移の管理
enum DISPLAY_STATE {
	DISPSTS_FILELIST_PRE,
	DISPSTS_FILELIST,
	DISPSTS_PLAY_PRE,
	DISPSTS_PLAY,
};

int main()
{
	//set_sys_clock_khz(240*1000, true);		// OK;300,280,250,240,200,180,170,140,100
	setupGpio(g_CartridgeMode_GpioTable);

	sleep_ms(1);
	gpio_put(MSX_A15_RESET, 1);	// /RESET = H
	sleep_us(1);
	gpio_put(MSX_LATCH_C, 0);	// LATCH_C = L	// 制御ラインを現状でラッチする

	static repeating_timer_t tim;
	add_repeating_timer_ms (1/*ms*/, timerproc_fot_ff, nullptr, &tim);

	#ifdef FOR_DEGUG
		stdio_init_all();
	#endif

	// OLED表示の準備とタイトルの表示
	CSsd1306I2c oled;
	oled.Start();
	oled.Clear();
	oled.Strings8x16(1*8+4, 1*16, "MGSPICO v1.1", false);
	oled.Strings8x16(1*8+4, 2*16, "by harumakkin", false);
	oled.Box(4, 14, 108, 16, true);
	oled.Present();

	// タイトル表示中SW2が押されてるかwチェック -> bForceOpll
	bool bForceOpll = !gpio_get(MSX_SW2);
	sleep_ms(900);
	bForceOpll &= !gpio_get(MSX_SW2);
	#ifdef FOR_DEGUG
		printf("MGSPICO by harumakkin.2024\n");
	#endif

	// エミュレータのセットアップ
	g_pMsx = GCC_NEW CHopStepZ();
	auto &msx = *g_pMsx;
	msx.Setup(bForceOpll);

	// PLAYERとの通信領域を0クリア
	for(size_t t = 0; t < sizeof(IF_PLAYER_PICO); ++t) {
		msx.WriteMemory(0x4800+t, 0x00);
	}

	// MGSファイル名をリストアップ - > mgsf
	disk_initialize(0);
	MgsFiles *pMgsFiles = GCC_NEW MgsFiles();
	auto &mgsf = *pMgsFiles;
	mgsf.ReadFileNames();

	// MGSDRV の本体部分を、0x6000 へ読み込む
	UINT readSize = 0;
	auto *p = g_WorkRam;
	sprintf(tempWorkPath, "%s", "MGSDRV.COM");
	if(!sd_fatReadFileFrom(tempWorkPath, Z80_PAGE_SIZE, p, &readSize) ) {
		displayNotFound(oled, tempWorkPath);
		return -1;
	}
	const uint8_t *pBody;
	uint16_t bodySize;
	if( t_Mgs_GetPtrBodyAndSize(reinterpret_cast<const STR_MGSDRVCOM*>(p), &pBody, &bodySize) ) {
		msx.WriteMemory(0x6000, pBody, bodySize);
	}

	// PLAYERS.COMを0x4000へ読み込んで実行する
	sprintf(tempWorkPath, "%s", "PLAYERS.COM");
	if( !sd_fatReadFileFrom(tempWorkPath, Z80_PAGE_SIZE, p, &readSize) ) {
		displayNotFound(oled, tempWorkPath);
		return -1;
	}
	msx.WriteMemory(0x4000, p, readSize);
	msx.RunStage1(0x4000, 0x5FFF);

	// 以降、エミュレータはCore1で動かす
	multicore_launch_core1(Core1Task);
	sleep_ms(100);	// MGSDRVが音源を検出するまでの時間稼ぎ

	// MGSDRVが検出した音源(ロゴ)を表示する
	displayMusicLogo(oled, msx);

	// 
	DISPLAY_STATE displaySts = DISPSTS_FILELIST_PRE;
	int seleFileNo = 1;					// 選択ファイル番号
	int pageTopNo = 1;					// 表示されているリストの先頭の番号
	uint32_t playStartTime = 0;			// 再生開始時の時刻
	uint32_t dispListStartTime = 0;		// リスト表示に切り替わった時刻
	uint32_t playTime = 0;				// 再生経過時間
	int playingFileNo = 1;				// 再生ファイル番号
	bool bPlaying = false;				// 再生中

	bool oldSw1 = true;
	bool oldSw2 = true;
	bool oldSw3 = true;
	int oldCurNo = -1;
	for(;;) {
		if( !mgsf.IsEmpty() ) {
			const bool sw1 = checkSw(SWINDEX_SW1);
			const bool sw2 = checkSw(SWINDEX_SW2);
			const bool sw3 = checkSw(SWINDEX_SW3);
			// [●]
			if( oldSw1 != sw1) {
				oldSw1 = sw1;
				if( !sw1 ) {
					bPlaying ^= true;
					if( bPlaying || displaySts==DISPSTS_FILELIST) {
						playingFileNo = seleFileNo;
						reloadPlay(msx, *mgsf.GetFileSpec(playingFileNo));
						displaySts = DISPSTS_PLAY_PRE;
						playStartTime = GetTimerCounterMS();
						bPlaying = true;
					}
					else {
						stopPlay(msx);
						displaySts = DISPSTS_FILELIST_PRE;
					}
				}
			}
			// [▼]
			if( oldSw2 != sw2) {
				oldSw2 = sw2;
				if( !sw2 ) {
					changeCurPos(mgsf.GetNumFiles(), &pageTopNo, &seleFileNo, +1);
					if( displaySts != DISPSTS_FILELIST )
						displaySts = DISPSTS_FILELIST_PRE;
				}
			}
			// [▲]
			if( oldSw3 != sw3) {
				oldSw3 = sw3;
				if( !sw3 ) {
					changeCurPos(mgsf.GetNumFiles(), &pageTopNo, &seleFileNo, -1);
					if( displaySts != DISPSTS_FILELIST )
						displaySts = DISPSTS_FILELIST_PRE;
				}
			}
		}

		bool bUpdatetd = false;
		switch(displaySts)
		{
			case DISPSTS_FILELIST_PRE:
				oled.Clear();
				bUpdatetd |= displaySoundIndicator(oled, msx, true);
				bUpdatetd |= displayPlayFileName(oled, mgsf, pageTopNo, seleFileNo);
				dispListStartTime = GetTimerCounterMS();
				displaySts = DISPSTS_FILELIST;
				bUpdatetd = true;
				break;
			case DISPSTS_FILELIST:
				if( bPlaying && 2000 < (GetTimerCounterMS()-dispListStartTime)) {
					displaySts = DISPSTS_PLAY_PRE;
				}
				if( oldCurNo != seleFileNo ) {
					oldCurNo = seleFileNo;
					bUpdatetd |= displayPlayFileName(oled, mgsf, pageTopNo, seleFileNo);
					if( bPlaying ) {
						dispListStartTime = GetTimerCounterMS();
					}
				}
				bUpdatetd |= displaySoundIndicator(oled, msx, bUpdatetd);
				break;
			case DISPSTS_PLAY_PRE:
				oled.Clear();
				oled.Bitmap(24, 5*8, PLAY_8x16_BITMAP, PLAY_LX, PLAY_LY);
				playTime = GetTimerCounterMS() - playStartTime;
				bUpdatetd |= displayPlayTime(oled, mgsf, playTime, playingFileNo, true);
				bUpdatetd |= displaySoundIndicator(oled, msx, true);
				displaySts = DISPSTS_PLAY;
				break;
			case DISPSTS_PLAY:
				playTime = GetTimerCounterMS() - playStartTime;
				bUpdatetd |= displayPlayTime(oled, mgsf, playTime, playingFileNo, false);
				bUpdatetd |= displaySoundIndicator(oled, msx, false);
				break;
		}
		// OLEDの表示更新
		if( bUpdatetd ) {
			if( g_bDiskAcc ) {
				// microSDのSPIとOLEDのI2Cは同じGPIOを共用しているので、
				// ファイルのアクセス後ではI2Cを初期化する必要がある oled.Start()
				oled.Start();
				g_bDiskAcc = false;
			}
			oled.Present();
		}

		// MIB領域(in DEBUG)
		printMIB();
	}
	return 0;
}




