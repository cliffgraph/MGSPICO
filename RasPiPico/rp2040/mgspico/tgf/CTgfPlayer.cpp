#include "../stdafx.h"
#include <stdio.h>
#include <memory.h>
#include <string.h>
#include "CTgfPlayer.h"
#include "../t_mgspico.h"

CTgfPlayer::CTgfPlayer()
{
	m_pStrm = GCC_NEW CReadFileStream();
	m_bFileIsOK = false;
	m_bPlay = false;
	m_RepeatCount = 0;
	m_CurStepCount = 0;
	m_TotalStepCount = 0;
	return;
}
CTgfPlayer::~CTgfPlayer()
{
	NULL_DELETE(m_pStrm);
	return;
}

bool CTgfPlayer::SetTargetFile(const char *pFname)
{
	m_pStrm->SetTargetFileName(pFname);
	auto fsize = m_pStrm->GetFileSize();
	if( 0 < fsize && (fsize%sizeof(tgf::ATOM)) == 0 ) {
		m_bFileIsOK = true;
		m_TotalStepCount = m_pStrm->GetFileSize() / sizeof(tgf::ATOM);
	}
	else{
		m_TotalStepCount = 0;
	}
	return m_bFileIsOK;
}

int CTgfPlayer::GetTotalStepCount() const
{
	return m_TotalStepCount;
}

int CTgfPlayer::GetCurStepCount() const
{
	return m_CurStepCount;
}

int CTgfPlayer::GetRepeatCount() const
{
	return m_RepeatCount;
}

void CTgfPlayer::Start()
{
	m_bPlay = true;
	g_Mtc.ResetBegin();
	m_bFirst = true;
	m_RepeatCount = 0;
	m_CurStepCount = 0;
	return;
}

void CTgfPlayer::Stop()
{
	m_bPlay = false;
}

// @return true ディスクアクセスあり
bool CTgfPlayer::FetchFile()
{
	return m_pStrm->FetchFile();
}

void CTgfPlayer::PlayLoop()
{
	if( !m_bFileIsOK || !m_bPlay)
		return;
	tgf::ATOM atom;
	if( !m_pStrm->Store(reinterpret_cast<uint8_t*>(&atom), sizeof(atom)) )
		return;
	switch(atom.mark)
	{
		case tgf::M_OPLL:
			mgspico::t_OutOPLL(atom.data1, atom.data2);
			break;
		case tgf::M_PSG:
			mgspico::t_OutPSG(atom.data1, atom.data2);
			break;
		case tgf::M_SCC:
			mgspico::t_OutSCC(atom.data1, atom.data2);
			break;
		case tgf::M_TC:
		{
#ifdef MGSPICO_3RD
			 mgspico::t_OutVSYNC(0);
#endif
			auto base = static_cast<tgf::timecode_t>((atom.data1<<16)|atom.data2);
			if( m_bFirst ){
				// 最初のtcは初期値として取り込む
				m_bFirst = false;
				m_padding = base;
			}else {
				if( base < m_oldBase )
					g_Mtc.ResetBegin();
			}
			tgf::timecode_t tc = 0;
			while( (m_padding+tc) < base && m_bPlay){
				tc = static_cast<tgf::timecode_t>(g_Mtc.GetTime()/16600);	// 16.6ms
			}
			m_oldBase = base;
			break;
		}
		// case tgf::M_NOP:
		// case tgf::M_SYSINFO:
		// case tgf::M_WAIT:
		default:
			// do nothing
			break;
	}
	if( ++m_CurStepCount == m_TotalStepCount){
		m_CurStepCount = 0;
		++m_RepeatCount;
	}

	return;
}

void CTgfPlayer::Mute()
{
	mgspico::t_MuteOPLL();
	mgspico::t_MutePSG();
	mgspico::t_MuteSCC();
	mgspico::t_OutVSYNC(0);
	return;
}

bool CTgfPlayer::EnableFMPAC()
{
	bool bRec = false;
#if defined(MGSPICO_1ST)
	static const char *pMark = "OPLL";
	static const int LEN_MARK = 8;
	char sample[LEN_MARK+1] = "\0\0\0\0\0\0\0\0";	// '\0' x LEN_MARK
	for( int cnt = 0; cnt < LEN_MARK; ++cnt) {
		sample[cnt] = (char)mgspico::t_ReadMem(0x4018 + cnt);
	}
	if( memcmp(sample+4, pMark, LEN_MARK-4) == 0) {
		printf("found OPLL: %s\n", sample);
		uint8_t v = mgspico::t_ReadMem(0x7ff6);
		mgspico::t_WriteMem(0x7ff6, v|0x01);
		bRec = true;;
	}
#endif
	// ついでにSCC-Iを使用していた時のためにSCC互換モードを有効化しておく
	mgspico::t_OutSCC(0xBFFE, 0x00);
	mgspico::t_OutSCC(0x9000, 0x3F);
	return bRec;
}

bool CTgfPlayer::EnableYAMANOOTO()
{
#if defined(MGSPICO_1ST)
	// For Yamanooto cartridge, enable PSG echo on standard ports #A0-#A3
	mgspico::t_WriteMem(0x7fff, mgspico::t_ReadMem(0x7fff) | 0x01);
	mgspico::t_WriteMem(0x7ffd, mgspico::t_ReadMem(0x7ffd) | 0x02);
	mgspico::t_WriteMem(0x7fff, mgspico::t_ReadMem(0x7fff) & 0xee);
#endif
	return true;
}


