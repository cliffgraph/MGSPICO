#include "stdafx.h"
#include <stdio.h>		// printf
#include "t_mmmspi.h"

//#define MGSPICO_3RC
#ifdef MGSPICO_3RC

namespace mmmspi
{
static const uint16_t LEN_BUFF = 1024;
#pragma pack(push,1)
static uint32_t g_Buff[LEN_BUFF];
#pragma pack(pop)
static uint16_t g_RecordNum;		// 格納数
static uint16_t g_ReadIndex;		// 読込みインデックス
static uint16_t g_WriteIndex;	// 書込みインデックス

bool Init()
{
	g_RecordNum = 0;
	g_ReadIndex = 0;
	g_WriteIndex = 0;
	//
	uint spd = spi_init( SPIMUSE, (uint)6600000 ); 		/* 2.5Mbps?? */
	sleep_ms(1000);
	printf("\nspd=%d\n", spd);
    gpio_set_function( MMC_SPIMUSE_TX_PIN, GPIO_FUNC_SPI );
    gpio_set_function( MMC_SPIMUSE_RX_PIN, GPIO_FUNC_SPI );
    gpio_set_function( MMC_SPIMUSE_SCK_PIN, GPIO_FUNC_SPI );
    /* CS# */
    gpio_init( MMC_SPIMUSE_CSN_PIN );
    gpio_set_dir( MMC_SPIMUSE_CSN_PIN, GPIO_OUT);
	//
    gpio_put( MMC_SPIMUSE_CSN_PIN, 1 );	/* Set CS# high */
	busy_wait_ms(10);

	return true;
}

void ClearBUff()
{
	g_RecordNum = 0;
	return;
}

RAM_FUNC void PushBuff(const uint32_t cmd, const uint32_t addr, const uint32_t data)
{
	if( LEN_BUFF <= g_RecordNum )
		return;
	// [5:CMD][11:ADDR][8:DATA]
	const uint32_t rec = (cmd<<19) | (addr<<8) | data;
	g_Buff[g_WriteIndex++] = rec;
	g_WriteIndex &= 0x03ff;	// 1024で0に戻す
	g_RecordNum++;
	return;
}

// RAM_FUNC bool PopBuff(uint32_t *pRec)
// {
// 	if( g_RecordNum == 0 )
// 		return false;
// 	*pRec = g_Buff[g_ReadIndex++];
// 	g_ReadIndex &= 0x03ff;	// 1024で0に戻す
// 	return true;
// }

RAM_FUNC void Present()
{
	const int num = g_RecordNum;
	for( int t = 0; t < num; ++t)
	{
		const uint32_t rec = g_Buff[g_ReadIndex++];
		g_ReadIndex &= 0x03ff;	// 1024で0に戻す
		g_RecordNum--;
		uint8_t temp[3] = {
			static_cast<uint8_t>(rec>>16),
			static_cast<uint8_t>((rec>>8)&0xff),
			static_cast<uint8_t>(rec&0xff)};
	    gpio_put( MMC_SPIMUSE_CSN_PIN, 0 );	/* Set CS# high */
		spi_write_blocking( SPIMUSE, temp, 3);
	    gpio_put( MMC_SPIMUSE_CSN_PIN, 1 );	/* Set CS# high */
	}
	return;
}

}; // namespace mmmspi

#endif // MGSPICO_3RC