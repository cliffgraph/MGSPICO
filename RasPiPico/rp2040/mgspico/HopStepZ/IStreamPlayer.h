#pragma once
class IStreamPlayer
{
public:
	virtual ~IStreamPlayer(){return;}

public:
	virtual bool SetTargetFile(const char *pFname) = 0;
	virtual int GetTotalStepCount() const = 0;
	virtual int GetCurStepCount() const = 0;
	virtual int GetRepeatCount() const = 0;
	virtual void Start() = 0;
	virtual void Stop() = 0;
	virtual bool FetchFile() = 0;
	virtual void PlayLoop() = 0;
	virtual void Mute() = 0;
	virtual bool EnableFMPAC() = 0;
	virtual bool EnableYAMANOOTO() = 0;
};

