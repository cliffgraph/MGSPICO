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

// 動作の管理
enum REQACT_STATE {
	REQACT_NONE,
	REQACT_WAIT_FOR_PLAY,	// 演奏開始前に１秒の間を置く
	REQACT_PLAY_MUSIC,
	REQACT_STOP_MUSIC,
	REQACT_PLAY_NEXT_MUSIC,
};

// 画面遷移の管理
enum DISPLAY_STATE {
	DISPSTS_FILELIST_PRE,
	DISPSTS_FILELIST,
	DISPSTS_PLAY_PRE,
	DISPSTS_PLAY,
};

enum MUSDRV {
	MUSDRV_MGS,		// MGSDRV.COM
	MUSDRV_KIN5,	// MuSICA(KINROU5.DRV)
};

struct INITGPTABLE {
	int gpno;
	int direction;
	bool bPullup;
	int	init_value;
};
 static const INITGPTABLE g_CartridgeMode_GpioTable[] = {
	{ MSX_A0_D0,	GPIO_OUT,	false, 1, },
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
		// if( pTable[t].direction == GPIO_OUT ){
		// 	gpio_set_drive_strength(no, GPIO_DRIVE_STRENGTH_2MA);
		//}
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

static void fadeoutPlay(CHopStepZ &msx)
{
	msx.WriteMemory(0x4800+7, 0x00);			// .request_res
	msx.WriteMemory(0x4800+6, 0x04/*FADEOUT*/);	// .request_from_pico
	// 待ち
	while( msx.ReadMemory(0x4800+7) == 0x00 );

	return;
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
		const static char *pNo = " No Data file.";
		oled.Strings8x16(0, 1*16, pNo, true);
	}
	else {
		// ファイル名リスト
		for( int t = 0; t < 3; ++t) {
			int index = pageTopNo + t;
			auto *pF = mgsf.GetFileSpec(index);
			if( pF == nullptr )
				continue;
			sprintf(tempWorkPath, "%03d:%-*s", index, LEN_FILE_NAME, pF->name);
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


struct INDICATOR
{
	static const int NUM_TRACKS = 17;
	static const int HEIGHT = 9;	// バーの高さ
	static const int FLAME_W = 7;	// 領域の幅
	static const int BAR_W = 5;		// バーの幅
	int16_t OldCnt[NUM_TRACKS];
	int8_t Lvl[NUM_TRACKS];
	int8_t OldLvl[NUM_TRACKS];
	z80memaddr_t WorkAddr;
	uint16_t WorkSize;
	z80memaddr_t AddrKey;
	void Setup(CHopStepZ &msx)
	{
		for( int t = 0; t < NUM_TRACKS; ++t) {
			Lvl[t] = 0;
			OldLvl[t] = HEIGHT;
		}
		WorkAddr = msx.ReadMemoryWord(0x4800+11);	// .work_track_top
		WorkSize = msx.ReadMemoryWord(0x4800+13);	// .work_track_size
	};
	z80memaddr_t GetKeyStateAddr(int trk)
	{
		return WorkAddr + WorkSize * trk;
	}
};

INDICATOR g_Indi;

static bool displaySoundIndicator(
	CSsd1306I2c &oled, CHopStepZ &msx, bool bForce,
	const MUSDRV musDrv, bool *pChangedSts)
{
	bool bUpdatedBar = false;
	static int waitc = 0;

	int16_t cnt[g_Indi.NUM_TRACKS];
	for( int trk = 0; trk < g_Indi.NUM_TRACKS; ++trk ) { 
		z80memaddr_t addr = g_Indi.GetKeyStateAddr(trk);
		if( musDrv == MUSDRV_MGS ) {
			cnt[trk] = msx.ReadMemoryWord(addr + 0x0001);
		}
		else {
			cnt[trk] = msx.ReadMemory(addr + 0x000d) & 0x80;
		}
		// key-on/offを判断してレベル値を決める
		if( g_Indi.OldCnt[trk] < cnt[trk] ) {
			g_Indi.Lvl[trk] = g_Indi.HEIGHT;	// key-on
		}
		else if( cnt[trk] < 0 ) {
			g_Indi.Lvl[trk] = 0;	// key-off
		}
		if (g_Indi.OldCnt[trk] != cnt[trk]) {
			g_Indi.OldCnt[trk] = cnt[trk];
			*pChangedSts = true;
		}
		// レベル値が変化していたらバーを描画する
		if( g_Indi.OldLvl[trk] != g_Indi.Lvl[trk] || bForce){
			g_Indi.OldLvl[trk] = g_Indi.Lvl[trk];
			bUpdatedBar = true;
			auto x = trk*g_Indi.FLAME_W;
			int y;
			for(y = 0; y < g_Indi.HEIGHT; ++y ){
				oled.Line(x, y, x+g_Indi.BAR_W, y, ((g_Indi.HEIGHT-y)<=g_Indi.Lvl[trk])?true:false);
			}
			oled.Line(x, y, x+g_Indi.BAR_W, y, true);
		}
		// レベル値を下降させる
		if( 0 < g_Indi.Lvl[trk] && 2 < waitc++ ) {
			g_Indi.Lvl[trk]--;
			waitc = 0;
		}
	}
	return bUpdatedBar;
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

static void displayMusicLogo(CSsd1306I2c &oled, CHopStepZ &msx, const MUSDRV musDrv)
{
	z80memaddr_t mibAddr = msx.ReadMemoryWord(0x4800+9);	// .mgs_mib_addr
	uint8_t NOTFOUND = (musDrv==MUSDRV_MGS)?0xFF:0x00;
	const bool bOPLL = (msx.ReadMemory(mibAddr+0)==NOTFOUND)?false:true;
	const bool bSCC = (msx.ReadMemory(mibAddr+1)==NOTFOUND)?false:true;
	if( bOPLL || bSCC ){
		oled.Start();
		oled.Clear();
		if( bOPLL )
			oled.Bitmap(4, 8, MSX_MUSIC_LOGO_32x32_BITMAP, MSX_MUSIC_LOGO_LX, MSX_MUSIC_LOGO_LY);
		if( bSCC )
			oled.Bitmap(40, 8, SCC_80x32_BITMAP, SCC_LOGO_LX, SCC_LOGO_LY);
		oled.Present();
		busy_wait_ms(1500);
	}
}

static void printMIB(CHopStepZ &msx, const MUSDRV musDrv)
{
#if defined(FOR_DEGUG) 
	if( musDrv == MUSDRV_MGS ) {
		z80memaddr_t mibAddr = msx.ReadMemoryWord(0x4800+9);	// .mgs_mib_addr
		static uint8_t oldCnt[2] = {255, 255};
		uint8_t cnt1 = msx.ReadMemory(mibAddr+5);
		uint8_t cnt2 = msx.ReadMemory(mibAddr+8);
		if( oldCnt[0] != cnt1 || oldCnt[1] != cnt2){
			oldCnt[0] = cnt1;
			oldCnt[1] = cnt2;
			for( int t = 0; t < 16; ++t){
				printf("%02X ", msx.ReadMemory(mibAddr+t));
			}
			printf("\n");
		}
	}
#endif
};

int main()
{
	//set_sys_clock_khz(240*1000, true);		// OK;300,280,250,240,200,180,170,140,100
	setupGpio(g_CartridgeMode_GpioTable);		// RESET信号

	static repeating_timer_t tim;
	add_repeating_timer_ms (1/*ms*/, timerproc_fot_ff, nullptr, &tim);

	#ifdef FOR_DEGUG
		stdio_init_all();
	#endif

	// タイトル表示中SW2が押されてるかチェック -> bForceOpll
	bool bForceOpll = !gpio_get(MSX_SW2);
	bool bMuSICA = !gpio_get(MSX_SW1);
	busy_wait_ms(100);
	bForceOpll &= !gpio_get(MSX_SW2);
	bMuSICA &= !gpio_get(MSX_SW1);
	MUSDRV musDrv = (bMuSICA)?MUSDRV_KIN5:MUSDRV_MGS;

	// OLED表示の準備とタイトルの表示
	CSsd1306I2c oled;
	oled.Start();
	oled.Clear();
	oled.Strings8x16(1*8+4, 1*16, "MGSPICO v1.4", false);
	oled.Strings8x16(1*8+4, 2*16, "by harumakkin", false);
	oled.Box(4, 14, 108, 16, true);
	const char *pForDrv = (musDrv==MUSDRV_MGS)?"for MGS":"for MuSICA";
	oled.Strings8x16(1*8+4, 3*16, pForDrv, false);
	oled.Present();

	busy_wait_ms(800);

	#ifdef FOR_DEGUG
		printf("MGSPICO by harumakkin.2024\n");
	#endif

	gpio_put(MSX_A15_RESET, 1);		// RESET信号を解除
	busy_wait_us(1);
	gpio_put(MSX_LATCH_C, 0);	// LATCH_C = L	// 制御ラインを現状でラッチする

	while(!gpio_get(MSX_SW1) ||!gpio_get(MSX_SW2));	// SWが解放されるまで待つ


	// エミュレータのセットアップ
	g_pMsx = GCC_NEW CHopStepZ();
	auto &msx = *g_pMsx;
	msx.Setup(bForceOpll, (musDrv==MUSDRV_KIN5)?true:false);

	// PLAYERとの通信領域を0クリア
	for(size_t t = 0; t < sizeof(IF_PLAYER_PICO); ++t) {
		msx.WriteMemory(0x4800+t, 0x00);
	}

	// MGSファイル名をリストアップ - > mgsf
	disk_initialize(0);
	MgsFiles *pMgsFiles = GCC_NEW MgsFiles();
	auto &mgsf = *pMgsFiles;
	const char *pWildCard = (musDrv==MUSDRV_MGS)?"*.MGS":"*.BGM";
	mgsf.ReadFileNames(pWildCard);

	// MGSDRV/KINROU5 の本体部分を、0x6000 へ読み込む
	UINT readSize = 0;
	auto *p = g_WorkRam;
	if( musDrv == MUSDRV_MGS ) {
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
	}
	else {
		sprintf(tempWorkPath, "%s", "KINROU5.DRV");
		if(!sd_fatReadFileFrom(tempWorkPath, Z80_PAGE_SIZE, p, &readSize) ) {
			displayNotFound(oled, tempWorkPath);
			return -1;
		}
		msx.WriteMemory(0x6000, p+7, readSize-7);
	}

	// PLAYERS.COMを0x4000へ読み込んで実行する
	const char *pPlayer = (musDrv==MUSDRV_MGS)?"PLAYERS.COM":"PLAYERSK.COM";
	sprintf(tempWorkPath, "%s", pPlayer);
	if( !sd_fatReadFileFrom(tempWorkPath, Z80_PAGE_SIZE, p, &readSize) ) {
		displayNotFound(oled, tempWorkPath);
		return -1;
	}
	msx.WriteMemory(0x4000, p, readSize);
	msx.RunStage1(0x4000, 0x5FFF);

	// 以降、エミュレータはCore1で動かす
	multicore_launch_core1(Core1Task);
	busy_wait_ms(100);	// ドライバが音源を検出するまでの時間稼ぎ

	// ドライバが検出した音源のロゴを表示する
	displayMusicLogo(oled, msx, musDrv);

	// レベルインジケーター表示の前準備
	g_Indi.Setup(msx);

	// 
	REQACT_STATE requestAct = REQACT_NONE;
	DISPLAY_STATE displaySts = DISPSTS_FILELIST_PRE;
	int seleFileNo = 1;					// 選択ファイル番号
	int pageTopNo = 1;					// 表示されているリストの先頭の番号
	uint32_t playStartTime = 0;			// 再生開始時の時刻
	uint32_t dispListStartTime = 0;		// リスト表示に切り替わった時刻
	uint32_t playTime = 0;				// 再生経過時間
	uint32_t waitTime = 0;	
	uint32_t silentTime = 0;			// 無音時間検出の時間カウンタ
	int playingFileNo = 1;				// 再生ファイル番号
	bool bPlaying = false;				// 再生中

	bool oldSw1 = true;
	bool oldSw2 = true;
	bool oldSw3 = true;
	int oldCurNo = -1;
	for(;;) {
		const uint32_t nowTime = GetTimerCounterMS();
		if( !mgsf.IsEmpty() ) {
			const bool sw1 = checkSw(SWINDEX_SW1);
			const bool sw2 = checkSw(SWINDEX_SW2);
			const bool sw3 = checkSw(SWINDEX_SW3);
			// [●]
			if( oldSw1 != sw1) {
				oldSw1 = sw1;
				if( !sw1 ) {
					requestAct = 
						((displaySts==DISPSTS_PLAY&&(bPlaying^true)) || displaySts==DISPSTS_FILELIST)
						? REQACT_PLAY_MUSIC : REQACT_STOP_MUSIC;
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

		switch(requestAct)
		{
			case REQACT_PLAY_NEXT_MUSIC:
			{
				bPlaying = false;
				msx.WriteMemory(0x4800+8,(uint8_t)STATUSOFPLAYER::IDLE);
				int temp = seleFileNo;
				changeCurPos(mgsf.GetNumFiles(), &pageTopNo, &seleFileNo, +1);
				if( temp != 1 && temp == seleFileNo )
					seleFileNo = 1;
				requestAct = REQACT_WAIT_FOR_PLAY;
				waitTime = nowTime;
				break;
			}
			// 一定時間後に
			case REQACT_WAIT_FOR_PLAY:
			{
				if( 1000 < (nowTime-waitTime) ){
					requestAct = REQACT_PLAY_MUSIC;
				}
				break;
			}
			// seleFileNoが示すデータを即再生する
			case REQACT_PLAY_MUSIC:
			{
				playingFileNo = seleFileNo;
				reloadPlay(msx, *mgsf.GetFileSpec(playingFileNo));
				displaySts = DISPSTS_PLAY_PRE;
				silentTime = playStartTime = nowTime;
				bPlaying = true;
				requestAct = REQACT_NONE;
				break;
			}
			case REQACT_STOP_MUSIC:
			{
				bPlaying = false;
				stopPlay(msx);
				displaySts = DISPSTS_FILELIST_PRE;
				requestAct = REQACT_NONE;
				break;
			}
			default:
				break;
		}

		// OLEDの表示更新
		bool bChangedKeySts = false;
		bool bUpdateDisplay = false;
		switch(displaySts)
		{
			case DISPSTS_FILELIST_PRE:
				oled.Clear();
				bUpdateDisplay |= displaySoundIndicator(oled, msx, true, musDrv, &bChangedKeySts);
				bUpdateDisplay |= displayPlayFileName(oled, mgsf, pageTopNo, seleFileNo);
				dispListStartTime = nowTime;
				displaySts = DISPSTS_FILELIST;
				bUpdateDisplay = true;
				break;
			case DISPSTS_FILELIST:
				if( bPlaying && 2000 < (nowTime-dispListStartTime)) {
					displaySts = DISPSTS_PLAY_PRE;
				}
				if( oldCurNo != seleFileNo ) {
					oldCurNo = seleFileNo;
					bUpdateDisplay |= displayPlayFileName(oled, mgsf, pageTopNo, seleFileNo);
					if( bPlaying ) {
						dispListStartTime = nowTime;
					}
				}
				bUpdateDisplay |= displaySoundIndicator(oled, msx, bUpdateDisplay, musDrv, &bChangedKeySts);
				break;
			case DISPSTS_PLAY_PRE:
				oled.Clear();
				oled.Bitmap(24, 5*8, PLAY_8x16_BITMAP, PLAY_LX, PLAY_LY);
				playTime = nowTime - playStartTime;
				bUpdateDisplay |= displayPlayTime(oled, mgsf, playTime, playingFileNo, true);
				bUpdateDisplay |= displaySoundIndicator(oled, msx, true, musDrv, &bChangedKeySts);
				displaySts = DISPSTS_PLAY;
				break;
			case DISPSTS_PLAY:
				playTime = nowTime - playStartTime;
				bUpdateDisplay |= displayPlayTime(oled, mgsf, playTime, playingFileNo, false);
				bUpdateDisplay |= displaySoundIndicator(oled, msx, false, musDrv, &bChangedKeySts);
				break;
		}
		if( bUpdateDisplay ) {
			if( g_bDiskAcc ) {
				// microSDのSPIとOLEDのI2Cは同じGPIOを共用しているので、
				// ファイルのアクセス後ではI2Cを初期化する必要がある oled.Start()
				oled.Start();
				g_bDiskAcc = false;
			}
			oled.Present();
		}

		if( bPlaying ) {
			const auto sts = static_cast<STATUSOFPLAYER>(msx.ReadMemory(0x4800+8));	// .status_of_player
			switch(sts)
			{
				case STATUSOFPLAYER::FINISH:
					// 演奏完了をチェックして次の曲を再生する
					if( 500 < (nowTime-playStartTime) ) {
						requestAct = REQACT_PLAY_NEXT_MUSIC;
					}
					break;
				case STATUSOFPLAYER::PLAYING:
					// 2.5秒以上、KeyON/OFFの変化が無かったら曲は停止していると判断して、次の曲再生を試みる
					if( bChangedKeySts ){
						silentTime = nowTime;
					}
					else{
						if( 2500 < nowTime - silentTime){
							requestAct = REQACT_PLAY_NEXT_MUSIC;
						}
					}
					// 再生回数が3回目に入ったらフェードアウトを指示する
					if(  3 <= msx.ReadMemory(0x4800+15) ) {
						fadeoutPlay(msx);
					}
					break;
				default:
					break;
			}
		}

		// MIB領域(in DEBUG)
		printMIB(msx, musDrv);
	}
	return 0;
}




