#include "oledssd1306.h"
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include <memory.h>
#include "FONT8X16MIN.h"
#include "FONT16X32.h"
#include "FONT16X16.h"

#define I2C_ADDR_SSD1306		(0x3c)
#define GPIO_DEV_I2C			i2c1
#define GPIO_PIN_I2C_SDA		18	// i2c1, GPIO_18
#define GPIO_PIN_I2C_SCL		19	// i2c1, GPIO_19
static const uint I2C_BAUDRATE	= 1000*1000;

static inline void swapInt(int &x, int &y)
{
	const int tmp = y;
	y = x;
	x = tmp;
	return;
}

CSsd1306I2c::CSsd1306I2c()
{
	// do nothing
	return;
}
CSsd1306I2c::~CSsd1306I2c()
{
	// do nothing
	return;
}
void CSsd1306I2c::initI2C()
{
	// gpio_init(GPIO_PIN_I2C_SDA);
	// gpio_put(GPIO_PIN_I2C_SDA, 1);
	// gpio_set_dir(GPIO_PIN_I2C_SDA, GPIO_OUT);

	// gpio_init(GPIO_PIN_I2C_SCL);
	// gpio_put(GPIO_PIN_I2C_SCL, 1);
	// gpio_set_dir(GPIO_PIN_I2C_SCL, GPIO_OUT);

	i2c_init(GPIO_DEV_I2C, I2C_BAUDRATE);
	gpio_set_function(GPIO_PIN_I2C_SDA, GPIO_FUNC_I2C);
	gpio_set_function(GPIO_PIN_I2C_SCL, GPIO_FUNC_I2C);
	gpio_pull_up(GPIO_PIN_I2C_SDA);
	gpio_pull_up(GPIO_PIN_I2C_SCL);
	busy_wait_ms(10);
	return;
}

void CSsd1306I2c::initSSD1306()
{
	static const uint8_t buff[] = 
	{
		(uint8_t)SSD1306::CONTROL_CMD_STREAM,					// [0]
		(uint8_t)SSD1306::DISPLAY_OFF,							// [1]		Display off
		(uint8_t)SSD1306::SET_MULTIPLEX_RATIO,		0x3F,		// [2-3]	Set MUX Ratio A8h, 3Fh
		(uint8_t)SSD1306::SET_DISPLAY_OFFSET,		0x00,		// [4-5]	Set Display Offset D3h, 00h
		(uint8_t)SSD1306::SET_DISPLAY_START_LINE,				// [6]		Set Display Start Line 40h
		(uint8_t)SSD1306::SET_SEGMENT_REMAP_LOW,				// [7]		Set Segment re-map A0h/A1h
		(uint8_t)SSD1306::SET_COM_SCAN_INC,						// [8]		Set COM Output Scan Direction C0h/C8h
		(uint8_t)SSD1306::SET_COM_PIN_MAP,			0x12,		// [9-10]	Set COM Pins hardware configuration DAh, 12h
		(uint8_t)SSD1306::SET_CONTRAST,				0x7F,		// [11-12]	Set Contrast Control 81h, 7Fh
		(uint8_t)SSD1306::DISPLAY_ALL_ON_RESUME,				// [13]		Disable Entire Display On A4h
		(uint8_t)SSD1306::NORMAL_DISPLAY,						// [14]		Set Normal Display A6h（データビット１を黒）
		(uint8_t)SSD1306::SET_DISPLAY_CLK_DIV,		0x80,		// [15-16]	Set Osc Frequency D5h, 80h
		(uint8_t)SSD1306::SET_MEMORY_ADDR_MODE,		0x00,		// [17-18]	[Horizontal addressing mode]
		(uint8_t)SSD1306::SET_COLUMN_RANGE,			0x00, 0x7F,	// [19-21]
		(uint8_t)SSD1306::SET_PAGE_RANGE,			0x00, 0x07,	// [22-24]
		(uint8_t)SSD1306::DEACTIVATE_SCROLL,					// [25]
		(uint8_t)SSD1306::SET_CHARGE_PUMP,			0x14,		// [26-27]	Enable charge pump regulator 8Dh, 14h */
		(uint8_t)SSD1306::DISPLAY_ON,							// [28] 	Display On AFh
	};
	i2c_write_blocking(
		GPIO_DEV_I2C, I2C_ADDR_SSD1306,
		buff, sizeof(buff),
		false);
	return;
};

void CSsd1306I2c::clearDisplayBuff()
{
	memset(m_Buffer.Buff, 0x00, sizeof(m_Buffer.Buff));
	return;
}

void CSsd1306I2c::Start()
{
	initI2C();
	initSSD1306();
	return;
}

void CSsd1306I2c::ResetI2C()
{
	initSSD1306();
	return;
}

void CSsd1306I2c::Pixel(const int x, const int y, bool bDot)
{
	const int pageNo = y / 8;
	const int bitNo = y % 8;
	const int index = pageNo * 128 + x;
	if( bDot ) {
		m_Buffer.Buff[index] |= 0x01 << bitNo;
	}
	else{
		m_Buffer.Buff[index] &= (0x01 << bitNo) ^ 0xff;
	}
	return;
}

void CSsd1306I2c::Line(int sx, int sy, int dx, int dy, bool bDot)
{
	if(dx < sx) {
		swapInt(sx, dx);
	}
	if(dy < sy) {
		swapInt(sy, dy);
	}
	int w = dx - sx +1;
	int h = dy - sy +1;
	if( h < w ) {
		for(int t = 0; t < w; ++t) {
			int y = (int)(((float)t)/w * h);
			Pixel(sx+t, sy+y, bDot);
		}
	}
	else
	{
		for(int t = 0; t < h; ++t) {
			int x = (int)(((float)t)/h * w);
			Pixel(sx+x, sy+t, bDot);
		}
	}
	return;
}

/** 矩形
 * @param sx 矩形左上X座標
 * @param sy 矩形左上Y座標
 * @param lx 横幅 0～
 * @param ly 横幅 0～
*/
void CSsd1306I2c::Box(int sx, int sy, int lx, int ly, bool bDot)
{
	Line(sx, sy, sx+lx, sy, bDot);
	Line(sx, sy+ly, sx+lx, sy+ly, bDot);
	Line(sx, sy, sx, sy+ly, bDot);
	Line(sx+lx, sy, sx+lx, sy+ly, bDot);
	return;
}


/** バッファ内容をクリアする
 * 
*/
void CSsd1306I2c::Clear()
{
	clearDisplayBuff();
	return;
}

/** バッファ内容を実際に表示させる
 * 
*/
void CSsd1306I2c::Present()
{
	m_Buffer.I2CCmd = SSD1306::CONTROL_DATA_STREAM;
	i2c_write_blocking(
		GPIO_DEV_I2C, I2C_ADDR_SSD1306,
		reinterpret_cast<uint8_t*>(&m_Buffer), sizeof(m_Buffer), false);
	return;
}

void CSsd1306I2c::Char8x16(
	const int sx, const int sy, const char ch, 
	const bool bInvert)
{
	if( ch < 0x20 || 0x7f < ch )
		return;

	const int y = sy / 8;
	const int windex = y * 128 + sx;
	const int findex = (ch - 0x20) * 16;
	if( !bInvert ){
		for( int cy = 0; cy < 2; ++cy ){
			for( int x = 0; x < 8; ++x ){
				uint8_t ptn = FONT8X16MIN_CHARBITMAP[findex + cy*8 + x];
				m_Buffer.Buff[windex+x+cy*128] = ptn;
			}
		}
	}
	else {
		for( int cy = 0; cy < 2; ++cy ){
			for( int x = 0; x < 8; ++x ){
				uint8_t ptn = FONT8X16MIN_CHARBITMAP[findex + cy*8 + x];
				m_Buffer.Buff[windex+x+cy*128] = ptn ^ 0xff;
			}
		}
	}
	return;
}

void CSsd1306I2c::Char16x16(const int sx, const int sy, const char ch )
{
	if( ch < '0' || ':' < ch )
		return;

	const int y = sy / 8;
	const int windex = y * 128 + sx;
	const int findex = (ch - '0') * 32;
	for( int cy = 0; cy < 2; ++cy ){
		for( int x = 0; x < 16; ++x ){
			uint8_t ptn = FONT16X16_CHARBITMAP[findex + cy*16 + x];
			m_Buffer.Buff[windex+x+cy*128] = ptn;
		}
	}
	return;
}

void CSsd1306I2c::Char16x32(const int sx, const int sy, const char ch )
{
	if( ch < '0' || ':' < ch )
		return;

	const int y = sy / 8;
	const int windex = y * 128 + sx;
	const int findex = (ch - '0') * 64;
	for( int cy = 0; cy < 4; ++cy ){
		for( int x = 0; x < 16; ++x ){
			uint8_t ptn = FONT16X32_CHARBITMAP[findex + cy*16 + x];
			m_Buffer.Buff[windex+x+cy*128] = ptn;
		}
	}
	return;
}


/** 8x16フォントの文字列を描画する
 * @param sx, sy 表示座標(ただしyは、8の倍数にアライメントされる）
 * @param pStr 文字列へのポインタ。 ASCIIの内、0x30～0x7f の文字のみ
 * @param bInvert 白／黒の領域を反転する
*/
void CSsd1306I2c::Strings8x16(
	const int sx, const int sy, const char *pStr, const bool bInvert)
{
	const int len = strlen(pStr);
	for( int t = 0; t < len; ++t ){
		Char8x16(sx+8*t, sy, pStr[t], bInvert);
	}
	return;
}


/** 16x16フォントの文字列を描画する（0-9,:のみ）
 * @param sx, sy 表示座標(ただしyは、8の倍数にアライメントされる）
 * @param pStr 文字列へのポインタ。 ASCIIの内、0x30～0x40 の文字のみ
 */
void CSsd1306I2c::Strings16x16(
	const int sx, const int sy, const char *pStr)
{
	const int len = strlen(pStr);
	for( int t = 0; t < len; ++t ){
		Char16x16(sx+16*t, sy, pStr[t]);
	}
	return;
}

/** 16x32フォントの文字列を描画する（0-9,:のみ）
 * @param sx, sy 表示座標(ただしyは、8の倍数にアライメントされる）
 * @param pStr 文字列へのポインタ。 ASCIIの内、0x30～0x40 の文字のみ
 */
void CSsd1306I2c::Strings16x32(
	const int sx, const int sy, const char *pStr)
{
	const int len = strlen(pStr);
	for( int t = 0; t < len; ++t ){
		Char16x32(sx+16*t, sy, pStr[t]);
	}
	return;
}

void CSsd1306I2c::Bitmap(
	const int sx, const int sy, const uint8_t *pBitmap, const int lx, const int ly)
{
	const int y = sy / 8;
	const int cylen = ly / 8;

	const int windex = y * 128 + sx;
	for( int cy = 0; cy < cylen; ++cy ){
		for( int x = 0; x < lx; ++x ){
			uint8_t ptn = pBitmap[cy*lx + x];
			m_Buffer.Buff[windex+x+cy*128] = ptn;
		}
	}
}
