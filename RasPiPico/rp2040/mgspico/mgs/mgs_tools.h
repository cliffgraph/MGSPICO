
#pragma once

#pragma pack(push,1)
struct STR_MGSDRVCOM
{
	uint8_t		jumpcode[3];	// +0000	C3, xx, xx
	uint8_t		id[6];			// +0003	"MGSDRV"
	uint16_t	sizeof_driver;	// +0009	MGSDRV本体のサイズ
	uint8_t		ver_L;			// +000B	MGSDRV Version(L)
	uint8_t		ver_H;			// +000C	MGSDRV Version(H)
	uint8_t		startof_driver;	// +000D	
};
#pragma pack(pop)

bool t_Mgs_GetPtrBodyAndSize(
	const STR_MGSDRVCOM *p, const uint8_t **pBody, uint16_t *pSize);
