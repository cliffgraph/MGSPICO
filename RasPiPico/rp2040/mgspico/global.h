#pragma once
#include <memory.h>
//#include "tools.h"

// ファイル名の文字長
static const int LEN_FILE_SPEC = 8;
static const int LEN_FILE_EXT = 3;
static const int LEN_FILE_NAME = LEN_FILE_SPEC+1+LEN_FILE_EXT;


// メモリ
const int MSX_EXSLOT_NUM = 4;		// 拡張スロット内のスロット数
const int MSX_PAGE_NUM = 4;			// ページ数 / Z80 main memory.
const int MEM_16K_SIZE = 16*1024;
const int MEM_32K_SIZE = 32*1024;
const int MEM_64K_SIZE = 64*1024;
struct MEM_16K
{
	uint8_t mem[MEM_16K_SIZE];
};

struct ARKWORKRAM
{
	// // 全16Kブロックの個数 12*16K = 192KB
	// static const int TOTAL_MEM_16K = 12;
	// MEM_16K		Blocks[TOTAL_MEM_16K];

	// 全16Kブロックの個数 4*16K = 64KB
	static const int TOTAL_MEM_16K = 6;
	MEM_16K		Blocks[TOTAL_MEM_16K];

	// マッパーセグメントとして使用するブロックの開始番号とブロック数
		int			NumMapperBlocks;
	//  各ページのマッパーセグメントへのポインタ
	uint8_t		*pMemSts[MSX_EXSLOT_NUM][MSX_PAGE_NUM];

	//
	ARKWORKRAM()
	{
		memset(Blocks, 0, sizeof(Blocks));
		NumMapperBlocks = TOTAL_MEM_16K;
	 	for( int t = 0; t < TOTAL_MEM_16K; t++){
			Blocks[t].mem[0] = 0x00;
		}
		SetDefaultMem();
		return;
	}
	virtual ~ARKWORKRAM()
	{
		return;
	}

	void SetVoidMem()
	{
		for( int st = 0; st < MSX_EXSLOT_NUM; ++st) {
			for( int pg = 0; pg < MSX_PAGE_NUM; ++pg) {
				pMemSts[st][pg] = nullptr;
			}
		}
		return;
	}

	void SetDefaultMem()
	{
		for( int t = 0; t < MSX_PAGE_NUM; ++t) {
			SetMemMap(0, t, t);
		}
		for( int st = 1; st < MSX_EXSLOT_NUM; ++st) {
			for( int pg = 0; pg < MSX_PAGE_NUM; ++pg) {
				pMemSts[st][pg] = nullptr;
			}
		}
		return;
	}

	inline void SetMemMap(const int slotNo, const int pgNo, const int blockNo)
	{
		pMemSts[slotNo][pgNo] =
			(blockNo < NumMapperBlocks)
			? Blocks[blockNo].mem
			: nullptr;

//		pMemSts[slotNo][pgNo] = Blocks[blockNo % NumMapperBlocks].mem;

		return;
	}

	inline uint8_t *GetPtrPage(const int slotNo, const int pgNo)
	{
		return pMemSts[slotNo][pgNo];
	}

	inline void SetPtrPage(const int slotNo, const int pgNo, uint8_t *p)
	{
		pMemSts[slotNo][pgNo] = p;
		return;
	}
};
extern ARKWORKRAM g_WorkRam;


#include "MgsFiles.h"
extern MgsFiles g_MgsFiles;
