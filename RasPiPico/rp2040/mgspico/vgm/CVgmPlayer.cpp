#include "../stdafx.h"
#include <string.h>
#include <stdio.h>		// printf
#include "CVgmPlayer.h"
#include "../t_mgspico.h"

CVgmPlayer::CVgmPlayer()
{
	m_pStrm = GCC_NEW CReadFileStream();
	m_bFileIsOK = false;
	m_bPlay = false;
	m_RepeatCount = 0;
	m_CurStepCount = 0;
	m_TotalStepCount = 0;

	for(int t = 0x00; t < 0x2f; ++t)
		m_ProcTable[t] = nullptr;

	for(int t = 0x30; t < 0x3f; ++t)
		m_ProcTable[t] = &CVgmPlayer::vgmOneOp;

	m_ProcTable[0x40] = &CVgmPlayer::vgmTwoOp;

	for(int t = 0x41; t < 0x4e; ++t)
		m_ProcTable[t] = nullptr;

	m_ProcTable[0x4f] = &CVgmPlayer::vgmTwoOp;
	m_ProcTable[0x50] = &CVgmPlayer::vgmPSG;			// PSG ?
	m_ProcTable[0x51] = &CVgmPlayer::vgmYM2413;			// OPLL

	for(int t = 0x52; t < 0x5f; ++t)
		m_ProcTable[t] = &CVgmPlayer::vgmTwoOp;

	m_ProcTable[0x61] = &CVgmPlayer::vgmWaitNNNN;
	m_ProcTable[0x62] = &CVgmPlayer::vgmWait735;
	m_ProcTable[0x63] = &CVgmPlayer::vgmWait882;
	m_ProcTable[0x66] = &CVgmPlayer::vgmEnd;
	m_ProcTable[0x67] = &CVgmPlayer::vgmDataBlocks;
	m_ProcTable[0x68] = &CVgmPlayer::vgmPcmData;

	for(int t = 0x70; t <= 0x7f; ++t)
		m_ProcTable[t] = &CVgmPlayer::vgmWait7n;

	for(int t = 0x80; t <= 0x8f; ++t)
		m_ProcTable[t] = &CVgmPlayer::vgmWait8n;

	for(int t = 0x90; t <= 0x95; ++t)
		m_ProcTable[t] = &CVgmPlayer::vgmDACStreamControlWrite;

	m_ProcTable[0xa0] = &CVgmPlayer::vgmPSG;			// PSG

	for(int t = 0xb0; t <= 0xbf; ++t)
		m_ProcTable[t] = &CVgmPlayer::vgmTwoOp;

	for(int t = 0xc0; t <= 0xc8; ++t)
		m_ProcTable[t] = &CVgmPlayer::vgmThreeOp;

	for(int t = 0xd0; t <= 0xd1; ++t)
		m_ProcTable[t] = &CVgmPlayer::vgmThreeOp;

	m_ProcTable[0xd2] = &CVgmPlayer::vgmSCC;			// SCC

	for(int t = 0xd3; t <= 0xd6; ++t)
		m_ProcTable[t] = &CVgmPlayer::vgmThreeOp;

	m_ProcTable[0xe0] = &CVgmPlayer::vgmFourOp;
	m_ProcTable[0xe1] = &CVgmPlayer::vgmFourOp;

	// RESERVED AREA
	for(int t = 0xc9; t <= 0xcf; ++t)
		m_ProcTable[t] = &CVgmPlayer::vgmThreeOp;
	for(int t = 0xd7; t <= 0xdf; ++t)
		m_ProcTable[t] = &CVgmPlayer::vgmThreeOp;
	for(int t = 0xe2; t <= 0xff; ++t)
		m_ProcTable[t] = &CVgmPlayer::vgmFourOp;

	//
	return;
}
CVgmPlayer::~CVgmPlayer()
{
	NULL_DELETE(m_pStrm);
	return;
}

bool CVgmPlayer::SetTargetFile(const char *pFname)
{
	m_TotalStepCount = 0;
	const UINT HEADSIZE = sizeof(m_VgmHeader);
	UINT readSize;
	if( !sd_fatReadFileFrom(pFname, (int)HEADSIZE, (uint8_t*)&m_VgmHeader, &readSize) )
		return false;
	if( readSize < 64 )	
	 	return false;
	const uint8_t IDENT[4] = {0x56,0x67,0x6d,0x20,};			// "Vgm "
	if( memcmp(m_VgmHeader.ident, IDENT, sizeof(IDENT)) != 0 )
		return false;
	//
	m_pStrm->SetTargetFileName(pFname);
	const uint32_t dataOffset =
		(m_VgmHeader.Version < 0x150)
		? (0x40)
		: (0x34 + m_VgmHeader.VGM_data_offset);
	m_pStrm->SetOffSet(dataOffset);
	m_bFileIsOK = true;
	m_TotalStepCount = m_VgmHeader.Total_Number_samples;

	// setup SCC
	if( 0x161 <= m_VgmHeader.Version && m_VgmHeader.K051649_clock != 0) {
		if( m_VgmHeader.K051649_clock & 0x80000000 )
			setupSCCP();	// K052539(SCC-I,SCC+)
		else 
			setupSCC();		// K051649
	}

	return m_bFileIsOK;
}

int CVgmPlayer::GetTotalStepCount() const
{
	return m_TotalStepCount;
}

int CVgmPlayer::GetCurStepCount() const
{
	return m_CurStepCount;
}

int CVgmPlayer::GetRepeatCount() const
{
	return m_RepeatCount;
}

void CVgmPlayer::Start()
{
	m_bPlay = true;
	g_Mtc.ResetBegin();
	m_bFirst = true;
	m_RepeatCount = 0;
	m_CurStepCount = 0;
	m_WaitSamples = 0;
	m_StartTime = time_us_64();

	return;
}

void CVgmPlayer::Stop()
{
	m_bPlay = false;
}

// @return true ディスクアクセスあり
bool CVgmPlayer::FetchFile()
{
	return m_pStrm->FetchFile();
}

void CVgmPlayer::PlayLoop()
{
	if( !m_bFileIsOK || !m_bPlay)
		return;

	uint8_t cmd;
	if( !m_pStrm->Store(&cmd, sizeof(cmd)) )
		return;
	//printf("cmd:%02x\n", cmd);

	// comannd
	VGM_PROC_OP pProc = m_ProcTable[cmd];
	(this->*pProc)(cmd, m_pStrm);

	// wait
	static uint64_t oldSam = 0;
	volatile uint64_t nowSam = m_StartTime + (uint64_t)(m_WaitSamples*23);
	if( 1 < nowSam - oldSam ){
		busy_wait_until(nowSam);
		oldSam = nowSam;
	}

	m_CurStepCount = m_WaitSamples;
	return;
}

void CVgmPlayer::Mute()
{
	mgspico::t_MuteOPLL();
	mgspico::t_MutePSG();
	mgspico::t_MuteSCC();
	return;
}

bool CVgmPlayer::EnableFMPAC()
{
	bool bRec = false;
#if !defined(MGS_MUSE_MACHINA)
	static const char *pMark = "OPLL";
	static const int LEN_MARK = 8;
	char sample[LEN_MARK+1] = "\0\0\0\0\0\0\0\0";	// '\0' x LEN_MARK
	for( int cnt = 0; cnt < LEN_MARK; ++cnt) {
		sample[cnt] = (char)mgspico::t_ReadMem(0x4018 + cnt);
	}
	if( memcmp(sample+4, pMark, LEN_MARK-4) == 0) {
		uint8_t v = mgspico::t_ReadMem(0x7ff6);
		mgspico::t_WriteMem(0x7ff6, v|0x01);
		bRec = true;;
	}
#endif
	return bRec;
}

void CVgmPlayer::setupSCC()
{
	// SCC動作
	mgspico::t_OutSCC(0xBFFE, 0x00);
	mgspico::t_OutSCC(0x9000, 0x3F);
	return;
}

void CVgmPlayer::setupSCCP()
{
	// SCC+動作
	mgspico::t_OutSCC(0xBFFE, 0x20);
	mgspico::t_OutSCC(0xB000, 0x80);
	return;
}

bool CVgmPlayer::vgmPSG(const uint8_t cmd, CReadFileStream *pStrm)
{
	uint8_t dt[2];
	pStrm->Store(dt, sizeof(dt));
	mgspico::t_OutPSG(dt[0], dt[1]);
	return true;
}

bool CVgmPlayer::vgmYM2413(const uint8_t cmd, CReadFileStream *pStrm)
{
	uint8_t dt[2];
	pStrm->Store(dt, sizeof(dt));
	mgspico::t_OutOPLL(dt[0], dt[1]);
	return true;
}

bool CVgmPlayer::vgmSCC(const uint8_t cmd, CReadFileStream *pStrm)
{
	const z80memaddr_t base[] = {
		0x9800,	// waveform
		0x9880,	// frequency
		0x988A,	// volume
		0x988F,	// key on/off
		0x9800,	// waveform(SCC+)
		0x98C0,	// test register
	};
	uint8_t dt[3];
	pStrm->Store(dt, sizeof(dt));
	const z80memaddr_t addr = base[dt[0]] + dt[1];
	mgspico::t_OutSCC(addr, dt[2]);
	return true;
}

bool CVgmPlayer::vgmOneOp(const uint8_t cmd, CReadFileStream *pStrm)
{
	pStrm->Store(nullptr, 1);
	return true;
}

bool CVgmPlayer::vgmTwoOp(const uint8_t cmd, CReadFileStream *pStrm)
{
	pStrm->Store(nullptr, 2);
	return true;
}

bool CVgmPlayer::vgmThreeOp(const uint8_t cmd, CReadFileStream *pStrm)
{
	pStrm->Store(nullptr, 3);
	return true;
}

bool CVgmPlayer::vgmFourOp(const uint8_t cmd, CReadFileStream *pStrm)
{
	pStrm->Store(nullptr, 4);
	return true;
}

bool CVgmPlayer::vgmWaitNNNN(const uint8_t cmd, CReadFileStream *pStrm)
{
	uint16_t w;
	pStrm->Store(reinterpret_cast<uint8_t *>(&w), 2);
	m_WaitSamples += w;
	return true;
}

bool CVgmPlayer::vgmWait735(const uint8_t cmd, CReadFileStream *pStrm)
{
	m_WaitSamples += 735;
	return true;
}

bool CVgmPlayer::vgmWait882(const uint8_t cmd, CReadFileStream *pStrm)
{
	m_WaitSamples += 882;
	return true;
}
bool CVgmPlayer::vgmEnd(const uint8_t cmd, CReadFileStream *pStrm)
{
	m_StartTime = time_us_64();
	m_WaitSamples = 0;
	++m_RepeatCount;
	pStrm->ResetFetch();
	return true;
}

bool CVgmPlayer::vgmDataBlocks(const uint8_t cmd, CReadFileStream *pStrm)
{
	uint32_t dataSize;
	pStrm->Store(nullptr, 1);			// 0x6?
	pStrm->Store(nullptr, 1);			// data type
	pStrm->Store(reinterpret_cast<uint8_t *>(&dataSize), 4);
	pStrm->Store(nullptr, dataSize);	// 全て読み捨て
	return true;
}

bool CVgmPlayer::vgmPcmData(const uint8_t cmd, CReadFileStream *pStrm)
{
	uint32_t dataSize;
	pStrm->Store(nullptr, 1);			// 0x66
	pStrm->Store(nullptr, 1);			// chip type
	pStrm->Store(nullptr, 3);			// offset 1
	pStrm->Store(nullptr, 3);			// offset 2
	pStrm->Store(reinterpret_cast<uint8_t *>(&dataSize), 3);
	dataSize >>= 8;
	pStrm->Store(nullptr, dataSize);	// 全て読み捨て
	return true;
}

bool CVgmPlayer::vgmWait7n(const uint8_t cmd, CReadFileStream *pStrm)
{
	m_WaitSamples += cmd - 0x70 + 1;
	return true;
}

bool CVgmPlayer::vgmWait8n(const uint8_t cmd, CReadFileStream *pStrm)
{
	m_WaitSamples += cmd - 0x80 + 0;	// +1ではない。
	return true;
}

bool CVgmPlayer::vgmDACStreamControlWrite(const uint8_t cmd, CReadFileStream *pStrm)
{
	pStrm->Store(nullptr, 4);
	return true;
}
