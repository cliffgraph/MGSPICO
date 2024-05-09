#include "sdfat.h"
#include <stdio.h>
#include <memory.h>
#include "def_gpio.h"

static FATFS g_fs;

int sd_fatMakdir(const char *pPath)
{
	disk_init_spi();

    FRESULT  ret;
    ret = f_mount(&g_fs, "", 1 );
    if( ret != FR_OK ) {
        return false;
    }
    ret = f_mkdir(pPath);
    if( ret != FR_OK ) {
        return false;
    }
	return true;
}

int sd_fatWriteFileTo(const char *pFileName, const uint8_t *pBuff, int size, bool bAppend )
{
	disk_init_spi();

    FRESULT  ret;
    ret = f_mount(&g_fs, "", 1 );
    if( ret != FR_OK ) {
        return false;
    }
    FIL  fil;
	BYTE mode = (bAppend) ? (FA_WRITE|FA_OPEN_APPEND) : (FA_WRITE|FA_CREATE_ALWAYS);
    ret = f_open( &fil, pFileName, mode);
    if( ret != FR_OK ) {
        return false;
    }
    UINT  wsize;
    ret = f_write( &fil, pBuff, (UINT)size, &wsize );
    if( ret != FR_OK ) {
	    f_close( &fil );
        return false;
    }
    f_close( &fil );
    return  (int)wsize;
}

bool sd_fatReadFileFrom(const char *pFileName, const int buffSize, uint8_t *pBuff, UINT *pReadSize)
{
	disk_init_spi();

    FRESULT  ret;
    ret = f_mount( &g_fs, "", 1 );
    if( ret != FR_OK ) {
        return false;
    }
    FIL  fil;
    ret = f_open( &fil, pFileName, FA_READ|FA_OPEN_EXISTING );
    if( ret != FR_OK ) {
        return false;
    }
    ret = f_read( &fil, pBuff, (UINT)buffSize, pReadSize);
    if( ret != FR_OK ) {
	    f_close( &fil );
        return false;
    }
    f_close( &fil );
    return true;
}

bool sd_fatReadFileFromOffset(
	const char *pFileName, const int offset,
	const int buffSize, uint8_t *pBuff, UINT *pReadSize)
{
	// offset は、ファイル先頭からの相対位置です。
	// この位置からファイルを読みだします。

	disk_init_spi();

    FRESULT  ret;
    ret = f_mount( &g_fs, "", 1 );
    if( ret != FR_OK ) {
        return false;
    }
    FIL  fil;
    ret = f_open( &fil, pFileName, FA_READ|FA_OPEN_EXISTING );
    if( ret != FR_OK ) {
        return false;
    }

    ret = f_lseek( &fil, (FSIZE_t)offset);
    if( ret != FR_OK ) {
        return false;
    }

    ret = f_read( &fil, pBuff, (UINT)buffSize, pReadSize);
    if( ret != FR_OK ) {
	    f_close( &fil );
        return false;
    }
    f_close( &fil );
    return true;
}


bool sd_fatExistFile(const char *pFileName)
{
	disk_init_spi();

    FRESULT  ret;
    ret = f_mount( &g_fs, "", 1 );
    if( ret != FR_OK ) {
        return false;
    }
	FILINFO info;
	ret = f_stat(pFileName, &info);
    if( ret != FR_OK ) {
        return false;
    }
	if (info.fattrib&AM_DIR) {
        return false;
    }

	disk_deinit_spi();
	return true;
}

bool sd_fatRemoveFile(const char *pFileName)
{
	disk_init_spi();

    FRESULT  ret;
    ret = f_mount( &g_fs, "", 1 );
    if( ret != FR_OK ) {
        return false;
    }
	ret = f_unlink(pFileName);
	bool bResult = (ret==FR_OK) ? true : false;
	return bResult;
}

uint32_t sd_fatGetFileSize(const char *pFileName)
{
	disk_init_spi();

    FRESULT  ret;
    ret = f_mount( &g_fs, "", 1 );
    if( ret != FR_OK ) {
        return 0;
    }
	FILINFO info;
	ret = f_stat(pFileName, &info);
	const uint32_t sz = (ret==FR_OK) ? info.fsize : 0;
	return sz;
}

