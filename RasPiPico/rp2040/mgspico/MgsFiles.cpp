// #include <stdint.h>		// for int8_t 等のサイズが保障されているプリミティブ型
// #include <stdlib.h>
// #include <string>
#include <string.h>

#include "global.h"
#include "MgsFiles.h"
#include "ff/ff.h"
#include "ff/diskio.h"
#include "sdfat.h"

MgsFiles::MgsFiles() 
{
	m_NumFiles = 0;
	return;
}

MgsFiles::~MgsFiles()
{
	// do nothing
	return;
}

void
MgsFiles::listupFiles(FILESPEC *pList, int *pNum, const char *pWild)
{
	for( int t = 0; t < MAX_FILES; ++t ) {
		pList[t].name[0] = '\0';
	}
	*pNum = 0;

	static FATFS g_fs;
    FRESULT ret = f_mount( &g_fs, "", 1 );
    if( ret != FR_OK ) {
        return;
    }
	DIR dirObj;
	FILINFO fno;
	FRESULT fr = f_findfirst(&dirObj, &fno, "\\", pWild);
    while (fr == FR_OK && fno.fname[0] && *pNum < MAX_FILES) {
		strcpy(pList[*pNum].name, fno.fname);
        fr = f_findnext(&dirObj, &fno); 
		++*pNum;
    }
	f_closedir(&dirObj);
	return;
};

void
MgsFiles::ReadFileNames(const char *pWild)
{
	listupFiles(m_Files, &m_NumFiles, pWild);
	return;
}

int
MgsFiles::GetNumFiles() const
{
	return m_NumFiles;
}

bool
MgsFiles::IsEmpty() const
{
	return (m_NumFiles==0)?true:false;
}


const MgsFiles::FILESPEC
*MgsFiles::GetFileSpec(const int no) const
{
	return (no <= 0 || m_NumFiles < no) ? nullptr : &m_Files[no-1];
}

