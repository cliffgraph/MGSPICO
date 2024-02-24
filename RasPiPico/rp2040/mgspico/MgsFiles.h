#pragma once
class MgsFiles
{
public:
	struct FILESPEC{
		char name[LEN_FILE_NAME+1];
	};

private:
	static const int MAX_FILES = 1000;
	FILESPEC m_Files[MAX_FILES];
	int m_NumFiles;

public:
	MgsFiles();
	virtual ~MgsFiles();

private:
	void listupFiles(FILESPEC *pList, int *pNum, const char *pWild);

public:
	void ReadFileNames(const char *pWild);
	int GetNumFiles() const;
	bool IsEmpty() const;
	const FILESPEC *GetFileSpec(const int no) const;

};
