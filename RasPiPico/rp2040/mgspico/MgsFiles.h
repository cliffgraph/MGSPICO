#pragma once
class MgsFiles
{
public:
	static const int MAXLEN_FILENAME = 8+1+3;
	struct FILESPEC{
		char name[MAXLEN_FILENAME+1];
	};

private:
	static const int MAX_FILES = 1000;
	FILESPEC m_Files[MAX_FILES];
	int m_NumFiles;

public:
	MgsFiles();
	virtual ~MgsFiles();

private:
	void listupFiles(FILESPEC *pList, int *pNum);

public:
	void ReadFileNames();
	int GetNumFiles() const;
	const FILESPEC *GetFileSpec(const int no) const;

};
