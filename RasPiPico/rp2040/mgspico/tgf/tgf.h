#pragma once
#include <stdint.h>		// for int8_t 等のサイズが保障されているプリミティブ型

namespace tgf
{
enum MARK : uint8_t
{
	M_NOP		= 0x00,
	M_SYSINFO	= 0x01,
	M_WAIT		= 0x02,
	M_TC		= 0x03,
	M_OPLL		= 0x04,
	M_PSG		= 0x05,
	M_SCC		= 0x06,
	//
	M_ENDOFDATA	= 0xff,		// 読み込んだときに末尾に付加する
};

typedef uint32_t timecode_t;

#pragma pack(push,1)
struct ATOM
{
	MARK		mark;
	uint16_t	data1;
	uint16_t	data2;
	ATOM() : mark(M_NOP), data1(0), data2(0) {}
	ATOM(const MARK c) : mark(c), data1(0), data2(0) {}
};
#pragma pack(pop)

}  // namespace tgf