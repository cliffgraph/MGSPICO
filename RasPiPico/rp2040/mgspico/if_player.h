
#include <stdint.h>
#pragma pack(push,1)
struct IF_PLAYER_PICO
{
	uint8_t	magic_spell[4];		//	+0	 Players->Pico: "POK." PlayersがPicoに対して準備OKを示す
	uint8_t	playerd_ver_hl[2];	//	+4	 Players->Pico: Playersバージョン. ex)0x01, 0x02 = 1.2 を示す(1.02ではない)
	uint8_t	request_from_pico;	//	+6	 Pico->Players: Pico側から指示を受ける
								//							0x00: none
								//							0x01: MGS演奏を停止する
								//							0x02: 8000hのMGSデータの演奏を開始する
								//							0x03: status_of_player を 0x00 にする
	uint8_t request_res;		//	+7	 Players->Pico: 
	uint8_t status_of_player;	//	+8	 Players->Pico: 状態
								//							0x00 
								//							0x01 無演奏の状態
								//							0x02 演奏中
								//							0x03 演奏が終了 or 停止された
	uint16_t work_mib_addr;		//	+9	 Players->Pico: MGSDRV の MIB 領域へのアドレス
	uint16_t work_track_top;	//	+11	 Players->Pico: トラックワークエリアの先頭アドレス
	uint16_t work_track_size;	//	+13	 Players->Pico: トラックワークエリアの1トラック分のバイト数
	uint8_t	laps;				//	+15	 Players->Pico: 再生回数（1回目の再生中は1、2回目で2になる。また、status_of_playerがPLAYING以外のときは不定値）
};

enum class STATUSOFPLAYER : uint8_t
{
	NONE		= 0x00,
	IDLE		= 0x01,	// 何もしていない
	PLAYING		= 0x02,	// 演奏中
	FINISH		= 0x03,	// 演奏が完了した
	FADEOUT		= 0x04,	// フェードアウト中
};

#pragma pack(pop)