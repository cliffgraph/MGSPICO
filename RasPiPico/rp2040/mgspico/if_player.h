
#include <stdint.h>
#pragma pack(push,1)
struct IF_PLAYER_PICO
{
	uint8_t	magic_spell[4];		// Players->Pico: "POK." PlayersがPicoに対して準備OKを示す
	uint8_t	playerd_ver_hl[2];	// Players->Pico: Playersバージョン. ex)0x01, 0x02 = 1.2 を示す(1.02ではない)
	uint8_t	request_from_pico;	// Pico->Players: Pico側から指示を受ける
								//					0x00: none
								//					0x01: MGS演奏を停止する
								//					0x02: 8000hのMGSデータの演奏を開始する
								//					0x03: status_of_player を 0x00 にする
	uint8_t request_res;		// Players->Pico: 
	uint8_t status_of_player;	// Players->Pico: 状態
								//					0x00 
								//					0x01 無演奏の状態
								//					0x02 演奏中
								//					0x03 演奏が終了 or 停止された
	uint16_t work_mib_addr;		// Players->Pico: MGSDRV の MIB 領域へのアドレス
	uint16_t work_track_top;	// Players->Pico: トラックワークエリアの先頭アドレス
	uint16_t work_track_size;	// Players->Pico: トラックワークエリアの1トラック分のバイト数
};

#pragma pack(pop)