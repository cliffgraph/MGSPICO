#include "MgspicoSettings.h"
#include "sdfat.h"


MgspicoSettings::MgspicoSettings()
{
	setDefault(&m_Setting);
	return;
}

MgspicoSettings::~MgspicoSettings()
{
	// do nothign
	return;
}

void MgspicoSettings::setDefault(SETTINGDATA *p)
{
	p->MusicType = MUSICDATA::MGS;
	p->Rp2040Clock = RP2040CLOCK::CLK125MHZ;
	p->AutoRun = 0;
	p->ShufflePlay = 0;
	p->EnforceOPLL = 0;
	for( int t = 0; t < (int)sizeof(m_Setting.Padding); ++t)
		p->Padding[t] = 0x00;
	return;
}

int	MgspicoSettings::GetNumItems() const
{
	return NUM_MENUITEMS;
}

const MgspicoSettings::ITEM *MgspicoSettings::GetItem(const int indexItem) const
{
	static const ITEM items[] = 
	{
		{"music",		4,	{"MGS", "MuSICA", "TGF", "VGM", }	},	// MuSICA(byKINROU5)
		{"clock",		2,	{"125MHz", "240MHz", }		},
		{"auto run",	2,	{"OFF", "ON", }				},
		{"shuffle",		2,	{"OFF", "ON", }				},
		{"enf.OPLL",	2,	{"OFF", "ON", }				},
	};
	return &items[indexItem];
}

void MgspicoSettings::SetChioce(const int indexItem, const int no)
{
	switch(indexItem)
	{
		case 0:	m_Setting.MusicType = (MUSICDATA)no;		break;
		case 1:	m_Setting.Rp2040Clock = (RP2040CLOCK)no;	break;
		case 2:	m_Setting.AutoRun = no;						break;
		case 3:	m_Setting.ShufflePlay = no;					break;
		case 4:	m_Setting.EnforceOPLL = no;					break;
		default:											break;
	}
	return;
}

int MgspicoSettings::GetChioce(const int indexItem) const
{
	int no = 0;
	switch(indexItem)
	{
		case 0:	no = (int)m_Setting.MusicType;		break;
		case 1:	no = (int)m_Setting.Rp2040Clock;	break;
		case 2:	no = m_Setting.AutoRun;				break;
		case 3:	no = m_Setting.ShufflePlay;			break;
		case 4:	no = m_Setting.EnforceOPLL;			break;
		default:									break;
	}
	return no;
}

bool MgspicoSettings::ReadSettingFrom(const char *pFilePath)
{
	UINT readSize;
	if( sd_fatReadFileFrom(pFilePath, sizeof(m_Setting), (uint8_t*)&m_Setting, &readSize) ) {
		if( sizeof(m_Setting) == readSize ) {
			return true;
		}
	}
	setDefault(&m_Setting);
	return false;;
}

bool MgspicoSettings::WriteSettingTo(const char *pFilePath)
{
	if( sd_fatWriteFileTo(pFilePath, (uint8_t *)&m_Setting, sizeof(m_Setting), false) )
		return true;
	return false;
}

MgspicoSettings::MUSICDATA MgspicoSettings::GetMusicType() const
{
	return m_Setting.MusicType;
}

void MgspicoSettings::SetMusicType(const MgspicoSettings::MUSICDATA type)
{
	m_Setting.MusicType = type;
	return;
}

bool MgspicoSettings::Is240MHz() const
{
	return (m_Setting.Rp2040Clock == RP2040CLOCK::CLK240MHZ);
}

MgspicoSettings::RP2040CLOCK MgspicoSettings::GetRp2040Clock() const
{
	return m_Setting.Rp2040Clock;
}

void MgspicoSettings::SetRp2040Clock(const MgspicoSettings::RP2040CLOCK clk)
{
	m_Setting.Rp2040Clock = clk;
	return;
}

bool MgspicoSettings::GetAutoRun() const
{
	return (m_Setting.AutoRun==0)?false:true;
}

void MgspicoSettings::SetAutoRun(const bool bAuto)
{
	m_Setting.AutoRun = (bAuto)?1:0;
	return;
}

bool MgspicoSettings::GetShufflePlay() const
{
	return (m_Setting.ShufflePlay==0)?false:true;
}

void MgspicoSettings::SetShufflePlay(const bool bRnd)
{
	m_Setting.ShufflePlay = (bRnd)?1:0;
	return;
}

bool MgspicoSettings::GetEnforceOPLL() const
{
	return (m_Setting.EnforceOPLL==0)?false:true;
}

void MgspicoSettings::SetEnforceOPLL(const bool bEnforce)
{
	m_Setting.EnforceOPLL = (bEnforce)?1:0;
	return;
}


