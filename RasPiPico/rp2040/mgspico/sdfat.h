#pragma once

#include "ff.h"
#include "diskio.h"

int sd_fatMakdir(const char *pPath);
int sd_fatWriteFileTo(const char *pFileName, const uint8_t *pBuff, int size, bool bAppend = false);
bool sd_fatReadFileFrom(const char *pFileName, const int buffSize, uint8_t *pBuff, UINT *pReadSize);
bool sd_fatReadFileFromOffset(const char *pFileName, const int offset, const int buffSize, uint8_t *pBuff, UINT *pReadSize);
bool sd_fatExistFile(const char *pFileName);
bool sd_fatRemoveFile(const char *pFileName);
uint32_t sd_fatGetFileSize(const char *pFileName);

