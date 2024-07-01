#pragma once
#include <stdint.h>		// for int8_t 等のサイズが保障されているプリミティブ型
#include "../CUTimeCount.h"
#include "../HopStepZ/msxdef.h"
#include "../CReadFileStream.h"
#include "vgm.h"
#include "IStreamPlayer.h"

class CVgmPlayer : public IStreamPlayer
{
private:
	//
	vgm::HEADER m_VgmHeader;
	//
	CReadFileStream *m_pStrm;
	bool m_bFileIsOK;
	bool m_bPlay;
	int m_RepeatCount;
	int m_CurStepCount;
	int m_TotalStepCount;
	CUTimeCount g_Mtc;
	bool m_bFirst;
	// tgf::timecode_t m_padding;
	// tgf::timecode_t m_oldBase;
	//
	uint32_t	m_WaitSamples;
	uint64_t	m_StartTime;

private:
	typedef  bool (CVgmPlayer::*VGM_PROC_OP)(const uint8_t cmd, CReadFileStream *pStrm);
	VGM_PROC_OP m_ProcTable[256];

public:
	CVgmPlayer();
	virtual ~CVgmPlayer();
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

private:
	bool vgmPSG(const uint8_t cmd, CReadFileStream *pStrm);		// SN76489/SN76496
	bool vgmYM2413(const uint8_t cmd, CReadFileStream *pStrm);
	bool vgmSCC(const uint8_t cmd, CReadFileStream *pStrm);		// K051649(SCC), K052539(SCCI)
private:
	bool vgmOneOp(const uint8_t cmd, CReadFileStream *pStrm);
	bool vgmTwoOp(const uint8_t cmd, CReadFileStream *pStrm);
	bool vgmThreeOp(const uint8_t cmd, CReadFileStream *pStrm);
	bool vgmFourOp(const uint8_t cmd, CReadFileStream *pStrm);
	bool vgmWaitNNNN(const uint8_t cmd, CReadFileStream *pStrm);
	bool vgmWait735(const uint8_t cmd, CReadFileStream *pStrm);
	bool vgmWait882(const uint8_t cmd, CReadFileStream *pStrm);
	bool vgmEnd(const uint8_t cmd, CReadFileStream *pStrm);
	bool vgmDataBlocks(const uint8_t cmd, CReadFileStream *pStrm);
	bool vgmPcmData(const uint8_t cmd, CReadFileStream *pStrm);
	bool vgmWait7n(const uint8_t cmd, CReadFileStream *pStrm);
	bool vgmWait8n(const uint8_t cmd, CReadFileStream *pStrm);
	bool vgmDACStreamControlWrite(const uint8_t cmd, CReadFileStream *pStrm);
};

