#pragma once
#include <stdint.h>

namespace vgm
{
// https://www.smspower.org/uploads/Music/vgmspec170.txt
//       00  01  02  03   04  05  06  07   08  09  0A  0B  0C  0D  0E  0F
// 0x00 ["Vgm " ident   ][EoF offset     ][Version        ][SN76489 clock  ]
// 0x10 [YM2413 clock   ][GD3 offset     ][Total # samples][Loop offset    ]
// 0x20 [Loop # samples ][Rate           ][SN FB ][SNW][SF][YM2612 clock   ]
// 0x30 [YM2151 clock   ][VGM data offset][Sega PCM clock ][SPCM Interface ]
// 0x40 [RF5C68 clock   ][YM2203 clock   ][YM2608 clock   ][YM2610/B clock ]
// 0x50 [YM3812 clock   ][YM3526 clock   ][Y8950 clock    ][YMF262 clock   ]
// 0x60 [YMF278B clock  ][YMF271 clock   ][YMZ280B clock  ][RF5C164 clock  ]
// 0x70 [PWM clock      ][AY8910 clock   ][AYT][AY Flags  ][VM] *** [LB][LM]
// 0x80 [GB DMG clock   ][NES APU clock  ][MultiPCM clock ][uPD7759 clock  ]
// 0x90 [OKIM6258 clock ][OF][KF][CF] *** [OKIM6295 clock ][K051649 clock  ]
// 0xA0 [K054539 clock  ][HuC6280 clock  ][C140 clock     ][K053260 clock  ]
// 0xB0 [Pokey clock    ][QSound clock   ] *** *** *** *** [Extra Hdr ofs  ]
// 0xC0  *** *** *** ***  *** *** *** ***  *** *** *** ***  *** *** *** ***
// 0xD0  *** *** *** ***  *** *** *** ***  *** *** *** ***  *** *** *** ***
// 0xE0  *** *** *** ***  *** *** *** ***  *** *** *** ***  *** *** *** ***
// 0xF0  *** *** *** ***  *** *** *** ***  *** *** *** ***  *** *** *** ***

const uint8_t IC_YM2413		= 0x51;
const uint8_t IC_AY8910		= 0xa0;		// 0xA0 aa dd : AY8910, write value dd to register aa
const uint8_t IC_SCC		= 0xd2;		// 0xD2 pp aa dd : SCC1 port pp, write value dd to register aa

#pragma pack(push,1)
struct HEADER
{
	uint8_t		ident[4];		// "Vgm " ident
	uint32_t	EoF_offset;
	uint32_t	Version;
	uint32_t	SN76489_clock;
	uint32_t	YM2413_clock;
	uint32_t	GD3_offset;
	uint32_t	Total_Number_samples;
	uint32_t	Loop_offset;
	uint32_t	Loop_Number_samples;
	uint32_t	Rate;
	uint16_t	SN_FB;
	uint8_t	 	SNW;
	uint8_t		SF;
	uint32_t	YM2612_clock;
	uint32_t	YM2151_clock;
	uint32_t	VGM_data_offset;
	uint32_t	Sega_PCM_clock;
	uint32_t	SPCM_Interface;
	uint32_t	RF5C68_clock;
	uint32_t	YM2203_clock;
	uint32_t	YM2608_clock;
	uint32_t	YM2610_B_clock;
	uint32_t	YM3812_clock;
	uint32_t	YM3526_clock;
	uint32_t	Y8950_clock;
	uint32_t	YMF262_clock;
	uint32_t	YMF278B_clock;
	uint32_t	YMF271_clock;
	uint32_t	YMZ280B_clock;
	uint32_t	RF5C164_clock;
	uint32_t	PWM_clock;
	uint32_t	AY8910_clock;
	uint8_t		AYT;
	uint16_t	AY_Flags;
	uint8_t		VM;
	uint8_t		padding1;
	uint8_t		LB;
	uint8_t		LM;
	uint32_t	GB_DMG_clock;
	uint32_t	NES_APU_clock;
	uint32_t	MultiPCM_clock;
	uint32_t	uPD7759_clock;
	uint32_t	OKIM6258_clock;
	uint8_t		OF;
	uint8_t		KF;
	uint8_t		CF;
	uint8_t		padding2;
	uint32_t	OKIM6295_clock;
	uint32_t	K051649_clock;		// SCC
	uint32_t	K054539_clock;
	uint32_t	HuC6280_clock;
	uint32_t	C140_clock;
	uint32_t	K053260_clock;
	uint32_t	Pokey_clock;
	uint32_t	QSound_clock;
	uint32_t	padding3;
	uint32_t	Extra_Hdr_ofs;
	uint32_t	padding4[4*4];
};
#pragma pack(pop)

}; // namespace vgm
