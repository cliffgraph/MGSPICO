﻿#include "stdafx.h"
#include <string.h>
#include "CReadFileStream.h"

//static uint8_t m_StrmBuff[32*1024];	// RAM容量チェック用

CReadFileStream::CReadFileStream()
{
	m_pBuff32k = GCC_NEW uint8_t[SIZE_SEGMEMT*NUM_SEGMEMTS];
	sem_init(&m_sem, 1, 1);
	init();
	return;
}

CReadFileStream::~CReadFileStream()
{
	NULL_DELETEARRAY(m_pBuff32k);
	// do nothing
	return;
}

void CReadFileStream::init()
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

void CReadFileStream::SetTargetFileName(const char *pFName)
{
	init();
	strcpy(m_filename, pFName);
	m_totalFileSize = sd_fatGetFileSize(m_filename);
	return;
}

uint32_t CReadFileStream::GetFileSize() const
{
	return m_totalFileSize;
}

// @return true ディスクアクセスあり
bool CReadFileStream::FetchFile()
{
	bool bDiskAcc = false;
	sem_acquire_blocking(&m_sem);
	int validNum = m_segs.ValidSegmentNum;
	sem_release(&m_sem);
	const int n = NUM_SEGMEMTS - validNum;
	for(int t = 0; t < n; ++t) {
		uint32_t est = m_totalFileSize - m_loadedFileSize;
		if( SIZE_SEGMEMT < est )
			est = SIZE_SEGMEMT;
		auto *pReadPos = &m_pBuff32k[SIZE_SEGMEMT*m_segs.WriteSegmentIndex];
		UINT readSize;
		bDiskAcc = true;
		if( sd_fatReadFileFromOffset( m_filename, m_loadedFileSize, est, pReadPos, &readSize)) {
			m_segs.Size[m_segs.WriteSegmentIndex] = readSize;
			m_loadedFileSize += readSize;
			m_segs.WriteSegmentIndex = (m_segs.WriteSegmentIndex +1) % NUM_SEGMEMTS;
			if( m_totalFileSize <= m_loadedFileSize ) {
				m_loadedFileSize = 0;
			}
			sem_acquire_blocking(&m_sem);
			++m_segs.ValidSegmentNum;
			sem_release(&m_sem);
		}
	}
	return bDiskAcc;
}

void CReadFileStream::updateReadIndex(SEGMENTS *pSegs, const int readSize)
{
	auto &sz = pSegs->Size[pSegs->ReadSegmentIndex];
	pSegs->ReadIndexInSegment += readSize;
	if( sz <= pSegs->ReadIndexInSegment ){
		pSegs->ReadIndexInSegment = 0;
		pSegs->ReadSegmentIndex = (pSegs->ReadSegmentIndex +1) % NUM_SEGMEMTS;
		sz = 0;
		sem_acquire_blocking(&m_sem);
		--(pSegs->ValidSegmentNum);
		sem_release(&m_sem);
	}
}

bool CReadFileStream::Store(uint8_t *pDt, const int size)
{
	if( m_totalFileSize == 0 )
		return false;
	int s = size;
	int destIndex = 0;
	bool bRetc = false;
	while( 0 < s ) {
		sem_acquire_blocking(&m_sem);
		int validNum = m_segs.ValidSegmentNum;
		sem_release(&m_sem);
		if( validNum == 0 )
			break;
		int sp = m_segs.Size[m_segs.ReadSegmentIndex] - m_segs.ReadIndexInSegment;
		if( s < sp )
			sp = s;
		auto *pReadPos = &m_pBuff32k[SIZE_SEGMEMT * m_segs.ReadSegmentIndex];
		if( pDt != nullptr ) {
			memcpy(&pDt[destIndex], &pReadPos[m_segs.ReadIndexInSegment], sp);
		}
		updateReadIndex(&m_segs, sp);
		s -= sp;
		destIndex += sp;
		bRetc = true;
	}
	return bRetc;
}
