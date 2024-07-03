

// クロックジェネレーターSi5351Aの出力周波数 fout を設定する方法。
// ●まずは原理を理解する。
// Si5351Aは、水晶振動子とPLL回路と分周器から構成される。各回路のつながりは下記の通り
//		[水晶振動子] --> [PLL回路] ---> [分周器] ---> 出力信号　　 ･･･(a)
// 水晶振動子は決められた周波数の出力信号を出力する素子である。
// PLL回路は、入力信号の周波数を、 n倍にして出力する回路である。
// 分周器は、入力信号の周波数を、1/m倍にして出力する回路である。
// PLL回路と分周器は、与えるパラメータによって、n、m を調整できる回路である。
// よって、水晶振動子の周波数を、PLL回路、分周器を使って、n倍したり、1/m倍したりして目的の
// 周波数の信号を得るようにする。そのためのn、mを決定する。
//		(水晶振動子 * ｎ) / ｍ => 出力周波数
//
// ◆PLL回路の n は、Pa、Pb、Pcの３つのパラメータで決定する。
// この３つの値を決定してI2C通信でSi5351Aに渡してあげればPLL回路の動作が決定する。
// PLL回路の入力周波数xtalと、出力周波数fvcoとの関係は、
//	fvco = xtal × (Pa + (Pb / Pc))  ･･･(b)
//  ｎ = (Pa + (Pb / Pc)             ･･･(c)
// ※Pa、Pb、Pcは、採りえることができる値の範囲が決められていることに注意する
//		15 <= Pa <= 90
//		 0 <= Pb <= 1048575
//		 1 <= Pc <= 1048575
// ◆分周器の m は、Da、Db、Dcの３つのパラメータで決定する。
// PLL同様、この３つの値を決定してI2C通信でSi5351Aに渡してあげれば分周器の動作が決定する。
// 分周器の入力周波数fvcoと、出力周波数foutとの関係は、
//	fout = fvco ÷ (Da + (Db / Dc))  ･･･(d)
//  ｎ = (Da + (Db / Dc)             ･･･(e)
// ※ｎは、6 <= ｎ <= 1800 の範囲であることが決められていることに注意する
// 
// ◆つまり、出力信号 fout を出力するようにSi5351Aをセッティングするには、
// Pa、Pb、Pc、Da、Db、Dcの６個のパラメータを決定する必要がある。
//
// ●求めたい周波数foutが、水晶振動子の周波数xtal より小さい場合、、、
//			 PLL回路の出力周波数fvcoを決め打ちし、分周器のmをfoutから算出する、を試みる。
// fout=21477270[Hz]、xtal=25[MHz]。この時、適当にfvco=400[MHz]と決める。
// PLL回路のパラメータを求める
// 	fvco = xtal × ｎ
//  ｎ = fvco / xtal
//	ｎ = (Pa + (Pb / Pc))なので、fvco / xtalの整数部をPa、少数部を(Pb / Pc)としてｎを３つのパラメータに分解する。
// Pa = (fvco / xtal)の整数部 = Fix(400000000 / 25000000) = 16  ※Paはこれで決定
// (Pb / Pc) = (fvco / xtal)の小数部 = (fvco%xtal)/xtal = (400000000 % 25000000) / 25000000 = 0 / 21477270 = 0
// (Pb / Pc) = 0になってしまうので、Pa = 16から15にして、(Pb / Pc) = 1にする。
// ゆえに、
// Pa = 15, Pb = 1, Pc = 1
// 分周器のパラメータを求める
// 	fout = fvco / ｍ
//  ｍ = fvco / fout;
//	ｍ = (Da + (Db / Dc))なので、fvco / foutの整数部をDa、少数部を(Db / Dc)としてｍを３つのパラメータに分解する。
// Da = (fvco / fout)の整数部 = Fix(400000000/21477270) = 18  ※Daはこれで決定
// (Db / Dc) = (fvco / fout)の小数部 = (fvco%fout)/fout = (400000000%21477270) / 21477270 = 13409140 / 21477270 = 約0.624
// ただ(fvco / fout)の小数部の小数部が解っても、Db、Dcのそれぞれがわからなければ意味がない。
// ここで、Dcの値を、PLLの場合と同じように1048575に決め打ちする。
// Db = (fvco % fout) / fout * Dc
//    = (400000000%21477270) / 21477270 * 1048575 = 654668(小数点以下切り捨て）
// ゆえに、
// Da = 18, Db = 654668, Dc = 1048575
//
// この値を、I2Cを使って、Si5351Aに伝える。の前に、Pa、Pb、Pc、Da、Db、Dcを、Si5351Aのレジスタ形式に変換する。
// CLK0を使用することとする。
// MSNA_P1[17:0] = Pa * 128 + Floor( ( Pb / Pc ) * 128 ) - 0x200
// MSNA_P2[19:0] = Pb * 128 – Pc * Floor( ( Pb / Pc ) * 128 )
// MSNA_P3[19:0] = Pc
// MS0_P1[17:0] = Da * 128 + Floor( ( Db / Dc ) * 128 ) - 0x200
// MS0_P2[19:0] = Db * 128 – Pc * Floor( ( Db / Dc ) * 128 )
// MS0_P3[19:0] = Dc

 // PLLA_SRC = 0	// PLL-A に水晶振動子を選択
 // XTAL_CL = 0x2	// 8pFを選択（理由は知らない勉強してない）、

#include "pico/stdlib.h"
#include "hardware/i2c.h"

#define I2C_ADDR_Si5351 	0x60			// 0b0110 0000
#define GPIO_DEV_I2C		i2c0
#define GPIO_PIN_I2C_SDA 	0				// i2c0, GPIO_0
#define GPIO_PIN_I2C_SCL 	1				// i2c0, GPIO_1
#define I2C_BAUDRATE		(100*1000)		// I2C CLK = 50KHz

static void initI2C_for_Si5351()
{
	i2c_init(GPIO_DEV_I2C, I2C_BAUDRATE);
	gpio_set_function(GPIO_PIN_I2C_SDA, GPIO_FUNC_I2C);
	gpio_set_function(GPIO_PIN_I2C_SCL, GPIO_FUNC_I2C);
	gpio_pull_up(GPIO_PIN_I2C_SDA);
	gpio_pull_up(GPIO_PIN_I2C_SCL);
	busy_wait_ms(10);
	return;
}

void sendReg8(const uint8_t reg, const uint8_t dt)
{
	const uint32_t WAITV = 500;
	uint8_t dts[2] = { reg, dt };
	i2c_write_blocking(
		GPIO_DEV_I2C, I2C_ADDR_Si5351, dts, sizeof(dts),	false);
	busy_wait_us(WAITV);
	return;
}

static void setup_Si5351_CLK0_21477270_Hz()
{
	const uint32_t FOUT = 21477270;		// 21.47727[MHz]　出力したい周波数
	const uint32_t XTAL = 25000000;		// 25[MHz] 水晶発振子の周波数
	const uint32_t FVCO = 900000000;	// 900[MHz] PLL回路の出力周波数（決め打ち値）

	const uint32_t Pc = 1048575;			// 決め打ち
	const uint32_t Pb = static_cast<uint32_t>(((FVCO % XTAL) / static_cast<float>(XTAL)) * Pc);
	const uint32_t Pa = FVCO / XTAL;
	const uint32_t MSNA_P1 = Pa * 128 + ((Pb / Pc) * 128) - 0x200;
	const uint32_t MSNA_P2 = Pb * 128 - Pc * ((Pb / Pc) * 128);
	const uint32_t MSNA_P3 = Pc;
	
	const uint32_t Dc = 1048575;			// 決め打ち
	const uint32_t Db = static_cast<uint32_t>(((FVCO % FOUT) / static_cast<float>(FOUT)) * Dc);
	const uint32_t Da = FVCO / FOUT;
	const uint32_t MS0_P1 = Da * 128 + ((Db / Dc) * 128) - 0x200;
	const uint32_t MS0_P2 = Db * 128 - Pc * (( Db / Dc) * 128);
	const uint32_t MS0_P3 = Dc;

	// MSNA_P1-3
	sendReg8(26, static_cast<uint8_t>(MSNA_P3>>8) & 0xff);
	sendReg8(27, static_cast<uint8_t>(MSNA_P3>>0) & 0xff);
	sendReg8(28, static_cast<uint8_t>(MSNA_P1>>16) & 0x03);
	sendReg8(29, static_cast<uint8_t>(MSNA_P1>>8) & 0xff);
	sendReg8(30, static_cast<uint8_t>(MSNA_P1>>0) & 0xff);
	sendReg8(31,
		(static_cast<uint8_t>(MSNA_P3>>(16-4)) & 0xf0) |
		(static_cast<uint8_t>(MSNA_P2>>(16+0)) & 0x0f) );
	sendReg8(32, static_cast<uint8_t>(MSNA_P2>>8) & 0xff);
	sendReg8(33, static_cast<uint8_t>(MSNA_P2>>0) & 0xff);

	// MS0_P1-3
	sendReg8(42, static_cast<uint8_t>(MS0_P3>>8) & 0xff);
	sendReg8(43, static_cast<uint8_t>(MS0_P3>>0) & 0xff);
	sendReg8(44, 
		(static_cast<uint8_t>(MS0_P1>>16) & 0x03) | 
		0x00 ); // R0_DIV = 1
	sendReg8(45, static_cast<uint8_t>(MS0_P1>>8) & 0xff);
	sendReg8(46, static_cast<uint8_t>(MS0_P1>>0) & 0xff);
	sendReg8(47,
		(static_cast<uint8_t>(MS0_P3>>(16-4)) & 0xf0) | 
		(static_cast<uint8_t>(MS0_P2>>(16+0)) & 0x0f) );
	sendReg8(48, static_cast<uint8_t>(MS0_P2>>8) & 0xff);
	sendReg8(49, static_cast<uint8_t>(MS0_P2>>0) & 0xff);

	// (177)Reset PLL
	sendReg8(177, 0xa0);

	// (16)CLK0 Control
	// 	MS0 = Integer mode.
	//	MS0の入力は、PLLA
	//	CLK0の入力は、MS0
	//	CLK0出力、8mA
	sendReg8(16, 0x4f);
	return;
}

static void setup_Si5351_CLK1__3579545_Hz()
{
	const uint32_t FOUT = 3579545;		// 3.579545[MHz]　出力したい周波数
	const uint32_t XTAL = 25000000;		// 25[MHz] 水晶発振子の周波数
	const uint32_t FVCO = 900000000;	// 900[MHz] PLL回路の出力周波数（決め打ち値）

	const uint32_t Pc = 1048575;			// 決め打ち
	const uint32_t Pb = static_cast<uint32_t>(((FVCO % XTAL) / static_cast<float>(XTAL)) * Pc);
	const uint32_t Pa = FVCO / XTAL;
	const uint32_t MSNA_P1 = Pa * 128 + ((Pb / Pc) * 128) - 0x200;
	const uint32_t MSNA_P2 = Pb * 128 - Pc * ((Pb / Pc) * 128);
	const uint32_t MSNA_P3 = Pc;
	
	const uint32_t Dc = 1048575;			// 決め打ち
	const uint32_t Db = static_cast<uint32_t>(((FVCO % FOUT) / static_cast<float>(FOUT)) * Dc);
	const uint32_t Da = FVCO / FOUT;
	const uint32_t MS1_P1 = Da * 128 + ((Db / Dc) * 128) - 0x200;
	const uint32_t MS1_P2 = Db * 128 - Pc * (( Db / Dc) * 128);
	const uint32_t MS1_P3 = Dc;

	// MSNB_P1-3
	sendReg8(34, static_cast<uint8_t>(MSNA_P3>>8) & 0xff);
	sendReg8(35, static_cast<uint8_t>(MSNA_P3>>0) & 0xff);
	sendReg8(36, static_cast<uint8_t>(MSNA_P1>>16) & 0x03);
	sendReg8(37, static_cast<uint8_t>(MSNA_P1>>8) & 0xff);
	sendReg8(38, static_cast<uint8_t>(MSNA_P1>>0) & 0xff);
	sendReg8(39,
		(static_cast<uint8_t>(MSNA_P3>>(16-4)) & 0xf0) |
		(static_cast<uint8_t>(MSNA_P2>>(16+0)) & 0x0f) );
	sendReg8(40, static_cast<uint8_t>(MSNA_P2>>8) & 0xff);
	sendReg8(41, static_cast<uint8_t>(MSNA_P2>>0) & 0xff);

	// MS1_P1-3
	sendReg8(50, static_cast<uint8_t>(MS1_P3>>8) & 0xff);
	sendReg8(51, static_cast<uint8_t>(MS1_P3>>0) & 0xff);
	sendReg8(52, 
		(static_cast<uint8_t>(MS1_P1>>16) & 0x03) | 
		0x00 ); // R0_DIV = 1
	sendReg8(53, static_cast<uint8_t>(MS1_P1>>8) & 0xff);
	sendReg8(54, static_cast<uint8_t>(MS1_P1>>0) & 0xff);
	sendReg8(55,
		(static_cast<uint8_t>(MS1_P3>>(16-4)) & 0xf0) | 
		(static_cast<uint8_t>(MS1_P2>>(16+0)) & 0x0f) );
	sendReg8(56, static_cast<uint8_t>(MS1_P2>>8) & 0xff);
	sendReg8(57, static_cast<uint8_t>(MS1_P2>>0) & 0xff);

	// (177)Reset PLL
	sendReg8(177, 0xa0);

	// (17)CLK1 Control
	// 	MS1 = Integer mode.
	//	MS1の入力は、PLLA
	//	CLK1の入力は、MS1
	//	CLK1出力、8mA
	sendReg8(17, 0x6f);	// 0b0110_1111
	return;
}

void t_SetupSi5351()
{
	initI2C_for_Si5351();

	// Reg.24 CLK3–0 Disable State is LOW
	// Reg.25 CLK7–4 Disable State is LOW
	sendReg8(24, 0x00);
	sendReg8(25, 0x00);
	// Reg.3 まず全CLKをdisable
	sendReg8(3, 0xff);
	// Reg.2 interrupt mask
	sendReg8(2, 0x00);
	// Reg.15 PLLA_SRC = 0, PLLB_SRC = 0
	sendReg8(15, 0x00);
	// Reg.16-23 Power Down
	for( int t = 0; t < 8; ++t){
		sendReg8(16+t, 0x80);
	}
	// Reg.183 XTAL_CL = 0x2	// 8pFを選択
	sendReg8(183, 0x92); // xx010010b, xx = 10=8pF -> 10010010b

	// CLK0 for SCC
	setup_Si5351_CLK0_21477270_Hz();
	// CLK1 for OPLL and PSG
	setup_Si5351_CLK1__3579545_Hz();

	// Reg.3 CLK0, CLK1の出力をEnbale
	sendReg8(3, 0xfc);

	i2c_deinit(GPIO_DEV_I2C);
	return;

}
