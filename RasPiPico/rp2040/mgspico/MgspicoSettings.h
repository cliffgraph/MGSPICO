#pragma once
#include <stdint.h>
#include "ff/ff.h"

class MgspicoSettings
{
public:
	struct ITEM
	{
		const char *pName;
		const int num;
		const char *pChoices[5];
	};
#if defined(MGSPICO_3RD) 
	const static int NUM_MENUITEMS = 5;
#elif defined(MGSPICO_2ND)
	const static int NUM_MENUITEMS = 4;
#elif defined(MGSPICO_1ST)
	const static int NUM_MENUITEMS = 6;
#endif
	enum class RP2040CLOCK : uint8_t {CLK125MHZ, CLK240MHZ};
	enum class MUSICDATA : uint8_t	{MGS=0, KIN5=1, TGF=2, VGM=3, NDP=4};
	enum class SCCMODULE : uint8_t {IKASCC=0, HRASCC=1};

private:
#pragma pack(push,1)
	struct SETTINGDATA {
		MUSICDATA	MusicType;
		RP2040CLOCK	Rp2040Clock;
		uint8_t		AutoRun;			// != 0 : 自動的に演奏を開始する
		uint8_t		ShufflePlay;		// != 0 : ランダムの曲順で再生する
		uint8_t		EnforceOPLL;		// != 0 : OPLLの存在模倣する
		SCCMODULE	SccModule;			// IKASCC, HRASCC
		uint8_t		YamanootoExtSlot;	// YAMANOOTO のいる拡張スロット番号
		uint8_t		Padding[121];		// (構造体サイズを128byteに保つこと)
	};
#pragma pack(pop)

private:
	FATFS m_fs;
	SETTINGDATA m_Setting;

public:
	MgspicoSettings();
	virtual ~MgspicoSettings();
private:
	void setDefault(SETTINGDATA *p);

public:
	int	GetNumItems() const;
	const ITEM *GetItem(const int index ) const;
	void SetChioce(const int indexItem, const int no);
	int GetChioce(const int indexItem) const;

	bool ReadSettingFrom(const char *pFilePath);
	bool WriteSettingTo(const char *pFilePath);
public:
	MUSICDATA GetMusicType() const;
	void SetMusicType(const MUSICDATA type);

	bool Is240MHz() const;
	RP2040CLOCK GetRp2040Clock() const;
	void SetRp2040Clock(const RP2040CLOCK clk);

	bool GetAutoRun() const;
	void SetAutoRun(const bool bAuto);

	bool GetShufflePlay() const;
	void SetShufflePlay(const bool bRnd);

	bool GetEnforceOPLL() const;
	void SetEnforceOPLL(const bool bEnforce);

	SCCMODULE GetSccModule() const;
	void SetSccModule(const SCCMODULE mod);

	int GetYamanootoExtSlot() const;
	void SetYamanootoExtSlot(const int slotNo);

};