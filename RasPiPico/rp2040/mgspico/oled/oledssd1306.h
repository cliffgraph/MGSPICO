#pragma once
#include <stdint.h>

enum class SSD1306 : uint8_t
{
	// Control byte
	CONTROL_CMD_SINGLE      				= 0x80,
	CONTROL_CMD_STREAM      				= 0x00,
	CONTROL_DATA_STREAM     				= 0x40,
	// Fundamental commands (pg.28)
	SET_CONTRAST            				= 0x81,
	DISPLAY_ALL_ON_RESUME   				= 0xA4,
	DISPLAY_ALL_ON_IGNORE   				= 0xA5,
	NORMAL_DISPLAY          				= 0xA6,
	INVERT_DISPLAY          				= 0xA7,
	DISPLAY_OFF             				= 0xAE,
	DISPLAY_ON              				= 0xAF,
	// Scrolling #defines (pg.28-30)
	RIGHT_HORIZONTAL_SCROLL                 = 0x26,
	LEFT_HORIZONTAL_SCROLL                  = 0x27,
	VERTICAL_AND_RIGHT_HORIZONTAL_SCROLL    = 0x29,
	VERTICAL_AND_LEFT_HORIZONTAL_SCROLL     = 0x2A,
	DEACTIVATE_SCROLL                       = 0x2E,
	ACTIVATE_SCROLL                         = 0x2F,
	SET_VERTICAL_SCROLL_AREA                = 0xA3,
	// Addressing Command Table (pg.30)
	SET_LOW_COLUMN          				= 0x00,
	SET_HIGH_COLUMN         				= 0x10,
	SET_MEMORY_ADDR_MODE    				= 0x20,    // follow with 0x00 = HORZ mode = Behave like a KS108 graphic LCD
	SET_COLUMN_RANGE        				= 0x21,    // can be used only in HORZ/VERT mode - follow with 0x00 and 0x7F = COL127
	SET_PAGE_RANGE          				= 0x22,    // can be used only in HORZ/VERT mode - follow with 0x00 and 0x07 = PAGE7
	SET_PAGE_START_ADDRESS  				= 0xB0,
	// Hardware Config (pg.31)
	SET_DISPLAY_START_LINE  				= 0x40,
	SET_SEGMENT_REMAP_LOW   				= 0xA0,
	SET_SEGMENT_REMAP_HIGH  				= 0xA1,
	SET_MULTIPLEX_RATIO     				= 0xA8,    // follow with 0x3F = 64 MUX
	SET_COM_SCAN_INC        				= 0xC0,
	SET_COM_SCAN_DEC        				= 0xC8,
	SET_DISPLAY_OFFSET      				= 0xD3,    // follow with 0x00
	SET_COM_PIN_MAP         				= 0xDA,    // follow with 0x12
	// Timing and Driving Scheme (pg.32)
	SET_DISPLAY_CLK_DIV     				= 0xD5,    // follow with 0x80
	SET_PRECHARGE           				= 0xD9,    // follow with 0xF1
	SET_VCOMH_DESELCT       				= 0xDB,    // follow with 0x30
	NOP                     				= 0xE3,    // NOP
	// Charge Pump (pg.62)
	SET_CHARGE_PUMP 						= 0x8D,    // follow with 0x14
};


class CSsd1306I2c
{
private:
	static const int WIDTH = 128;
	static const int HEIGHT = 64;
	
private:
#pragma pack(push,1)
	struct SENDBUFF
	{
		SSD1306	I2CCmd;
		uint8_t Buff[(HEIGHT/8) * WIDTH];
		// Buff[0] は 画面左上から縦に8ドットに相当する
		// Buff[0]=0x03は、画面左上とその下の１ドットの計２か所に点が表示される
	};
#pragma pack(pop)
	SENDBUFF m_Buffer;
public:
	CSsd1306I2c();
	virtual ~CSsd1306I2c();
private:
	void initI2C();
	void initSSD1306();
	void clearDisplayBuff();

public:
	void Start();
	void ResetI2C();
	void Clear();
	void Present();
	void Pixel(int x, int y, bool bDot);
	void Line(int sx, int sy, int dx, int dy, bool bDot);
	void Box(int sx, int sy, int lx, int ly, bool bDot);
	void Char8x16(const int lx, const int ly, const char ch, const bool bInvert = false);
	void Char16x16(const int lx, const int ly, const char ch);
	void Char16x32(const int lx, const int ly, const char ch);
	void Strings8x16(const int sx, const int sy, const char *pStr, const bool bInvert = false);
	void Strings16x16(const int sx, const int sy, const char *pStr);
	void Strings16x32(const int sx, const int sy, const char *pStr);
	void Bitmap(const int sx, const int sy, const uint8_t *pBitmap, const int lx, const int ly);

};
