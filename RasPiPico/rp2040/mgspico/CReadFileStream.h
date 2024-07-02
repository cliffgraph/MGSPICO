#pragma once
#include <stdlib.h>
#include <stdint.h>		// for int8_t 等のサイズが保障されているプリミティブ型
#include "pico/multicore.h"
#include "../global.h"
#include "../sdfat.h"

class CReadFileStream
{
private:
	static const int SIZE_SEGMEMT = 4*1024;
	static const int NUM_SEGMEMTS = 8;	// 4K*8 = 32K
	uint8_t *m_pBuff32k;				// バッファへのポインタ(32KBytes）
	struct SEGMENTS
	{
		int Size[NUM_SEGMEMTS];
		int	ValidSegmentNum;
		int	WriteSegmentIndex;
		int	ReadSegmentIndex;
		int	ReadIndexInSegment;
	};
	SEGMENTS m_segs;
	//
	char m_filename[LEN_FILE_NAME+1];
	uint32_t m_totalFileSize;			// ファイル総サイズ
	uint32_t m_loadedFileSize;			// ファイルサイズに対する読込済みサイズ
	uint32_t m_offset;					// 読み込み対象外にする先頭サイズ
	semaphore_t m_sem;

public:
	CReadFileStream();
	virtual ~CReadFileStream();
private:
	void init();
	void reset();
	void updateReadIndex(SEGMENTS *pSegs, const int readSize);
public:
	void SetTargetFileName(const char *pFName);
	uint32_t GetFileSize() const;
	void SetOffSet(const uint32_t off);
	void ResetFetch();
	bool FetchFile();
	bool Store(uint8_t *pDt, const int size);
};

