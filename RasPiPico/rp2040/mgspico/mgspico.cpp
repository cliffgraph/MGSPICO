/**
 * MGSPICO (RaspberryPiPico firmware)
 * Copyright (c) 2024 Harumakkin.
 * SPDX-License-Identifier: MIT
 */
// https://spdx.org/licenses/

#include "for_debug.h"
#include <stdio.h>				// printf
#include <memory.h>
#include <stdint.h>
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include <hardware/flash.h>
#include <hardware/clocks.h>	 // set_sys_clock_khz()

#ifdef FOR_DEBUG
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
#include "t_mgspico.h"
#include "oled/oledssd1306.h"
#include "oled/SOUNDLOGO.h"
#include "MgsFiles.h"
#include "tgf/CTgfPlayer.h"
#include "vgm/CVgmPlayer.h"
#if defined(MGSPICO_2ND) || defined(MGSPICO_3RD)
#include "t_si5351.h"
#endif
#if defined(MGSPICO_3RD)
#include "t_mmmspi.h"
#endif
#include "playlib.h"

const z80memaddr_t	ADDR_PLAYER 	= 0x4000;	// player
const z80memaddr_t	ADDR_IF_PP		= 0x4300;	// 通信領域
const z80memaddr_t	ADDR_STACK		= 0x43FF;	// Stack bottom
const z80memaddr_t	ADDR_DRIVER 	= 0x6000;	// ドライバー(NDP以外)
const z80memaddr_t	ADDR_DRIVER_NDP	= 0xC000;	// ドライバー(NDP)
const z80memaddr_t	ADDR_MUSDT		= 0x8000;	// 楽曲データ(NDP以外)
const z80memaddr_t	ADDR_MUSDT_NDP	= 0x4400;	// 楽曲データ(NDP)

inline bool IsMGS(MgspicoSettings::MUSICDATA x) { return (MgspicoSettings::MUSICDATA::MGS==x); }
inline bool IsKIN5(MgspicoSettings::MUSICDATA x) { return (MgspicoSettings::MUSICDATA::KIN5==x); }
inline bool IsNDP(MgspicoSettings::MUSICDATA x) { return (MgspicoSettings::MUSICDATA::NDP==x); }
inline bool IsSNDDRV(MgspicoSettings::MUSICDATA x) { return ((MgspicoSettings::MUSICDATA::MGS==x)||(MgspicoSettings::MUSICDATA::KIN5==x)||(MgspicoSettings::MUSICDATA::NDP==x)); }
inline bool IsTGF(MgspicoSettings::MUSICDATA x) { return (MgspicoSettings::MUSICDATA::TGF==x); }
inline bool IsVGM(MgspicoSettings::MUSICDATA x) { return (MgspicoSettings::MUSICDATA::VGM==x); }
inline bool IsTGForVGM(MgspicoSettings::MUSICDATA x) { return ((MgspicoSettings::MUSICDATA::TGF==x)||(MgspicoSettings::MUSICDATA::VGM==x)); }


struct INITGPTABLE {
	int gpno;
	int direction;
	bool bPullup;
	int	init_value;
};

static const INITGPTABLE g_CartridgeMode_GpioTable[] = {
#ifdef MGSPICO_2ND
	{ MMM_D0,		GPIO_OUT,	false,	1, },
	{ MMM_D1,		GPIO_OUT,	false,	1, },
	{ MMM_D2,		GPIO_OUT,	false,	1, },
	{ MMM_D3,		GPIO_OUT,	false,	1, },
	{ MMM_D4,		GPIO_OUT,	false,	1, },
	{ MMM_D5,		GPIO_OUT,	false,	1, },
	{ MMM_D6,		GPIO_OUT,	false,	1, },
	{ MMM_D7,		GPIO_OUT,	false,	1, },
	{ MMM_AEX1,		GPIO_OUT,	false,	1, },
	{ MMM_ADDT_SCC,	GPIO_OUT,	false,	1, },
	{ MMM_CSWR_PSG,	GPIO_OUT,	false,	1, },
	{ MMM_CSWR_FM,	GPIO_OUT,	false,	1, },
	{ MMM_CSWR_SCC,	GPIO_OUT,	false,	1, },
	{ MMM_S_RESET,	GPIO_OUT,	false,	0, },	// pull-up/down設定を行わないこと（回路でpull-downしている）
	{ MMM_AEX0,		GPIO_OUT,	false,	1, },
	{ MMM_MODESW, 	GPIO_IN,	true,   0, },
#elif defined(MGSPICO_3RD)
	{ MMM_EN_PWR3V3,GPIO_OUT,	false,  1, },
	{ MMM_S_RESET,	GPIO_OUT,	false,	0, },	// pull-up/down設定を行わないこと（回路でpull-downしている）
#elif defined(MGSPICO_1ST)
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
#endif
	{ MGSPICO_SW1,	GPIO_IN,	true,  0, },
	{ MGSPICO_SW2,	GPIO_IN,	true,  0, },
	{ MGSPICO_SW3,	GPIO_IN,	true,  0, },
	{ GP_PICO_LED,	GPIO_OUT,	false, 1, },
 	{ -1,			0,			false, 0, },	// eot
};

static const char *g_pMGSPICO_DAT = "mgspico.dat";
static uint8_t g_WorkRam[Z80_PAGE_SIZE];
static char tempWorkPath[255+1];
static bool g_bDiskAcc = true;
static void setupGpio(const INITGPTABLE pTable[] )
{
	for (int t = 0; pTable[t].gpno != -1; ++t) {
		const int no = pTable[t].gpno;
		gpio_init(no);
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


volatile static uint32_t timercnt = 0;
bool __time_critical_func(timerproc_fot_ff)(repeating_timer_t *rt)
{
	++timercnt;
	// disk_timerproc();
	return true;
}
uint32_t __time_critical_func(GetTimerCounterMS)()
{
	return timercnt;
}

static void fadeoutPlay(CHopStepZ &msx)
{
	msx.WriteMemory(ADDR_IF_PP+7, 0x00);			// .request_res
	msx.WriteMemory(ADDR_IF_PP+6, 0x04/*FADEOUT*/);	// .request_from_pico
	// 待ち
	while( msx.ReadMemory(ADDR_IF_PP+7) == 0x00 );

	return;
}

static void stopPlay(CHopStepZ &msx)
{
	// MGS再生を停止指示し、停止するまで待つ

	msx.WriteMemory(ADDR_IF_PP+7, 0x00);			// .request_res
	msx.WriteMemory(ADDR_IF_PP+6, 0x01/*停止*/);	// .request_from_pico
	// 待ち
	while( msx.ReadMemory(ADDR_IF_PP+7) == 0x00 );

	return;
}

static void reloadPlay(
	CHopStepZ &msx, const MgsFiles::FILESPEC &f,
	const MgspicoSettings::MUSICDATA musType)
{
	// 再生を停止指示し、停止するまで待つ
	stopPlay(msx);

	// メモリ楽曲データファイルを読み込む
	auto *p = g_WorkRam;
	UINT readSize = 0;
	g_bDiskAcc = true;
	if(sd_fatReadFileFrom(f.name, Z80_PAGE_SIZE, p, &readSize) ) {
		switch(musType)
		{
			case MgspicoSettings::MUSICDATA::NDP:
				msx.WriteMemory(ADDR_MUSDT_NDP, p+7, readSize-7);	// BLOAD/BSAVE形式のヘッダ部はのぞく
				break;
			default:
				msx.WriteMemory(ADDR_MUSDT, p, readSize);
				break;
		}
	}
	// 再生を指示する
	msx.WriteMemory(ADDR_IF_PP+7, 0x00);			// .request_res
	msx.WriteMemory(ADDR_IF_PP+6, 0x02/*再生*/);

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
		WorkAddr = msx.ReadMemoryWord(ADDR_IF_PP+11);	// .work_track_top
		WorkSize = msx.ReadMemoryWord(ADDR_IF_PP+13);	// .work_track_size
	};
	z80memaddr_t GetKeyStateAddr(int trk)
	{
		return WorkAddr + WorkSize * trk;
	}
};

INDICATOR g_Indi;

static bool displaySoundIndicator(
	CSsd1306I2c &oled, CHopStepZ &msx, bool bForce,
	const MgspicoSettings::MUSICDATA musType, bool *pChangedSts)
{
	bool bUpdatedBar = false;
	static int waitc = 0;

	// MGS、KINROU5の場合のトラックの並びを定義するテーブル
	struct TRACKINFOTABLE {
		int num;				// トラック数
		int index[3+5+9];		// PSG, SCC, FMの順になるようにする参照インデックス
	};
	static const TRACKINFOTABLE tracks_info[] =
	{
		{g_Indi.NUM_TRACKS, {0,1,2, 3,4,5,6,7, 8,9,10,11,12,13,14,15,16} },	// MGS
		{g_Indi.NUM_TRACKS, {9,10,11, 12,13,14,15,16, 0,1,2,3,4,5,6,7,8} },	// KINROU5
	};

	// const int num_track_table[] = {g_Indi.NUM_TRACKS, g_Indi.NUM_TRACKS, 0,0, 4};
	// const int num_tracks = num_track_table[(int)musType];

	// 各音源のワークエリアを参照して、各トラックのキーオン／オフ状態を判断する
	// 凍んインジケーターは音量は考慮しない、オンされたらゲージは触れる仕組みである
	int16_t cnt[g_Indi.NUM_TRACKS];
	int numTrk = 0;
	switch(musType)
	{
		case MgspicoSettings::MUSICDATA::MGS:
		{
			auto *pInfo = &tracks_info[(int)musType];
			numTrk = pInfo->num;
			for( int trk = 0; trk < numTrk; ++trk ) { 
				const int t = pInfo->index[trk];
				z80memaddr_t addr = g_Indi.GetKeyStateAddr(t);
				cnt[trk] = msx.ReadMemoryWord(addr + 0x0001);
			}
			break;
		}
		case MgspicoSettings::MUSICDATA::KIN5:
		{
			auto *pInfo = &tracks_info[(int)musType];
			numTrk = pInfo->num;
			for( int trk = 0; trk < numTrk; ++trk ) { 
				const int t = pInfo->index[trk];
				z80memaddr_t addr = g_Indi.GetKeyStateAddr(t);
				cnt[trk] = msx.ReadMemory(addr + 0x000d) & 0x80;
			}
			break;
		}
		case MgspicoSettings::MUSICDATA::NDP:
		{
			static const z80memaddr_t refIndex[] = {
				ADDR_DRIVER_NDP + 28*3 + 61*1 + 31,	// 通常トラックA 発音中の音量
				ADDR_DRIVER_NDP + 28*3 + 61*2 + 31,	// 通常トラックB 発音中の音量
				ADDR_DRIVER_NDP + 28*3 + 61*3 + 31,	// 通常トラックC 発音中の音量
				ADDR_DRIVER_NDP + 28*3 + 61*0 + 4,		// リズムトラック 音調カウンタを参照
			};
			cnt[0] = msx.ReadMemory(refIndex[0]);
			cnt[1] = msx.ReadMemory(refIndex[1]);
			cnt[2] = msx.ReadMemory(refIndex[2]);
			cnt[3] = msx.ReadMemory(refIndex[3]);
			numTrk = 4;
			break;
		}
		default:
			break;
	}

	// 各トラックのキーオン／オフ状態からゲージのレベルを作成する
	for( int trk = 0; trk < numTrk; ++trk ) { 
		// key-on/offを判断してレベル値を決める
		int offsetX = 0;
		if( 8 <= trk )		// SCCとFMの隙間
			offsetX = 6;
		else if( 3 <= trk )	// PSGとSCCの隙間
			offsetX = 3;

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
			auto x = trk*g_Indi.FLAME_W + offsetX;
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

static bool displayStepCount(CSsd1306I2c &oled, IStreamPlayer &stp )
{
	bool bUpdated = false;
	char txt[5+5 +1];
	static int oldProg = 0, oldRept = 0;
	const int prog = (int)((float)stp.GetCurStepCount() / stp.GetTotalStepCount() * 100);
	const int rept = stp.GetRepeatCount() +1;
	if( oldProg != prog || oldRept != rept ){
		oldProg = prog;
		oldRept = rept;
		sprintf(txt, "%3d%% @%d", prog, rept);
		oled.Strings8x16(0, 0, txt);
		bUpdated = true;
	}
	return bUpdated;
}

inline void aliveLamp()
{
	static CUTimeCount tim;
	static uint8_t lamp = 0;
	if( 100*1000 < tim.GetTime() ){
		++lamp;
		tim.ResetBegin();
	}
	gpio_put(GP_PICO_LED, (lamp&0x01));
	return;
}


static CHopStepZ *g_pMsx = nullptr;
static IStreamPlayer *g_pSTRP = nullptr;
static void Core1Task()
{
	switch(g_Setting.GetMusicType())
	{
		case MgspicoSettings::MUSICDATA::MGS:
		case MgspicoSettings::MUSICDATA::KIN5:
		case MgspicoSettings::MUSICDATA::NDP:
		{
			for(;;) {
				g_pMsx->RunStage2();
				static uint8_t lamp = 0;
				gpio_put(GP_PICO_LED, (((++lamp)>>7)&0x01));
			}
			break;
		}
		case MgspicoSettings::MUSICDATA::TGF:
		case MgspicoSettings::MUSICDATA::VGM:
		{
			for(;;) {
				g_pSTRP->PlayLoop();
				aliveLamp();
			}
			break;
		}
	}
	return;
}

enum SWINDEX : int 
{
	SWINDEX_SW1,		// [●]
	SWINDEX_SW2,		// [▼]
	SWINDEX_SW3,		// [▲]
	SWINDEX_SW_NUM,
};
static bool checkSw(SWINDEX swIndex, const uint32_t nowTime)
{
	static const uint32_t swtable[SWINDEX_SW_NUM] = {MGSPICO_SW1, MGSPICO_SW2, MGSPICO_SW3};
	static bool swStsCandi[SWINDEX_SW_NUM] = {false, false, false};
	static bool swSts[SWINDEX_SW_NUM] = {false, false, false};
	static uint32_t timerCnts[SWINDEX_SW_NUM] = {0,0,0};
	const bool sts = gpio_get(swtable[swIndex]);
	if( timerCnts[swIndex] == 0 ) {
		if( swStsCandi[swIndex] != sts ) {
			timerCnts[swIndex] = nowTime;
			swStsCandi[swIndex] = sts;
		}
	}
	else if( 15 < nowTime - timerCnts[swIndex] ) {
		if( swStsCandi[swIndex] == sts ) {
			swStsCandi[swIndex] = sts;
			swSts[swIndex] = sts;			// 確定状態
			timerCnts[swIndex] = 0;
		}
	}
	return !swSts[swIndex];
}

enum KEYCODE : int 
{
	KEY_APPLY,		// [●]
	KEY_DOWN,		// [▼]
	KEY_UP,			// [▲]
	KEY_RANDMIZE,	// [▲]＆[▼]
	KEY_NUM,
};
static bool getPushSw(KEYCODE *pKey, bool *pPush, const uint32_t nowTime)
{
	static bool bPush[KEY_NUM] = {false, false, false, false,};
	static uint32_t fastfiles = 0;
	bool b[KEY_NUM];

	for( int t = 0 ; t < SWINDEX_SW_NUM; ++t)
		b[t] = checkSw((SWINDEX)t, nowTime);
	b[KEY_RANDMIZE] = b[KEY_DOWN] && b[KEY_UP];

	if( !b[KEY_DOWN] && !b[KEY_UP] )
		fastfiles = nowTime;

	if( bPush[KEY_RANDMIZE] != b[KEY_RANDMIZE] ){
		bPush[KEY_RANDMIZE] = b[KEY_RANDMIZE];
		*pKey = KEY_RANDMIZE;
		*pPush = b[KEY_RANDMIZE];
		return true;
	}
	if( bPush[KEY_APPLY] != b[KEY_APPLY] ){
		bPush[KEY_APPLY] = b[KEY_APPLY];
		*pKey = KEY_APPLY;
		*pPush = b[KEY_APPLY];
		return true;
	}
	if( !bPush[KEY_RANDMIZE] ) {
		for( int t = KEY_DOWN; t <= KEY_UP; ++t) {
			bool bt = b[t];
			if( bPush[t] != bt ){
				bPush[t] = bt;
				*pKey = (KEYCODE)t;
				*pPush = bt;
				fastfiles = (bt)?nowTime:0;
				return true;
			}
			if( bPush[t] ) {
				if( fastfiles + 700 < nowTime) {
					*pKey = (KEYCODE)t;
					*pPush = true;
					return true;
				}
			}
		}
	}
	return false;
}

static void waitForKeyRelease()
{
	for(;;) {
		const uint32_t nowTime = GetTimerCounterMS();
		if( checkSw(SWINDEX_SW1, nowTime) || checkSw(SWINDEX_SW2, nowTime) || checkSw(SWINDEX_SW3, nowTime) )
			continue;
		break;
	}
	KEYCODE key;
	bool push;
	getPushSw(&key, &push, GetTimerCounterMS());
	return;
}

static void displayNotFound(CSsd1306I2c &oled, const char *pPathName)
{
	oled.Start();
	oled.Clear();
	oled.Strings8x16(0*8, 1*16, "Not found", false);
	oled.Strings8x16(0*8, 2*16, pPathName, false);
	oled.Present();
	#ifdef FOR_DEBUG
		printf("not found %s\n", pPathName);
	#endif
	return;
}

static void displayMusicLogo(CSsd1306I2c &oled, CHopStepZ &msx, const MgspicoSettings::MUSICDATA musType)
{
	if( musType == MgspicoSettings::MUSICDATA::NDP )
		return;
	z80memaddr_t mibAddr = msx.ReadMemoryWord(ADDR_IF_PP+9);	// .mgs_mib_addr
	uint8_t NOTFOUND = (IsMGS(musType))?0xFF:0x00;
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


static void printMIB(CHopStepZ &msx, const MgspicoSettings::MUSICDATA musType)
{
#if defined(FOR_DEBUG) 
	if( musType == MgspicoSettings::MUSICDATA::MGS ) {
		z80memaddr_t mibAddr = msx.ReadMemoryWord(ADDR_IF_PP+9);	// .mgs_mib_addr
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

static void dislplayTitle(CSsd1306I2c &disp, const MgspicoSettings::MUSICDATA musType)
{
	disp.Start();
	disp.ResetI2C();
	disp.Clear();

	// VERSION
	// v1.11 ・SCC+に対応していなかったのを修正した（MGSPICO1,2）
	// v1.12 ・SCCの音が高温に聞こえる問題を修正した（MGSPICO2）
	//		 ・VGMデータの再生を改善した（MGSPICO1,2）
	//
	// MGSPICO3
	// v2.13 ・MGSPICO3  に対応した
	//
	// MGSPICO, MGSPICO2
	// v1.14 ・NDP  に対応した

#if defined(MGSPICO_2ND)
	if( g_Setting.Is240MHz() )
		disp.Strings8x16(13*8+4, 0*16, "*", false);
	disp.Strings8x16(1*8+4, 0*16, "MGS MUSE", false);
	disp.Strings8x16(1*8+4, 1*16, "MACHINA v1.14", false);
	disp.Box(4, 0, 116, 30, true);
#elif defined(MGSPICO_3RD)
	if( g_Setting.Is240MHz() )
		disp.Strings8x16(13*8+4, 0*16, "*", false);
	disp.Strings8x16(1*8+4, 0*16, "MGSPICO 3", false);
	disp.Strings8x16(1*8+4, 1*16, "        v2.14", false);
	disp.Box(4, 0, 116, 30, true);
#elif defined(MGSPICO_1ST)
	if( g_Setting.Is240MHz() )
		disp.Strings8x16(14*8+4, 0*16, "*", false);
	disp.Strings8x16(1*8+4, 1*16, "MGSPICO v1.14", false);
	disp.Box(4, 14, 116, 16, true);
#endif

	disp.Strings8x16(1*8+4, 2*16, "by harumakkin", false);
	const char *pForDrv[] = {"for MGS", "for MuSICA", "for TGF", "for VGM", "for NDP"};
	disp.Strings8x16(1*8+4, 3*16, pForDrv[(int)musType], false);
	disp.Present();
	return;
}

void listupMusicFiles(MgsFiles *pMgsf, const MgspicoSettings::MUSICDATA musType)
{
	const char *pWildCard[] = {"*.MGS", "*.BGM", "*.TGF", "*.VGM", "*.NDP"};
	pMgsf->ReadFileNames(pWildCard[(int)musType]);
	return;
}

bool loadMusicDriver(CHopStepZ *pMsx, CSsd1306I2c &disp, const MgspicoSettings::MUSICDATA musType)
{
	// MGSDRV/KINROU5/NDP の本体部分を、ADDR_DRIVER へ読み込む
	UINT readSize = 0;
	auto *p = g_WorkRam;
	switch(musType) 
	{
		case MgspicoSettings::MUSICDATA::MGS:
		{
			if( pMsx != nullptr ) {
				sprintf(tempWorkPath, "%s", "MGSDRV.COM");
				if(!sd_fatReadFileFrom(tempWorkPath, Z80_PAGE_SIZE, p, &readSize) ) {
					displayNotFound(disp, tempWorkPath);
					return false;
				}
				const uint8_t *pBody;
				uint16_t bodySize;
				if( t_Mgs_GetPtrBodyAndSize(reinterpret_cast<const STR_MGSDRVCOM*>(p), &pBody, &bodySize) ) {
					pMsx->WriteMemory(ADDR_DRIVER, pBody, bodySize);
				}
			}
			break;
		}
		case MgspicoSettings::MUSICDATA::KIN5:
		{
			if( pMsx != nullptr ) {
				sprintf(tempWorkPath, "%s", "KINROU5.DRV");
				if(!sd_fatReadFileFrom(tempWorkPath, Z80_PAGE_SIZE, p, &readSize) ) {
					displayNotFound(disp, tempWorkPath);
					return false;
				}
				pMsx->WriteMemory(ADDR_DRIVER, p+7, readSize-7);	// BSAVE/BLOAD形式のヘッダ部をのぞく
			}
			break;
		}
		case MgspicoSettings::MUSICDATA::NDP:
		{
			if( pMsx != nullptr ) {
				sprintf(tempWorkPath, "%s", "NDP.BIN");
				if(!sd_fatReadFileFrom(tempWorkPath, Z80_PAGE_SIZE, p, &readSize) ) {
					displayNotFound(disp, tempWorkPath);
					return false;
				}
				pMsx->WriteMemory(ADDR_DRIVER_NDP, p+7, readSize-7);	// BSAVE/BLOAD形式のヘッダ部をのぞく
			}
			break;
		}
		case MgspicoSettings::MUSICDATA::TGF:
		case MgspicoSettings::MUSICDATA::VGM:
		default:
		{
			// do nothing
			break;
		}
	}
	return true;
}

bool loadPlayersCom(CHopStepZ *pMsx, CSsd1306I2c &disp, const MgspicoSettings::MUSICDATA musType)
{
	switch(musType) 
	{
		case MgspicoSettings::MUSICDATA::MGS:
		case MgspicoSettings::MUSICDATA::KIN5:
		case MgspicoSettings::MUSICDATA::NDP:
		{
			if( pMsx != nullptr ) {
				const uint8_t *p = _binary_player_bin_start;
				const uint32_t sz = (uint32_t)_binary_player_bin_end - (uint32_t)_binary_player_bin_start + 1;
				pMsx->WriteMemory(ADDR_PLAYER, p, sz);
				pMsx->WriteMemory(ADDR_IF_PP+16, (uint8_t)musType);		// ドライバの種類を指定する
				pMsx->RunStage1(ADDR_PLAYER, ADDR_STACK);
				// 以降、エミュレータはCore1で動かす
				multicore_launch_core1(Core1Task);
				busy_wait_ms(100);	// ドライバが音源を検出するまでの時間稼ぎ
				// ドライバが検出した音源のロゴを表示する
				displayMusicLogo(disp, *pMsx, musType);
				// レベルインジケーター表示の前準備
				g_Indi.Setup(*pMsx);
				//
			}
			break;
		}
		case MgspicoSettings::MUSICDATA::TGF:
		{
			g_pSTRP = GCC_NEW CTgfPlayer();
			if( g_Setting.GetEnforceOPLL() ){

#ifdef MGSPICO_1ST
// #pragma message("MGSPICO_1ST 検討中お項目");
// #pragma message(" TODO:");
// #pragma message("核スロは、拡張スロット上にOPLLがマークされている。");
// #pragma message("拡張スロット有無を検査して、探すこと");
// #pragma message("どうしようめんどうだ。");
// #pragma message("MGSDRVを一回動かして、SCCのスロットを見つけてもらう？SCCも同様");
#endif
				//uint8_t v = mgspico::t_ReadMem(0x7ff6);
				//mgspico::t_WriteMem(0x7ff6, v|0x01);
				g_pSTRP->EnableFMPAC();
				g_pSTRP->EnableYAMANOOTO();
			}
			multicore_launch_core1(Core1Task);
			break;
		}
		case MgspicoSettings::MUSICDATA::VGM:
		{
			g_pSTRP = GCC_NEW CVgmPlayer();
			if( g_Setting.GetEnforceOPLL() ){
#ifdef MGSPICO_1ST
// #pragma message("拡張スロット有無を検査して、探すこと");
#endif
				//uint8_t v = mgspico::t_ReadMem(0x7ff6);
				//mgspico::t_WriteMem(0x7ff6, v|0x01);
				g_pSTRP->EnableFMPAC();
				g_pSTRP->EnableYAMANOOTO();
			}
			multicore_launch_core1(Core1Task);
			break;
		}
	}
	return true;
}


const int NUM_DISP_MENUITEMS = 3;

static void dsipSttMenu(CSsd1306I2c &disp, const int y, const MgspicoSettings::ITEM &item, const int seleChoice, const bool bCursor)
{
	disp.Strings8x16(0, y, item.pName);
	disp.Strings8x16(7*10, y, item.pChoices[seleChoice], bCursor);
	if( bCursor )
		disp.Line(0, y+13, 80, y+13, true);
	return;
}

static void dispMenus(CSsd1306I2c &disp, const int dispTopIndex, const int seleOffset, const MgspicoSettings &stt)
{
	disp.Clear();
	disp.Strings8x16(2, 0, "SETTINGS");
	disp.Box(0, 0, 2+8*8, 13, true);
	for( int t = 0; t < NUM_DISP_MENUITEMS; ++t) {
		const int dispIndex = dispTopIndex+t;
		auto &item = *stt.GetItem(dispIndex);
		dsipSttMenu(disp, 16+16*t, item, stt.GetChioce(dispIndex), (t==seleOffset));
	}
	return;
}

static void settingMenuMain(CSsd1306I2c &disp)
{
	gpio_put(GP_PICO_LED, 0);

	disp.Start();
	disp.ResetI2C();

	int dispTopIndex = 0, oldDispTopIndex = -1;
	int seleIndex = 0, oldSeleIndex = -1;
	dispMenus(disp, dispTopIndex, seleIndex, g_Setting);
	disp.Present();

	waitForKeyRelease();

	bool bExit = false;
	while(!bExit) {
		const uint32_t nowTime = GetTimerCounterMS();
		KEYCODE key;
		bool bPushKey;
		bool bUpdateDisplay = false;
		if( getPushSw(&key, &bPushKey, nowTime)) {
			// [●]
			if( bPushKey && key == KEY_APPLY) {
				const int index = dispTopIndex+seleIndex;
				auto &item = *g_Setting.GetItem(index);
				int n = g_Setting.GetChioce(index);
				const int temp = n;
				++n;
				if( item.num <= n )
					n = 0;
				if( temp != n ) {
					gpio_put(GP_PICO_LED, 1);
					bUpdateDisplay = true;
					g_Setting.SetChioce(index, n);
					g_Setting.WriteSettingTo(g_pMGSPICO_DAT);
					busy_wait_ms(200);
					g_bDiskAcc = true;
					gpio_put(GP_PICO_LED, 0);
				}
			}
			// [▼]
			if( bPushKey && key == KEY_DOWN ) {
				++seleIndex;
				if( NUM_DISP_MENUITEMS <= seleIndex ) {
					seleIndex = NUM_DISP_MENUITEMS-1;
					++dispTopIndex;
					const int numItems = g_Setting.GetNumItems();
					if( numItems-NUM_DISP_MENUITEMS < dispTopIndex )
						dispTopIndex = numItems-NUM_DISP_MENUITEMS;
				}
			}
			// [▲]
			if( bPushKey && key == KEY_UP ) {
				--seleIndex;
				if( seleIndex < 0 ) {
					seleIndex = 0;
					--dispTopIndex;
					if( dispTopIndex < 0 )
						dispTopIndex = 0;
				}
			}
			// [EXIT]
			if( bPushKey && key == KEY_RANDMIZE ) {
				bExit = true;
			}
		}

		if( oldDispTopIndex != dispTopIndex || oldSeleIndex != seleIndex || bUpdateDisplay) {
			oldDispTopIndex = dispTopIndex, oldSeleIndex = seleIndex;
			dispMenus(disp, dispTopIndex, seleIndex, g_Setting);
			bUpdateDisplay = true;
		}

		if( bUpdateDisplay ) {
			if( g_bDiskAcc ) {
				// microSDのSPIとOLEDのI2Cは同じGPIOを共用しているので、
				// ファイルのアクセス後ではI2Cを初期化する必要がある disp.Start()
				disp.Start();
				g_bDiskAcc = false;
			}
			disp.Present();
		}
	}
}


static void playingMusicMain(
	CHopStepZ *pMsx, CSsd1306I2c &disp, MgsFiles &mgsf,
	const MgspicoSettings::MUSICDATA musType)
{
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
	// 
	REQACT_STATE requestAct = REQACT_NONE;
	DISPLAY_STATE displaySts = DISPSTS_FILELIST_PRE;
	int seleFileNo = 1;					// 選択ファイル番号
	int pageTopNo = 1;					// 表示されているリストの先頭の番号
	//
	uint32_t playStartTime = 0;			// 再生開始時の時刻
	uint32_t dispListStartTime = 0;		// リスト表示に切り替わった時刻
	uint32_t playTime = 0;				// 再生経過時間
	uint32_t waitTime = 0;	
	//
	int playingFileNo = 1;				// 再生ファイル番号
	bool bPlaying = false;				// 再生中
	int playRepeatCount = 0;
	uint32_t silentTime = 0;			// 無音時間検出の時間カウンタ
	int oldCurNo = -1;
	g_bDiskAcc = true;

	// For shuffle function   *Daniel Padilla*
	bool bRandomize = g_Setting.GetShufflePlay();
	bool bFirstSeq = true;

	for(;;) {
		const uint32_t nowTime = GetTimerCounterMS();
		KEYCODE key;
		bool bPushKey;
		if( !mgsf.IsEmpty() ) {
			if( g_Setting.GetAutoRun() && bFirstSeq ) {
				requestAct = REQACT_PLAY_MUSIC;
				bFirstSeq = false;
			}
			if(  getPushSw(&key, &bPushKey, nowTime)) {
				// [●]
				if( bPushKey && key == KEY_APPLY) {
					requestAct = 
						((displaySts==DISPSTS_PLAY&&(bPlaying^true)) || displaySts==DISPSTS_FILELIST)
						? REQACT_PLAY_MUSIC : REQACT_STOP_MUSIC;
				}
				// [▼]
				if( bPushKey && key == KEY_DOWN /*|| fastfiles + 1000 < nowTime*/ ) {
					changeCurPos(mgsf.GetNumFiles(), &pageTopNo, &seleFileNo, +1);
					if( displaySts != DISPSTS_FILELIST )
						displaySts = DISPSTS_FILELIST_PRE;
				}
				// [▲]
				if( bPushKey && key == KEY_UP /*|| fastfiles + 1000 < nowTime*/ ) {
					changeCurPos(mgsf.GetNumFiles(), &pageTopNo, &seleFileNo, -1);
					if( displaySts != DISPSTS_FILELIST )
						displaySts = DISPSTS_FILELIST_PRE;
				}
				// [▲]&[▼]
				if( !bPushKey && key == KEY_RANDMIZE ) {
					bRandomize = !bRandomize;
					displaySts = DISPSTS_PLAY_PRE;
				}
			}
		}

		switch(requestAct)
		{
			case REQACT_PLAY_NEXT_MUSIC:
			{
				bPlaying = false;
				if( IsSNDDRV(musType) ) {
					pMsx->WriteMemory(ADDR_IF_PP+8,(uint8_t)STATUSOFPLAYER::IDLE);
				}
				if( bRandomize == true){
					seleFileNo=rand() % mgsf.GetNumFiles() + 1;	
				}
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
				if( IsSNDDRV(musType) ) {
					if( 1000 < (nowTime-waitTime) ){
						requestAct = REQACT_PLAY_MUSIC;
					}
				}
				else if( IsTGForVGM(musType) ){
					requestAct = REQACT_PLAY_MUSIC;
				}
				break;
			}
			// seleFileNoが示すデータを即再生する
			case REQACT_PLAY_MUSIC:
			{
				playingFileNo = seleFileNo;
				if( IsSNDDRV(musType) ) {
					reloadPlay(*pMsx, *mgsf.GetFileSpec(playingFileNo), musType);
				}
				else if( IsTGForVGM(musType) ) {
					auto fn = *mgsf.GetFileSpec(playingFileNo);
					g_bDiskAcc = true;
					g_pSTRP->Stop();
					g_pSTRP->Mute();
					if( !g_pSTRP->SetTargetFile(fn.name) )
						break;
					g_pSTRP->Start();
				}
				displaySts = DISPSTS_PLAY_PRE;
				silentTime = playStartTime = nowTime;
				playRepeatCount = 0;
				bPlaying = true;
				requestAct = REQACT_NONE;
				break;
			}
			case REQACT_STOP_MUSIC:
			{
				bPlaying = false;
				if( IsSNDDRV(musType) ) {
					stopPlay(*pMsx);
				}
				else if( IsTGForVGM(musType) ) {
					g_pSTRP->Stop();
					g_pSTRP->Mute();
				}
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
				disp.Clear();
				if( IsSNDDRV(musType) ) {
					bUpdateDisplay |= displaySoundIndicator(disp, *pMsx, true, musType, &bChangedKeySts);
				}
				else if( IsTGForVGM(musType) && bPlaying ){
					bUpdateDisplay |= displayStepCount(disp, *g_pSTRP);
				}
				bUpdateDisplay |= displayPlayFileName(disp, mgsf, pageTopNo, seleFileNo);
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
					bUpdateDisplay |= displayPlayFileName(disp, mgsf, pageTopNo, seleFileNo);
					if( bPlaying ) {
						dispListStartTime = nowTime;
					}
				}
				if( IsSNDDRV(musType) ) {
					bUpdateDisplay |= displaySoundIndicator(disp, *pMsx, bUpdateDisplay, musType, &bChangedKeySts);
				}
				else if( bPlaying ){
					if( IsTGForVGM(musType) ){
						bUpdateDisplay |= displayStepCount(disp, *g_pSTRP);
					}
				}
				break;
			case DISPSTS_PLAY_PRE:
				disp.Clear();
				if (bRandomize == true) {
					disp.Bitmap(4, 5*8, RANDOM_13x16_BITMAP, RANDOM_LX,RANDOM_LY);
				}
				disp.Bitmap(24, 5*8, PLAY_8x16_BITMAP, PLAY_LX, PLAY_LY);
				playTime = nowTime - playStartTime;
				bUpdateDisplay |= displayPlayTime(disp, mgsf, playTime, playingFileNo, true);
				if( IsSNDDRV(musType) ) {
					bUpdateDisplay |= displaySoundIndicator(disp, *pMsx, true, musType, &bChangedKeySts);
				}
				else if( IsTGForVGM(musType) && bPlaying ){
					bUpdateDisplay |= displayStepCount(disp, *g_pSTRP);
				}
				displaySts = DISPSTS_PLAY;
				break;
			case DISPSTS_PLAY:
				playTime = (bPlaying)?(nowTime - playStartTime):0;
				bUpdateDisplay |= displayPlayTime(disp, mgsf, playTime, playingFileNo, false);
				if( IsSNDDRV(musType) ) {
					bUpdateDisplay |= displaySoundIndicator(disp, *pMsx, false, musType, &bChangedKeySts);
				}
				else if( IsTGForVGM(musType) ){
					bUpdateDisplay |= displayStepCount(disp, *g_pSTRP);
				}
				break;
		}
		if( bUpdateDisplay ) {
			if( g_bDiskAcc ) {
				// microSDのSPIとOLEDのI2Cは同じGPIOを共用しているので、
				// ファイルのアクセス後ではI2Cを初期化する必要がある disp.Start()
				disp.Start();
				g_bDiskAcc = false;
			}
			disp.Present();
		}

		if( bPlaying ) {
			if( IsSNDDRV(musType) ) {
				const auto sts = static_cast<STATUSOFPLAYER>(pMsx->ReadMemory(ADDR_IF_PP+8));	// .status_of_player
				switch(sts)
				{
					case STATUSOFPLAYER::FINISH:
					{
						// 演奏完了をチェックして次の曲を再生する
						if( 500 < (nowTime-playStartTime) ) {
							requestAct = REQACT_PLAY_NEXT_MUSIC;
						}
						break;
					}
					case STATUSOFPLAYER::PLAYING:
					{
						// 2.5秒以上、KeyON/OFFの変化が無かったら曲は停止していると判断して、次の曲再生を試みる
						if( bChangedKeySts ){
							silentTime = nowTime;
						}
						else{
							if( 2500 < nowTime - silentTime){
								requestAct = REQACT_PLAY_NEXT_MUSIC;
							}
						}
						// 再生回数が2回を超えたらフェードアウトを指示する
						static const uint8_t LOOPCNT = 2;
						const uint8_t n = pMsx->ReadMemory(ADDR_IF_PP+15);
						if(  LOOPCNT < n ) {
							fadeoutPlay(*pMsx);
						}
						break;
					}
					default:
						break;
				}
			}
			else if( IsTGForVGM(musType) ) {
				g_bDiskAcc |= g_pSTRP->FetchFile();
				const int rpt = g_pSTRP->GetRepeatCount();
				// ２分経過後に、曲の終端に達したら次の曲に移る
				// （どんなに短い曲でも２分は繰り返し再生するということ）
				if( playRepeatCount != rpt ) {
					playRepeatCount = rpt;
					const uint32_t tim = nowTime-playStartTime;
					if( 2*60*1000 < tim) {
						requestAct = REQACT_PLAY_NEXT_MUSIC;
					}
				}
			}
		}

		// MIB領域(in DEBUG)
		if( IsSNDDRV(musType) ) {
			printMIB(*pMsx, musType);
		}
	}
	return;
}

/**
 * エントリ
 */
int main()
{
	//set_sys_clock_khz(240*1000, true);
	setupGpio(g_CartridgeMode_GpioTable);
	static repeating_timer_t tim;
	add_repeating_timer_ms (1/*ms*/, timerproc_fot_ff, nullptr, &tim);

	#ifdef FOR_DEBUG
		stdio_init_all();
	#endif

#if defined(MGSPICO_2ND) || defined(MGSPICO_3RD)
		t_SetupSi5351();
	#endif

#ifdef MGSPICO_3RD
	mmmspi::Init();
#endif

	// setup for filesystem
	disk_initialize(0);

	CSsd1306I2c oled;

	// ●SW(SW1)が押下されていたら、設定画面を表示する
	bool bConfigMenu = !gpio_get(MGSPICO_SW1);
	// ●MODE SW
#if defined(MGSPICO_2ND)
	bool bNormalSpeed = gpio_get(MMM_MODESW);
#endif
	busy_wait_ms(100);

	bConfigMenu &= !gpio_get(MGSPICO_SW1);
#if defined(MGSPICO_2ND)
	bNormalSpeed &= gpio_get(MMM_MODESW);
#endif

	g_Setting.ReadSettingFrom(g_pMGSPICO_DAT);
	if( bConfigMenu )
		settingMenuMain(oled);

#if defined(MGSPICO_2ND) 
	if(!bNormalSpeed/*modesw is B*/){
		g_Setting.SetRp2040Clock(MgspicoSettings::RP2040CLOCK::CLK240MHZ);
	}
#endif

	if( g_Setting.Is240MHz() )
		set_sys_clock_khz(240*1000, true);

	const auto musType = g_Setting.GetMusicType();

	//  タイトルの表示
	dislplayTitle(oled, musType);
	busy_wait_ms(800);

	#ifdef FOR_DEBUG
		printf("MGSPICO by harumakkin.2024\n");
	#endif

#if defined(MGSPICO_2ND) || defined(MGSPICO_3RD)
	// RESET信号を解除
	gpio_put(MMM_S_RESET, 1);
#elif defined(MGSPICO_1ST)
	// RESET信号を解除
	gpio_put(MSX_A15_RESET, 1);	// RESET = H
	busy_wait_us(1);
	gpio_put(MSX_LATCH_C, 0);	// LATCH_C = L	// 制御ラインを現状でラッチする
#endif

#if defined(MGSPICO_3RD) 
	mgspico::t_OutSelSccMod((uint32_t)g_Setting.GetSccModule());
#endif

	if( IsSNDDRV(musType) ) {
		// エミュレータのセットアップ
		g_pMsx = GCC_NEW CHopStepZ();
#if defined(MGSPICO_2ND) || defined(MGSPICO_3RD)
		const bool bEnforceOPLL = true;
#elif defined(MGSPICO_1ST)
		const bool bEnforceOPLL = g_Setting.GetEnforceOPLL();
#endif
		g_pMsx->Setup(bEnforceOPLL, IsKIN5(musType));
		// PLAYERとの通信領域を0クリア
		for(size_t t = 0; t < sizeof(IF_PLAYER_PICO); ++t) {
			g_pMsx->WriteMemory(ADDR_IF_PP+t, 0x00);
		}
	}

	// 音楽データファイル名をリストアップ - > mgsf
	MgsFiles *pMgsFiles = GCC_NEW MgsFiles();
	listupMusicFiles(pMgsFiles, musType);
	auto &mgsf = *pMgsFiles;

	// ドライバ、PLAYERS.COMの読み込みとMSXエミュレータの起動
	if( !loadMusicDriver(g_pMsx, oled, musType) )
		return -1;
	if( !loadPlayersCom(g_pMsx, oled, musType) )
		return -1;

	// 
	waitForKeyRelease();
	playingMusicMain(g_pMsx, oled, mgsf, musType);
	return 0;
}




