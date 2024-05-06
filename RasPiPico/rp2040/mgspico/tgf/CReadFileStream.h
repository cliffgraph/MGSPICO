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
		uint16_t Size[NUM_SEGMEMTS];
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
	semaphore_t m_sem;

public:
	CReadFileStream(uint8_t *pBuff32k)
	{
		sem_init(&m_sem, 1, 1);
		m_pBuff32k = pBuff32k;
		init();
		return;
	}

	virtual ~CReadFileStream()
	{
		// do nothing
		return;
	}

private:
	void init()
	{
		for( int t = 0; t < NUM_SEGMEMTS; ++t){
			m_segs.Size[t] = 0;
		}
		m_segs.ValidSegmentNum = 0;
		m_segs.WriteSegmentIndex = 0;
		m_segs.ReadSegmentIndex = 0;
		m_segs.ReadIndexInSegment = 0;
		//
		m_totalFileSize = 0;
		m_loadedFileSize = 0;
		return;
	}

public:
	void SetTargetFileName(const char *pFName)
	{
		init();
		strcpy(m_filename, pFName);
		m_totalFileSize = sd_fatGetFileSize(m_filename);
		return;
	}

	uint32_t GetFileSize() const
	{
		return m_totalFileSize;
	}

	bool FetchFile()
	{
		sem_acquire_blocking(&m_sem);
		int validNum = m_segs.ValidSegmentNum;
		sem_release(&m_sem);
		const int n = NUM_SEGMEMTS - validNum;
		for(int t = 0; t < n; ++t) {
			if( m_totalFileSize <= m_loadedFileSize ) {
				m_loadedFileSize = 0;
			}
			uint32_t est = m_totalFileSize - m_loadedFileSize;
			est = (4095<est)?4095:est;
			auto *pReadPos = &m_pBuff32k[SIZE_SEGMEMT*m_segs.WriteSegmentIndex];
			UINT readSize;
			if( sd_fatReadFileFromOffset( m_filename, m_loadedFileSize, est, pReadPos, &readSize)) {
				//printf("load:%d - %d, %d\n", m_segs.WriteSegmentIndex, readSize, m_loadedFileSize);
				m_segs.Size[m_segs.WriteSegmentIndex] = readSize;
				m_loadedFileSize += readSize;
				m_segs.WriteSegmentIndex = (m_segs.WriteSegmentIndex +1) % NUM_SEGMEMTS;
				sem_acquire_blocking(&m_sem);
				++m_segs.ValidSegmentNum;
				sem_release(&m_sem);
			}
		}
		return false;
	}

	bool Store(uint8_t *pDt, const int size)
	{
		bool bRetc = false;
		sem_acquire_blocking(&m_sem);
		int validNum = m_segs.ValidSegmentNum;
		sem_release(&m_sem);
		if( m_totalFileSize == 0 || validNum == 0 )
			return bRetc;
		auto &sz = m_segs.Size[m_segs.ReadSegmentIndex];
		if( m_segs.ReadIndexInSegment+size <= sz ) { 
			auto *pReadPos = &m_pBuff32k[SIZE_SEGMEMT*m_segs.ReadSegmentIndex];
			memcpy(pDt, &pReadPos[m_segs.ReadIndexInSegment], size);
			bRetc = true;
		}
		//printf("Sore:%d-%d\n", m_segs.ReadSegmentIndex, m_segs.ReadIndexInSegment);
		m_segs.ReadIndexInSegment += size;
		if( sz <= m_segs.ReadIndexInSegment ){
			m_segs.ReadIndexInSegment = 0;
			m_segs.ReadSegmentIndex = (m_segs.ReadSegmentIndex +1) % NUM_SEGMEMTS;
			sz = 0;
			sem_acquire_blocking(&m_sem);
			--m_segs.ValidSegmentNum;
			sem_release(&m_sem);
		}
		return bRetc;
	}
};

