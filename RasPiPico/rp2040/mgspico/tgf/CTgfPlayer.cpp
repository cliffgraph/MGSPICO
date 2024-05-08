#include "../stdafx.h"
#include <string.h>
#include <stdio.h>			// printf
#include "CTgfPlayer.h"
#include "../t_mgspico.h"

static uint8_t g_StrmBuff[32*1024];

CTgfPlayer::CTgfPlayer()
{
	m_pStrm = GCC_NEW CReadFileStream(g_StrmBuff);
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
			outOPLL(atom.data1, atom.data2);
			break;
		case tgf::M_PSG:
			outPSG(atom.data1, atom.data2);
			break;
		case tgf::M_SCC:
			outSCC(atom.data1, atom.data2);
			break;
		case tgf::M_TC:
		{
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
		case tgf::M_NOP:
		case tgf::M_SYSINFO:
		case tgf::M_WAIT:
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
	// 音量を0にする
	// それ以外のレジスタはいじらない
	// OPLL
	outOPLL(0x30, 0x0F);	// Vol = 0
	outOPLL(0x31, 0x0F);
	outOPLL(0x32, 0x0F);
	outOPLL(0x33, 0x0F);
	outOPLL(0x34, 0x0F);
	outOPLL(0x35, 0x0F);
	outOPLL(0x36, 0x0F);
	outOPLL(0x37, 0xFF);
	outOPLL(0x38, 0xFF);
	// PSG
	outPSG(0x08, 0x00);
	outPSG(0x09, 0x00);
	outPSG(0x0A, 0x00);
	// SCC+
	outSCC(0xbffe, 0x30);
	outSCC(0xb000, 0x80);
	outSCC(0xb8aa, 0x00);
	outSCC(0xb8ab, 0x00);
	outSCC(0xb8ac, 0x00);
	outSCC(0xb8ad, 0x00);
	outSCC(0xb8ae, 0x00);
	outSCC(0xb8af, 0x00);	// turn off, CH.A-E
	// SCC
	outSCC(0xbffe, 0x00);
	outSCC(0x9000, 0x3f);
	outSCC(0x988a, 0x00);
	outSCC(0x988b, 0x00);
	outSCC(0x988c, 0x00);
	outSCC(0x988d, 0x00);
	outSCC(0x988e, 0x00);
	outSCC(0x98af, 0x00);	// turn off, CH.A-E
	return;
}

bool CTgfPlayer::EnableFMPAC()
{
	bool bRec = false;
	static const char *pMark = "PAC2OPLL";
	static const int MARKLEN = 8;
	char sample[MARKLEN+1] = {'\0','\0','\0','\0','\0','\0','\0','\0','\0',};
	for( int cnt = 0; cnt < MARKLEN; ++cnt) {
		sample[cnt] = (char)mgspico::t_ReadMem(0x4018 + cnt);
	}
	if( memcmp(sample, pMark, MARKLEN) == 0) {
		uint8_t v = mgspico::t_ReadMem(0x7ff6);
		mgspico::t_WriteMem(0x7ff6, v|0x01);
		bRec = true;;
	}
	return bRec;
}


void CTgfPlayer::outOPLL(const uint16_t addr, const uint16_t data)
{
	mgspico::t_OutPort(0x7C, (uint8_t)addr);
	busy_wait_us(4);
	mgspico::t_OutPort(0x7D, (uint8_t)data);
	busy_wait_us(24);
	return;
}

void CTgfPlayer::outPSG(const uint16_t addr, const uint16_t data)
{
	mgspico::t_OutPort(0xA0, (uint8_t)addr);
	busy_wait_us(1);
	mgspico::t_OutPort(0xA1, (uint8_t)data);
	return;
}

void CTgfPlayer::outSCC(const z80memaddr_t addr, const uint16_t data)
{
	mgspico::t_WriteMem(addr, data);
	return;
}
