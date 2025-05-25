#pragma once
#include <stdint.h>		// for int8_t 等のサイズが保障されているプリミティブ型
#include "../CUTimeCount.h"
#include "../HopStepZ/msxdef.h"
#include "../CReadFileStream.h"
#include "tgf.h"
#include "IStreamPlayer.h"

class CTgfPlayer : public IStreamPlayer
{
private:
	CReadFileStream *m_pStrm;
	bool m_bFileIsOK;
	bool m_bPlay;
	int m_RepeatCount;
	int m_CurStepCount;
	int m_TotalStepCount;
	CUTimeCount g_Mtc;
	bool m_bFirst;
	tgf::timecode_t m_padding;
	tgf::timecode_t m_oldBase;

public:
	CTgfPlayer();
	virtual ~CTgfPlayer();

// member of IStreamPlayer
public:
	bool SetTargetFile(const char *pFname);
	int GetTotalStepCount() const;
	int GetCurStepCount() const;
	int GetRepeatCount() const;
	void Start();
	void Stop();
	bool FetchFile();
	void PlayLoop();
	void Mute();
	bool EnableFMPAC();
	bool EnableYAMANOOTO();
};

