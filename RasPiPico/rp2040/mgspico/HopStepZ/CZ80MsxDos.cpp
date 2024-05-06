#include <assert.h>
#include "../stdafx.h"
#include "msxdef.h"
#include "../CUTimeCount.h"
#include "CZ80MsxDos.h"
#include "CMsxMemSlotSystem.h"
#include "CMsxIoSystem.h"
//#include <chrono>
//#include <thread>	// for sleep_for
#include "pico/multicore.h"	// for sleep_us
#include "pico/stdlib.h"

CZ80MsxDos::CZ80MsxDos()
{
	m_pMemSys = nullptr;
	m_pIoSys = nullptr;
	setup();
	ResetCpu();
	return;
}

CZ80MsxDos::~CZ80MsxDos()
{
	// do nothing
	return;
}

void CZ80MsxDos::ResetCpu()
{
	m_bHalt = false;
	m_bIFF1 = m_bIFF2 = false;
	m_IM = INTERRUPTMODE0;
	m_R.Reset();
	return;
}

void CZ80MsxDos::ResetCpu(const z80memaddr_t pc, const z80memaddr_t sp)
{
	m_bHalt = false;
	m_bIFF1 = m_bIFF2 = false;
	m_IM = INTERRUPTMODE0;
	m_R.Reset();
	m_R.PC = pc;
	m_R.SP = sp;
	return;
}

void CZ80MsxDos::Push16(const uint16_t w)
{
	m_pMemSys->Write(--m_R.SP, (w>>8)&0xff);
	m_pMemSys->Write(--m_R.SP, (w>>0)&0xff);
	return;
}

uint16_t CZ80MsxDos::Pop16()
{
	uint16_t v;
	v = m_pMemSys->Read(m_R.SP++);
	v |= m_pMemSys->Read(m_R.SP++) << 8;
	return v;
}

z80memaddr_t CZ80MsxDos::GetPC() const
{
	return m_R.PC;
}

z80memaddr_t CZ80MsxDos::GetSP() const
{
	return m_R.SP;
}

const CZ80Regs &CZ80MsxDos::GetCurrentRegs() const
{
	return m_R;
}

void CZ80MsxDos::SetSubSystem(
	CMsxMemSlotSystem *pMem, CMsxIoSystem *pIo)
{
	m_pMemSys = pMem;
	m_pIoSys = pIo;
	return;
}

void __time_critical_func(CZ80MsxDos::Execution())
{
	OpCodeMachine();
	InterruptMachine();
	BiosFunctionCall();
	MsxDosFunctionCall();
	ExtendedBiosFunctionCall();
	MainRomFunctionCall();
	return;
}

void __time_critical_func(CZ80MsxDos::OpCodeMachine())
{
#if !defined(NDEBUG)
	m_PcHist.push_back(m_R);
#endif

	assert(m_pMemSys != nullptr);
	assert(m_pIoSys != nullptr);

	if( !m_bHalt ) {
		m_R.CodePC = m_R.PC++;
		m_R.Code = m_pMemSys->Read(m_R.CodePC);
		auto pFunc = m_Op.Single[m_R.Code];
		(this->*pFunc)();
	}
	return;
}

void __time_critical_func(CZ80MsxDos::InterruptMachine())
{
	uint32_t VSYNC = 16600;			// 16.6ms
	if( m_Tim.GetTime() <= VSYNC )
		return;
	m_Tim.ResetBegin();

	// if( !m_bIFF1 )
	// 	return;
	// // H.TIMI の呼び出し
	// Push16(m_R.PC);
	// m_R.PC = 0xFD9F;
	// op_DI();

	// カウントアップ
	uint16_t v = m_pMemSys->ReadWord(0xFC9E);
	m_pMemSys->WriteWord(0xFC9E, ++v);
	return;
}

//
const static z80memaddr_t BIOS_RDSLT		= 0x000C;	// 指定スロットのメモリ・１バイト読み出し
const static z80memaddr_t BIOS_WRSLT		= 0x0014;	// 指定スロットのメモリ・１バイト書き込み
const static z80memaddr_t BIOS_CALSL		= 0x001C;	// インタースロット・コール
const static z80memaddr_t BIOS_ENASLT		= 0x0024;	// スロット切り替え
const static z80memaddr_t BIOS_CALLF 		= 0x0030;	// 別のスロットにあるルーチンを呼び出す
const static z80memaddr_t BIOS_KEYINT 		= 0x0038;	// タイマ割り込みの処理ルーチンを実行します。
const static z80memaddr_t BIOS_HSZ_ST16MS	= 0x0039;	// (HopStepZオリジナル)16msウェイトの基点
const static z80memaddr_t BIOS_HSZ_WT16MS	= 0x003A;	// (HopStepZオリジナル)16ms経過まで待つ

const static z80memaddr_t DOS_SYSTEMCALL	= 0x0005;	// DOSシステムコール
const static dosfuncno_t  DOS_CONOUT		= 0x02;		// コンソールへ 1 文字出力
const static dosfuncno_t  DOS_STROUT		= 0x09;		// コンソールへ 文字列の出力（'$'終端）
const static dosfuncno_t  DOS_TERM			= 0x62;		// エラーコードを伴った終了
const static dosfuncno_t  DOS_GENV			= 0x6B;		// 環境変数の獲得
const static dosfuncno_t  DOS_SENV			= 0x6C;		// 環境変数のセット
const static dosfuncno_t  DOS_DOSVER		= 0x6F;		// DOSのバージョン番号の獲得



/** PCの値を見張っていて、特定の位置にPCが来たら対応するファンクションを実行する
*/
void __time_critical_func(CZ80MsxDos::BiosFunctionCall())
{
	if( 0x0100 <= m_R.PC )
		return;
	switch(m_R.PC)
	{
		case BIOS_HSZ_ST16MS:
		{
			m_Tim16ms.ResetBegin();
			op_RET();
			break;
		}
		case BIOS_HSZ_WT16MS:
		{
			static uint32_t overTime = 0;
			const uint32_t VSYNCTIME = 16600;	// 16.6ms
			uint32_t et = m_Tim16ms.GetTime();
			uint32_t padd = VSYNCTIME - et;
			if( et < VSYNCTIME ){
				if( 0 < overTime ) {
					if( padd < overTime ) {
						overTime -= padd;
					}
					else {
						busy_wait_us(padd-overTime);
						overTime = 0;
					}
				}
				else {
					busy_wait_us(padd);
				}
			}
			else {
				const uint32_t ov = et-VSYNCTIME;
				if( overTime < ov)		// overTime += ov; だとやりすぎになる
					overTime = ov;
				// printf( "%d\n", et);
			}
			op_RET();
			break;
		}
		case BIOS_RDSLT:
		{
			auto pageNo = static_cast<msxpageno_t>(m_R.GetHL()/Z80_PAGE_SIZE);
			const msxslotno_t tempNo = m_pMemSys->GetSlotByPage(pageNo);
			const msxslotno_t slotNo = m_R.A & 0x0f;
			m_pMemSys->SetSlotToPage(pageNo, slotNo);
			m_R.A = m_pMemSys->Read(m_R.GetHL());
			m_pMemSys->SetSlotToPage(pageNo, tempNo);
			op_DI();
			op_RET();
			break;
		}
		case BIOS_WRSLT:
		{
			DEBUG_BREAK;
			op_DI();
			op_RET();
			break;
		}
		case BIOS_CALSL:
		{
			auto slotNo = static_cast<msxslotno_t>((m_R.IY >> 8) & 0x0F);
			z80memaddr_t ad = m_R.IX;
			if (slotNo == 0x00 && ad == 0x183/*GETCPU*/) {
				m_R.A = 0x00; /*Z80モードで動いてます*/
			//	m_R.A = 0x11; /*R800+DRAMモードで動いてます*/
			}
			else {
				DEBUG_BREAK;
			}
			op_RET() ;
			break;
		}
		case BIOS_ENASLT:
		{
			auto pageNo = static_cast<msxpageno_t>((m_R.H >> 6) & 0x03);
			auto slotNo = static_cast<msxslotno_t>((m_R.A >> 0) & 0x0F);
			m_pMemSys->SetSlotToPage(pageNo, slotNo);
			m_pMemSys->Write(0xF341 + pageNo, m_R.A);
			op_DI();
			op_RET();
			break;
		}
		case BIOS_CALLF:
		{
			z80memaddr_t retad = Pop16();
			z80memaddr_t v;
			retad++;		// スロット番号は 0x80なので、ひとまず、、無視
			v = m_pMemSys->Read(retad++);
			v |= m_pMemSys->Read(retad++) << 8;
			Push16(retad);
			m_R.PC = v;
			break;
		}
		case BIOS_KEYINT:
		{
			op_RET();
		}
		
		// do nothing
		case 0x0000:
		case DOS_SYSTEMCALL:
			break; 

		default:
		{
			// std::wcout
			// 		<< _T("\n **HopStepZ >> Not supported BIOS call address = ")
			// 		<< std::hex << (int)m_R.PC << _T("\n");
			DEBUG_BREAK;
			break;
		}

	}
	return;
}

/** 拡張BIOS（メモリマッパ）
 * @note
 * PCの値を見張っていて、特定の位置にPCが来たら対応するファンクションを実行する
 * 情報源：http://www.ascat.jp/tg/tg2.html
*/
void __time_critical_func(CZ80MsxDos::ExtendedBiosFunctionCall())
{
static const z80memaddr_t MM_ALL_SEG 	= 0xFF01;	// 16Kのセグメントを割り付ける
static const z80memaddr_t MM_FRE_SEG 	= 0xFF02;	// 16Kのセグメントを開放する
static const z80memaddr_t MM_RD_SEG		= 0xFF03;	// セグメント番号Ａの番地ＨＬの内容を読む
static const z80memaddr_t MM_WR_SEG		= 0xFF04;	// セグメント番号Ａの番地ＨＬにＥの値を書く
static const z80memaddr_t MM_CAL_SEG	= 0xFF05;	// インターセグメントコール（インデックスレジスタ）
static const z80memaddr_t MM_CALLS		= 0xFF06;	// インターセグメントコール（インラインパラメーター）
static const z80memaddr_t MM_PUT_PH		= 0xFF07;	// Ｈレジスタの上位２ビットのページを切り換える
static const z80memaddr_t MM_GET_PH		= 0xFF08;	// Ｈレジスタの上位２ビットのページのセグメント番号を得る
static const z80memaddr_t MM_PUT_P0		= 0xFF09;	// ページ０のセグメントを切り換える
static const z80memaddr_t MM_GET_P0		= 0xFF0A;	// ページ０の現在のセグメント番号を得る
static const z80memaddr_t MM_PUT_P1		= 0xFF0B;	// ページ１のセグメントを切り換える
static const z80memaddr_t MM_GET_P1		= 0xFF0C;	// ページ１の現在のセグメント番号を得る
static const z80memaddr_t MM_PUT_P2		= 0xFF0D;	// ページ２のセグメントを切り換える
static const z80memaddr_t MM_GET_P2		= 0xFF0E;	// ページ２の現在のセグメント番号を得る
static const z80memaddr_t MM_PUT_P3		= 0xFF0F;	// 何もせずに戻る
static const z80memaddr_t MM_GET_P3		= 0xFF10;	// ページ３の現在のセグメント番号を得る
const static z80memaddr_t BIOS_EXTBIO 	= 0xFFCA;	// 拡張BIOS

	if( m_R.PC < MM_ALL_SEG )
		return;
	switch(m_R.PC)
	{
		case BIOS_EXTBIO:
		{
			if( m_R.GetDE() == 0x0402 ){
				// マッパサポートルーチンの先頭アドレスを得る
				m_R.A = 4;			// CRam64k プライマリマッパの総セグメント数、
				m_R.B = 3;			// プライマリマッパのスロット番号、
				m_R.C = m_R.A - 4;	// Cにプライマリマッパの未使用セグメント数、(4つは既にDOSが使用しているとする）
				m_R.SetHL(0xFF00);	// HLにジャンプテーブルの先頭アドレスを返す。（仮に0xFF00と定めた）
				// 
				//  +0H　ALL_SEG　　 16Kのセグメントを割り付ける
				m_pMemSys->Write(0xFF00+ 0, 0xC3);
				m_pMemSys->Write(0xFF00+ 1, 0x01);
				m_pMemSys->Write(0xFF00+ 2, 0xFF);
				//  +3H　FRE_SEG　　 16Kのセグメントを開放する
				m_pMemSys->Write(0xFF00+ 3, 0xC3);
				m_pMemSys->Write(0xFF00+ 4, 0x02);
				m_pMemSys->Write(0xFF00+ 5, 0xFF);
				//  +6H　RD_SEG　　　セグメント番号Ａの番地ＨＬの内容を読む
				m_pMemSys->Write(0xFF00+ 6, 0xC3);
				m_pMemSys->Write(0xFF00+ 7, 0x03);
				m_pMemSys->Write(0xFF00+ 8, 0xFF);
				//  +9H　WR_SEG　　　セグメント番号Ａの番地ＨＬにＥの値を書く
				m_pMemSys->Write(0xFF00+ 9, 0xC3);
				m_pMemSys->Write(0xFF00+10, 0x04);
				m_pMemSys->Write(0xFF00+11, 0xFF);
				//  +CH　CAL_SEG　　 インターセグメントコール（インデックスレジスタ）
				m_pMemSys->Write(0xFF00+12, 0xC3);
				m_pMemSys->Write(0xFF00+13, 0x05);
				m_pMemSys->Write(0xFF00+14, 0xFF);
				//  +FH　CALLS　　　 インターセグメントコール（インラインパラメーター）
				m_pMemSys->Write(0xFF00+15, 0xC3);
				m_pMemSys->Write(0xFF00+16, 0x06);
				m_pMemSys->Write(0xFF00+17, 0xFF);
				// +12H　PUT_PH　　　Ｈレジスタの上位２ビットのページを切り換える
				m_pMemSys->Write(0xFF00+18, 0xC3);
				m_pMemSys->Write(0xFF00+19, 0x07);
				m_pMemSys->Write(0xFF00+20, 0xFF);
				// +15H　GET_PH　　　Ｈレジスタの上位２ビットのページのセグメント番号を得る
				m_pMemSys->Write(0xFF00+21, 0xC3);
				m_pMemSys->Write(0xFF00+22, 0x08);
				m_pMemSys->Write(0xFF00+23, 0xFF);
				// +18H　PUT_P0　　　ページ０のセグメントを切り換える
				m_pMemSys->Write(0xFF00+24, 0xC3);
				m_pMemSys->Write(0xFF00+25, 0x09);
				m_pMemSys->Write(0xFF00+26, 0xFF);
				// +1BH　GET_P0　　　ページ０の現在のセグメント番号を得る
				m_pMemSys->Write(0xFF00+27, 0xC3);
				m_pMemSys->Write(0xFF00+28, 0x0A);
				m_pMemSys->Write(0xFF00+29, 0xFF);
				// +1EH　PUT_P1　　　ページ１のセグメントを切り換える
				m_pMemSys->Write(0xFF00+30, 0xC3);
				m_pMemSys->Write(0xFF00+31, 0x0B);
				m_pMemSys->Write(0xFF00+32, 0xFF);
				// +21H　GET_P1　　　ページ１の現在のセグメント番号を得る
				m_pMemSys->Write(0xFF00+33, 0xC3);
				m_pMemSys->Write(0xFF00+34, 0x0C);
				m_pMemSys->Write(0xFF00+35, 0xFF);
				// +24H　PUT_P2　　　ページ２のセグメントを切り換える
				m_pMemSys->Write(0xFF00+36, 0xC3);
				m_pMemSys->Write(0xFF00+37, 0x0D);
				m_pMemSys->Write(0xFF00+38, 0xFF);
				// +27H　GET_P2　　　ページ２の現在のセグメント番号を得る
				m_pMemSys->Write(0xFF00+39, 0xC3);
				m_pMemSys->Write(0xFF00+40, 0x0E);
				m_pMemSys->Write(0xFF00+41, 0xFF);
				// +2AH　PUT_P3　　　何もせずに戻る
				m_pMemSys->Write(0xFF00+42, 0xC3);
				m_pMemSys->Write(0xFF00+43, 0x0F);
				m_pMemSys->Write(0xFF00+44, 0xFF);
				// +2DH　GET_P3　　　ページ３の現在のセグメント番号を得る
				m_pMemSys->Write(0xFF00+45, 0xC3);
				m_pMemSys->Write(0xFF00+46, 0x10);
				m_pMemSys->Write(0xFF00+47, 0xFF);
			}
			else if (m_R.GetDE() == 0xf000) {
				// 何者だろう？
			}
			else{
				// std::wcout
				// 	<< _T("\n **HopStepZ >> Not supported Extended-BIOS call device No. = ")
				// 	<< std::hex << (int)m_R.D << _T("\n");
				DEBUG_BREAK;
			}
			op_RET();
			break;
		}
		case MM_ALL_SEG:	// 16Kのセグメントを割り付ける
		{
			int pageNo;
			if( patchingMapper(&pageNo, m_R.A+1)) {
				m_R.A = static_cast<uint8_t>(pageNo);
				m_R.F.C = 0;
			}
			else{
				m_R.F.C = 1;
			}
			op_RET();
			break;
		}
		case MM_FRE_SEG:	// 16Kのセグメントを開放する
		{
			m_MemoryMapper[m_R.A] = 0;
			m_R.F.C = 0;
			op_RET();
			break;
		}
		case MM_RD_SEG:		// セグメント番号Ａの番地ＨＬの内容を読む
			DEBUG_BREAK;
			op_RET();
			break;
		case MM_WR_SEG:		// セグメント番号Ａの番地ＨＬにＥの値を書く
			DEBUG_BREAK;
			op_RET();
			break;
		case MM_CAL_SEG:	// インターセグメントコール（インデックスレジスタ）
			DEBUG_BREAK;
			op_RET();
			break;
		case MM_CALLS:		// インターセグメントコール（インラインパラメーター）
			DEBUG_BREAK;
			op_RET();
			break;
		case MM_PUT_PH:		// Ｈレジスタの上位２ビットのページを切り換える
			DEBUG_BREAK;
			op_RET();
			break;
		case MM_GET_PH:		// Ｈレジスタの上位２ビットのページのセグメント番号を得る
			DEBUG_BREAK;
			op_RET();
			break;
		case MM_PUT_P0:		// ページ０のセグメントを切り換える
			m_pIoSys->Out(0xFC+0, m_R.A);
			op_RET();
			break;
		case MM_GET_P0:		// ページ０の現在のセグメント番号を得る
			m_R.A = m_pIoSys->In(0xFC+0);
			op_RET();
			break;
		case MM_PUT_P1:		// ページ１のセグメントを切り換える
			m_pIoSys->Out(0xFC+1, m_R.A);
			op_RET();
			break;
		case MM_GET_P1:		// ページ１の現在のセグメント番号を得る
			m_R.A = m_pIoSys->In(0xFC+1);
			op_RET();
			break;
		case MM_PUT_P2:		// ページ２のセグメントを切り換える
			m_pIoSys->Out(0xFC+2, m_R.A);
			op_RET();
			break;
		case MM_GET_P2:		// ページ２の現在のセグメント番号を得る
			m_R.A = m_pIoSys->In(0xFC+2);
			op_RET();
			break;
		case MM_PUT_P3:		// 何もせずに戻る
			m_pIoSys->Out(0xFC+3, m_R.A);
			op_RET();
			break;
		case MM_GET_P3:		// ページ３の現在のセグメント番号を得る
			m_R.A = m_pIoSys->In(0xFC+3);
			op_RET();
			break;
	}
	return;
}

/** MAIN-ROM
*/
void __time_critical_func(CZ80MsxDos::MainRomFunctionCall())
{
	if( m_R.PC != 0x4601 )
		return;
	msxslotno_t slotNo = m_pMemSys->GetSlotByPage(1);
	// MAIN-ROM.NEWSTT
	if( slotNo == 0x00 ) {
		// 中身の実装は無し、ただ終了させるのみ
		m_R.PC = 0;
		m_pMemSys->SetSlotToPage(/*page*/0, 0x03);
		m_pMemSys->SetSlotToPage(/*page*/1, 0x03);
	}
	return;
}

/** PCの値を見張っていて、特定の位置にPCが来たら対応するファンクションを実行する
*/
void __time_critical_func(CZ80MsxDos::MsxDosFunctionCall())
{
	if( m_R.PC != DOS_SYSTEMCALL )
		return;
	auto no = static_cast<dosfuncno_t>(m_R.C);
	switch(no)
	{
		case DOS_CONOUT:
		{
			//::wprintf(_T("%c"), m_R.E);
			//printf(_T("%c"), m_R.E);
			break;
		}
		case DOS_STROUT:
		{
			for(z80memaddr_t ad = m_R.GetDE(); true; ++ad){
				uint8_t ch = m_pMemSys->Read(ad);
				if( ch == '$' )
					break;
				//::wprintf(_T("%c"), ch);
				//printf(_T("%c"), ch);
			}
			break;
		}
		case DOS_TERM:
		{
			//std::wcout << _T("\n **HopStepZ >> terminate with errocode=") << (int)m_R.B << _T("\n");
			m_R.PC =0;	// 終了
			break;
		}
		case DOS_GENV:
		{
			const z80memaddr_t src = m_R.GetHL();	// 環境変数名が格納された領域へのポインタ(\0ターミネータ)
			z80memaddr_t dest = m_R.GetDE();		// 環境変数の内容の格納先
			//int areaSize = m_R.B;					// 格納先のサイズ
		
			std::string name;
			m_pMemSys->ReadString(&name, src);
			if( name == "PARAMETERS" ) {
				static const uint8_t cmd[] = { '/','K','0',' ','/','Z' };
				for( int t = 0; t < static_cast<int>(sizeof(cmd)); ++t )
					m_pMemSys->Write(dest+t, cmd[t]);
				m_R.A = 0;
			}else if( name == "SHELL" ) {
				static const uint8_t cmd[] = { 'A',':','\\','C','O','M','M','A','N','D','2','.','C','O','M' };
				for( int t = 0; t < static_cast<int>(sizeof(cmd)); ++t )
					m_pMemSys->Write(dest+t, cmd[t]);
				m_R.A = 0;
			}else {
				m_pMemSys->Write(dest+0, '\0');
				m_R.A = 0;
			}
			m_R.F.Z = 1;
			break;
		}
		case DOS_SENV:
		{
			m_R.A = 0;
			// do nothing
			break;
		}
		case DOS_DOSVER:
		{
			m_R.SetBC(0x0230);	// Kernel バージョン 2.30
			m_R.SetDE(0x0220);	// MSXDOS.SYS バージョン 2.20
			m_R.A = 0;
			break;
		}
		default:
		{
			//std::wcout << _T("\n **HopStepZ >> Not supported function no. ") << (int)no << _T("\n");
			DEBUG_BREAK;
			//　非対応ファンクションは、A=B=0でリターンする。
			m_R.A = 0;
			m_R.B = 0;
			break;
		}
	}
	op_RET();
	return;
}

bool CZ80MsxDos::patchingMapper(int *pPsegNo, int usersys)
{
	for( int t = 0; static_cast<int>(m_MemoryMapper.size()); ++t){
		if( m_MemoryMapper[t] == 0 ){
			m_MemoryMapper[t] = usersys;
			*pPsegNo = t;
			return true;
		}
	}
	return false;
}

void CZ80MsxDos::setup()
{
	// TODO: 即値になっている、将来直せ。
	// MemoryMapper[]の要素番号そのものがセグメント番号を示し、
	// 中の値は、割り当て済みかどうかを示している。0=未割当、1=ユーザー、2=システム
	for( int t = 0; t < 16; ++t)
		m_MemoryMapper.push_back(0);
	m_MemoryMapper[0] = 2;
	m_MemoryMapper[1] = 2;
	m_MemoryMapper[2] = 2;
	m_MemoryMapper[3] = 2;

	m_Tim.ResetBegin();

	m_Op.Single[0x00] = &CZ80MsxDos::op_NOP;
	m_Op.Single[0x01] = &CZ80MsxDos::op_LD_BC_ad;
	m_Op.Single[0x02] = &CZ80MsxDos::op_LD_memBC_A;
	m_Op.Single[0x03] = &CZ80MsxDos::op_INC_BC;
	m_Op.Single[0x04] = &CZ80MsxDos::op_INC_B;
	m_Op.Single[0x05] = &CZ80MsxDos::op_DEC_B;
	m_Op.Single[0x06] = &CZ80MsxDos::op_LD_B_v;
	m_Op.Single[0x07] = &CZ80MsxDos::op_RLCA;
	m_Op.Single[0x08] = &CZ80MsxDos::op_EX_AF_AF;
	m_Op.Single[0x09] = &CZ80MsxDos::op_ADD_HL_BC;
	m_Op.Single[0x0A] = &CZ80MsxDos::op_LD_A_memBC;
	m_Op.Single[0x0B] = &CZ80MsxDos::op_DEC_BC;
	m_Op.Single[0x0C] = &CZ80MsxDos::op_INC_C;
	m_Op.Single[0x0D] = &CZ80MsxDos::op_DEC_C;
	m_Op.Single[0x0E] = &CZ80MsxDos::op_LD_C_v;
	m_Op.Single[0x0F] = &CZ80MsxDos::op_RRCA;
	m_Op.Single[0x10] = &CZ80MsxDos::op_DJNZ_v;
	m_Op.Single[0x11] = &CZ80MsxDos::op_LD_DE_ad;
	m_Op.Single[0x12] = &CZ80MsxDos::op_LD_memDE_A;
	m_Op.Single[0x13] = &CZ80MsxDos::op_INC_DE;
	m_Op.Single[0x14] = &CZ80MsxDos::op_INC_D;
	m_Op.Single[0x15] = &CZ80MsxDos::op_DEC_D;
	m_Op.Single[0x16] = &CZ80MsxDos::op_LD_D_v;
	m_Op.Single[0x17] = &CZ80MsxDos::op_RLA;
	m_Op.Single[0x18] = &CZ80MsxDos::op_JR_v;
	m_Op.Single[0x19] = &CZ80MsxDos::op_ADD_HL_DE;
	m_Op.Single[0x1A] = &CZ80MsxDos::op_LD_A_memDE;
	m_Op.Single[0x1B] = &CZ80MsxDos::op_DEC_DE;
	m_Op.Single[0x1C] = &CZ80MsxDos::op_INC_E;
	m_Op.Single[0x1D] = &CZ80MsxDos::op_DEC_E;
	m_Op.Single[0x1E] = &CZ80MsxDos::op_LD_E_v;
	m_Op.Single[0x1F] = &CZ80MsxDos::op_RRA;
	m_Op.Single[0x20] = &CZ80MsxDos::op_JR_nz_v;
	m_Op.Single[0x21] = &CZ80MsxDos::op_LD_HL_ad;
	m_Op.Single[0x22] = &CZ80MsxDos::op_LD_memAD_HL;
	m_Op.Single[0x23] = &CZ80MsxDos::op_INC_HL;
	m_Op.Single[0x24] = &CZ80MsxDos::op_INC_H;
	m_Op.Single[0x25] = &CZ80MsxDos::op_DEC_H;
	m_Op.Single[0x26] = &CZ80MsxDos::op_LD_H_v;
	m_Op.Single[0x27] = &CZ80MsxDos::op_DAA;
	m_Op.Single[0x28] = &CZ80MsxDos::op_JR_z_v;
	m_Op.Single[0x29] = &CZ80MsxDos::op_ADD_HL_HL;
	m_Op.Single[0x2A] = &CZ80MsxDos::op_LD_HL_memAD;
	m_Op.Single[0x2B] = &CZ80MsxDos::op_DEC_HL;
	m_Op.Single[0x2C] = &CZ80MsxDos::op_INC_L;
	m_Op.Single[0x2D] = &CZ80MsxDos::op_DEC_L;
	m_Op.Single[0x2E] = &CZ80MsxDos::op_LD_L_v;
	m_Op.Single[0x2F] = &CZ80MsxDos::op_CPL;
	m_Op.Single[0x30] = &CZ80MsxDos::op_JR_nc_v;
	m_Op.Single[0x31] = &CZ80MsxDos::op_LD_SP_ad;
	m_Op.Single[0x32] = &CZ80MsxDos::op_LD_memAD_A;
	m_Op.Single[0x33] = &CZ80MsxDos::op_INC_SP;
	m_Op.Single[0x34] = &CZ80MsxDos::op_INC_memHL;
	m_Op.Single[0x35] = &CZ80MsxDos::op_DEC_memHL;
	m_Op.Single[0x36] = &CZ80MsxDos::op_LD_memHL_v;
	m_Op.Single[0x37] = &CZ80MsxDos::op_SCF;
	m_Op.Single[0x38] = &CZ80MsxDos::op_JR_C_v;
	m_Op.Single[0x39] = &CZ80MsxDos::op_ADD_HL_SP;
	m_Op.Single[0x3A] = &CZ80MsxDos::op_LD_A_memAD;
	m_Op.Single[0x3B] = &CZ80MsxDos::op_DEC_SP;
	m_Op.Single[0x3C] = &CZ80MsxDos::op_INC_A;
	m_Op.Single[0x3D] = &CZ80MsxDos::op_DEC_A;
	m_Op.Single[0x3E] = &CZ80MsxDos::op_LD_A_v;
	m_Op.Single[0x3F] = &CZ80MsxDos::op_CCF;
	m_Op.Single[0x40] = &CZ80MsxDos::op_LD_B_B;
	m_Op.Single[0x41] = &CZ80MsxDos::op_LD_B_C;
	m_Op.Single[0x42] = &CZ80MsxDos::op_LD_B_D;
	m_Op.Single[0x43] = &CZ80MsxDos::op_LD_B_E;
	m_Op.Single[0x44] = &CZ80MsxDos::op_LD_B_H;
	m_Op.Single[0x45] = &CZ80MsxDos::op_LD_B_L;
	m_Op.Single[0x46] = &CZ80MsxDos::op_LD_B_memHL;
	m_Op.Single[0x47] = &CZ80MsxDos::op_LD_B_A;
	m_Op.Single[0x48] = &CZ80MsxDos::op_LD_C_B;
	m_Op.Single[0x49] = &CZ80MsxDos::op_LD_C_C;
	m_Op.Single[0x4A] = &CZ80MsxDos::op_LD_C_D;
	m_Op.Single[0x4B] = &CZ80MsxDos::op_LD_C_E;
	m_Op.Single[0x4C] = &CZ80MsxDos::op_LD_C_H;
	m_Op.Single[0x4D] = &CZ80MsxDos::op_LD_C_L;
	m_Op.Single[0x4E] = &CZ80MsxDos::op_LD_C_memHL;
	m_Op.Single[0x4F] = &CZ80MsxDos::op_LD_C_A;
	m_Op.Single[0x50] = &CZ80MsxDos::op_LD_D_B;
	m_Op.Single[0x51] = &CZ80MsxDos::op_LD_D_C;
	m_Op.Single[0x52] = &CZ80MsxDos::op_LD_D_D;
	m_Op.Single[0x53] = &CZ80MsxDos::op_LD_D_E;
	m_Op.Single[0x54] = &CZ80MsxDos::op_LD_D_H;
	m_Op.Single[0x55] = &CZ80MsxDos::op_LD_D_L;
	m_Op.Single[0x56] = &CZ80MsxDos::op_LD_D_memHL;
	m_Op.Single[0x57] = &CZ80MsxDos::op_LD_D_A;
	m_Op.Single[0x58] = &CZ80MsxDos::op_LD_E_B;
	m_Op.Single[0x59] = &CZ80MsxDos::op_LD_E_C;
	m_Op.Single[0x5A] = &CZ80MsxDos::op_LD_E_D;
	m_Op.Single[0x5B] = &CZ80MsxDos::op_LD_E_E;
	m_Op.Single[0x5C] = &CZ80MsxDos::op_LD_E_H;
	m_Op.Single[0x5D] = &CZ80MsxDos::op_LD_E_L;
	m_Op.Single[0x5E] = &CZ80MsxDos::op_LD_E_memHL;
	m_Op.Single[0x5F] = &CZ80MsxDos::op_LD_E_A;
	m_Op.Single[0x60] = &CZ80MsxDos::op_LD_H_B;
	m_Op.Single[0x61] = &CZ80MsxDos::op_LD_H_C;
	m_Op.Single[0x62] = &CZ80MsxDos::op_LD_H_D;
	m_Op.Single[0x63] = &CZ80MsxDos::op_LD_H_E;
	m_Op.Single[0x64] = &CZ80MsxDos::op_LD_H_H;
	m_Op.Single[0x65] = &CZ80MsxDos::op_LD_H_L;
	m_Op.Single[0x66] = &CZ80MsxDos::op_LD_H_memHL;
	m_Op.Single[0x67] = &CZ80MsxDos::op_LD_H_A;
	m_Op.Single[0x68] = &CZ80MsxDos::op_LD_L_B;
	m_Op.Single[0x69] = &CZ80MsxDos::op_LD_L_C;
	m_Op.Single[0x6A] = &CZ80MsxDos::op_LD_L_D;
	m_Op.Single[0x6B] = &CZ80MsxDos::op_LD_L_E;
	m_Op.Single[0x6C] = &CZ80MsxDos::op_LD_L_H;
	m_Op.Single[0x6D] = &CZ80MsxDos::op_LD_L_L;
	m_Op.Single[0x6E] = &CZ80MsxDos::op_LD_L_memHL;
	m_Op.Single[0x6F] = &CZ80MsxDos::op_LD_L_A;
	m_Op.Single[0x70] = &CZ80MsxDos::op_LD_memHL_B;
	m_Op.Single[0x71] = &CZ80MsxDos::op_LD_memHL_C;
	m_Op.Single[0x72] = &CZ80MsxDos::op_LD_memHL_D;
	m_Op.Single[0x73] = &CZ80MsxDos::op_LD_memHL_E;
	m_Op.Single[0x74] = &CZ80MsxDos::op_LD_memHL_H;
	m_Op.Single[0x75] = &CZ80MsxDos::op_LD_memHL_L;
	m_Op.Single[0x76] = &CZ80MsxDos::op_HALT;
	m_Op.Single[0x77] = &CZ80MsxDos::op_LD_memHL_A;
	m_Op.Single[0x78] = &CZ80MsxDos::op_LD_A_B;
	m_Op.Single[0x79] = &CZ80MsxDos::op_LD_A_C;
	m_Op.Single[0x7A] = &CZ80MsxDos::op_LD_A_D;
	m_Op.Single[0x7B] = &CZ80MsxDos::op_LD_A_E;
	m_Op.Single[0x7C] = &CZ80MsxDos::op_LD_A_H;
	m_Op.Single[0x7D] = &CZ80MsxDos::op_LD_A_L;
	m_Op.Single[0x7E] = &CZ80MsxDos::op_LD_A_memHL;
	m_Op.Single[0x7F] = &CZ80MsxDos::op_LD_A_A;
	m_Op.Single[0x80] = &CZ80MsxDos::op_ADD_A_B;
	m_Op.Single[0x81] = &CZ80MsxDos::op_ADD_A_C;
	m_Op.Single[0x82] = &CZ80MsxDos::op_ADD_A_D;
	m_Op.Single[0x83] = &CZ80MsxDos::op_ADD_A_E;
	m_Op.Single[0x84] = &CZ80MsxDos::op_ADD_A_H;
	m_Op.Single[0x85] = &CZ80MsxDos::op_ADD_A_L;
	m_Op.Single[0x86] = &CZ80MsxDos::op_ADD_A_memHL;
	m_Op.Single[0x87] = &CZ80MsxDos::op_ADD_A_A;
	m_Op.Single[0x88] = &CZ80MsxDos::op_ADC_A_B;
	m_Op.Single[0x89] = &CZ80MsxDos::op_ADC_A_C;
	m_Op.Single[0x8A] = &CZ80MsxDos::op_ADC_A_D;
	m_Op.Single[0x8B] = &CZ80MsxDos::op_ADC_A_E;
	m_Op.Single[0x8C] = &CZ80MsxDos::op_ADC_A_H;
	m_Op.Single[0x8D] = &CZ80MsxDos::op_ADC_A_L;
	m_Op.Single[0x8E] = &CZ80MsxDos::op_ADC_A_memHL;
	m_Op.Single[0x8F] = &CZ80MsxDos::op_ADC_A_A;
	m_Op.Single[0x90] = &CZ80MsxDos::op_SUB_B;
	m_Op.Single[0x91] = &CZ80MsxDos::op_SUB_C;
	m_Op.Single[0x92] = &CZ80MsxDos::op_SUB_D;
	m_Op.Single[0x93] = &CZ80MsxDos::op_SUB_E;
	m_Op.Single[0x94] = &CZ80MsxDos::op_SUB_H;
	m_Op.Single[0x95] = &CZ80MsxDos::op_SUB_L;
	m_Op.Single[0x96] = &CZ80MsxDos::op_SUB_memHL;
	m_Op.Single[0x97] = &CZ80MsxDos::op_SUB_A;
	m_Op.Single[0x98] = &CZ80MsxDos::op_SBC_A_B;
	m_Op.Single[0x99] = &CZ80MsxDos::op_SBC_A_C;
	m_Op.Single[0x9A] = &CZ80MsxDos::op_SBC_A_D;
	m_Op.Single[0x9B] = &CZ80MsxDos::op_SBC_A_E;
	m_Op.Single[0x9C] = &CZ80MsxDos::op_SBC_A_H;
	m_Op.Single[0x9D] = &CZ80MsxDos::op_SBC_A_L;
	m_Op.Single[0x9E] = &CZ80MsxDos::op_SBC_A_memHL;
	m_Op.Single[0x9F] = &CZ80MsxDos::op_SBC_A;
	m_Op.Single[0xA0] = &CZ80MsxDos::op_AND_B;
	m_Op.Single[0xA1] = &CZ80MsxDos::op_AND_C;
	m_Op.Single[0xA2] = &CZ80MsxDos::op_AND_D;
	m_Op.Single[0xA3] = &CZ80MsxDos::op_AND_E;
	m_Op.Single[0xA4] = &CZ80MsxDos::op_AND_H;
	m_Op.Single[0xA5] = &CZ80MsxDos::op_AND_L;
	m_Op.Single[0xA6] = &CZ80MsxDos::op_AND_memHL;
	m_Op.Single[0xA7] = &CZ80MsxDos::op_AND_A;
	m_Op.Single[0xA8] = &CZ80MsxDos::op_XOR_B;
	m_Op.Single[0xA9] = &CZ80MsxDos::op_XOR_C;
	m_Op.Single[0xAA] = &CZ80MsxDos::op_XOR_D;
	m_Op.Single[0xAB] = &CZ80MsxDos::op_XOR_E;
	m_Op.Single[0xAC] = &CZ80MsxDos::op_XOR_H;
	m_Op.Single[0xAD] = &CZ80MsxDos::op_XOR_L;
	m_Op.Single[0xAE] = &CZ80MsxDos::op_XOR_memHL;
	m_Op.Single[0xAF] = &CZ80MsxDos::op_XOR_A;
	m_Op.Single[0xB0] = &CZ80MsxDos::op_OR_B;
	m_Op.Single[0xB1] = &CZ80MsxDos::op_OR_C;
	m_Op.Single[0xB2] = &CZ80MsxDos::op_OR_D;
	m_Op.Single[0xB3] = &CZ80MsxDos::op_OR_E;
	m_Op.Single[0xB4] = &CZ80MsxDos::op_OR_H;
	m_Op.Single[0xB5] = &CZ80MsxDos::op_OR_L;
	m_Op.Single[0xB6] = &CZ80MsxDos::op_OR_memHL;
	m_Op.Single[0xB7] = &CZ80MsxDos::op_OR_A;
	m_Op.Single[0xB8] = &CZ80MsxDos::op_CP_B;
	m_Op.Single[0xB9] = &CZ80MsxDos::op_CP_C;
	m_Op.Single[0xBA] = &CZ80MsxDos::op_CP_D;
	m_Op.Single[0xBB] = &CZ80MsxDos::op_CP_E;
	m_Op.Single[0xBC] = &CZ80MsxDos::op_CP_H;
	m_Op.Single[0xBD] = &CZ80MsxDos::op_CP_L;
	m_Op.Single[0xBE] = &CZ80MsxDos::op_CP_memHL;
	m_Op.Single[0xBF] = &CZ80MsxDos::op_CP_A;
	m_Op.Single[0xC0] = &CZ80MsxDos::op_RET_nz;
	m_Op.Single[0xC1] = &CZ80MsxDos::op_POP_BC;
	m_Op.Single[0xC2] = &CZ80MsxDos::op_JP_nz_ad;
	m_Op.Single[0xC3] = &CZ80MsxDos::op_JP_ad;
	m_Op.Single[0xC4] = &CZ80MsxDos::op_CALL_nz_ad;
	m_Op.Single[0xC5] = &CZ80MsxDos::op_PUSH_BC;
	m_Op.Single[0xC6] = &CZ80MsxDos::op_ADD_A_v;
	m_Op.Single[0xC7] = &CZ80MsxDos::op_RST_0h;
	m_Op.Single[0xC8] = &CZ80MsxDos::op_RET_z;
	m_Op.Single[0xC9] = &CZ80MsxDos::op_RET;
	m_Op.Single[0xCA] = &CZ80MsxDos::op_JP_z_ad;
	m_Op.Single[0xCB] = &CZ80MsxDos::op_EXTENDED_1;
	m_Op.Single[0xCC] = &CZ80MsxDos::op_CALL_z_ad;
	m_Op.Single[0xCD] = &CZ80MsxDos::op_CALL_ad;
	m_Op.Single[0xCE] = &CZ80MsxDos::op_ADC_A_v;
	m_Op.Single[0xCF] = &CZ80MsxDos::op_RST_8h;
	m_Op.Single[0xD0] = &CZ80MsxDos::op_RET_nc;
	m_Op.Single[0xD1] = &CZ80MsxDos::op_POP_DE;
	m_Op.Single[0xD2] = &CZ80MsxDos::op_JP_nc_ad;
	m_Op.Single[0xD3] = &CZ80MsxDos::op_OUT_memv_A;
	m_Op.Single[0xD4] = &CZ80MsxDos::op_CALL_nc_ad;
	m_Op.Single[0xD5] = &CZ80MsxDos::op_PUSH_DE;
	m_Op.Single[0xD6] = &CZ80MsxDos::op_SUB_v;
	m_Op.Single[0xD7] = &CZ80MsxDos::op_RST_10h;
	m_Op.Single[0xD8] = &CZ80MsxDos::op_RET_c;
	m_Op.Single[0xD9] = &CZ80MsxDos::op_EXX;
	m_Op.Single[0xDA] = &CZ80MsxDos::op_JP_c_ad;
	m_Op.Single[0xDB] = &CZ80MsxDos::op_IN_A_memv;
	m_Op.Single[0xDC] = &CZ80MsxDos::op_CALL_c_ad;
	m_Op.Single[0xDD] = &CZ80MsxDos::op_EXTENDED_2IX;
	m_Op.Single[0xDE] = &CZ80MsxDos::op_SBC_A_v;
	m_Op.Single[0xDF] = &CZ80MsxDos::op_RST_18h;
	m_Op.Single[0xE0] = &CZ80MsxDos::op_RET_po;
	m_Op.Single[0xE1] = &CZ80MsxDos::op_POP_HL;
	m_Op.Single[0xE2] = &CZ80MsxDos::op_JP_po_ad;
	m_Op.Single[0xE3] = &CZ80MsxDos::op_EX_memSP_HL;
	m_Op.Single[0xE4] = &CZ80MsxDos::op_CALL_po_ad;
	m_Op.Single[0xE5] = &CZ80MsxDos::op_PUSH_HL;
	m_Op.Single[0xE6] = &CZ80MsxDos::op_AND_v;
	m_Op.Single[0xE7] = &CZ80MsxDos::op_RST_20h;
	m_Op.Single[0xE8] = &CZ80MsxDos::op_RET_pe;
	m_Op.Single[0xE9] = &CZ80MsxDos::op_JP_memHL;
	m_Op.Single[0xEA] = &CZ80MsxDos::op_JP_pe_ad;
	m_Op.Single[0xEB] = &CZ80MsxDos::op_EX_DE_HL;
	m_Op.Single[0xEC] = &CZ80MsxDos::op_CALL_pe_ad;
	m_Op.Single[0xED] = &CZ80MsxDos::op_EXTENDED_3;
	m_Op.Single[0xEE] = &CZ80MsxDos::op_XOR_v;
	m_Op.Single[0xEF] = &CZ80MsxDos::op_RST_28h;
	m_Op.Single[0xF0] = &CZ80MsxDos::op_RET_p;
	m_Op.Single[0xF1] = &CZ80MsxDos::op_POP_AF;
	m_Op.Single[0xF2] = &CZ80MsxDos::op_JP_p_ad;
	m_Op.Single[0xF3] = &CZ80MsxDos::op_DI;
	m_Op.Single[0xF4] = &CZ80MsxDos::op_CALL_p_ad;
	m_Op.Single[0xF5] = &CZ80MsxDos::op_PUSH_AF;
	m_Op.Single[0xF6] = &CZ80MsxDos::op_OR_v;
	m_Op.Single[0xF7] = &CZ80MsxDos::op_RST_30h;
	m_Op.Single[0xF8] = &CZ80MsxDos::op_RET_m;
	m_Op.Single[0xF9] = &CZ80MsxDos::op_LD_SP_HL;
	m_Op.Single[0xFA] = &CZ80MsxDos::op_JP_m_ad;
	m_Op.Single[0xFB] = &CZ80MsxDos::op_EI;
	m_Op.Single[0xFC] = &CZ80MsxDos::op_CALL_m_ad;
	m_Op.Single[0xFD] = &CZ80MsxDos::op_EXTENDED_4IY;
	m_Op.Single[0xFE] = &CZ80MsxDos::op_CP_v;
	m_Op.Single[0xFF] = &CZ80MsxDos::op_RST_38h;

	m_Op.Extended1[0x00] = &CZ80MsxDos::op_RLC_B;
	m_Op.Extended1[0x01] = &CZ80MsxDos::op_RLC_C;
	m_Op.Extended1[0x02] = &CZ80MsxDos::op_RLC_D;
	m_Op.Extended1[0x03] = &CZ80MsxDos::op_RLC_E;
	m_Op.Extended1[0x04] = &CZ80MsxDos::op_RLC_H;
	m_Op.Extended1[0x05] = &CZ80MsxDos::op_RLC_L;
	m_Op.Extended1[0x06] = &CZ80MsxDos::op_RLC_memHL;
	m_Op.Extended1[0x07] = &CZ80MsxDos::op_RLC_A;
	m_Op.Extended1[0x08] = &CZ80MsxDos::op_RRC_B;
	m_Op.Extended1[0x09] = &CZ80MsxDos::op_RRC_C;
	m_Op.Extended1[0x0A] = &CZ80MsxDos::op_RRC_D;
	m_Op.Extended1[0x0B] = &CZ80MsxDos::op_RRC_E;
	m_Op.Extended1[0x0C] = &CZ80MsxDos::op_RRC_H;
	m_Op.Extended1[0x0D] = &CZ80MsxDos::op_RRC_L;
	m_Op.Extended1[0x0E] = &CZ80MsxDos::op_RRC_memHL;
	m_Op.Extended1[0x0F] = &CZ80MsxDos::op_RRC_A;
	m_Op.Extended1[0x10] = &CZ80MsxDos::op_RL_B;
	m_Op.Extended1[0x11] = &CZ80MsxDos::op_RL_C;
	m_Op.Extended1[0x12] = &CZ80MsxDos::op_RL_D;
	m_Op.Extended1[0x13] = &CZ80MsxDos::op_RL_E;
	m_Op.Extended1[0x14] = &CZ80MsxDos::op_RL_H;
	m_Op.Extended1[0x15] = &CZ80MsxDos::op_RL_L;
	m_Op.Extended1[0x16] = &CZ80MsxDos::op_RL_memHL;
	m_Op.Extended1[0x17] = &CZ80MsxDos::op_RL_A;
	m_Op.Extended1[0x18] = &CZ80MsxDos::op_RR_B;
	m_Op.Extended1[0x19] = &CZ80MsxDos::op_RR_C;
	m_Op.Extended1[0x1A] = &CZ80MsxDos::op_RR_D;
	m_Op.Extended1[0x1B] = &CZ80MsxDos::op_RR_E;
	m_Op.Extended1[0x1C] = &CZ80MsxDos::op_RR_H;
	m_Op.Extended1[0x1D] = &CZ80MsxDos::op_RR_L;
	m_Op.Extended1[0x1E] = &CZ80MsxDos::op_RR_memHL;
	m_Op.Extended1[0x1F] = &CZ80MsxDos::op_RR_A;
	m_Op.Extended1[0x20] = &CZ80MsxDos::op_SLA_B;
	m_Op.Extended1[0x21] = &CZ80MsxDos::op_SLA_C;
	m_Op.Extended1[0x22] = &CZ80MsxDos::op_SLA_D;
	m_Op.Extended1[0x23] = &CZ80MsxDos::op_SLA_E;
	m_Op.Extended1[0x24] = &CZ80MsxDos::op_SLA_H;
	m_Op.Extended1[0x25] = &CZ80MsxDos::op_SLA_L;
	m_Op.Extended1[0x26] = &CZ80MsxDos::op_SLA_memHL;
	m_Op.Extended1[0x27] = &CZ80MsxDos::op_SLA_A;
	m_Op.Extended1[0x28] = &CZ80MsxDos::op_SRA_B;
	m_Op.Extended1[0x29] = &CZ80MsxDos::op_SRA_C;
	m_Op.Extended1[0x2A] = &CZ80MsxDos::op_SRA_D;
	m_Op.Extended1[0x2B] = &CZ80MsxDos::op_SRA_E;
	m_Op.Extended1[0x2C] = &CZ80MsxDos::op_SRA_H;
	m_Op.Extended1[0x2D] = &CZ80MsxDos::op_SRA_L;
	m_Op.Extended1[0x2E] = &CZ80MsxDos::op_SRA_memHL;
	m_Op.Extended1[0x2F] = &CZ80MsxDos::op_SRA_A;
	m_Op.Extended1[0x30] = &CZ80MsxDos::op_SLL_B;
	m_Op.Extended1[0x31] = &CZ80MsxDos::op_SLL_C;
	m_Op.Extended1[0x32] = &CZ80MsxDos::op_SLL_D;
	m_Op.Extended1[0x33] = &CZ80MsxDos::op_SLL_E;
	m_Op.Extended1[0x34] = &CZ80MsxDos::op_SLL_H;
	m_Op.Extended1[0x35] = &CZ80MsxDos::op_SLL_L;
	m_Op.Extended1[0x36] = &CZ80MsxDos::op_SLL_memHL;
	m_Op.Extended1[0x37] = &CZ80MsxDos::op_SLL_A;
	m_Op.Extended1[0x38] = &CZ80MsxDos::op_SRL_B;
	m_Op.Extended1[0x39] = &CZ80MsxDos::op_SRL_C;
	m_Op.Extended1[0x3A] = &CZ80MsxDos::op_SRL_D;
	m_Op.Extended1[0x3B] = &CZ80MsxDos::op_SRL_E;
	m_Op.Extended1[0x3C] = &CZ80MsxDos::op_SRL_H;
	m_Op.Extended1[0x3D] = &CZ80MsxDos::op_SRL_L;
	m_Op.Extended1[0x3E] = &CZ80MsxDos::op_SRL_memHL;
	m_Op.Extended1[0x3F] = &CZ80MsxDos::op_SRL_A;
	m_Op.Extended1[0x40] = &CZ80MsxDos::op_BIT_0_B;
	m_Op.Extended1[0x41] = &CZ80MsxDos::op_BIT_0_C;
	m_Op.Extended1[0x42] = &CZ80MsxDos::op_BIT_0_D;
	m_Op.Extended1[0x43] = &CZ80MsxDos::op_BIT_0_E;
	m_Op.Extended1[0x44] = &CZ80MsxDos::op_BIT_0_H;
	m_Op.Extended1[0x45] = &CZ80MsxDos::op_BIT_0_L;
	m_Op.Extended1[0x46] = &CZ80MsxDos::op_BIT_0_memHL;
	m_Op.Extended1[0x47] = &CZ80MsxDos::op_BIT_0_A;
	m_Op.Extended1[0x48] = &CZ80MsxDos::op_BIT_1_B;
	m_Op.Extended1[0x49] = &CZ80MsxDos::op_BIT_1_C;
	m_Op.Extended1[0x4A] = &CZ80MsxDos::op_BIT_1_D;
	m_Op.Extended1[0x4B] = &CZ80MsxDos::op_BIT_1_E;
	m_Op.Extended1[0x4C] = &CZ80MsxDos::op_BIT_1_H;
	m_Op.Extended1[0x4D] = &CZ80MsxDos::op_BIT_1_L;
	m_Op.Extended1[0x4E] = &CZ80MsxDos::op_BIT_1_memHL;
	m_Op.Extended1[0x4F] = &CZ80MsxDos::op_BIT_1_A;
	m_Op.Extended1[0x50] = &CZ80MsxDos::op_BIT_2_B;
	m_Op.Extended1[0x51] = &CZ80MsxDos::op_BIT_2_C;
	m_Op.Extended1[0x52] = &CZ80MsxDos::op_BIT_2_D;
	m_Op.Extended1[0x53] = &CZ80MsxDos::op_BIT_2_E;
	m_Op.Extended1[0x54] = &CZ80MsxDos::op_BIT_2_H;
	m_Op.Extended1[0x55] = &CZ80MsxDos::op_BIT_2_L;
	m_Op.Extended1[0x56] = &CZ80MsxDos::op_BIT_2_memHL;
	m_Op.Extended1[0x57] = &CZ80MsxDos::op_BIT_2_A;
	m_Op.Extended1[0x58] = &CZ80MsxDos::op_BIT_3_B;
	m_Op.Extended1[0x59] = &CZ80MsxDos::op_BIT_3_C;
	m_Op.Extended1[0x5A] = &CZ80MsxDos::op_BIT_3_D;
	m_Op.Extended1[0x5B] = &CZ80MsxDos::op_BIT_3_E;
	m_Op.Extended1[0x5C] = &CZ80MsxDos::op_BIT_3_H;
	m_Op.Extended1[0x5D] = &CZ80MsxDos::op_BIT_3_L;
	m_Op.Extended1[0x5E] = &CZ80MsxDos::op_BIT_3_memHL;
	m_Op.Extended1[0x5F] = &CZ80MsxDos::op_BIT_3_A;
	m_Op.Extended1[0x60] = &CZ80MsxDos::op_BIT_4_B;
	m_Op.Extended1[0x61] = &CZ80MsxDos::op_BIT_4_C;
	m_Op.Extended1[0x62] = &CZ80MsxDos::op_BIT_4_D;
	m_Op.Extended1[0x63] = &CZ80MsxDos::op_BIT_4_E;
	m_Op.Extended1[0x64] = &CZ80MsxDos::op_BIT_4_H;
	m_Op.Extended1[0x65] = &CZ80MsxDos::op_BIT_4_L;
	m_Op.Extended1[0x66] = &CZ80MsxDos::op_BIT_4_memHL;
	m_Op.Extended1[0x67] = &CZ80MsxDos::op_BIT_4_A;
	m_Op.Extended1[0x68] = &CZ80MsxDos::op_BIT_5_B;
	m_Op.Extended1[0x69] = &CZ80MsxDos::op_BIT_5_C;
	m_Op.Extended1[0x6A] = &CZ80MsxDos::op_BIT_5_D;
	m_Op.Extended1[0x6B] = &CZ80MsxDos::op_BIT_5_E;
	m_Op.Extended1[0x6C] = &CZ80MsxDos::op_BIT_5_H;
	m_Op.Extended1[0x6D] = &CZ80MsxDos::op_BIT_5_L;
	m_Op.Extended1[0x6E] = &CZ80MsxDos::op_BIT_5_memHL;
	m_Op.Extended1[0x6F] = &CZ80MsxDos::op_BIT_5_A;
	m_Op.Extended1[0x70] = &CZ80MsxDos::op_BIT_6_B;
	m_Op.Extended1[0x71] = &CZ80MsxDos::op_BIT_6_C;
	m_Op.Extended1[0x72] = &CZ80MsxDos::op_BIT_6_D;
	m_Op.Extended1[0x73] = &CZ80MsxDos::op_BIT_6_E;
	m_Op.Extended1[0x74] = &CZ80MsxDos::op_BIT_6_H;
	m_Op.Extended1[0x75] = &CZ80MsxDos::op_BIT_6_L;
	m_Op.Extended1[0x76] = &CZ80MsxDos::op_BIT_6_memHL;
	m_Op.Extended1[0x77] = &CZ80MsxDos::op_BIT_6_A;
	m_Op.Extended1[0x78] = &CZ80MsxDos::op_BIT_7_B;
	m_Op.Extended1[0x79] = &CZ80MsxDos::op_BIT_7_C;
	m_Op.Extended1[0x7A] = &CZ80MsxDos::op_BIT_7_D;
	m_Op.Extended1[0x7B] = &CZ80MsxDos::op_BIT_7_E;
	m_Op.Extended1[0x7C] = &CZ80MsxDos::op_BIT_7_H;
	m_Op.Extended1[0x7D] = &CZ80MsxDos::op_BIT_7_L;
	m_Op.Extended1[0x7E] = &CZ80MsxDos::op_BIT_7_memHL;
	m_Op.Extended1[0x7F] = &CZ80MsxDos::op_BIT_7_A;
	m_Op.Extended1[0x80] = &CZ80MsxDos::op_RES_0_B;
	m_Op.Extended1[0x81] = &CZ80MsxDos::op_RES_0_C;
	m_Op.Extended1[0x82] = &CZ80MsxDos::op_RES_0_D;
	m_Op.Extended1[0x83] = &CZ80MsxDos::op_RES_0_E;
	m_Op.Extended1[0x84] = &CZ80MsxDos::op_RES_0_H;
	m_Op.Extended1[0x85] = &CZ80MsxDos::op_RES_0_L;
	m_Op.Extended1[0x86] = &CZ80MsxDos::op_RES_0_memHL;
	m_Op.Extended1[0x87] = &CZ80MsxDos::op_RES_0_A;
	m_Op.Extended1[0x88] = &CZ80MsxDos::op_RES_1_B;
	m_Op.Extended1[0x89] = &CZ80MsxDos::op_RES_1_C;
	m_Op.Extended1[0x8A] = &CZ80MsxDos::op_RES_1_D;
	m_Op.Extended1[0x8B] = &CZ80MsxDos::op_RES_1_E;
	m_Op.Extended1[0x8C] = &CZ80MsxDos::op_RES_1_H;
	m_Op.Extended1[0x8D] = &CZ80MsxDos::op_RES_1_L;
	m_Op.Extended1[0x8E] = &CZ80MsxDos::op_RES_1_memHL;
	m_Op.Extended1[0x8F] = &CZ80MsxDos::op_RES_1_A;
	m_Op.Extended1[0x90] = &CZ80MsxDos::op_RES_2_B;
	m_Op.Extended1[0x91] = &CZ80MsxDos::op_RES_2_C;
	m_Op.Extended1[0x92] = &CZ80MsxDos::op_RES_2_D;
	m_Op.Extended1[0x93] = &CZ80MsxDos::op_RES_2_E;
	m_Op.Extended1[0x94] = &CZ80MsxDos::op_RES_2_H;
	m_Op.Extended1[0x95] = &CZ80MsxDos::op_RES_2_L;
	m_Op.Extended1[0x96] = &CZ80MsxDos::op_RES_2_memHL;
	m_Op.Extended1[0x97] = &CZ80MsxDos::op_RES_2_A;
	m_Op.Extended1[0x98] = &CZ80MsxDos::op_RES_3_B;
	m_Op.Extended1[0x99] = &CZ80MsxDos::op_RES_3_C;
	m_Op.Extended1[0x9A] = &CZ80MsxDos::op_RES_3_D;
	m_Op.Extended1[0x9B] = &CZ80MsxDos::op_RES_3_E;
	m_Op.Extended1[0x9C] = &CZ80MsxDos::op_RES_3_H;
	m_Op.Extended1[0x9D] = &CZ80MsxDos::op_RES_3_L;
	m_Op.Extended1[0x9E] = &CZ80MsxDos::op_RES_3_memHL;
	m_Op.Extended1[0x9F] = &CZ80MsxDos::op_RES_3_A;
	m_Op.Extended1[0xA0] = &CZ80MsxDos::op_RES_4_B;
	m_Op.Extended1[0xA1] = &CZ80MsxDos::op_RES_4_C;
	m_Op.Extended1[0xA2] = &CZ80MsxDos::op_RES_4_D;
	m_Op.Extended1[0xA3] = &CZ80MsxDos::op_RES_4_E;
	m_Op.Extended1[0xA4] = &CZ80MsxDos::op_RES_4_H;
	m_Op.Extended1[0xA5] = &CZ80MsxDos::op_RES_4_L;
	m_Op.Extended1[0xA6] = &CZ80MsxDos::op_RES_4_memHL;
	m_Op.Extended1[0xA7] = &CZ80MsxDos::op_RES_4_A;
	m_Op.Extended1[0xA8] = &CZ80MsxDos::op_RES_5_B;
	m_Op.Extended1[0xA9] = &CZ80MsxDos::op_RES_5_C;
	m_Op.Extended1[0xAA] = &CZ80MsxDos::op_RES_5_D;
	m_Op.Extended1[0xAB] = &CZ80MsxDos::op_RES_5_E;
	m_Op.Extended1[0xAC] = &CZ80MsxDos::op_RES_5_H;
	m_Op.Extended1[0xAD] = &CZ80MsxDos::op_RES_5_L;
	m_Op.Extended1[0xAE] = &CZ80MsxDos::op_RES_5_memHL;
	m_Op.Extended1[0xAF] = &CZ80MsxDos::op_RES_5_A;
	m_Op.Extended1[0xB0] = &CZ80MsxDos::op_RES_6_B;
	m_Op.Extended1[0xB1] = &CZ80MsxDos::op_RES_6_C;
	m_Op.Extended1[0xB2] = &CZ80MsxDos::op_RES_6_D;
	m_Op.Extended1[0xB3] = &CZ80MsxDos::op_RES_6_E;
	m_Op.Extended1[0xB4] = &CZ80MsxDos::op_RES_6_H;
	m_Op.Extended1[0xB5] = &CZ80MsxDos::op_RES_6_L;
	m_Op.Extended1[0xB6] = &CZ80MsxDos::op_RES_6_memHL;
	m_Op.Extended1[0xB7] = &CZ80MsxDos::op_RES_6_A;
	m_Op.Extended1[0xB8] = &CZ80MsxDos::op_RES_7_B;
	m_Op.Extended1[0xB9] = &CZ80MsxDos::op_RES_7_C;
	m_Op.Extended1[0xBA] = &CZ80MsxDos::op_RES_7_D;
	m_Op.Extended1[0xBB] = &CZ80MsxDos::op_RES_7_E;
	m_Op.Extended1[0xBC] = &CZ80MsxDos::op_RES_7_H;
	m_Op.Extended1[0xBD] = &CZ80MsxDos::op_RES_7_L;
	m_Op.Extended1[0xBE] = &CZ80MsxDos::op_RES_7_memHL;
	m_Op.Extended1[0xBF] = &CZ80MsxDos::op_RES_7_A;
	m_Op.Extended1[0xC0] = &CZ80MsxDos::op_SET_0_B;
	m_Op.Extended1[0xC1] = &CZ80MsxDos::op_SET_0_C;
	m_Op.Extended1[0xC2] = &CZ80MsxDos::op_SET_0_D;
	m_Op.Extended1[0xC3] = &CZ80MsxDos::op_SET_0_E;
	m_Op.Extended1[0xC4] = &CZ80MsxDos::op_SET_0_H;
	m_Op.Extended1[0xC5] = &CZ80MsxDos::op_SET_0_L;
	m_Op.Extended1[0xC6] = &CZ80MsxDos::op_SET_0_memHL;
	m_Op.Extended1[0xC7] = &CZ80MsxDos::op_SET_0_A;
	m_Op.Extended1[0xC8] = &CZ80MsxDos::op_SET_1_B;
	m_Op.Extended1[0xC9] = &CZ80MsxDos::op_SET_1_C;
	m_Op.Extended1[0xCA] = &CZ80MsxDos::op_SET_1_D;
	m_Op.Extended1[0xCB] = &CZ80MsxDos::op_SET_1_E;
	m_Op.Extended1[0xCC] = &CZ80MsxDos::op_SET_1_H;
	m_Op.Extended1[0xCD] = &CZ80MsxDos::op_SET_1_L;
	m_Op.Extended1[0xCE] = &CZ80MsxDos::op_SET_1_memHL;
	m_Op.Extended1[0xCF] = &CZ80MsxDos::op_SET_1_A;
	m_Op.Extended1[0xD0] = &CZ80MsxDos::op_SET_2_B;
	m_Op.Extended1[0xD1] = &CZ80MsxDos::op_SET_2_C;
	m_Op.Extended1[0xD2] = &CZ80MsxDos::op_SET_2_D;
	m_Op.Extended1[0xD3] = &CZ80MsxDos::op_SET_2_E;
	m_Op.Extended1[0xD4] = &CZ80MsxDos::op_SET_2_H;
	m_Op.Extended1[0xD5] = &CZ80MsxDos::op_SET_2_L;
	m_Op.Extended1[0xD6] = &CZ80MsxDos::op_SET_2_memHL;
	m_Op.Extended1[0xD7] = &CZ80MsxDos::op_SET_2_A;
	m_Op.Extended1[0xD8] = &CZ80MsxDos::op_SET_3_B;
	m_Op.Extended1[0xD9] = &CZ80MsxDos::op_SET_3_C;
	m_Op.Extended1[0xDA] = &CZ80MsxDos::op_SET_3_D;
	m_Op.Extended1[0xDB] = &CZ80MsxDos::op_SET_3_E;
	m_Op.Extended1[0xDC] = &CZ80MsxDos::op_SET_3_H;
	m_Op.Extended1[0xDD] = &CZ80MsxDos::op_SET_3_L;
	m_Op.Extended1[0xDE] = &CZ80MsxDos::op_SET_3_memHL;
	m_Op.Extended1[0xDF] = &CZ80MsxDos::op_SET_3_A;
	m_Op.Extended1[0xE0] = &CZ80MsxDos::op_SET_4_B;
	m_Op.Extended1[0xE1] = &CZ80MsxDos::op_SET_4_C;
	m_Op.Extended1[0xE2] = &CZ80MsxDos::op_SET_4_D;
	m_Op.Extended1[0xE3] = &CZ80MsxDos::op_SET_4_E;
	m_Op.Extended1[0xE4] = &CZ80MsxDos::op_SET_4_H;
	m_Op.Extended1[0xE5] = &CZ80MsxDos::op_SET_4_L;
	m_Op.Extended1[0xE6] = &CZ80MsxDos::op_SET_4_memHL;
	m_Op.Extended1[0xE7] = &CZ80MsxDos::op_SET_4_A;
	m_Op.Extended1[0xE8] = &CZ80MsxDos::op_SET_5_B;
	m_Op.Extended1[0xE9] = &CZ80MsxDos::op_SET_5_C;
	m_Op.Extended1[0xEA] = &CZ80MsxDos::op_SET_5_D;
	m_Op.Extended1[0xEB] = &CZ80MsxDos::op_SET_5_E;
	m_Op.Extended1[0xEC] = &CZ80MsxDos::op_SET_5_H;
	m_Op.Extended1[0xED] = &CZ80MsxDos::op_SET_5_L;
	m_Op.Extended1[0xEE] = &CZ80MsxDos::op_SET_5_memHL;
	m_Op.Extended1[0xEF] = &CZ80MsxDos::op_SET_5_A;
	m_Op.Extended1[0xF0] = &CZ80MsxDos::op_SET_6_B;
	m_Op.Extended1[0xF1] = &CZ80MsxDos::op_SET_6_C;
	m_Op.Extended1[0xF2] = &CZ80MsxDos::op_SET_6_D;
	m_Op.Extended1[0xF3] = &CZ80MsxDos::op_SET_6_E;
	m_Op.Extended1[0xF4] = &CZ80MsxDos::op_SET_6_H;
	m_Op.Extended1[0xF5] = &CZ80MsxDos::op_SET_6_L;
	m_Op.Extended1[0xF6] = &CZ80MsxDos::op_SET_6_memHL;
	m_Op.Extended1[0xF7] = &CZ80MsxDos::op_SET_6_A;
	m_Op.Extended1[0xF8] = &CZ80MsxDos::op_SET_7_B;
	m_Op.Extended1[0xF9] = &CZ80MsxDos::op_SET_7_C;
	m_Op.Extended1[0xFA] = &CZ80MsxDos::op_SET_7_D;
	m_Op.Extended1[0xFB] = &CZ80MsxDos::op_SET_7_E;
	m_Op.Extended1[0xFC] = &CZ80MsxDos::op_SET_7_H;
	m_Op.Extended1[0xFD] = &CZ80MsxDos::op_SET_7_L;
	m_Op.Extended1[0xFE] = &CZ80MsxDos::op_SET_7_memHL;
	m_Op.Extended1[0xFF] = &CZ80MsxDos::op_SET_7_A;

	m_Op.Extended2IX[0x00] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x01] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x02] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x03] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x04] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x05] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x06] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x07] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x08] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x09] = &CZ80MsxDos::op_ADD_IX_BC;
	m_Op.Extended2IX[0x0A] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x0B] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x0C] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x0D] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x0E] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x0F] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x10] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x11] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x12] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x13] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x14] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x15] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x16] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x17] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x18] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x19] = &CZ80MsxDos::op_ADD_IX_DE;
	m_Op.Extended2IX[0x1A] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x1B] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x1C] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x1D] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x1E] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x1F] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x20] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x21] = &CZ80MsxDos::op_LD_IX_ad;
	m_Op.Extended2IX[0x22] = &CZ80MsxDos::op_LD_memAD_IX;
	m_Op.Extended2IX[0x23] = &CZ80MsxDos::op_INC_IX;
	m_Op.Extended2IX[0x24] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x25] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x26] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x27] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x28] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x29] = &CZ80MsxDos::op_ADD_IX_IX;
	m_Op.Extended2IX[0x2A] = &CZ80MsxDos::op_LD_IX_memAD;
	m_Op.Extended2IX[0x2B] = &CZ80MsxDos::op_DEC_IX;
	m_Op.Extended2IX[0x2C] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x2D] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x2E] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x2F] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x30] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x31] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x32] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x33] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x34] = &CZ80MsxDos::op_INC_memIXpV;
	m_Op.Extended2IX[0x35] = &CZ80MsxDos::op_DEC_memIXpV;
	m_Op.Extended2IX[0x36] = &CZ80MsxDos::op_LD_memIXpV_v;
	m_Op.Extended2IX[0x37] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x38] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x39] = &CZ80MsxDos::op_ADD_IX_SP;
	m_Op.Extended2IX[0x3A] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x3B] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x3C] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x3D] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x3E] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x3F] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x40] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x41] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x42] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x43] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x44] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x45] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x46] = &CZ80MsxDos::op_LD_B_memIXpV;
	m_Op.Extended2IX[0x47] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x48] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x49] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x4A] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x4B] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x4C] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x4D] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x4E] = &CZ80MsxDos::op_LD_C_memIXpV;
	m_Op.Extended2IX[0x4F] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x50] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x51] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x52] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x53] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x54] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x55] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x56] = &CZ80MsxDos::op_LD_D_memIXpV;
	m_Op.Extended2IX[0x57] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x58] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x59] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x5A] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x5B] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x5C] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x5D] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x5E] = &CZ80MsxDos::op_LD_E_memIXpV;
	m_Op.Extended2IX[0x5F] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x60] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x61] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x62] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x63] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x64] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x65] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x66] = &CZ80MsxDos::op_LD_H_memIXpV;
	m_Op.Extended2IX[0x67] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x68] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x69] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x6A] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x6B] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x6C] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x6D] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x6E] = &CZ80MsxDos::op_LD_L_memIXpV;
	m_Op.Extended2IX[0x6F] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x70] = &CZ80MsxDos::op_LD_memIXpV_B;
	m_Op.Extended2IX[0x71] = &CZ80MsxDos::op_LD_memIXpV_C;
	m_Op.Extended2IX[0x72] = &CZ80MsxDos::op_LD_memIXpV_D;
	m_Op.Extended2IX[0x73] = &CZ80MsxDos::op_LD_memIXpV_E;
	m_Op.Extended2IX[0x74] = &CZ80MsxDos::op_LD_memIXpV_H;
	m_Op.Extended2IX[0x75] = &CZ80MsxDos::op_LD_memIXpV_L;
	m_Op.Extended2IX[0x76] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x77] = &CZ80MsxDos::op_LD_memIXpV_A;
	m_Op.Extended2IX[0x78] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x79] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x7A] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x7B] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x7C] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x7D] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x7E] = &CZ80MsxDos::op_LD_A_memIXpV;
	m_Op.Extended2IX[0x7F] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x80] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x81] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x82] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x83] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x84] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x85] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x86] = &CZ80MsxDos::op_ADD_A_memIXpV;
	m_Op.Extended2IX[0x87] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x88] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x89] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x8A] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x8B] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x8C] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x8D] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x8E] = &CZ80MsxDos::op_ADC_A_memIXpV;
	m_Op.Extended2IX[0x8F] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x90] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x91] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x92] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x93] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x94] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x95] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x96] = &CZ80MsxDos::op_SUB_memIXpV;
	m_Op.Extended2IX[0x97] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x98] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x99] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x9A] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x9B] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x9C] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x9D] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0x9E] = &CZ80MsxDos::op_SBC_A_memIXpV;
	m_Op.Extended2IX[0x9F] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xA0] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xA1] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xA2] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xA3] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xA4] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xA5] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xA6] = &CZ80MsxDos::op_AND_memIXpV;
	m_Op.Extended2IX[0xA7] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xA8] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xA9] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xAA] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xAB] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xAC] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xAD] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xAE] = &CZ80MsxDos::op_XOR_memIXpV;
	m_Op.Extended2IX[0xAF] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xB0] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xB1] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xB2] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xB3] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xB4] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xB5] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xB6] = &CZ80MsxDos::op_OR_memIXpV;
	m_Op.Extended2IX[0xB7] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xB8] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xB9] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xBA] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xBB] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xBC] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xBD] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xBE] = &CZ80MsxDos::op_CP_memIXpV;
	m_Op.Extended2IX[0xBF] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xC0] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xC1] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xC2] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xC3] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xC4] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xC5] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xC6] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xC7] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xC8] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xC9] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xCA] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xCB] = &CZ80MsxDos::op_EXTENDED_2IX2;
	m_Op.Extended2IX[0xCC] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xCD] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xCE] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xCF] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xD0] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xD1] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xD2] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xD3] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xD4] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xD5] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xD6] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xD7] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xD8] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xD9] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xDA] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xDB] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xDC] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xDD] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xDE] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xDF] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xE0] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xE1] = &CZ80MsxDos::op_POP_IX;
	m_Op.Extended2IX[0xE2] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xE3] = &CZ80MsxDos::op_EX_memSP_IX;
	m_Op.Extended2IX[0xE4] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xE5] = &CZ80MsxDos::op_PUSH_IX;
	m_Op.Extended2IX[0xE6] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xE7] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xE8] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xE9] = &CZ80MsxDos::op_JP_memIX;
	m_Op.Extended2IX[0xEA] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xEB] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xEC] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xED] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xEE] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xEF] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xF0] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xF1] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xF2] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xF3] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xF4] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xF5] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xF6] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xF7] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xF8] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xF9] = &CZ80MsxDos::op_LD_SP_IX;
	m_Op.Extended2IX[0xFA] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xFB] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xFC] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xFD] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xFE] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX[0xFF] = &CZ80MsxDos::op_UNDEFINED;

	m_Op.Extended2IX2[0x00] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x01] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x02] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x03] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x04] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x05] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x06] = &CZ80MsxDos::op_RLC_memVpIX;
	m_Op.Extended2IX2[0x07] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x08] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x09] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x0A] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x0B] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x0C] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x0D] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x0E] = &CZ80MsxDos::op_RRC_memVpIX;
	m_Op.Extended2IX2[0x0F] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x10] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x11] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x12] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x13] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x14] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x15] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x16] = &CZ80MsxDos::op_RL_memVpIX;
	m_Op.Extended2IX2[0x17] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x18] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x19] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x1A] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x1B] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x1C] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x1D] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x1E] = &CZ80MsxDos::op_RR_memVpIX;
	m_Op.Extended2IX2[0x1F] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x20] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x21] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x22] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x23] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x24] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x25] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x26] = &CZ80MsxDos::op_SLA_memVpIX;
	m_Op.Extended2IX2[0x27] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x28] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x29] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x2A] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x2B] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x2C] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x2D] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x2E] = &CZ80MsxDos::op_SRA_memVpIX;
	m_Op.Extended2IX2[0x2F] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x30] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x31] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x32] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x33] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x34] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x35] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x36] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x37] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x38] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x39] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x3A] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x3B] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x3C] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x3D] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x3E] = &CZ80MsxDos::op_SRL_memVpIX;
	m_Op.Extended2IX2[0x3F] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x40] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x41] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x42] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x43] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x44] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x45] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x46] = &CZ80MsxDos::op_BIT_0_memVpIX;
	m_Op.Extended2IX2[0x47] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x48] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x49] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x4A] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x4B] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x4C] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x4D] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x4E] = &CZ80MsxDos::op_BIT_1_memVpIX;
	m_Op.Extended2IX2[0x4F] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x50] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x51] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x52] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x53] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x54] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x55] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x56] = &CZ80MsxDos::op_BIT_2_memVpIX;
	m_Op.Extended2IX2[0x57] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x58] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x59] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x5A] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x5B] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x5C] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x5D] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x5E] = &CZ80MsxDos::op_BIT_3_memVpIX;
	m_Op.Extended2IX2[0x5F] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x60] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x61] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x62] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x63] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x64] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x65] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x66] = &CZ80MsxDos::op_BIT_4_memVpIX;
	m_Op.Extended2IX2[0x67] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x68] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x69] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x6A] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x6B] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x6C] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x6D] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x6E] = &CZ80MsxDos::op_BIT_5_memVpIX;
	m_Op.Extended2IX2[0x6F] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x70] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x71] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x72] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x73] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x74] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x75] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x76] = &CZ80MsxDos::op_BIT_6_memVpIX;
	m_Op.Extended2IX2[0x77] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x78] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x79] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x7A] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x7B] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x7C] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x7D] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x7E] = &CZ80MsxDos::op_BIT_7_memVpIX;
	m_Op.Extended2IX2[0x7F] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x80] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x81] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x82] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x83] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x84] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x85] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x86] = &CZ80MsxDos::op_RES_0_memVpIX;
	m_Op.Extended2IX2[0x87] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x88] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x89] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x8A] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x8B] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x8C] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x8D] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x8E] = &CZ80MsxDos::op_RES_1_memVpIX;
	m_Op.Extended2IX2[0x8F] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x90] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x91] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x92] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x93] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x94] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x95] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x96] = &CZ80MsxDos::op_RES_2_memVpIX;
	m_Op.Extended2IX2[0x97] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x98] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x99] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x9A] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x9B] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x9C] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x9D] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0x9E] = &CZ80MsxDos::op_RES_3_memVpIX;
	m_Op.Extended2IX2[0x9F] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xA0] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xA1] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xA2] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xA3] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xA4] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xA5] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xA6] = &CZ80MsxDos::op_RES_4_memVpIX;
	m_Op.Extended2IX2[0xA7] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xA8] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xA9] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xAA] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xAB] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xAC] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xAD] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xAE] = &CZ80MsxDos::op_RES_5_memVpIX;
	m_Op.Extended2IX2[0xAF] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xB0] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xB1] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xB2] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xB3] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xB4] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xB5] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xB6] = &CZ80MsxDos::op_RES_6_memVpIX;
	m_Op.Extended2IX2[0xB7] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xB8] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xB9] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xBA] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xBB] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xBC] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xBD] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xBE] = &CZ80MsxDos::op_RES_7_memVpIX;
	m_Op.Extended2IX2[0xBF] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xC0] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xC1] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xC2] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xC3] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xC4] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xC5] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xC6] = &CZ80MsxDos::op_SET_0_memVpIX;
	m_Op.Extended2IX2[0xC7] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xC8] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xC9] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xCA] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xCB] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xCC] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xCD] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xCE] = &CZ80MsxDos::op_SET_1_memVpIX;
	m_Op.Extended2IX2[0xCF] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xD0] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xD1] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xD2] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xD3] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xD4] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xD5] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xD6] = &CZ80MsxDos::op_SET_2_memVpIX;
	m_Op.Extended2IX2[0xD7] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xD8] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xD9] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xDA] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xDB] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xDC] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xDD] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xDE] = &CZ80MsxDos::op_SET_3_memVpIX;
	m_Op.Extended2IX2[0xDF] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xE0] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xE1] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xE2] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xE3] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xE4] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xE5] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xE6] = &CZ80MsxDos::op_SET_4_memVpIX;
	m_Op.Extended2IX2[0xE7] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xE8] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xE9] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xEA] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xEB] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xEC] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xED] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xEE] = &CZ80MsxDos::op_SET_5_memVpIX;
	m_Op.Extended2IX2[0xEF] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xF0] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xF1] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xF2] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xF3] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xF4] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xF5] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xF6] = &CZ80MsxDos::op_SET_6_memVpIX;
	m_Op.Extended2IX2[0xF7] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xF8] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xF9] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xFA] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xFB] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xFC] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xFD] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended2IX2[0xFE] = &CZ80MsxDos::op_SET_7_memVpIX;
	m_Op.Extended2IX2[0xFF] = &CZ80MsxDos::op_UNDEFINED;

	m_Op.Extended4IY[0x00] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x01] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x02] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x03] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x04] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x05] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x06] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x07] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x08] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x09] = &CZ80MsxDos::op_ADD_IY_BC;
	m_Op.Extended4IY[0x0A] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x0B] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x0C] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x0D] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x0E] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x0F] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x10] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x11] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x12] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x13] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x14] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x15] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x16] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x17] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x18] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x19] = &CZ80MsxDos::op_ADD_IY_DE;
	m_Op.Extended4IY[0x1A] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x1B] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x1C] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x1D] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x1E] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x1F] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x20] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x21] = &CZ80MsxDos::op_LD_IY_ad;
	m_Op.Extended4IY[0x22] = &CZ80MsxDos::op_LD_memAD_IY;
	m_Op.Extended4IY[0x23] = &CZ80MsxDos::op_INC_IY;
	m_Op.Extended4IY[0x24] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x25] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x26] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x27] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x28] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x29] = &CZ80MsxDos::op_ADD_IY_IY;
	m_Op.Extended4IY[0x2A] = &CZ80MsxDos::op_LD_IY_memAD;
	m_Op.Extended4IY[0x2B] = &CZ80MsxDos::op_DEC_IY;
	m_Op.Extended4IY[0x2C] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x2D] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x2E] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x2F] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x30] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x31] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x32] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x33] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x34] = &CZ80MsxDos::op_INC_memIYpV;
	m_Op.Extended4IY[0x35] = &CZ80MsxDos::op_DEC_memIYpV;
	m_Op.Extended4IY[0x36] = &CZ80MsxDos::op_LD_memIYpV_v;
	m_Op.Extended4IY[0x37] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x38] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x39] = &CZ80MsxDos::op_ADD_IY_SP;
	m_Op.Extended4IY[0x3A] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x3B] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x3C] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x3D] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x3E] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x3F] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x40] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x41] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x42] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x43] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x44] = &CZ80MsxDos::op_LD_B_IYH;
	m_Op.Extended4IY[0x45] = &CZ80MsxDos::op_LD_B_IYL;
	m_Op.Extended4IY[0x46] = &CZ80MsxDos::op_LD_B_memIYpV;
	m_Op.Extended4IY[0x47] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x48] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x49] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x4A] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x4B] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x4C] = &CZ80MsxDos::op_LD_C_IYH;
	m_Op.Extended4IY[0x4D] = &CZ80MsxDos::op_LD_C_IYL;
	m_Op.Extended4IY[0x4E] = &CZ80MsxDos::op_LD_C_memIYpV;
	m_Op.Extended4IY[0x4F] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x50] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x51] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x52] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x53] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x54] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x55] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x56] = &CZ80MsxDos::op_LD_D_memIYpV;
	m_Op.Extended4IY[0x57] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x58] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x59] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x5A] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x5B] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x5C] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x5D] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x5E] = &CZ80MsxDos::op_LD_E_memIYpV;
	m_Op.Extended4IY[0x5F] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x60] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x61] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x62] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x63] = &CZ80MsxDos::op_LD_IYH_E;
	m_Op.Extended4IY[0x64] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x65] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x66] = &CZ80MsxDos::op_LD_H_memIYpV;
	m_Op.Extended4IY[0x67] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x68] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x69] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x6A] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x6B] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x6C] = &CZ80MsxDos::op_LD_IYL_IYH;
	m_Op.Extended4IY[0x6D] = &CZ80MsxDos::op_LD_IYL_IYL;
	m_Op.Extended4IY[0x6E] = &CZ80MsxDos::op_LD_L_memIYpV;
	m_Op.Extended4IY[0x6F] = &CZ80MsxDos::op_LD_IYL_A;
	m_Op.Extended4IY[0x70] = &CZ80MsxDos::op_LD_memIYpV_B;
	m_Op.Extended4IY[0x71] = &CZ80MsxDos::op_LD_memIYpV_C;
	m_Op.Extended4IY[0x72] = &CZ80MsxDos::op_LD_memIYpV_D;
	m_Op.Extended4IY[0x73] = &CZ80MsxDos::op_LD_memIYpV_E;
	m_Op.Extended4IY[0x74] = &CZ80MsxDos::op_LD_memIYpV_H;
	m_Op.Extended4IY[0x75] = &CZ80MsxDos::op_LD_memIYpV_L;
	m_Op.Extended4IY[0x76] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x77] = &CZ80MsxDos::op_LD_memIYpV_A;
	m_Op.Extended4IY[0x78] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x79] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x7A] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x7B] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x7C] = &CZ80MsxDos::op_LD_A_IYH;
	m_Op.Extended4IY[0x7D] = &CZ80MsxDos::op_LD_A_IYL;
	m_Op.Extended4IY[0x7E] = &CZ80MsxDos::op_LD_A_memIYpV;
	m_Op.Extended4IY[0x7F] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x80] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x81] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x82] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x83] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x84] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x85] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x86] = &CZ80MsxDos::op_ADD_A_memIYpV;
	m_Op.Extended4IY[0x87] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x88] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x89] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x8A] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x8B] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x8C] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x8D] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x8E] = &CZ80MsxDos::op_ADC_A_memIYpV;
	m_Op.Extended4IY[0x8F] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x90] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x91] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x92] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x93] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x94] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x95] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x96] = &CZ80MsxDos::op_SUB_memIYpV;
	m_Op.Extended4IY[0x97] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x98] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x99] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x9A] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x9B] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x9C] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x9D] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0x9E] = &CZ80MsxDos::op_SBC_A_memIYpV;
	m_Op.Extended4IY[0x9F] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xA0] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xA1] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xA2] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xA3] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xA4] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xA5] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xA6] = &CZ80MsxDos::op_AND_memIYpV;
	m_Op.Extended4IY[0xA7] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xA8] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xA9] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xAA] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xAB] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xAC] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xAD] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xAE] = &CZ80MsxDos::op_XOR_memIYpV;
	m_Op.Extended4IY[0xAF] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xB0] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xB1] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xB2] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xB3] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xB4] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xB5] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xB6] = &CZ80MsxDos::op_OR_memIYpV;
	m_Op.Extended4IY[0xB7] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xB8] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xB9] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xBA] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xBB] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xBC] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xBD] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xBE] = &CZ80MsxDos::op_CP_memIYpV;
	m_Op.Extended4IY[0xBF] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xC0] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xC1] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xC2] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xC3] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xC4] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xC5] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xC6] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xC7] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xC8] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xC9] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xCA] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xCB] = &CZ80MsxDos::op_EXTENDED_4IY2;
	m_Op.Extended4IY[0xCC] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xCD] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xCE] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xCF] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xD0] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xD1] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xD2] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xD3] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xD4] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xD5] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xD6] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xD7] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xD8] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xD9] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xDA] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xDB] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xDC] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xDD] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xDE] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xDF] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xE0] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xE1] = &CZ80MsxDos::op_POP_IY;
	m_Op.Extended4IY[0xE2] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xE3] = &CZ80MsxDos::op_EX_memSP_IY;
	m_Op.Extended4IY[0xE4] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xE5] = &CZ80MsxDos::op_PUSH_IY;
	m_Op.Extended4IY[0xE6] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xE7] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xE8] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xE9] = &CZ80MsxDos::op_JP_memIY;
	m_Op.Extended4IY[0xEA] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xEB] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xEC] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xED] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xEE] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xEF] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xF0] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xF1] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xF2] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xF3] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xF4] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xF5] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xF6] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xF7] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xF8] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xF9] = &CZ80MsxDos::op_LD_SP_IY;
	m_Op.Extended4IY[0xFA] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xFB] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xFC] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xFD] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xFE] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY[0xFF] = &CZ80MsxDos::op_UNDEFINED;

	m_Op.Extended4IY2[0x00] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x01] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x02] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x03] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x04] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x05] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x06] = &CZ80MsxDos::op_RLC_memVpIY;
	m_Op.Extended4IY2[0x07] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x08] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x09] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x0A] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x0B] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x0C] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x0D] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x0E] = &CZ80MsxDos::op_RRC_memVpIY;
	m_Op.Extended4IY2[0x0F] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x10] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x11] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x12] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x13] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x14] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x15] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x16] = &CZ80MsxDos::op_RL_memVpIY;
	m_Op.Extended4IY2[0x17] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x18] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x19] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x1A] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x1B] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x1C] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x1D] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x1E] = &CZ80MsxDos::op_RR_memVpIY;
	m_Op.Extended4IY2[0x1F] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x20] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x21] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x22] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x23] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x24] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x25] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x26] = &CZ80MsxDos::op_SLA_memVpIY;
	m_Op.Extended4IY2[0x27] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x28] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x29] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x2A] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x2B] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x2C] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x2D] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x2E] = &CZ80MsxDos::op_SRA_memVpIY;
	m_Op.Extended4IY2[0x2F] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x30] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x31] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x32] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x33] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x34] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x35] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x36] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x37] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x38] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x39] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x3A] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x3B] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x3C] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x3D] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x3E] = &CZ80MsxDos::op_SRL_memVpIY;
	m_Op.Extended4IY2[0x3F] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x40] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x41] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x42] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x43] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x44] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x45] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x46] = &CZ80MsxDos::op_BIT_0_memVpIY;
	m_Op.Extended4IY2[0x47] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x48] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x49] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x4A] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x4B] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x4C] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x4D] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x4E] = &CZ80MsxDos::op_BIT_1_memVpIY;
	m_Op.Extended4IY2[0x4F] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x50] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x51] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x52] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x53] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x54] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x55] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x56] = &CZ80MsxDos::op_BIT_2_memVpIY;
	m_Op.Extended4IY2[0x57] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x58] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x59] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x5A] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x5B] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x5C] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x5D] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x5E] = &CZ80MsxDos::op_BIT_3_memVpIY;
	m_Op.Extended4IY2[0x5F] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x60] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x61] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x62] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x63] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x64] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x65] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x66] = &CZ80MsxDos::op_BIT_4_memVpIY;
	m_Op.Extended4IY2[0x67] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x68] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x69] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x6A] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x6B] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x6C] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x6D] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x6E] = &CZ80MsxDos::op_BIT_5_memVpIY;
	m_Op.Extended4IY2[0x6F] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x70] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x71] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x72] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x73] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x74] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x75] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x76] = &CZ80MsxDos::op_BIT_6_memVpIY;
	m_Op.Extended4IY2[0x77] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x78] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x79] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x7A] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x7B] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x7C] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x7D] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x7E] = &CZ80MsxDos::op_BIT_7_memVpIY;
	m_Op.Extended4IY2[0x7F] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x80] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x81] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x82] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x83] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x84] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x85] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x86] = &CZ80MsxDos::op_RES_0_memVpIY;
	m_Op.Extended4IY2[0x87] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x88] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x89] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x8A] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x8B] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x8C] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x8D] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x8E] = &CZ80MsxDos::op_RES_1_memVpIY;
	m_Op.Extended4IY2[0x8F] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x90] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x91] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x92] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x93] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x94] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x95] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x96] = &CZ80MsxDos::op_RES_2_memVpIY;
	m_Op.Extended4IY2[0x97] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x98] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x99] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x9A] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x9B] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x9C] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x9D] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0x9E] = &CZ80MsxDos::op_RES_3_memVpIY;
	m_Op.Extended4IY2[0x9F] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xA0] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xA1] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xA2] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xA3] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xA4] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xA5] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xA6] = &CZ80MsxDos::op_RES_4_memVpIY;
	m_Op.Extended4IY2[0xA7] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xA8] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xA9] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xAA] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xAB] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xAC] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xAD] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xAE] = &CZ80MsxDos::op_RES_5_memVpIY;
	m_Op.Extended4IY2[0xAF] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xB0] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xB1] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xB2] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xB3] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xB4] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xB5] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xB6] = &CZ80MsxDos::op_RES_6_memVpIY;
	m_Op.Extended4IY2[0xB7] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xB8] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xB9] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xBA] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xBB] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xBC] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xBD] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xBE] = &CZ80MsxDos::op_RES_7_memVpIY;
	m_Op.Extended4IY2[0xBF] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xC0] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xC1] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xC2] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xC3] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xC4] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xC5] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xC6] = &CZ80MsxDos::op_SET_0_memVpIY;
	m_Op.Extended4IY2[0xC7] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xC8] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xC9] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xCA] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xCB] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xCC] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xCD] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xCE] = &CZ80MsxDos::op_SET_1_memVpIY;
	m_Op.Extended4IY2[0xCF] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xD0] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xD1] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xD2] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xD3] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xD4] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xD5] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xD6] = &CZ80MsxDos::op_SET_2_memVpIY;
	m_Op.Extended4IY2[0xD7] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xD8] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xD9] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xDA] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xDB] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xDC] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xDD] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xDE] = &CZ80MsxDos::op_SET_3_memVpIY;
	m_Op.Extended4IY2[0xDF] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xE0] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xE1] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xE2] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xE3] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xE4] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xE5] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xE6] = &CZ80MsxDos::op_SET_4_memVpIY;
	m_Op.Extended4IY2[0xE7] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xE8] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xE9] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xEA] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xEB] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xEC] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xED] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xEE] = &CZ80MsxDos::op_SET_5_memVpIY;
	m_Op.Extended4IY2[0xEF] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xF0] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xF1] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xF2] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xF3] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xF4] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xF5] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xF6] = &CZ80MsxDos::op_SET_6_memVpIY;
	m_Op.Extended4IY2[0xF7] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xF8] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xF9] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xFA] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xFB] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xFC] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xFD] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended4IY2[0xFE] = &CZ80MsxDos::op_SET_7_memVpIY;
	m_Op.Extended4IY2[0xFF] = &CZ80MsxDos::op_UNDEFINED;

	m_Op.Extended3[0x00] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x01] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x02] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x03] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x04] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x05] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x06] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x07] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x08] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x09] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x0A] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x0B] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x0C] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x0D] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x0E] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x0F] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x10] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x11] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x12] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x13] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x14] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x15] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x16] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x17] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x18] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x19] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x1A] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x1B] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x1C] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x1D] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x1E] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x1F] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x20] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x21] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x22] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x23] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x24] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x25] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x26] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x27] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x28] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x29] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x2A] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x2B] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x2C] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x2D] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x2E] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x2F] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x30] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x31] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x32] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x33] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x34] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x35] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x36] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x37] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x38] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x39] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x3A] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x3B] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x3C] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x3D] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x3E] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x3F] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x40] = &CZ80MsxDos::op_IN_B_memC;
	m_Op.Extended3[0x41] = &CZ80MsxDos::op_OUT_memC_B;
	m_Op.Extended3[0x42] = &CZ80MsxDos::op_SBC_HL_BC;
	m_Op.Extended3[0x43] = &CZ80MsxDos::op_LD_memAD_BC;
	m_Op.Extended3[0x44] = &CZ80MsxDos::op_NEG;
	m_Op.Extended3[0x45] = &CZ80MsxDos::op_RETN;
	m_Op.Extended3[0x46] = &CZ80MsxDos::op_IM_0;
	m_Op.Extended3[0x47] = &CZ80MsxDos::op_LD_i_A;
	m_Op.Extended3[0x48] = &CZ80MsxDos::op_IN_C_memC;
	m_Op.Extended3[0x49] = &CZ80MsxDos::op_OUT_memC_C;
	m_Op.Extended3[0x4A] = &CZ80MsxDos::op_ADC_HL_BC;
	m_Op.Extended3[0x4B] = &CZ80MsxDos::op_LD_BC_memAD;
	m_Op.Extended3[0x4C] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x4D] = &CZ80MsxDos::op_RETI;
	m_Op.Extended3[0x4E] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x4F] = &CZ80MsxDos::op_LD_R_A;
	m_Op.Extended3[0x50] = &CZ80MsxDos::op_IN_D_memC;
	m_Op.Extended3[0x51] = &CZ80MsxDos::op_OUT_memC_D;
	m_Op.Extended3[0x52] = &CZ80MsxDos::op_SBC_HL_DE;
	m_Op.Extended3[0x53] = &CZ80MsxDos::op_LD_memAD_DE;
	m_Op.Extended3[0x54] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x55] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x56] = &CZ80MsxDos::op_IM_1;
	m_Op.Extended3[0x57] = &CZ80MsxDos::op_LD_A_i;
	m_Op.Extended3[0x58] = &CZ80MsxDos::op_IN_E_memC;
	m_Op.Extended3[0x59] = &CZ80MsxDos::op_OUT_memC_E;
	m_Op.Extended3[0x5A] = &CZ80MsxDos::op_ADC_HL_DE;
	m_Op.Extended3[0x5B] = &CZ80MsxDos::op_LD_DE_memAD;
	m_Op.Extended3[0x5C] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x5D] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x5E] = &CZ80MsxDos::op_IM_2;
	m_Op.Extended3[0x5F] = &CZ80MsxDos::op_LD_A_R;
	m_Op.Extended3[0x60] = &CZ80MsxDos::op_IN_H_memC;
	m_Op.Extended3[0x61] = &CZ80MsxDos::op_OUT_memC_H;
	m_Op.Extended3[0x62] = &CZ80MsxDos::op_SBC_HL_HL;
	m_Op.Extended3[0x63] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x64] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x65] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x66] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x67] = &CZ80MsxDos::op_RRD;
	m_Op.Extended3[0x68] = &CZ80MsxDos::op_IN_L_memC;
	m_Op.Extended3[0x69] = &CZ80MsxDos::op_OUT_memC_L;
	m_Op.Extended3[0x6A] = &CZ80MsxDos::op_ADC_HL_HL;
	m_Op.Extended3[0x6B] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x6C] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x6D] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x6E] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x6F] = &CZ80MsxDos::op_RLD;
	m_Op.Extended3[0x70] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x71] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x72] = &CZ80MsxDos::op_SBC_HL_SP;
	m_Op.Extended3[0x73] = &CZ80MsxDos::op_LD_memAD_SP;
	m_Op.Extended3[0x74] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x75] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x76] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x77] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x78] = &CZ80MsxDos::op_IN_A_memC;
	m_Op.Extended3[0x79] = &CZ80MsxDos::op_OUT_memC_A;
	m_Op.Extended3[0x7A] = &CZ80MsxDos::op_ADC_HL_SP;
	m_Op.Extended3[0x7B] = &CZ80MsxDos::op_LD_SP_memAD;
	m_Op.Extended3[0x7C] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x7D] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x7E] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x7F] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x80] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x81] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x82] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x83] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x84] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x85] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x86] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x87] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x88] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x89] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x8A] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x8B] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x8C] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x8D] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x8E] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x8F] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x90] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x91] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x92] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x93] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x94] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x95] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x96] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x97] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x98] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x99] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x9A] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x9B] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x9C] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x9D] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x9E] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0x9F] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xA0] = &CZ80MsxDos::op_LDI;
	m_Op.Extended3[0xA1] = &CZ80MsxDos::op_CPI;
	m_Op.Extended3[0xA2] = &CZ80MsxDos::op_INI;
	m_Op.Extended3[0xA3] = &CZ80MsxDos::op_OUTI;
	m_Op.Extended3[0xA4] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xA5] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xA6] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xA7] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xA8] = &CZ80MsxDos::op_LDD;
	m_Op.Extended3[0xA9] = &CZ80MsxDos::op_CPD;
	m_Op.Extended3[0xAA] = &CZ80MsxDos::op_IND;
	m_Op.Extended3[0xAB] = &CZ80MsxDos::op_OUTD;
	m_Op.Extended3[0xAC] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xAD] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xAE] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xAF] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xB0] = &CZ80MsxDos::op_LDIR;
	m_Op.Extended3[0xB1] = &CZ80MsxDos::op_CPIR;
	m_Op.Extended3[0xB2] = &CZ80MsxDos::op_INIR;
	m_Op.Extended3[0xB3] = &CZ80MsxDos::op_OTIR;
	m_Op.Extended3[0xB4] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xB5] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xB6] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xB7] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xB8] = &CZ80MsxDos::op_LDDR;
	m_Op.Extended3[0xB9] = &CZ80MsxDos::op_CPDR;
	m_Op.Extended3[0xBA] = &CZ80MsxDos::op_INDR;
	m_Op.Extended3[0xBB] = &CZ80MsxDos::op_OUTR;
	m_Op.Extended3[0xBC] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xBD] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xBE] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xBF] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xC0] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xC1] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xC2] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xC3] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xC4] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xC5] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xC6] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xC7] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xC8] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xC9] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xCA] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xCB] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xCC] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xCD] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xCE] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xCF] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xD0] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xD1] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xD2] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xD3] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xD4] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xD5] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xD6] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xD7] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xD8] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xD9] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xDA] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xDB] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xDC] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xDD] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xDE] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xDF] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xE0] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xE1] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xE2] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xE3] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xE4] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xE5] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xE6] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xE7] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xE8] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xE9] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xEA] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xEB] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xEC] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xED] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xEE] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xEF] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xF0] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xF1] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xF2] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xF3] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xF4] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xF5] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xF6] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xF7] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xF8] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xF9] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xFA] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xFB] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xFC] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xFD] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xFE] = &CZ80MsxDos::op_UNDEFINED;
	m_Op.Extended3[0xFF] = &CZ80MsxDos::op_UNDEFINED;
	return;
};


void CZ80MsxDos::op_UNDEFINED()
{
	// std::wcout
	// 	<< _T("\n **HopStepZ >> Undefined Z80 operation code in ")
	// 	<< std::hex << std::setfill(_T('0')) << std::setw(4) << (int)m_R.PC << _T("h\n");
	assert(false);
}
// single
void CZ80MsxDos::op_NOP()
{
	// do nothing
	return;
}
void CZ80MsxDos::op_LD_BC_ad()
{
	m_R.C = m_pMemSys->Read(m_R.PC++);
	m_R.B = m_pMemSys->Read(m_R.PC++);
	return;
}
void CZ80MsxDos::op_LD_memBC_A()
{
	m_pMemSys->Write(m_R.GetBC(), m_R.A);
	return;
}
void CZ80MsxDos::op_INC_BC()
{
	// 16ビットINCはフラグを変化させない
	m_R.SetBC( static_cast<uint16_t>(m_R.GetBC()+1) );
	return;
}
void CZ80MsxDos::op_INC_B()
{
	m_R.Inc8( &m_R.B );
	return;
}
void CZ80MsxDos::op_DEC_B()
{
	m_R.Dec8( &m_R.B );
	return;
}
void CZ80MsxDos::op_LD_B_v()
{
	// フラグ変化なし
	m_R.B = m_pMemSys->Read(m_R.PC++);
	return;
}
void CZ80MsxDos::op_RLCA()
{
	const uint8_t temp = (m_R.A >> 7) & 0x01;
	m_R.A = (m_R.A << 1) | temp;
	//
	m_R.F.C = temp;
	m_R.F.Z = m_R.F.Z;
	m_R.F.PV= m_R.F.PV;
	m_R.F.S = m_R.F.S;
	m_R.F.N = 0;
	m_R.F.H = 0;
	return;
}
void CZ80MsxDos::op_EX_AF_AF()
{
	// フラグの状態は変化しない
	m_R.Swap(&m_R.A, &m_R.Ad);
	m_R.Swap(&m_R.F, &m_R.Fd);
	return;
}
void CZ80MsxDos::op_ADD_HL_BC()
{
	uint16_t hl = m_R.GetHL();
	m_R.Add16(&hl, m_R.GetBC());
	m_R.SetHL(hl);
	return;
}
void CZ80MsxDos::op_LD_A_memBC()
{
	// フラグ変化なし
	m_R.A = m_pMemSys->Read(m_R.GetBC());
	return;
}

void CZ80MsxDos::op_DEC_BC()
{
	// 16ビットDECはフラグを変化させない
	m_R.SetBC( static_cast<uint16_t>(m_R.GetBC()-1) );
	return;
}
void CZ80MsxDos::op_INC_C()
{
	m_R.Inc8(&m_R.C);
	return;
}
void CZ80MsxDos::op_DEC_C()
{
	m_R.Dec8(&m_R.C);
	return;
}
void CZ80MsxDos::op_LD_C_v()
{
	// フラグ変化なし
	m_R.C = m_pMemSys->Read(m_R.PC++);
	return;
}
void CZ80MsxDos::op_RRCA()
{
	const uint8_t temp = m_R.A & 0x01;
	m_R.A = (m_R.A >> 1) | (temp << 7);
	//
	m_R.F.C = temp;
	m_R.F.Z = m_R.F.Z;
	m_R.F.PV= m_R.F.PV;
	m_R.F.S = m_R.F.S;
	m_R.F.N = 0;
	m_R.F.H = 0;
	return;
}
void CZ80MsxDos::op_DJNZ_v()
{
	m_R.B--;
	if( m_R.B == 0 ){
		m_R.PC++;	// v を読み捨て
	}
	else{
		int8_t off = m_pMemSys->ReadInt8(m_R.PC);
		m_R.PC = static_cast<uint16_t>(static_cast<int32_t>(m_R.PC-1) + 2 + off);
	}
	return;
}
void CZ80MsxDos::op_LD_DE_ad()
{
	m_R.E = m_pMemSys->Read(m_R.PC++);
	m_R.D = m_pMemSys->Read(m_R.PC++);
	return;
}
void CZ80MsxDos::op_LD_memDE_A()
{
	uint16_t ad = m_R.GetDE();
	m_pMemSys->Write(ad, m_R.A);
	return;
}
void CZ80MsxDos::op_INC_DE()
{
	// 16ビットINCはフラグを変化させない
	m_R.SetDE(static_cast<uint16_t>(m_R.GetDE() + 1));
	return;
}
void CZ80MsxDos::op_INC_D()
{
	m_R.Inc8(&m_R.D);
	return;
}
void CZ80MsxDos::op_DEC_D()
{
	m_R.Dec8(&m_R.D);
	return;
}
void CZ80MsxDos::op_LD_D_v()
{
	// フラグ変化なし
	m_R.D = m_pMemSys->Read(m_R.PC++);
	return;
}
void CZ80MsxDos::op_RLA()
{
	const uint8_t temp = (m_R.A >> 7) & 0x01;
	m_R.A = (m_R.A << 1) | static_cast<uint8_t>(m_R.F.C);
	//
	m_R.F.C = temp;
	m_R.F.Z = m_R.F.Z;
	m_R.F.PV= m_R.F.PV;
	m_R.F.S = m_R.F.S;
	m_R.F.N = 0;
	m_R.F.H = 0;
	return;
}
void CZ80MsxDos::op_JR_v()
{
	int8_t off = m_pMemSys->ReadInt8(m_R.PC);
	m_R.PC = static_cast<uint16_t>(static_cast<int32_t>(m_R.PC-1) + 2 + off);
	return;
}
void CZ80MsxDos::op_ADD_HL_DE()
{
	uint16_t hl = m_R.GetHL();
	m_R.Add16(&hl, m_R.GetDE());
	m_R.SetHL(hl);
	return;
}
void CZ80MsxDos::op_LD_A_memDE()
{
	uint16_t ad = m_R.GetDE();
	m_R.A = m_pMemSys->Read(ad);
	return;
}
void CZ80MsxDos::op_DEC_DE()
{
	// 16ビットDECはフラグを変化させない
	m_R.SetDE( static_cast<uint16_t>(m_R.GetDE()-1) );
	return;
}
void CZ80MsxDos::op_INC_E()
{
	m_R.Inc8(&m_R.E);
	return;
}
void CZ80MsxDos::op_DEC_E()
{
	m_R.Dec8(&m_R.E);
	return;
}
void CZ80MsxDos::op_LD_E_v()
{
	// フラグ変化なし
	m_R.E = m_pMemSys->Read(m_R.PC++);
	return;
}
void CZ80MsxDos::op_RRA()
{
	const uint8_t temp = m_R.A & 0x01;
	m_R.A = (m_R.A >> 1) | (m_R.F.C << 7);
	//
	m_R.F.C = temp;
	m_R.F.Z = m_R.F.Z;
	m_R.F.PV= m_R.F.PV;
	m_R.F.S = m_R.F.S;
	m_R.F.N = 0;
	m_R.F.H = 0;
	return;
}
void CZ80MsxDos::op_JR_nz_v()
{
	if( m_R.F.Z == 0 ){
		int8_t off = m_pMemSys->ReadInt8(m_R.PC);
		m_R.PC = static_cast<uint16_t>(static_cast<int32_t>(m_R.PC-1) + 2 + off);
	}
	else{
		m_R.PC++;	// v を読み捨て
	}
	return;
}
void CZ80MsxDos::op_LD_HL_ad()
{
	m_R.L = m_pMemSys->Read(m_R.PC++);
	m_R.H = m_pMemSys->Read(m_R.PC++);
	return;
}
void CZ80MsxDos::op_LD_memAD_HL()
{
	uint16_t ad = m_pMemSys->Read(m_R.PC++);
	ad |= static_cast<z80memaddr_t>(m_pMemSys->Read(m_R.PC++)) << 8;
	//
	m_pMemSys->Write(ad+0, m_R.L);
	m_pMemSys->Write(ad+1, m_R.H);
	return;
}
void CZ80MsxDos::op_INC_HL()
{
	// 16ビットINCはフラグを変化させない
	m_R.SetHL( static_cast<uint16_t>(m_R.GetHL()+1) );
	return;
}
void CZ80MsxDos::op_INC_H()
{
	m_R.Inc8(&m_R.H);
	return;
}
void CZ80MsxDos::op_DEC_H()
{
	m_R.Dec8(&m_R.H);
	return;
}
void CZ80MsxDos::op_LD_H_v()
{
	// フラグ変化なし
	m_R.H = m_pMemSys->Read(m_R.PC++);
	return;
}
void CZ80MsxDos::op_DAA()
{
	const uint8_t Lo = (m_R.A >> 0) & 0x0f;
	const uint8_t Hi = (m_R.A >> 4) & 0x0f;
	// ややこしいので、Z80ファミリハンドブック(第七版) P.86の機能表をそのままIF構文で実装する
	// 動けばいいいんだよ
	uint8_t v = 0;
	if( m_R.F.N == 0 ){
		if( m_R.F.C == 0 && (0x0 <= Hi && Hi <= 0x9) && m_R.F.H == 0 && (0x0 <= Lo && Lo <= 0x9)) v = 0x00, m_R.F.C = 0;
		if( m_R.F.C == 0 && (0x0 <= Hi && Hi <= 0x8) && m_R.F.H == 0 && (0xA <= Lo && Lo <= 0xF)) v = 0x06, m_R.F.C = 0;
		if( m_R.F.C == 0 && (0x0 <= Hi && Hi <= 0x9) && m_R.F.H == 1 && (0x0 <= Lo && Lo <= 0x3)) v = 0x06, m_R.F.C = 0;
		if( m_R.F.C == 0 && (0xA <= Hi && Hi <= 0xF) && m_R.F.H == 1 && (0x0 <= Lo && Lo <= 0x9)) v = 0x60, m_R.F.C = 1;
		if( m_R.F.C == 0 && (0x9 <= Hi && Hi <= 0xF) && m_R.F.H == 0 && (0xA <= Lo && Lo <= 0xF)) v = 0x66, m_R.F.C = 1;
		if( m_R.F.C == 0 && (0xA <= Hi && Hi <= 0xF) && m_R.F.H == 1 && (0x0 <= Lo && Lo <= 0x3)) v = 0x66, m_R.F.C = 1;
		if( m_R.F.C == 1 && (0x0 <= Hi && Hi <= 0x2) && m_R.F.H == 0 && (0x0 <= Lo && Lo <= 0x9)) v = 0x60, m_R.F.C = 1;
		if( m_R.F.C == 1 && (0x0 <= Hi && Hi <= 0x2) && m_R.F.H == 0 && (0xA <= Lo && Lo <= 0xF)) v = 0x66, m_R.F.C = 1;
		if( m_R.F.C == 1 && (0x0 <= Hi && Hi <= 0x3) && m_R.F.H == 1 && (0x0 <= Lo && Lo <= 0x3)) v = 0x66, m_R.F.C = 1;
		m_R.A += v;
		m_R.F.H = ((m_R.F.C!=0)||(Hi<((m_R.A>>4)&0x0F)))?1:0;
	}else{
		if( m_R.F.C == 0 && (0x0 <= Hi && Hi <= 0x9) && m_R.F.H == 0 && (0x0 <= Lo && Lo <= 0x9)) v = 0x00, m_R.F.C = 0;
		if( m_R.F.C == 0 && (0x0 <= Hi && Hi <= 0x8) && m_R.F.H == 1 && (0x6 <= Lo && Lo <= 0xF)) v = 0xFA, m_R.F.C = 0;
		if( m_R.F.C == 1 && (0x7 <= Hi && Hi <= 0xF) && m_R.F.H == 0 && (0x0 <= Lo && Lo <= 0x9)) v = 0xA0, m_R.F.C = 1;
		if( m_R.F.C == 1 && (0x6 <= Hi && Hi <= 0xF) && m_R.F.H == 1 && (0x6 <= Lo && Lo <= 0xF)) v = 0x9A, m_R.F.C = 1;
		m_R.A += v;
		m_R.F.H = ((m_R.F.C!=0)||(((m_R.A>>4)&0xF)<Hi))?1:0;
	}
	m_R.F.Z = (m_R.A==0)?1:0;		// 0になったか
	m_R.F.PV= CZ80Regs::CheckParytyEven(m_R.A);
	m_R.F.S = (m_R.A>=0x80)?1:0;	// 符号付きか
	m_R.F.N = m_R.F.N;				// 変化なし
	return;
}
void CZ80MsxDos::op_JR_z_v()
{
	if( m_R.F.Z != 0 ){
		int8_t off = m_pMemSys->ReadInt8(m_R.PC);
		m_R.PC = static_cast<uint16_t>(static_cast<int32_t>(m_R.PC-1) + 2 + off);
	}
	else{
		m_R.PC++;	// v を読み捨て
	}
	return;
}
void CZ80MsxDos::op_ADD_HL_HL()
{
	uint16_t hl = m_R.GetHL();
	m_R.Add16(&hl, m_R.GetHL());
	m_R.SetHL(hl);
	return;
}
void CZ80MsxDos::op_LD_HL_memAD()
{
	uint16_t ad = m_pMemSys->Read(m_R.PC++);
	ad |= static_cast<z80memaddr_t>(m_pMemSys->Read(m_R.PC++)) << 8;
	//
	m_R.L = m_pMemSys->Read(ad+0);
	m_R.H = m_pMemSys->Read(ad+1);
	return;
}
void CZ80MsxDos::op_DEC_HL()
{
	// 16ビットDECはフラグを変化させない
	m_R.SetHL( static_cast<uint16_t>(m_R.GetHL()-1) );
	return;
}
void CZ80MsxDos::op_INC_L()
{
	m_R.Inc8(&m_R.L);
	return;
}
void CZ80MsxDos::op_DEC_L()
{
	m_R.Dec8(&m_R.L);
	return;
}
void CZ80MsxDos::op_LD_L_v()
{
	// フラグ変化なし
	m_R.L = m_pMemSys->Read(m_R.PC++);
	return;
}
void CZ80MsxDos::op_CPL()
{
	m_R.A = m_R.A ^ 0xff;
	//
	m_R.F.C = m_R.F.C;
	m_R.F.Z = m_R.F.Z;	 // Zが変化しないことに注意
	m_R.F.PV= m_R.F.PV;
	m_R.F.S = m_R.F.S;
	m_R.F.N = 1;
	m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_JR_nc_v()
{
	if( m_R.F.C == 0 ){
		int8_t off = m_pMemSys->ReadInt8(m_R.PC);
		m_R.PC = static_cast<uint16_t>(static_cast<int32_t>(m_R.PC-1) + 2 + off);
	}
	else{
		m_R.PC++;	// vを読み捨て
	}
	return;
}
void CZ80MsxDos::op_LD_SP_ad()
{
	m_R.SP = m_pMemSys->Read(m_R.PC++);
	m_R.SP |= static_cast<uint16_t>(m_pMemSys->Read(m_R.PC++)) << 8;
	return;
}
void CZ80MsxDos::op_LD_memAD_A()
{
	uint16_t ad = m_pMemSys->Read(m_R.PC++);
	ad |= static_cast<z80memaddr_t>(m_pMemSys->Read(m_R.PC++)) << 8;
	//
	m_pMemSys->Write(ad, m_R.A);
	return;
}
void CZ80MsxDos::op_INC_SP()
{
	// 16ビットINCはフラグを変化させない
	++m_R.SP;
	return;
}
void CZ80MsxDos::op_INC_memHL()
{
	uint16_t ad = m_R.GetHL();
	uint8_t temp = m_pMemSys->Read(ad);
	m_R.Inc8(&temp);
	m_pMemSys->Write(ad, temp);
	return;
}
void CZ80MsxDos::op_DEC_memHL()
{
	uint16_t ad = m_R.GetHL();
	uint8_t temp = m_pMemSys->Read(ad);
	m_R.Dec8(&temp);
	m_pMemSys->Write(ad, temp);
	return;
}
void CZ80MsxDos::op_LD_memHL_v()
{
	uint8_t v = m_pMemSys->Read(m_R.PC++);
	m_pMemSys->Write(m_R.GetHL(), v);
	return;
}
void CZ80MsxDos::op_SCF()
{
	m_R.F.C = 1;
	return;
}
void CZ80MsxDos::op_JR_C_v()
{
	if( m_R.F.C != 0 ){
		int8_t off = m_pMemSys->ReadInt8(m_R.PC);
		m_R.PC = static_cast<uint16_t>(static_cast<int32_t>(m_R.PC-1) + 2 + off);
	}
	else{
		m_R.PC++;	// v を読み捨て
	}
	return;
}
void CZ80MsxDos::op_ADD_HL_SP()
{
	uint16_t hl = m_R.GetHL();
	m_R.Add16(&hl, m_R.SP);
	m_R.SetHL(hl);
	return;
}
void CZ80MsxDos::op_LD_A_memAD()
{
	uint16_t ad = m_pMemSys->Read(m_R.PC++);
	ad |= static_cast<z80memaddr_t>(m_pMemSys->Read(m_R.PC++)) << 8;
	//
	m_R.A = m_pMemSys->Read(ad);
	return;
}
void CZ80MsxDos::op_DEC_SP()
{
	// 16ビットDECはフラグを変化させない
	--m_R.SP;
	return;
}
void CZ80MsxDos::op_INC_A()
{
	m_R.Inc8(&m_R.A);
	return;
}
void CZ80MsxDos::op_DEC_A()
{
	m_R.Dec8(&m_R.A);
	return;
}

void CZ80MsxDos::op_LD_A_v()
{
	m_R.A = m_pMemSys->Read(m_R.PC++);
	return;
}
void CZ80MsxDos::op_CCF()
{
	m_R.F.C = (m_R.F.C==0)?1:0;
	return;
}
void CZ80MsxDos::op_LD_B_B()
{
	m_R.B = m_R.B;
	return;
}
void CZ80MsxDos::op_LD_B_C()
{
	m_R.B = m_R.C;
	return;
}
void CZ80MsxDos::op_LD_B_D()
{
	m_R.B = m_R.D;
	return;
}
void CZ80MsxDos::op_LD_B_E()
{
	m_R.B = m_R.E;
	return;
}
void CZ80MsxDos::op_LD_B_H()
{
	m_R.B = m_R.H;
	return;
}
void CZ80MsxDos::op_LD_B_L()
{
	m_R.B = m_R.L;
	return;
}
void CZ80MsxDos::op_LD_B_memHL()
{
	uint16_t ad = m_R.GetHL();
	m_R.B = m_pMemSys->Read(ad);
	return;
}
void CZ80MsxDos::op_LD_B_A()
{
	m_R.B = m_R.A;
	return;
}
void CZ80MsxDos::op_LD_C_B()
{
	m_R.C = m_R.B;
	return;
}
void CZ80MsxDos::op_LD_C_C()
{
	m_R.C = m_R.C;
	return;
}
void CZ80MsxDos::op_LD_C_D()
{
	m_R.C = m_R.D;
	return;
}
void CZ80MsxDos::op_LD_C_E()
{
	m_R.C = m_R.E;
	return;
}
void CZ80MsxDos::op_LD_C_H()
{
	m_R.C = m_R.H;
	return;
}
void CZ80MsxDos::op_LD_C_L()
{
	m_R.C = m_R.L;
	return;
}
void CZ80MsxDos::op_LD_C_memHL()
{
	uint16_t ad = m_R.GetHL();
	m_R.C = m_pMemSys->Read(ad);
	return;
}
void CZ80MsxDos::op_LD_C_A()
{
	m_R.C = m_R.A;
	return;
}
void CZ80MsxDos::op_LD_D_B()
{
	m_R.D = m_R.B;
	return;
}
void CZ80MsxDos::op_LD_D_C()
{
	m_R.D = m_R.C;
	return;
}
void CZ80MsxDos::op_LD_D_D()
{
	m_R.D = m_R.D;
	return;
}
void CZ80MsxDos::op_LD_D_E()
{
	m_R.D = m_R.E;
	return;
}
void CZ80MsxDos::op_LD_D_H()
{
	m_R.D = m_R.H;
	return;
}
void CZ80MsxDos::op_LD_D_L()
{
	m_R.D = m_R.L;
	return;
}
void CZ80MsxDos::op_LD_D_memHL()
{
	uint16_t ad = m_R.GetHL();
	m_R.D = m_pMemSys->Read(ad);
	return;
}
void CZ80MsxDos::op_LD_D_A()
{
	m_R.D = m_R.A;
	return;
}
void CZ80MsxDos::op_LD_E_B()
{
	m_R.E = m_R.B;
	return;
}
void CZ80MsxDos::op_LD_E_C()
{
	m_R.E = m_R.C;
	return;
}
void CZ80MsxDos::op_LD_E_D()
{
	m_R.E = m_R.D;
	return;
}
void CZ80MsxDos::op_LD_E_E()
{
	m_R.E = m_R.E;
	return;
}
void CZ80MsxDos::op_LD_E_H()
{
	m_R.E = m_R.H;
	return;
}
void CZ80MsxDos::op_LD_E_L()
{
	m_R.E = m_R.L;
	return;
}
void CZ80MsxDos::op_LD_E_memHL()
{
	uint16_t ad = m_R.GetHL();
	m_R.E = m_pMemSys->Read(ad);
	return;
}
void CZ80MsxDos::op_LD_E_A()
{
	m_R.E = m_R.A;
	return;
}
void CZ80MsxDos::op_LD_H_B()
{
	m_R.H = m_R.B;
	return;
}
void CZ80MsxDos::op_LD_H_C()
{
	m_R.H = m_R.C;
	return;
}
void CZ80MsxDos::op_LD_H_D()
{
	m_R.H = m_R.D;
	return;
}
void CZ80MsxDos::op_LD_H_E()
{
	m_R.H = m_R.E;
	return;
}
void CZ80MsxDos::op_LD_H_H()
{
	m_R.H = m_R.H;
	return;
}
void CZ80MsxDos::op_LD_H_L()
{
	m_R.H = m_R.L;
	return;
}
void CZ80MsxDos::op_LD_H_memHL()
{
	uint16_t ad = m_R.GetHL();
	m_R.H = m_pMemSys->Read(ad);
	return;
}
void CZ80MsxDos::op_LD_H_A()
{
	m_R.H = m_R.A;
	return;
}
void CZ80MsxDos::op_LD_L_B()
{
	m_R.L = m_R.B;
	return;
}
void CZ80MsxDos::op_LD_L_C()
{
	m_R.L = m_R.C;
	return;
}
void CZ80MsxDos::op_LD_L_D()
{
	m_R.L = m_R.D;
	return;
}
void CZ80MsxDos::op_LD_L_E()
{
	m_R.L = m_R.E;
	return;
}
void CZ80MsxDos::op_LD_L_H()
{
	m_R.L = m_R.H;
	return;
}
void CZ80MsxDos::op_LD_L_L()
{
	m_R.L = m_R.L;
	return;
}
void CZ80MsxDos::op_LD_L_memHL()
{
	uint16_t ad = m_R.GetHL();
	m_R.L = m_pMemSys->Read(ad);
	return;
}
void CZ80MsxDos::op_LD_L_A()
{
	m_R.L = m_R.A;
	return;
}
void CZ80MsxDos::op_LD_memHL_B()
{
	uint16_t ad = m_R.GetHL();
	m_pMemSys->Write(ad, m_R.B);
	return;
}
void CZ80MsxDos::op_LD_memHL_C()
{
	uint16_t ad = m_R.GetHL();
	m_pMemSys->Write(ad, m_R.C);
	return;
}
void CZ80MsxDos::op_LD_memHL_D()
{
	uint16_t ad = m_R.GetHL();
	m_pMemSys->Write(ad, m_R.D);
	return;
}
void CZ80MsxDos::op_LD_memHL_E()
{
	uint16_t ad = m_R.GetHL();
	m_pMemSys->Write(ad, m_R.E);
	return;
}
void CZ80MsxDos::op_LD_memHL_H()
{
	uint16_t ad = m_R.GetHL();
	m_pMemSys->Write(ad, m_R.H);
	return;
}
void CZ80MsxDos::op_LD_memHL_L()
{
	uint16_t ad = m_R.GetHL();
	m_pMemSys->Write(ad, m_R.L);
	return;
}
void CZ80MsxDos::op_HALT()
{
	m_bHalt = true;
	return;
}
void CZ80MsxDos::op_LD_memHL_A()
{
	uint16_t ad = m_R.GetHL();
	m_pMemSys->Write(ad, m_R.A);
	return;
}
void CZ80MsxDos::op_LD_A_B()
{
	m_R.A = m_R.B;
	return;
}
void CZ80MsxDos::op_LD_A_C()
{
	m_R.A = m_R.C;
	return;
}
void CZ80MsxDos::op_LD_A_D()
{
	m_R.A = m_R.D;
	return;
}
void CZ80MsxDos::op_LD_A_E()
{
	m_R.A = m_R.E;
	return;
}
void CZ80MsxDos::op_LD_A_H()
{
	m_R.A = m_R.H;
	return;
}
void CZ80MsxDos::op_LD_A_L()
{
	m_R.A = m_R.L;
	return;
}
void CZ80MsxDos::op_LD_A_memHL()
{
	uint16_t ad = m_R.GetHL();
	m_R.A = m_pMemSys->Read(ad);
	return;
}
void CZ80MsxDos::op_LD_A_A()
{
	m_R.A = m_R.A;
	return;
}

void CZ80MsxDos::op_ADD_A_B()
{
	m_R.Add8(&m_R.A, m_R.B);
	return;
}
void CZ80MsxDos::op_ADD_A_C()
{
	m_R.Add8(&m_R.A, m_R.C);
	return;
}
void CZ80MsxDos::op_ADD_A_D()
{
	m_R.Add8(&m_R.A, m_R.D);
	return;
}
void CZ80MsxDos::op_ADD_A_E()
{
	m_R.Add8(&m_R.A, m_R.E);
	return;
}
void CZ80MsxDos::op_ADD_A_H()
{
	m_R.Add8(&m_R.A, m_R.H);
	return;
}
void CZ80MsxDos::op_ADD_A_L()
{
	m_R.Add8(&m_R.A, m_R.L);
	return;
}
void CZ80MsxDos::op_ADD_A_memHL()
{
	uint16_t ad = m_R.GetHL();
	uint8_t v = m_pMemSys->Read(ad);
	m_R.Add8(&m_R.A, v);
	return;
}
void CZ80MsxDos::op_ADD_A_A()
{
	m_R.Add8(&m_R.A, m_R.A);
	return;
}
void CZ80MsxDos::op_ADC_A_B()
{
	m_R.Add8Cy(&m_R.A, m_R.B, m_R.F.C);
	return;
}
void CZ80MsxDos::op_ADC_A_C()
{
	m_R.Add8Cy(&m_R.A, m_R.C, m_R.F.C);
	return;
}
void CZ80MsxDos::op_ADC_A_D()
{
	m_R.Add8Cy(&m_R.A, m_R.D, m_R.F.C);
	return;
}
void CZ80MsxDos::op_ADC_A_E()
{
	m_R.Add8Cy(&m_R.A, m_R.E, m_R.F.C);
	return;
}
void CZ80MsxDos::op_ADC_A_H()
{
	m_R.Add8Cy(&m_R.A, m_R.H, m_R.F.C);
	return;
}
void CZ80MsxDos::op_ADC_A_L()
{
	m_R.Add8Cy(&m_R.A, m_R.L, m_R.F.C);
	return;
}
void CZ80MsxDos::op_ADC_A_memHL()
{
	uint16_t ad = m_R.GetHL();
	uint8_t v = m_pMemSys->Read(ad);
	m_R.Add8Cy(&m_R.A, v, m_R.F.C);
	return;
}
void CZ80MsxDos::op_ADC_A_A()
{
	m_R.Add8Cy(&m_R.A, m_R.A, m_R.F.C);
	return;
}
void CZ80MsxDos::op_SUB_B()
{
	m_R.Sub8(&m_R.A, m_R.B);
	return;
}
void CZ80MsxDos::op_SUB_C()
{
	m_R.Sub8(&m_R.A, m_R.C);
	return;
}
void CZ80MsxDos::op_SUB_D()
{
	m_R.Sub8(&m_R.A, m_R.D);
	return;
}
void CZ80MsxDos::op_SUB_E()
{
	m_R.Sub8(&m_R.A, m_R.E);
	return;
}
void CZ80MsxDos::op_SUB_H()
{
	m_R.Sub8(&m_R.A, m_R.H);
	return;
}
void CZ80MsxDos::op_SUB_L()
{
	m_R.Sub8(&m_R.A, m_R.L);
	return;
}
void CZ80MsxDos::op_SUB_memHL()
{
	uint16_t ad = m_R.GetHL();
	uint8_t v = m_pMemSys->Read(ad);
	m_R.Sub8(&m_R.A, v);
	return;
}
void CZ80MsxDos::op_SUB_A()
{
	m_R.Sub8(&m_R.A, m_R.A);
	return;
}
void CZ80MsxDos::op_SBC_A_B()
{
	m_R.Sub8Cy(&m_R.A, m_R.B, m_R.F.C);
	return;
}
void CZ80MsxDos::op_SBC_A_C()
{
	m_R.Sub8Cy(&m_R.A, m_R.C, m_R.F.C);
	return;
}
void CZ80MsxDos::op_SBC_A_D()
{
	m_R.Sub8Cy(&m_R.A, m_R.D, m_R.F.C);
	return;
}
void CZ80MsxDos::op_SBC_A_E()
{
	m_R.Sub8Cy(&m_R.A, m_R.E, m_R.F.C);
	return;
}
void CZ80MsxDos::op_SBC_A_H()
{
	m_R.Sub8Cy(&m_R.A, m_R.H, m_R.F.C);
	return;
}
void CZ80MsxDos::op_SBC_A_L()
{
	m_R.Sub8Cy(&m_R.A, m_R.L, m_R.F.C);
	return;
}
void CZ80MsxDos::op_SBC_A_memHL()
{
	uint16_t ad = m_R.GetHL();
	uint8_t v = m_pMemSys->Read(ad);
	m_R.Sub8Cy(&m_R.A, v, m_R.F.C);
	return;
}
void CZ80MsxDos::op_SBC_A()
{
	m_R.Sub8Cy(&m_R.A, m_R.A, m_R.F.C);
	return;
}
void CZ80MsxDos::op_AND_B()
{
	m_R.And8(&m_R.A, m_R.B);
	return;
}
void CZ80MsxDos::op_AND_C()
{
	m_R.And8(&m_R.A, m_R.C);
	return;
}
void CZ80MsxDos::op_AND_D()
{
	m_R.And8(&m_R.A, m_R.D);
	return;
}
void CZ80MsxDos::op_AND_E()
{
	m_R.And8(&m_R.A, m_R.E);
	return;
}
void CZ80MsxDos::op_AND_H()
{
	m_R.And8(&m_R.A, m_R.H);
	return;
}
void CZ80MsxDos::op_AND_L()
{
	m_R.And8(&m_R.A, m_R.L);
	return;
}
void CZ80MsxDos::op_AND_memHL()
{
	uint16_t ad = m_R.GetHL();
	uint8_t v = m_pMemSys->Read(ad);
	m_R.And8(&m_R.A, v);
	return;
}
void CZ80MsxDos::op_AND_A()
{
	m_R.And8(&m_R.A, m_R.A);
	return;
}
void CZ80MsxDos::op_XOR_B()
{
	m_R.Xor8(&m_R.A, m_R.B);
	return;
}
void CZ80MsxDos::op_XOR_C()
{
	m_R.Xor8(&m_R.A, m_R.C);
	return;
}
void CZ80MsxDos::op_XOR_D()
{
	m_R.Xor8(&m_R.A, m_R.D);
	return;
}
void CZ80MsxDos::op_XOR_E()
{
	m_R.Xor8(&m_R.A, m_R.E);
	return;
}
void CZ80MsxDos::op_XOR_H()
{
	m_R.Xor8(&m_R.A, m_R.H);
	return;
}
void CZ80MsxDos::op_XOR_L()
{
	m_R.Xor8(&m_R.A, m_R.L);
	return;
}
void CZ80MsxDos::op_XOR_memHL()
{
	uint16_t ad = m_R.GetHL();
	uint8_t v = m_pMemSys->Read(ad);
	m_R.Xor8(&m_R.A, v);
	return;
}
void CZ80MsxDos::op_XOR_A()
{
	m_R.Xor8(&m_R.A, m_R.A);
	return;
}
void CZ80MsxDos::op_OR_B()
{
	m_R.Or8(&m_R.A, m_R.B);
	return;
}
void CZ80MsxDos::op_OR_C()
{
	m_R.Or8(&m_R.A, m_R.C);
	return;
}
void CZ80MsxDos::op_OR_D()
{
	m_R.Or8(&m_R.A, m_R.D);
	return;
}
void CZ80MsxDos::op_OR_E()
{
	m_R.Or8(&m_R.A, m_R.E);
	return;
}
void CZ80MsxDos::op_OR_H()
{
	m_R.Or8(&m_R.A, m_R.H);
	return;
}
void CZ80MsxDos::op_OR_L()
{
	m_R.Or8(&m_R.A, m_R.L);
	return;
}
void CZ80MsxDos::op_OR_memHL()
{
	uint16_t ad = m_R.GetHL();
	uint8_t v = m_pMemSys->Read(ad);
	m_R.Or8(&m_R.A, v);
	return;
}
void CZ80MsxDos::op_OR_A()
{
	m_R.Or8(&m_R.A, m_R.A);
	return;
}
void CZ80MsxDos::op_CP_B()
{
	uint8_t temp = m_R.A;
	m_R.Sub8(&temp, m_R.B);
	return;
}
void CZ80MsxDos::op_CP_C()
{
	uint8_t temp = m_R.A;
	m_R.Sub8(&temp, m_R.C);
	return;
}
void CZ80MsxDos::op_CP_D()
{
	uint8_t temp = m_R.A;
	m_R.Sub8(&temp, m_R.D);
	return;
}
void CZ80MsxDos::op_CP_E()
{
	uint8_t temp = m_R.A;
	m_R.Sub8(&temp, m_R.E);
	return;
}
void CZ80MsxDos::op_CP_H()
{
	uint8_t temp = m_R.A;
	m_R.Sub8(&temp, m_R.H);
	return;
}
void CZ80MsxDos::op_CP_L()
{
	uint8_t temp = m_R.A;
	m_R.Sub8(&temp, m_R.L);
	return;
}
void CZ80MsxDos::op_CP_memHL()
{
	uint16_t ad = m_R.GetHL();
	uint8_t v = m_pMemSys->Read(ad);
	uint8_t temp = m_R.A;
	m_R.Sub8(&temp, v);
	return;
}
void CZ80MsxDos::op_CP_A()
{
	uint8_t temp = m_R.A;
	m_R.Sub8(&temp, m_R.A);
	return;
}
void CZ80MsxDos::op_RET_nz()
{
	if( m_R.F.Z == 0 ){
		m_R.PC = m_pMemSys->Read(m_R.SP++);
		m_R.PC |= m_pMemSys->Read(m_R.SP++) << 8;
	}
	return;
}
void CZ80MsxDos::op_POP_BC()
{
	m_R.C = m_pMemSys->Read(m_R.SP++);
	m_R.B = m_pMemSys->Read(m_R.SP++);
	return;
}
void CZ80MsxDos::op_JP_nz_ad()
{
	if( m_R.F.Z == 0 ){
		uint16_t temp = m_pMemSys->Read(m_R.PC++);
		temp |= m_pMemSys->Read(m_R.PC) << 8;
		m_R.PC = temp;
	}
	else{
		m_R.PC += 2;	// nnを読み捨て
	}
	return;
}
void CZ80MsxDos::op_JP_ad()
{
	uint16_t temp = m_pMemSys->Read(m_R.PC++);
	temp |= m_pMemSys->Read(m_R.PC) << 8;
	m_R.PC = temp;
	return;
}
void CZ80MsxDos::op_CALL_nz_ad()
{
	if( m_R.F.Z == 0 ){
		uint16_t destAddr = m_pMemSys->Read(m_R.PC++);
		destAddr |= static_cast<uint16_t>(m_pMemSys->Read(m_R.PC++)) << 8;
		m_pMemSys->Write(--m_R.SP, (m_R.PC>>8)&0xff);
		m_pMemSys->Write(--m_R.SP, (m_R.PC>>0)&0xff);
		m_R.PC = destAddr;
	}
	else{
		m_R.PC += 2;
	}
	return;
}
void CZ80MsxDos::op_PUSH_BC()
{
	m_pMemSys->Write(--m_R.SP, m_R.B);
	m_pMemSys->Write(--m_R.SP, m_R.C);
	return;
}
void CZ80MsxDos::op_ADD_A_v()
{
	uint8_t v = m_pMemSys->Read(m_R.PC++);
	m_R.Add8(&m_R.A, v);
	return;
}
void CZ80MsxDos::op_RST_0h()
{
	m_pMemSys->Write(--m_R.SP, (m_R.PC>>8)&0xff);
	m_pMemSys->Write(--m_R.SP, (m_R.PC>>0)&0xff);
	m_R.PC = 0x00;
	return;
}
void CZ80MsxDos::op_RET_z()
{
	if( m_R.F.Z != 0 ){
		m_R.PC = m_pMemSys->Read(m_R.SP++);
		m_R.PC |= m_pMemSys->Read(m_R.SP++) << 8;
	}
	return;
}
void CZ80MsxDos::op_RET()
{
	m_R.PC = m_pMemSys->Read(m_R.SP++);
	m_R.PC |= m_pMemSys->Read(m_R.SP++) << 8;
	return;
}
void CZ80MsxDos::op_JP_z_ad()
{
	if( m_R.F.Z != 0 ){
		uint16_t temp = m_pMemSys->Read(m_R.PC++);
		temp |= m_pMemSys->Read(m_R.PC) << 8;
		m_R.PC = temp;
	}
	else{
		m_R.PC += 2;	// nnを読み捨て
	}
	return;
}
void CZ80MsxDos::op_EXTENDED_1()
{
	uint8_t opcd = m_pMemSys->Read(m_R.PC++);
	auto pFunc = m_Op.Extended1[opcd];
	(this->*pFunc)();
	return;
}
void CZ80MsxDos::op_CALL_z_ad()
{
	if( m_R.F.Z != 0 ){
		uint16_t destAddr = m_pMemSys->Read(m_R.PC++);
		destAddr |= static_cast<uint16_t>(m_pMemSys->Read(m_R.PC++)) << 8;
		m_pMemSys->Write(--m_R.SP, (m_R.PC>>8)&0xff);
		m_pMemSys->Write(--m_R.SP, (m_R.PC>>0)&0xff);
		m_R.PC = destAddr;
	}
	else{
		m_R.PC += 2;
	}
	return;
}
void CZ80MsxDos::op_CALL_ad()
{
	uint16_t destAddr = m_pMemSys->Read(m_R.PC++);
	destAddr |= static_cast<uint16_t>(m_pMemSys->Read(m_R.PC++)) << 8;
	m_pMemSys->Write(--m_R.SP, (m_R.PC>>8)&0xff);
	m_pMemSys->Write(--m_R.SP, (m_R.PC>>0)&0xff);
	m_R.PC = destAddr;
	return;
}
void CZ80MsxDos::op_ADC_A_v()
{
	uint8_t v= m_pMemSys->Read(m_R.PC++);
	m_R.Add8Cy(&m_R.A, v, m_R.F.C);
	return;
}
void CZ80MsxDos::op_RST_8h()
{
	m_pMemSys->Write(--m_R.SP, (m_R.PC>>8)&0xff);
	m_pMemSys->Write(--m_R.SP, (m_R.PC>>0)&0xff);
	m_R.PC = 0x08;
	return;
}
void CZ80MsxDos::op_RET_nc()
{
	if( m_R.F.C == 0 ){
		m_R.PC = m_pMemSys->Read(m_R.SP++);
		m_R.PC |= m_pMemSys->Read(m_R.SP++) << 8;
	}
	return;
}
void CZ80MsxDos::op_POP_DE()
{
	m_R.E = m_pMemSys->Read(m_R.SP++);
	m_R.D = m_pMemSys->Read(m_R.SP++);
	return;
}
void CZ80MsxDos::op_JP_nc_ad()
{
	if( m_R.F.C == 0 ){
		uint16_t temp = m_pMemSys->Read(m_R.PC++);
		temp |= m_pMemSys->Read(m_R.PC) << 8;
		m_R.PC = temp;
	}
	else{
		m_R.PC += 2;	// nnを読み捨て
	}
	return;
}
void __time_critical_func(CZ80MsxDos::op_OUT_memv_A)()
{
	uint8_t v = m_pMemSys->Read(m_R.PC++);
	m_pIoSys->Out(v, m_R.A);
	return;
}
void CZ80MsxDos::op_CALL_nc_ad()
{
	if( m_R.F.C == 0 ){
		uint16_t destAddr = m_pMemSys->Read(m_R.PC++);
		destAddr |= static_cast<uint16_t>(m_pMemSys->Read(m_R.PC++)) << 8;
		m_pMemSys->Write(--m_R.SP, (m_R.PC>>8)&0xff);
		m_pMemSys->Write(--m_R.SP, (m_R.PC>>0)&0xff);
		m_R.PC = destAddr;
	}
	else{
		m_R.PC += 2;
	}
	return;
}
void CZ80MsxDos::op_PUSH_DE()
{
	m_pMemSys->Write(--m_R.SP, m_R.D);
	m_pMemSys->Write(--m_R.SP, m_R.E);
	return;
}
void CZ80MsxDos::op_SUB_v()
{
	uint8_t v = m_pMemSys->Read(m_R.PC++);
	m_R.Sub8(&m_R.A, v);
	return;
}
void CZ80MsxDos::op_RST_10h()
{
	m_pMemSys->Write(--m_R.SP, (m_R.PC>>8)&0xff);
	m_pMemSys->Write(--m_R.SP, (m_R.PC>>0)&0xff);
	m_R.PC = 0x10;
	return;
}
void CZ80MsxDos::op_RET_c()
{
	if( m_R.F.C != 0 ){
		m_R.PC = m_pMemSys->Read(m_R.SP++);
		m_R.PC |= m_pMemSys->Read(m_R.SP++) << 8;
	}
	return;
}
void CZ80MsxDos::op_EXX()
{
	m_R.Exchange();
	return;
}
void CZ80MsxDos::op_JP_c_ad()
{
	if( m_R.F.C != 0 ){
		uint16_t temp = m_pMemSys->Read(m_R.PC++);
		temp |= m_pMemSys->Read(m_R.PC) << 8;
		m_R.PC = temp;
	}
	else{
		m_R.PC += 2;	// nnを読み捨て
	}
	return;
}
void __time_critical_func(CZ80MsxDos::op_IN_A_memv)()
{
	uint8_t v = m_pMemSys->Read(m_R.PC++);
	m_R.A = m_pIoSys->In(v);
	return;
}
void CZ80MsxDos::op_CALL_c_ad()
{
	if( m_R.F.C != 0 ){
		uint16_t destAddr = m_pMemSys->Read(m_R.PC++);
		destAddr |= static_cast<uint16_t>(m_pMemSys->Read(m_R.PC++)) << 8;
		m_pMemSys->Write(--m_R.SP, (m_R.PC>>8)&0xff);
		m_pMemSys->Write(--m_R.SP, (m_R.PC>>0)&0xff);
		m_R.PC = destAddr;
	}
	else{
		m_R.PC += 2;
	}
	return;
}
void CZ80MsxDos::op_EXTENDED_2IX()
{
	uint8_t opcd = m_pMemSys->Read(m_R.PC++);
	auto pFunc = m_Op.Extended2IX[opcd];
	(this->*pFunc)();
	return;
}
void CZ80MsxDos::op_SBC_A_v()
{
	uint8_t v = m_pMemSys->Read(m_R.PC++);
	m_R.Sub8Cy(&m_R.A, v, m_R.F.C);
	return;
}
void CZ80MsxDos::op_RST_18h()
{
	m_pMemSys->Write(--m_R.SP, (m_R.PC>>8)&0xff);
	m_pMemSys->Write(--m_R.SP, (m_R.PC>>0)&0xff);
	m_R.PC = 0x18;
	return;
}
void CZ80MsxDos::op_RET_po()
{
	if( m_R.F.PV == 0 ){	// PO
		m_R.PC = m_pMemSys->Read(m_R.SP++);
		m_R.PC |= m_pMemSys->Read(m_R.SP++) << 8;
	}
	return;
}
void CZ80MsxDos::op_POP_HL()
{
	m_R.L = m_pMemSys->Read(m_R.SP++);
	m_R.H = m_pMemSys->Read(m_R.SP++);
	return;
}
void CZ80MsxDos::op_JP_po_ad()
{
	if( m_R.F.PV == 0 ){
		uint16_t temp = m_pMemSys->Read(m_R.PC++);
		temp |= m_pMemSys->Read(m_R.PC) << 8;
		m_R.PC = temp;
	}
	else{
		m_R.PC += 2;	// nnを読み捨て
	}
	return;
}
void CZ80MsxDos::op_EX_memSP_HL()
{
	uint8_t rl = m_R.L, rh = m_R.H;
	m_R.L = m_pMemSys->Read(m_R.SP+0);
	m_R.H = m_pMemSys->Read(m_R.SP+1);
	m_pMemSys->Write(m_R.SP+0, rl);
	m_pMemSys->Write(m_R.SP+1, rh);
	return;
}
void CZ80MsxDos::op_CALL_po_ad()
{
	if( m_R.F.PV == 0 ){
		uint16_t destAddr = m_pMemSys->Read(m_R.PC++);
		destAddr |= static_cast<uint16_t>(m_pMemSys->Read(m_R.PC++)) << 8;
		m_pMemSys->Write(--m_R.SP, (m_R.PC>>8)&0xff);
		m_pMemSys->Write(--m_R.SP, (m_R.PC>>0)&0xff);
		m_R.PC = destAddr;
	}
	else{
		m_R.PC += 2;
	}
	return;
}
void CZ80MsxDos::op_PUSH_HL()
{
	m_pMemSys->Write(--m_R.SP, m_R.H);
	m_pMemSys->Write(--m_R.SP, m_R.L);
	return;
}
void CZ80MsxDos::op_AND_v()
{
	uint8_t v = m_pMemSys->Read(m_R.PC++);
	m_R.And8(&m_R.A, v);
	return;
}
void CZ80MsxDos::op_RST_20h()
{
	m_pMemSys->Write(--m_R.SP, (m_R.PC>>8)&0xff);
	m_pMemSys->Write(--m_R.SP, (m_R.PC>>0)&0xff);
	m_R.PC = 0x20;
	return;
}
void CZ80MsxDos::op_RET_pe()
{
	if( m_R.F.PV != 0 ){	// PE
		m_R.PC = m_pMemSys->Read(m_R.SP++);
		m_R.PC |= m_pMemSys->Read(m_R.SP++) << 8;
	}
	return;
}
void CZ80MsxDos::op_JP_memHL()
{
	m_R.PC = m_R.GetHL();	// HLの値そのものがアドレス値である。
	return;
}

void CZ80MsxDos::op_JP_pe_ad()
{
	if( m_R.F.PV != 0 ){ // PE
		uint16_t temp = m_pMemSys->Read(m_R.PC++);
		temp |= m_pMemSys->Read(m_R.PC) << 8;
		m_R.PC = temp;
	}
	else{
		m_R.PC += 2;	// nnを読み捨て
	}
	return;
}
void CZ80MsxDos::op_EX_DE_HL()
{
	uint16_t temp = m_R.GetDE();
	m_R.SetDE(m_R.GetHL());
	m_R.SetHL(temp);
	return;
}
void CZ80MsxDos::op_CALL_pe_ad()
{
	if( m_R.F.PV != 0 ){ // PE
		uint16_t destAddr = m_pMemSys->Read(m_R.PC++);
		destAddr |= static_cast<uint16_t>(m_pMemSys->Read(m_R.PC++)) << 8;
		m_pMemSys->Write(--m_R.SP, (m_R.PC>>8)&0xff);
		m_pMemSys->Write(--m_R.SP, (m_R.PC>>0)&0xff);
		m_R.PC = destAddr;
	}
	else{
		m_R.PC += 2;
	}
	return;
}
void CZ80MsxDos::op_EXTENDED_3()
{
	uint8_t opcd = m_pMemSys->Read(m_R.PC++);
	auto pFunc = m_Op.Extended3[opcd];
	(this->*pFunc)();
	return;
}
void CZ80MsxDos::op_XOR_v()
{
	uint8_t v = m_pMemSys->Read(m_R.PC++);
	m_R.Xor8(&m_R.A, v);
	return;
}
void CZ80MsxDos::op_RST_28h()
{
	m_pMemSys->Write(--m_R.SP, (m_R.PC>>8)&0xff);
	m_pMemSys->Write(--m_R.SP, (m_R.PC>>0)&0xff);
	m_R.PC = 0x28;
	return;
}
void CZ80MsxDos::op_RET_p()
{
	if( m_R.F.S == 0 ){	// P
		m_R.PC = m_pMemSys->Read(m_R.SP++);
		m_R.PC |= m_pMemSys->Read(m_R.SP++) << 8;
	}
	return;
}
void CZ80MsxDos::op_POP_AF()
{
	m_R.F.Set(m_pMemSys->Read(m_R.SP++));
	m_R.A = m_pMemSys->Read(m_R.SP++);
	return;
}
void CZ80MsxDos::op_JP_p_ad()
{
	if( m_R.F.S == 0 ){ // P
		uint16_t temp = m_pMemSys->Read(m_R.PC++);
		temp |= m_pMemSys->Read(m_R.PC) << 8;
		m_R.PC = temp;
	}
	else{
		m_R.PC += 2;	// nnを読み捨て
	}
	return;
}
void CZ80MsxDos::op_DI()
{
	m_bIFF1 = m_bIFF2 = false;
	return;
}
void CZ80MsxDos::op_CALL_p_ad()
{
	if( m_R.F.S == 0 ){ // P
		uint16_t destAddr = m_pMemSys->Read(m_R.PC++);
		destAddr |= static_cast<uint16_t>(m_pMemSys->Read(m_R.PC++)) << 8;
		m_pMemSys->Write(--m_R.SP, (m_R.PC>>8)&0xff);
		m_pMemSys->Write(--m_R.SP, (m_R.PC>>0)&0xff);
		m_R.PC = destAddr;
	}
	else{
		m_R.PC += 2;
	}
	return;
}
void CZ80MsxDos::op_PUSH_AF()
{
	m_pMemSys->Write(--m_R.SP, m_R.A);
	m_pMemSys->Write(--m_R.SP, m_R.F.Get());
	return;
}
void CZ80MsxDos::op_OR_v()
{
	uint8_t v = m_pMemSys->Read(m_R.PC++);
	m_R.Or8(&m_R.A, v);
	return;
}
void CZ80MsxDos::op_RST_30h()
{
	m_pMemSys->Write(--m_R.SP, (m_R.PC>>8)&0xff);
	m_pMemSys->Write(--m_R.SP, (m_R.PC>>0)&0xff);
	m_R.PC = 0x30;
	return;
}
void CZ80MsxDos::op_RET_m()
{
	if( m_R.F.S != 0 ){	// M
		m_R.PC = m_pMemSys->Read(m_R.SP++);
		m_R.PC |= m_pMemSys->Read(m_R.SP++) << 8;
	}
	return;
}
void CZ80MsxDos::op_LD_SP_HL()
{
	m_R.SP = m_R.GetHL();
	return;
}
void CZ80MsxDos::op_JP_m_ad()
{
	if( m_R.F.S != 0 ){ // M
		uint16_t temp = m_pMemSys->Read(m_R.PC++);
		temp |= m_pMemSys->Read(m_R.PC) << 8;
		m_R.PC = temp;
	}
	else{
		m_R.PC += 2;	// nnを読み捨て
	}
	return;
}
void CZ80MsxDos::op_EI()
{
	m_bIFF1 = m_bIFF2 = true;
	return;
}
void CZ80MsxDos::op_CALL_m_ad()
{
	if( m_R.F.S != 0 ){ // M
		uint16_t destAddr = m_pMemSys->Read(m_R.PC++);
		destAddr |= static_cast<uint16_t>(m_pMemSys->Read(m_R.PC++)) << 8;
		m_pMemSys->Write(--m_R.SP, (m_R.PC>>8)&0xff);
		m_pMemSys->Write(--m_R.SP, (m_R.PC>>0)&0xff);
		m_R.PC = destAddr;
	}
	else{
		m_R.PC += 2;
	}
	return;
}
void CZ80MsxDos::op_EXTENDED_4IY()
{
	uint8_t opcd = m_pMemSys->Read(m_R.PC++);
	auto pFunc = m_Op.Extended4IY[opcd];
	(this->*pFunc)();
	return;
}
void CZ80MsxDos::op_CP_v()
{
	uint8_t tempA = m_R.A;
	uint8_t v = m_pMemSys->Read(m_R.PC++);
	m_R.Sub8(&tempA, v);
	return;
}
void CZ80MsxDos::op_RST_38h()
{
	m_pMemSys->Write(--m_R.SP, (m_R.PC>>8)&0xff);
	m_pMemSys->Write(--m_R.SP, (m_R.PC>>0)&0xff);
	m_R.PC = 0x38;
	return;
}

// extended1
void CZ80MsxDos::op_RLC_B()
{
	m_R.Rlc8(&m_R.B);
	return;
}
void CZ80MsxDos::op_RLC_C()
{
	m_R.Rlc8(&m_R.C);
	return;
}
void CZ80MsxDos::op_RLC_D()
{
	m_R.Rlc8(&m_R.D);
	return;
}
void CZ80MsxDos::op_RLC_E()
{
	m_R.Rlc8(&m_R.E);
	return;
}
void CZ80MsxDos::op_RLC_H()
{
	m_R.Rlc8(&m_R.H);
	return;
}
void CZ80MsxDos::op_RLC_L()
{
	m_R.Rlc8(&m_R.L);
	return;
}
void CZ80MsxDos::op_RLC_memHL()
{
	uint16_t ad = m_R.GetHL();
	uint8_t v = m_pMemSys->Read(ad);
	m_R.Rlc8(&v);
	m_pMemSys->Write(m_R.GetHL(), v);
	return;
}
void CZ80MsxDos::op_RLC_A()
{
	m_R.Rlc8(&m_R.A);
	return;
}
void CZ80MsxDos::op_RRC_B()
{
	m_R.Rrc8(&m_R.B);
	return;
}
void CZ80MsxDos::op_RRC_C()
{
	m_R.Rrc8(&m_R.C);
	return;
}
void CZ80MsxDos::op_RRC_D()
{
	m_R.Rrc8(&m_R.D);
	return;
}
void CZ80MsxDos::op_RRC_E()
{
	m_R.Rrc8(&m_R.E);
	return;
}
void CZ80MsxDos::op_RRC_H()
{
	m_R.Rrc8(&m_R.H);
	return;
}
void CZ80MsxDos::op_RRC_L()
{
	m_R.Rrc8(&m_R.L);
	return;
}
void CZ80MsxDos::op_RRC_memHL()
{
	uint16_t ad = m_R.GetHL();
	uint8_t v = m_pMemSys->Read(ad);
	m_R.Rrc8(&v);
	m_pMemSys->Write(m_R.GetHL(), v);
	return;
}
void CZ80MsxDos::op_RRC_A()
{
	m_R.Rrc8(&m_R.A);
	return;
}
void CZ80MsxDos::op_RL_B()
{
	m_R.Rl8(&m_R.B);
	return;
}
void CZ80MsxDos::op_RL_C()
{
	m_R.Rl8(&m_R.C);
	return;
}
void CZ80MsxDos::op_RL_D()
{
	m_R.Rl8(&m_R.D);
	return;
}
void CZ80MsxDos::op_RL_E()
{
	m_R.Rl8(&m_R.E);
	return;
}
void CZ80MsxDos::op_RL_H()
{
	m_R.Rl8(&m_R.H);
	return;
}
void CZ80MsxDos::op_RL_L()
{
	m_R.Rl8(&m_R.L);
	return;
}
void CZ80MsxDos::op_RL_memHL()
{
	uint16_t ad = m_R.GetHL();
	uint8_t v = m_pMemSys->Read(ad);
	m_R.Rl8(&v);
	m_pMemSys->Write(m_R.GetHL(), v);
	return;
}
void CZ80MsxDos::op_RL_A()
{
	m_R.Rl8(&m_R.A);
	return;
}
void CZ80MsxDos::op_RR_B()
{
	m_R.Rr8(&m_R.B);
	return;
}
void CZ80MsxDos::op_RR_C()
{
	m_R.Rr8(&m_R.C);
	return;
}
void CZ80MsxDos::op_RR_D()
{
	m_R.Rr8(&m_R.D);
	return;
}
void CZ80MsxDos::op_RR_E()
{
	m_R.Rr8(&m_R.E);
	return;
}
void CZ80MsxDos::op_RR_H()
{
	m_R.Rr8(&m_R.H);
	return;
}
void CZ80MsxDos::op_RR_L()
{
	m_R.Rr8(&m_R.L);
	return;
}
void CZ80MsxDos::op_RR_memHL()
{
	uint16_t ad = m_R.GetHL();
	uint8_t v = m_pMemSys->Read(ad);
	m_R.Rr8(&v);
	m_pMemSys->Write(m_R.GetHL(), v);
	return;
}
void CZ80MsxDos::op_RR_A()
{
	m_R.Rr8(&m_R.A);
	return;
}
void CZ80MsxDos::op_SLA_B()
{
	m_R.Sla8(&m_R.B);
	return;
}
void CZ80MsxDos::op_SLA_C()
{
	m_R.Sla8(&m_R.C);
	return;
}
void CZ80MsxDos::op_SLA_D()
{
	m_R.Sla8(&m_R.D);
	return;
}
void CZ80MsxDos::op_SLA_E()
{
	m_R.Sla8(&m_R.E);
	return;
}
void CZ80MsxDos::op_SLA_H()
{
	m_R.Sla8(&m_R.H);
	return;
}
void CZ80MsxDos::op_SLA_L()
{
	m_R.Sla8(&m_R.L);
	return;
}
void CZ80MsxDos::op_SLA_memHL()
{
	uint16_t ad = m_R.GetHL();
	uint8_t v = m_pMemSys->Read(ad);
	m_R.Sla8(&v);
	m_pMemSys->Write(m_R.GetHL(), v);
	return;
}
void CZ80MsxDos::op_SLA_A()
{
	m_R.Sla8(&m_R.A);
	return;
}
void CZ80MsxDos::op_SRA_B()
{
	m_R.Sra8(&m_R.B);
	return;
}
void CZ80MsxDos::op_SRA_C()
{
	m_R.Sra8(&m_R.C);
	return;
}
void CZ80MsxDos::op_SRA_D()
{
	m_R.Sra8(&m_R.D);
	return;
}
void CZ80MsxDos::op_SRA_E()
{
	m_R.Sra8(&m_R.E);
	return;
}
void CZ80MsxDos::op_SRA_H()
{
	m_R.Sra8(&m_R.H);
	return;
}
void CZ80MsxDos::op_SRA_L()
{
	m_R.Sra8(&m_R.L);
	return;
}
void CZ80MsxDos::op_SRA_memHL()
{
	uint16_t ad = m_R.GetHL();
	uint8_t v = m_pMemSys->Read(ad);
	m_R.Sra8(&v);
	m_pMemSys->Write(m_R.GetHL(), v);
	return;
}
void CZ80MsxDos::op_SRA_A()
{
	m_R.Sra8(&m_R.L);
	return;
}
void CZ80MsxDos::op_SLL_B()	// undoc.Z80
{
	m_R.Sll8(&m_R.B);
	return;
}
void CZ80MsxDos::op_SLL_C()	// undoc.Z80
{
	m_R.Sll8(&m_R.C);
	return;
}
void CZ80MsxDos::op_SLL_D()	// undoc.Z80
{
	m_R.Sll8(&m_R.D);
	return;
}
void CZ80MsxDos::op_SLL_E()	// undoc.Z80
{
	m_R.Sll8(&m_R.E);
	return;
}
void CZ80MsxDos::op_SLL_H()	// undoc.Z80
{
	m_R.Sll8(&m_R.H);
	return;
}
void CZ80MsxDos::op_SLL_L()	// undoc.Z80
{
	m_R.Sll8(&m_R.L);
	return;
}
void CZ80MsxDos::op_SLL_memHL()	// undoc.Z80
{
	uint16_t ad = m_R.GetHL();
	uint8_t v = m_pMemSys->Read(ad);
	m_R.Sll8(&v);
	m_pMemSys->Write(m_R.GetHL(), v);
	return;
}
void CZ80MsxDos::op_SLL_A()	// undoc.Z80
{
	m_R.Sll8(&m_R.A);
	return;
}
void CZ80MsxDos::op_SRL_B()
{
	m_R.Srl8(&m_R.B);
	return;
}
void CZ80MsxDos::op_SRL_C()
{
	m_R.Srl8(&m_R.C);
	return;
}
void CZ80MsxDos::op_SRL_D()
{
	m_R.Srl8(&m_R.D);
	return;
}
void CZ80MsxDos::op_SRL_E()
{
	m_R.Srl8(&m_R.E);
	return;
}
void CZ80MsxDos::op_SRL_H()
{
	m_R.Srl8(&m_R.H);
	return;
}
void CZ80MsxDos::op_SRL_L()
{
	m_R.Srl8(&m_R.L);
	return;
}
void CZ80MsxDos::op_SRL_memHL()
{
	uint16_t ad = m_R.GetHL();
	uint8_t v = m_pMemSys->Read(ad);
	m_R.Srl8(&v);
	m_pMemSys->Write(m_R.GetHL(), v);
	return;
}
void CZ80MsxDos::op_SRL_A()
{
	m_R.Srl8(&m_R.A);
	return;
}
void CZ80MsxDos::op_BIT_0_B()
{
	m_R.F.Z = (m_R.B & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_0_C()
{
	m_R.F.Z = (m_R.C & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_0_D()
{
	m_R.F.Z = (m_R.D & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_0_E()
{
	m_R.F.Z = (m_R.E & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_0_H()
{
	m_R.F.Z = (m_R.H & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_0_L()
{
	m_R.F.Z = (m_R.L & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_0_memHL()
{
	uint16_t ad = m_R.GetHL();
	uint8_t v = m_pMemSys->Read(ad);
	m_R.F.Z = (v & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_0_A()
{
	m_R.F.Z = (m_R.A & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_1_B()
{
	m_R.F.Z = ((m_R.B>>1) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_1_C()
{
	m_R.F.Z = ((m_R.C>>1) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_1_D()
{
	m_R.F.Z = ((m_R.D>>1) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_1_E()
{
	m_R.F.Z = ((m_R.E>>1) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_1_H()
{
	m_R.F.Z = ((m_R.H>>1) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_1_L()
{
	m_R.F.Z = ((m_R.L>>1) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_1_memHL()
{
	uint16_t ad = m_R.GetHL();
	uint8_t v = m_pMemSys->Read(ad);
	m_R.F.Z = ((v>>1) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_1_A()
{
	m_R.F.Z = ((m_R.A>>1) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_2_B()
{
	m_R.F.Z = ((m_R.B>>2) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_2_C()
{
	m_R.F.Z = ((m_R.C>>2) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_2_D()
{
	m_R.F.Z = ((m_R.D>>2) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_2_E()
{
	m_R.F.Z = ((m_R.E>>2) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_2_H()
{
	m_R.F.Z = ((m_R.H>>2) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_2_L()
{
	m_R.F.Z = ((m_R.L>>2) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_2_memHL()
{
	uint16_t ad = m_R.GetHL();
	uint8_t v = m_pMemSys->Read(ad);
	m_R.F.Z = ((v>>2) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_2_A()
{
	m_R.F.Z = ((m_R.A>>2) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_3_B()
{
	m_R.F.Z = ((m_R.B>>3) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_3_C()
{
	m_R.F.Z = ((m_R.C>>3) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_3_D()
{
	m_R.F.Z = ((m_R.D>>3) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_3_E()
{
	m_R.F.Z = ((m_R.E>>3) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_3_H()
{
	m_R.F.Z = ((m_R.H>>3) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_3_L()
{
	m_R.F.Z = ((m_R.L>>3) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_3_memHL()
{
	uint16_t ad = m_R.GetHL();
	uint8_t v = m_pMemSys->Read(ad);
	m_R.F.Z = ((v>>3) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_3_A()
{
	m_R.F.Z = ((m_R.A>>3) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_4_B()
{
	m_R.F.Z = ((m_R.B>>4) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_4_C()
{
	m_R.F.Z = ((m_R.C>>4) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_4_D()
{
	m_R.F.Z = ((m_R.D>>4) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_4_E()
{
	m_R.F.Z = ((m_R.E>>4) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_4_H()
{
	m_R.F.Z = ((m_R.H>>4) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_4_L()
{
	m_R.F.Z = ((m_R.L>>4) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_4_memHL()
{
	uint16_t ad = m_R.GetHL();
	uint8_t v = m_pMemSys->Read(ad);
	m_R.F.Z = ((v>>4) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_4_A()
{
	m_R.F.Z = ((m_R.A>>4) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_5_B()
{
	m_R.F.Z = ((m_R.B>>5) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_5_C()
{
	m_R.F.Z = ((m_R.C>>5) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_5_D()
{
	m_R.F.Z = ((m_R.D>>5) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_5_E()
{
	m_R.F.Z = ((m_R.E>>5) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_5_H()
{
	m_R.F.Z = ((m_R.H>>5) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_5_L()
{
	m_R.F.Z = ((m_R.L>>5) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_5_memHL()
{
	uint16_t ad = m_R.GetHL();
	uint8_t v = m_pMemSys->Read(ad);
	m_R.F.Z = ((v>>5) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_5_A()
{
	m_R.F.Z = ((m_R.A>>5) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_6_B()
{
	m_R.F.Z = ((m_R.B>>6) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_6_C()
{
	m_R.F.Z = ((m_R.C>>6) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_6_D()
{
	m_R.F.Z = ((m_R.D>>6) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_6_E()
{
	m_R.F.Z = ((m_R.E>>6) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_6_H()
{
	m_R.F.Z = ((m_R.H>>6) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_6_L()
{
	m_R.F.Z = ((m_R.L>>6) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_6_memHL()
{
	uint16_t ad = m_R.GetHL();
	uint8_t v = m_pMemSys->Read(ad);
	m_R.F.Z = ((v>>6) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_6_A()
{
	m_R.F.Z = ((m_R.A>>6) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_7_B()
{
	m_R.F.Z = ((m_R.B>>7) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_7_C()
{
	m_R.F.Z = ((m_R.C>>7) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_7_D()
{
	m_R.F.Z = ((m_R.D>>7) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_7_E()
{
	m_R.F.Z = ((m_R.E>>7) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_7_H()
{
	m_R.F.Z = ((m_R.H>>7) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_7_L()
{
	m_R.F.Z = ((m_R.L>>7) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_7_memHL()
{
	uint16_t ad = m_R.GetHL();
	uint8_t v = m_pMemSys->Read(ad);
	m_R.F.Z = ((v>>7) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_7_A()
{
	m_R.F.Z = ((m_R.A>>7) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_RES_0_B()
{
	m_R.B &= ((0x01 << 0) ^ 0xFF);
	return;
}
void CZ80MsxDos::op_RES_0_C()
{
	m_R.C &= ((0x01 << 0) ^ 0xFF);
	return;
}
void CZ80MsxDos::op_RES_0_D()
{
	m_R.D &= ((0x01 << 0) ^ 0xFF);
	return;
}
void CZ80MsxDos::op_RES_0_E()
{
	m_R.E &= ((0x01 << 0) ^ 0xFF);
	return;
}
void CZ80MsxDos::op_RES_0_H()
{
	m_R.H &= ((0x01 << 0) ^ 0xFF);
	return;
}
void CZ80MsxDos::op_RES_0_L()
{
	m_R.L &= ((0x01 << 0) ^ 0xFF);
	return;
}
void CZ80MsxDos::op_RES_0_memHL()
{
	uint16_t ad = m_R.GetHL();
	uint8_t v = m_pMemSys->Read(ad);
	v &= ((0x01 << 0) ^ 0xFF);
	m_pMemSys->Write(m_R.GetHL(), v);
	return;
}
void CZ80MsxDos::op_RES_0_A()
{
	m_R.A &= ((0x01 << 0) ^ 0xFF);
	return;
}

void CZ80MsxDos::op_RES_1_B()
{
	m_R.B &= ((0x01 << 1) ^ 0xFF);
	return;
}
void CZ80MsxDos::op_RES_1_C()
{
	m_R.C &= ((0x01 << 1) ^ 0xFF);
	return;
}
void CZ80MsxDos::op_RES_1_D()
{
	m_R.D &= ((0x01 << 1) ^ 0xFF);
	return;
}
void CZ80MsxDos::op_RES_1_E()
{
	m_R.E &= ((0x01 << 1) ^ 0xFF);
	return;
}
void CZ80MsxDos::op_RES_1_H()
{
	m_R.H &= ((0x01 << 1) ^ 0xFF);
	return;
}
void CZ80MsxDos::op_RES_1_L()
{
	m_R.L &= ((0x01 << 1) ^ 0xFF);
	return;
}
void CZ80MsxDos::op_RES_1_memHL()
{
	uint16_t ad = m_R.GetHL();
	uint8_t v = m_pMemSys->Read(ad);
	v &= ((0x01 << 1) ^ 0xFF);
	m_pMemSys->Write(m_R.GetHL(), v);
	return;
}
void CZ80MsxDos::op_RES_1_A()
{
	m_R.A &= ((0x01 << 1) ^ 0xFF);
	return;
}
void CZ80MsxDos::op_RES_2_B()
{
	m_R.B &= ((0x01 << 2) ^ 0xFF);
	return;
}
void CZ80MsxDos::op_RES_2_C()
{
	m_R.C &= ((0x01 << 2) ^ 0xFF);
	return;
}
void CZ80MsxDos::op_RES_2_D()
{
	m_R.D &= ((0x01 << 2) ^ 0xFF);
	return;
}
void CZ80MsxDos::op_RES_2_E()
{
	m_R.E &= ((0x01 << 2) ^ 0xFF);
	return;
}
void CZ80MsxDos::op_RES_2_H()
{
	m_R.H &= ((0x01 << 2) ^ 0xFF);
	return;
}
void CZ80MsxDos::op_RES_2_L()
{
	m_R.L &= ((0x01 << 2) ^ 0xFF);
	return;
}
void CZ80MsxDos::op_RES_2_memHL()
{
	uint16_t ad = m_R.GetHL();
	uint8_t v = m_pMemSys->Read(ad);
	v &= ((0x01 << 2) ^ 0xFF);
	m_pMemSys->Write(m_R.GetHL(), v);
	return;
}
void CZ80MsxDos::op_RES_2_A()
{
	m_R.A &= ((0x01 << 2) ^ 0xFF);
	return;
}
void CZ80MsxDos::op_RES_3_B()
{
	m_R.B &= ((0x01 << 3) ^ 0xFF);
	return;
}
void CZ80MsxDos::op_RES_3_C()
{
	m_R.C &= ((0x01 << 3) ^ 0xFF);
	return;
}
void CZ80MsxDos::op_RES_3_D()
{
	m_R.D &= ((0x01 << 3) ^ 0xFF);
	return;
}
void CZ80MsxDos::op_RES_3_E()
{
	m_R.E &= ((0x01 << 3) ^ 0xFF);
	return;
}
void CZ80MsxDos::op_RES_3_H()
{
	m_R.H &= ((0x01 << 3) ^ 0xFF);
	return;
}
void CZ80MsxDos::op_RES_3_L()
{
	m_R.L &= ((0x01 << 3) ^ 0xFF);
	return;
}
void CZ80MsxDos::op_RES_3_memHL()
{
	uint16_t ad = m_R.GetHL();
	uint8_t v = m_pMemSys->Read(ad);
	v &= ((0x01 << 3) ^ 0xFF);
	m_pMemSys->Write(m_R.GetHL(), v);
	return;
}
void CZ80MsxDos::op_RES_3_A()
{
	m_R.A &= ((0x01 << 3) ^ 0xFF);
	return;
}
void CZ80MsxDos::op_RES_4_B()
{
	m_R.B &= ((0x01 << 4) ^ 0xFF);
	return;
}
void CZ80MsxDos::op_RES_4_C()
{
	m_R.C &= ((0x01 << 4) ^ 0xFF);
	return;
}
void CZ80MsxDos::op_RES_4_D()
{
	m_R.D &= ((0x01 << 4) ^ 0xFF);
	return;
}
void CZ80MsxDos::op_RES_4_E()
{
	m_R.E &= ((0x01 << 4) ^ 0xFF);
	return;
}
void CZ80MsxDos::op_RES_4_H()
{
	m_R.H &= ((0x01 << 4) ^ 0xFF);
	return;
}
void CZ80MsxDos::op_RES_4_L()
{
	m_R.L &= ((0x01 << 4) ^ 0xFF);
	return;
}
void CZ80MsxDos::op_RES_4_memHL()
{
	uint16_t ad = m_R.GetHL();
	uint8_t v = m_pMemSys->Read(ad);
	v &= ((0x01 << 4) ^ 0xFF);
	m_pMemSys->Write(m_R.GetHL(), v);
	return;
}
void CZ80MsxDos::op_RES_4_A()
{
	m_R.A &= ((0x01 << 4) ^ 0xFF);
	return;
}
void CZ80MsxDos::op_RES_5_B()
{
	m_R.B &= ((0x01 << 5) ^ 0xFF);
	return;
}
void CZ80MsxDos::op_RES_5_C()
{
	m_R.C &= ((0x01 << 5) ^ 0xFF);
	return;
}
void CZ80MsxDos::op_RES_5_D()
{
	m_R.D &= ((0x01 << 5) ^ 0xFF);
	return;
}
void CZ80MsxDos::op_RES_5_E()
{
	m_R.E &= ((0x01 << 5) ^ 0xFF);
	return;
}
void CZ80MsxDos::op_RES_5_H()
{
	m_R.H &= ((0x01 << 5) ^ 0xFF);
	return;
}
void CZ80MsxDos::op_RES_5_L()
{
	m_R.L &= ((0x01 << 5) ^ 0xFF);
	return;
}
void CZ80MsxDos::op_RES_5_memHL()
{
	uint16_t ad = m_R.GetHL();
	uint8_t v = m_pMemSys->Read(ad);
	v &= ((0x01 << 5) ^ 0xFF);
	m_pMemSys->Write(m_R.GetHL(), v);
	return;
}
void CZ80MsxDos::op_RES_5_A()
{
	m_R.A &= ((0x01 << 5) ^ 0xFF);
	return;
}
void CZ80MsxDos::op_RES_6_B()
{
	m_R.B &= ((0x01 << 6) ^ 0xFF);
	return;
}
void CZ80MsxDos::op_RES_6_C()
{
	m_R.C &= ((0x01 << 6) ^ 0xFF);
	return;
}
void CZ80MsxDos::op_RES_6_D()
{
	m_R.D &= ((0x01 << 6) ^ 0xFF);
	return;
}
void CZ80MsxDos::op_RES_6_E()
{
	m_R.E &= ((0x01 << 6) ^ 0xFF);
	return;
}
void CZ80MsxDos::op_RES_6_H()
{
	m_R.H &= ((0x01 << 6) ^ 0xFF);
	return;
}
void CZ80MsxDos::op_RES_6_L()
{
	m_R.L &= ((0x01 << 6) ^ 0xFF);
	return;
}
void CZ80MsxDos::op_RES_6_memHL()
{
	uint16_t ad = m_R.GetHL();
	uint8_t v = m_pMemSys->Read(ad);
	v &= ((0x01 << 6) ^ 0xFF);
	m_pMemSys->Write(m_R.GetHL(), v);
	return;
}
void CZ80MsxDos::op_RES_6_A()
{
	m_R.A &= ((0x01 << 6) ^ 0xFF);
	return;
}
void CZ80MsxDos::op_RES_7_B()
{
	m_R.B &= ((0x01 << 7) ^ 0xFF);
	return;
}
void CZ80MsxDos::op_RES_7_C()
{
	m_R.C &= ((0x01 << 7) ^ 0xFF);
	return;
}
void CZ80MsxDos::op_RES_7_D()
{
	m_R.D &= ((0x01 << 7) ^ 0xFF);
	return;
}
void CZ80MsxDos::op_RES_7_E()
{
	m_R.E &= ((0x01 << 7) ^ 0xFF);
	return;
}
void CZ80MsxDos::op_RES_7_H()
{
	m_R.H &= ((0x01 << 7) ^ 0xFF);
	return;
}
void CZ80MsxDos::op_RES_7_L()
{
	m_R.L &= ((0x01 << 7) ^ 0xFF);
	return;
}
void CZ80MsxDos::op_RES_7_memHL()
{
	uint16_t ad = m_R.GetHL();
	uint8_t v = m_pMemSys->Read(ad);
	v &= ((0x01 << 7) ^ 0xFF);
	m_pMemSys->Write(m_R.GetHL(), v);
	return;
}
void CZ80MsxDos::op_RES_7_A()
{
	m_R.A &= ((0x01 << 7) ^ 0xFF);
	return;
}
void CZ80MsxDos::op_SET_0_B()
{
	m_R.B |= 0x01 << 0;
	return;
}
void CZ80MsxDos::op_SET_0_C()
{
	m_R.C |= 0x01 << 0;
	return;
}
void CZ80MsxDos::op_SET_0_D()
{
	m_R.D |= 0x01 << 0;
	return;
}
void CZ80MsxDos::op_SET_0_E()
{
	m_R.E |= 0x01 << 0;
	return;
}
void CZ80MsxDos::op_SET_0_H()
{
	m_R.H |= 0x01 << 0;
	return;
}
void CZ80MsxDos::op_SET_0_L()
{
	m_R.L |= 0x01 << 0;
	return;
}
void CZ80MsxDos::op_SET_0_memHL()
{
	uint16_t ad = m_R.GetHL();
	uint8_t v = m_pMemSys->Read(ad);
	v |= 0x01 << 0;
	m_pMemSys->Write(m_R.GetHL(), v);
	return;
}
void CZ80MsxDos::op_SET_0_A()
{
	m_R.A |= 0x01 << 0;
	return;
}
void CZ80MsxDos::op_SET_1_B()
{
	m_R.B |= 0x01 << 1;
	return;
}
void CZ80MsxDos::op_SET_1_C()
{
	m_R.C |= 0x01 << 1;
	return;
}
void CZ80MsxDos::op_SET_1_D()
{
	m_R.D |= 0x01 << 1;
	return;
}
void CZ80MsxDos::op_SET_1_E()
{
	m_R.E |= 0x01 << 1;
	return;
}
void CZ80MsxDos::op_SET_1_H()
{
	m_R.H |= 0x01 << 1;
	return;
}
void CZ80MsxDos::op_SET_1_L()
{
	m_R.L |= 0x01 << 1;
	return;
}
void CZ80MsxDos::op_SET_1_memHL()
{
	uint16_t ad = m_R.GetHL();
	uint8_t v = m_pMemSys->Read(ad);
	v |= 0x01 << 1;
	m_pMemSys->Write(m_R.GetHL(), v);
	return;
}
void CZ80MsxDos::op_SET_1_A()
{
	m_R.A |= 0x01 << 1;
	return;
}
void CZ80MsxDos::op_SET_2_B()
{
	m_R.B |= 0x01 << 2;
	return;
}
void CZ80MsxDos::op_SET_2_C()
{
	m_R.C |= 0x01 << 2;
	return;
}
void CZ80MsxDos::op_SET_2_D()
{
	m_R.D |= 0x01 << 2;
	return;
}
void CZ80MsxDos::op_SET_2_E()
{
	m_R.E |= 0x01 << 2;
	return;
}
void CZ80MsxDos::op_SET_2_H()
{
	m_R.H |= 0x01 << 2;
	return;
}
void CZ80MsxDos::op_SET_2_L()
{
	m_R.L |= 0x01 << 2;
	return;
}
void CZ80MsxDos::op_SET_2_memHL()
{
	uint16_t ad = m_R.GetHL();
	uint8_t v = m_pMemSys->Read(ad);
	v |= 0x01 << 2;
	m_pMemSys->Write(m_R.GetHL(), v);
	return;
}
void CZ80MsxDos::op_SET_2_A()
{
	m_R.A |= 0x01 << 2;
	return;
}
void CZ80MsxDos::op_SET_3_B()
{
	m_R.B |= 0x01 << 3;
	return;
}
void CZ80MsxDos::op_SET_3_C()
{
	m_R.C |= 0x01 << 3;
	return;
}
void CZ80MsxDos::op_SET_3_D()
{
	m_R.D |= 0x01 << 3;
	return;
}
void CZ80MsxDos::op_SET_3_E()
{
	m_R.E |= 0x01 << 3;
	return;
}
void CZ80MsxDos::op_SET_3_H()
{
	m_R.H |= 0x01 << 3;
	return;
}
void CZ80MsxDos::op_SET_3_L()
{
	m_R.L |= 0x01 << 3;
	return;
}
void CZ80MsxDos::op_SET_3_memHL()
{
	uint16_t ad = m_R.GetHL();
	uint8_t v = m_pMemSys->Read(ad);
	v |= 0x01 << 3;
	m_pMemSys->Write(m_R.GetHL(), v);
	return;
}
void CZ80MsxDos::op_SET_3_A()
{
	m_R.A |= 0x01 << 3;
	return;
}
void CZ80MsxDos::op_SET_4_B()
{
	m_R.B |= 0x01 << 4;
	return;
}
void CZ80MsxDos::op_SET_4_C()
{
	m_R.C |= 0x01 << 4;
	return;
}
void CZ80MsxDos::op_SET_4_D()
{
	m_R.D |= 0x01 << 4;
	return;
}
void CZ80MsxDos::op_SET_4_E()
{
	m_R.E |= 0x01 << 4;
	return;
}
void CZ80MsxDos::op_SET_4_H()
{
	m_R.H |= 0x01 << 4;
	return;
}
void CZ80MsxDos::op_SET_4_L()
{
	m_R.L |= 0x01 << 4;
	return;
}
void CZ80MsxDos::op_SET_4_memHL()
{
	uint16_t ad = m_R.GetHL();
	uint8_t v = m_pMemSys->Read(ad);
	v |= 0x01 << 4;
	m_pMemSys->Write(m_R.GetHL(), v);
	return;
}
void CZ80MsxDos::op_SET_4_A()
{
	m_R.A |= 0x01 << 4;
	return;
}
void CZ80MsxDos::op_SET_5_B()
{
	m_R.B |= 0x01 << 5;
	return;
}
void CZ80MsxDos::op_SET_5_C()
{
	m_R.C |= 0x01 << 5;
	return;
}
void CZ80MsxDos::op_SET_5_D()
{
	m_R.D |= 0x01 << 5;
	return;
}
void CZ80MsxDos::op_SET_5_E()
{
	m_R.E |= 0x01 << 5;
	return;
}
void CZ80MsxDos::op_SET_5_H()
{
	m_R.H |= 0x01 << 5;
	return;
}
void CZ80MsxDos::op_SET_5_L()
{
	m_R.L |= 0x01 << 5;
	return;
}
void CZ80MsxDos::op_SET_5_memHL()
{
	uint16_t ad = m_R.GetHL();
	uint8_t v = m_pMemSys->Read(ad);
	v |= 0x01 << 5;
	m_pMemSys->Write(m_R.GetHL(), v);
	return;
}
void CZ80MsxDos::op_SET_5_A()
{
	m_R.A |= 0x01 << 5;
	return;
}
void CZ80MsxDos::op_SET_6_B()
{
	m_R.B |= 0x01 << 6;
	return;
}
void CZ80MsxDos::op_SET_6_C()
{
	m_R.C |= 0x01 << 6;
	return;
}
void CZ80MsxDos::op_SET_6_D()
{
	m_R.D |= 0x01 << 6;
	return;
}
void CZ80MsxDos::op_SET_6_E()
{
	m_R.E |= 0x01 << 6;
	return;
}
void CZ80MsxDos::op_SET_6_H()
{
	m_R.H |= 0x01 << 6;
	return;
}
void CZ80MsxDos::op_SET_6_L()
{
	m_R.L |= 0x01 << 6;
	return;
}
void CZ80MsxDos::op_SET_6_memHL()
{
	uint16_t ad = m_R.GetHL();
	uint8_t v = m_pMemSys->Read(ad);
	v |= 0x01 << 6;
	m_pMemSys->Write(m_R.GetHL(), v);
	return;
}
void CZ80MsxDos::op_SET_6_A()
{
	m_R.A |= 0x01 << 6;
	return;
}
void CZ80MsxDos::op_SET_7_B()
{
	m_R.B |= 0x01 << 7;
	return;
}
void CZ80MsxDos::op_SET_7_C()
{
	m_R.C |= 0x01 << 7;
	return;
}
void CZ80MsxDos::op_SET_7_D()
{
	m_R.D |= 0x01 << 7;
	return;
}
void CZ80MsxDos::op_SET_7_E()
{
	m_R.E |= 0x01 << 7;
	return;
}
void CZ80MsxDos::op_SET_7_H()
{
	m_R.H |= 0x01 << 7;
	return;
}
void CZ80MsxDos::op_SET_7_L()
{
	m_R.L |= 0x01 << 7;
	return;
}
void CZ80MsxDos::op_SET_7_memHL()
{
	uint16_t ad = m_R.GetHL();
	uint8_t v = m_pMemSys->Read(ad);
	v |= 0x01 << 7;
	m_pMemSys->Write(m_R.GetHL(), v);
	return;
}
void CZ80MsxDos::op_SET_7_A()
{
	m_R.A |= 0x01 << 7;
	return;
}

// extended2 for IX
void CZ80MsxDos::op_ADD_IX_BC()
{
	m_R.Add16(&m_R.IX, m_R.GetBC());
	return;
}
void CZ80MsxDos::op_ADD_IX_DE()
{
	m_R.Add16(&m_R.IX, m_R.GetDE());
	return;
}
void CZ80MsxDos::op_LD_IX_ad()
{
	m_R.IX = m_pMemSys->Read(m_R.PC++);
	m_R.IX |= static_cast<uint16_t>(m_pMemSys->Read(m_R.PC++)) << 8;
	return;
}
void CZ80MsxDos::op_LD_memAD_IX()
{
	uint16_t ad = m_pMemSys->Read(m_R.PC++);
	ad |= static_cast<z80memaddr_t>(m_pMemSys->Read(m_R.PC++)) << 8;
	//
	m_pMemSys->Write(ad+0, (m_R.IX>>0)&0xff);
	m_pMemSys->Write(ad+1, (m_R.IX>>8)&0xff);
	return;
}
void CZ80MsxDos::op_INC_IX()
{
	++m_R.IX;
	return;
}
void CZ80MsxDos::op_ADD_IX_IX()
{
	m_R.Add16(&m_R.IX, m_R.IX);
	return;
}
void CZ80MsxDos::op_LD_IX_memAD()
{
	uint16_t ad = m_pMemSys->Read(m_R.PC++);
	ad |= static_cast<z80memaddr_t>(m_pMemSys->Read(m_R.PC++)) << 8;
	//
	m_R.IX = m_pMemSys->Read(ad + 0);
	m_R.IX |= static_cast<uint16_t>(m_pMemSys->Read(ad + 1)) << 8;
	return;
}
void CZ80MsxDos::op_DEC_IX()
{
	--m_R.IX;
	return;
}
void CZ80MsxDos::op_INC_memIXpV()
{
	uint16_t ad = m_R.IX + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	m_R.Inc8(&v);
	m_pMemSys->Write(ad, v);
	return;
}
void CZ80MsxDos::op_DEC_memIXpV()
{
	uint16_t ad = m_R.IX + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	m_R.Dec8(&v);
	m_pMemSys->Write(ad, v);
	return;
}
void CZ80MsxDos::op_LD_memIXpV_v()
{
	uint16_t ad = m_R.IX + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(m_R.PC++);
	m_pMemSys->Write(ad, v);
	return;
}
void CZ80MsxDos::op_ADD_IX_SP()
{
	m_R.Add16(&m_R.IX, m_R.SP);
	return;
}
void CZ80MsxDos::op_LD_B_memIXpV()
{
	uint16_t ad = m_R.IX + m_pMemSys->Read(m_R.PC++);
	m_R.B = m_pMemSys->Read(ad);
	return;
}
void CZ80MsxDos::op_LD_C_memIXpV()
{
	uint16_t ad = m_R.IX + m_pMemSys->Read(m_R.PC++);
	m_R.C = m_pMemSys->Read(ad);
	return;
}
void CZ80MsxDos::op_LD_D_memIXpV()
{
	uint16_t ad = m_R.IX + m_pMemSys->Read(m_R.PC++);
	m_R.D = m_pMemSys->Read(ad);
	return;
}
void CZ80MsxDos::op_LD_E_memIXpV()
{
	uint16_t ad = m_R.IX + m_pMemSys->Read(m_R.PC++);
	m_R.E = m_pMemSys->Read(ad);
	return;
}
void CZ80MsxDos::op_LD_H_memIXpV()
{
	uint16_t ad = m_R.IX + m_pMemSys->Read(m_R.PC++);
	m_R.H = m_pMemSys->Read(ad);
	return;
}
void CZ80MsxDos::op_LD_L_memIXpV()
{
	uint16_t ad = m_R.IX + m_pMemSys->Read(m_R.PC++);
	m_R.L = m_pMemSys->Read(ad);
	return;
}
void CZ80MsxDos::op_LD_memIXpV_B()
{
	uint16_t ad = m_R.IX + m_pMemSys->Read(m_R.PC++);
	m_pMemSys->Write(ad, m_R.B);
	return;
}
void CZ80MsxDos::op_LD_memIXpV_C()
{
	uint16_t ad = m_R.IX + m_pMemSys->Read(m_R.PC++);
	m_pMemSys->Write(ad, m_R.C);
	return;
}
void CZ80MsxDos::op_LD_memIXpV_D()
{
	uint16_t ad = m_R.IX + m_pMemSys->Read(m_R.PC++);
	m_pMemSys->Write(ad, m_R.D);
	return;
}
void CZ80MsxDos::op_LD_memIXpV_E()
{
	uint16_t ad = m_R.IX + m_pMemSys->Read(m_R.PC++);
	m_pMemSys->Write(ad, m_R.E);
	return;
}
void CZ80MsxDos::op_LD_memIXpV_H()
{
	uint16_t ad = m_R.IX + m_pMemSys->Read(m_R.PC++);
	m_pMemSys->Write(ad, m_R.H);
	return;
}
void CZ80MsxDos::op_LD_memIXpV_L()
{
	uint16_t ad = m_R.IX + m_pMemSys->Read(m_R.PC++);
	m_pMemSys->Write(ad, m_R.L);
	return;
}
void CZ80MsxDos::op_LD_memIXpV_A()
{
	uint16_t ad = m_R.IX + m_pMemSys->Read(m_R.PC++);
	m_pMemSys->Write(ad, m_R.A);
	return;
}
void CZ80MsxDos::op_LD_A_memIXpV()
{
	uint16_t ad = m_R.IX + m_pMemSys->Read(m_R.PC++);
	m_R.A = m_pMemSys->Read(ad);
	return;
}
void CZ80MsxDos::op_ADD_A_memIXpV()
{
	uint16_t ad = m_R.IX + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	m_R.Add8(&m_R.A, v);
	return;
}
void CZ80MsxDos::op_ADC_A_memIXpV()
{
	uint16_t ad = m_R.IX + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	m_R.Add8Cy(&m_R.A, v, m_R.F.C);
	return;
}
void CZ80MsxDos::op_SUB_memIXpV()
{
	uint16_t ad = m_R.IX + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	m_R.Sub8(&m_R.A, v);
	return;
}
void CZ80MsxDos::op_SBC_A_memIXpV()
{
	uint16_t ad = m_R.IX + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	m_R.Sub8Cy(&m_R.A, v, m_R.F.C);
	return;
}
void CZ80MsxDos::op_AND_memIXpV()
{
	uint16_t ad = m_R.IX + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	m_R.And8(&m_R.A, v);
	return;
}
void CZ80MsxDos::op_XOR_memIXpV()
{
	uint16_t ad = m_R.IX + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	m_R.Xor8(&m_R.A, v);
	return;
}
void CZ80MsxDos::op_OR_memIXpV()
{
	uint16_t ad = m_R.IX + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	m_R.Or8(&m_R.A, v);
	return;
}
void CZ80MsxDos::op_CP_memIXpV()
{
	uint16_t ad = m_R.IX + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	uint8_t tempA = m_R.A;
	m_R.Sub8(&tempA, v);
	return;
}
void CZ80MsxDos::op_EXTENDED_2IX2()
{
	// このマシンコードは、DDh+CBh+nn+vv の形式になっていて、
	// データ部nnがコードの途中に位置していることに注意する
	// このメソッドが呼ばれた時点で、DDh+CBhまではデコードされているからPCの位置はnnを示している。
	// 子メソッドを呼び出す時はこの位置を維持する。子メソッドから戻ったら06hの分のPC++を行う。
	uint8_t vv = m_pMemSys->Read(m_R.PC+1);
	auto pFunc = m_Op.Extended2IX2[vv];
	(this->*pFunc)();
	m_R.PC++;
	return;
}
void CZ80MsxDos::op_POP_IX()
{
	m_R.IX = m_pMemSys->Read(m_R.SP++);
	m_R.IX |= static_cast<uint16_t>(m_pMemSys->Read(m_R.SP++)) << 8;
	return;
}
void CZ80MsxDos::op_EX_memSP_IX()
{
	uint8_t ixl = (m_R.IX >> 0) & 0xFF;
	uint8_t ixh = (m_R.IX >> 8) & 0xFF;
	m_R.IX = m_pMemSys->Read(m_R.SP+0);
	m_R.IX |= static_cast<uint16_t>(m_pMemSys->Read(m_R.SP+1)) < 8;;
	m_pMemSys->Write(m_R.SP+0, ixl);
	m_pMemSys->Write(m_R.SP+1, ixh);
	return;
}
void CZ80MsxDos::op_PUSH_IX()
{
	m_pMemSys->Write(--m_R.SP, (m_R.IX>>8)&0xFF);
	m_pMemSys->Write(--m_R.SP, (m_R.IX>>0)&0xFF);
	return;
}
void CZ80MsxDos::op_JP_memIX()
{
	m_R.PC = m_R.IX;	// IXの値そのものがアドレス値である
	return;
}
void CZ80MsxDos::op_LD_SP_IX()
{
	m_R.SP = m_R.IX;
	return;
}

// extended2 for IX - 2
void CZ80MsxDos::op_RLC_memVpIX()
{
	z80memaddr_t ad = m_R.IX + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	m_R.Rlc8(&v);
	m_pMemSys->Write(ad, v);
	return;
}
void CZ80MsxDos::op_RRC_memVpIX()
{
	z80memaddr_t ad = m_R.IX + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	m_R.Rrc8(&v);
	m_pMemSys->Write(ad, v);
	return;
}
void CZ80MsxDos::op_RL_memVpIX()
{
	z80memaddr_t ad = m_R.IX + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	m_R.Rl8(&v);
	m_pMemSys->Write(ad, v);
	return;
}
void CZ80MsxDos::op_RR_memVpIX()
{
	z80memaddr_t ad = m_R.IX + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	m_R.Rr8(&v);
	m_pMemSys->Write(ad, v);
	return;
}
void CZ80MsxDos::op_SLA_memVpIX()
{
	z80memaddr_t ad = m_R.IX + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	m_R.Sla8(&v);
	m_pMemSys->Write(ad, v);
	return;
}
void CZ80MsxDos::op_SRA_memVpIX()
{
	z80memaddr_t ad = m_R.IX + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	m_R.Sra8(&v);
	m_pMemSys->Write(ad, v);
	return;
}
void CZ80MsxDos::op_SRL_memVpIX()
{
	z80memaddr_t ad = m_R.IX + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	m_R.Srl8(&v);
	m_pMemSys->Write(ad, v);
	return;
}
void CZ80MsxDos::op_BIT_0_memVpIX()
{
	z80memaddr_t ad = m_R.IX + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	m_R.F.Z = (v & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_1_memVpIX()
{
	z80memaddr_t ad = m_R.IX + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	m_R.F.Z = ((v>>1) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_2_memVpIX()
{
	z80memaddr_t ad = m_R.IX + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	m_R.F.Z = ((v>>2) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_3_memVpIX()
{
	z80memaddr_t ad = m_R.IX + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	m_R.F.Z = ((v>>3) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_4_memVpIX()
{
	z80memaddr_t ad = m_R.IX + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	m_R.F.Z = ((v>>4) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_5_memVpIX()
{
	z80memaddr_t ad = m_R.IX + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	m_R.F.Z = ((v>>5) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_6_memVpIX()
{
	z80memaddr_t ad = m_R.IX + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	m_R.F.Z = ((v>>6) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_7_memVpIX()
{
	z80memaddr_t ad = m_R.IX + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	m_R.F.Z = ((v>>7) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_RES_0_memVpIX()
{
	z80memaddr_t ad = m_R.IX + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	v &= ((0x01 << 0) ^ 0xFF);
	m_pMemSys->Write(ad, v);
	return;
}
void CZ80MsxDos::op_RES_1_memVpIX()
{
	z80memaddr_t ad = m_R.IX + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	v &= ((0x01 << 1) ^ 0xFF);
	m_pMemSys->Write(ad, v);
	return;
}
void CZ80MsxDos::op_RES_2_memVpIX()
{
	z80memaddr_t ad = m_R.IX + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	v &= ((0x01 << 2) ^ 0xFF);
	m_pMemSys->Write(ad, v);
	return;
}
void CZ80MsxDos::op_RES_3_memVpIX()
{
	z80memaddr_t ad = m_R.IX + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	v &= ((0x01 << 3) ^ 0xFF);
	m_pMemSys->Write(ad, v);
	return;
}
void CZ80MsxDos::op_RES_4_memVpIX()
{
	z80memaddr_t ad = m_R.IX + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	v &= ((0x01 << 4) ^ 0xFF);
	m_pMemSys->Write(ad, v);
	return;
}
void CZ80MsxDos::op_RES_5_memVpIX()
{
	z80memaddr_t ad = m_R.IX + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	v &= ((0x01 << 5) ^ 0xFF);
	m_pMemSys->Write(ad, v);
	return;
}
void CZ80MsxDos::op_RES_6_memVpIX()
{
	z80memaddr_t ad = m_R.IX + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	v &= ((0x01 << 6) ^ 0xFF);
	m_pMemSys->Write(ad, v);
	return;
}
void CZ80MsxDos::op_RES_7_memVpIX()
{
	z80memaddr_t ad = m_R.IX + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	v &= ((0x01 << 7) ^ 0xFF);
	m_pMemSys->Write(ad, v);
	return;
}
void CZ80MsxDos::op_SET_0_memVpIX()
{
	z80memaddr_t ad = m_R.IX + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	v |= 0x01 << 0;
	m_pMemSys->Write(ad, v);
	return;
}
void CZ80MsxDos::op_SET_1_memVpIX()
{
	z80memaddr_t ad = m_R.IX + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	v |= 0x01 << 1;
	m_pMemSys->Write(ad, v);
	return;
}
void CZ80MsxDos::op_SET_2_memVpIX()
{
	z80memaddr_t ad = m_R.IX + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	v |= 0x01 << 2;
	m_pMemSys->Write(ad, v);
	return;
}
void CZ80MsxDos::op_SET_3_memVpIX()
{
	z80memaddr_t ad = m_R.IX + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	v |= 0x01 << 3;
	m_pMemSys->Write(ad, v);
	return;
}
void CZ80MsxDos::op_SET_4_memVpIX()
{
	z80memaddr_t ad = m_R.IX + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	v |= 0x01 << 4;
	m_pMemSys->Write(ad, v);
	return;
}
void CZ80MsxDos::op_SET_5_memVpIX()
{
	z80memaddr_t ad = m_R.IX + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	v |= 0x01 << 5;
	m_pMemSys->Write(ad, v);
	return;
}
void CZ80MsxDos::op_SET_6_memVpIX()
{
	z80memaddr_t ad = m_R.IX + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	v |= 0x01 << 6;
	m_pMemSys->Write(ad, v);
	return;
}
void CZ80MsxDos::op_SET_7_memVpIX()
{
	z80memaddr_t ad = m_R.IX + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	v |= 0x01 << 7;
	m_pMemSys->Write(ad, v);
	return;
}

// extended3 - for debug
void CZ80MsxDos::op_DEBUGBREAK()
{
	DEBUG_BREAK;
	return;

}

// extended3
void __time_critical_func(CZ80MsxDos::op_IN_B_memC)()
{
	// ※Bレジスタによる16bitアドレスは考慮しない。
	m_R.B = m_pIoSys->In(m_R.C);
	m_R.SetFlagByIN(m_R.B);
	return;
}
void __time_critical_func(CZ80MsxDos::op_OUT_memC_B)()
{
	m_pIoSys->Out(m_R.C, m_R.B);
	return;
}
void CZ80MsxDos::op_SBC_HL_BC()
{
	uint16_t v = m_R.GetHL();
	m_R.Sub16Cy(&v, m_R.GetBC(), m_R.F.C);
	m_R.SetHL(v);
	return;
}
void CZ80MsxDos::op_LD_memAD_BC()
{
	z80memaddr_t ad = m_pMemSys->Read(m_R.PC++);
	ad |= static_cast<z80memaddr_t>(m_pMemSys->Read(m_R.PC++)) << 8;
	m_pMemSys->Write(ad+0, m_R.C);
	m_pMemSys->Write(ad+1, m_R.B);
	return;
}
void CZ80MsxDos::op_NEG()
{
	if( m_R.A == 0 )
	{
		m_R.F.C = 0;
		m_R.F.Z = 1;
		m_R.F.PV= 0;
		m_R.F.S = 0;
		m_R.F.N = 1;
		m_R.F.H = 0;
	}
	else{
		m_R.A = (0 - m_R.A) & 0xFF;
		//
		m_R.F.C = 1;
		m_R.F.Z = 0;
		m_R.F.PV= 0;
		m_R.F.S = (m_R.A>>7)&0x1;
		m_R.F.N = 1;
		m_R.F.H = 1;
	}
	return;
}
void CZ80MsxDos::op_RETN()
{
	m_R.PC = m_pMemSys->Read(m_R.SP++);
	m_R.PC |= m_pMemSys->Read(m_R.SP++) << 8;
	m_bIFF1 = m_bIFF2;
	return;
}
void CZ80MsxDos::op_IM_0()
{
	m_IM = INTERRUPTMODE0;
	return;
}
void CZ80MsxDos::op_LD_i_A()
{
	m_R.I = m_R.A;
	return;
}
void __time_critical_func(CZ80MsxDos::op_IN_C_memC)()
{
	m_R.C = m_pIoSys->In(m_R.C);
	m_R.SetFlagByIN(m_R.C);
	return;
}
void __time_critical_func(CZ80MsxDos::op_OUT_memC_C)()
{
	m_pIoSys->Out(m_R.C, m_R.C);
	return;
}
void CZ80MsxDos::op_ADC_HL_BC()
{
	uint16_t v = m_R.GetHL();
	m_R.Add16Cy(&v, m_R.GetBC(), m_R.F.C);
	m_R.SetHL(v);
	return;
}
void CZ80MsxDos::op_LD_BC_memAD()
{
	z80memaddr_t ad = m_pMemSys->Read(m_R.PC++);
	ad |= static_cast<z80memaddr_t>(m_pMemSys->Read(m_R.PC++)) << 8;
	m_R.C = m_pMemSys->Read(ad+0);
	m_R.B = m_pMemSys->Read(ad+1);
	return;
}
void CZ80MsxDos::op_RETI()
{
	m_R.PC = m_pMemSys->Read(m_R.SP++);
	m_R.PC |= m_pMemSys->Read(m_R.SP++) << 8;
	m_bIFF1 = m_bIFF2;
	return;
}
void CZ80MsxDos::op_LD_R_A()
{
	m_R.R = m_R.A;
	return;
}
void __time_critical_func(CZ80MsxDos::op_IN_D_memC)()
{
	m_R.D = m_pIoSys->In(m_R.C);
	m_R.SetFlagByIN(m_R.D);
	return;
}
void __time_critical_func(CZ80MsxDos::op_OUT_memC_D)()
{
	m_pIoSys->Out(m_R.C, m_R.D);
	return;
}
void CZ80MsxDos::op_SBC_HL_DE()
{
	uint16_t v = m_R.GetHL();
	m_R.Sub16Cy(&v, m_R.GetDE(), m_R.F.C);
	m_R.SetHL(v);
	return;
}
void CZ80MsxDos::op_LD_memAD_DE()
{
	z80memaddr_t ad = m_pMemSys->Read(m_R.PC++);
	ad |= static_cast<z80memaddr_t>(m_pMemSys->Read(m_R.PC++)) << 8;
	m_pMemSys->Write(ad+0, m_R.E);
	m_pMemSys->Write(ad+1, m_R.D);
	return;
}
void CZ80MsxDos::op_IM_1()
{
	m_IM = INTERRUPTMODE1;
	return;
}
void CZ80MsxDos::op_LD_A_i()
{
	m_R.A = m_R.I;
	//
	m_R.F.C = m_R.F.C;
	m_R.F.Z = (m_R.A==0)?1:0;
	m_R.F.PV = (m_bIFF2)?1:0;
	m_R.F.S = (m_R.A>>7)&0x01;
	m_R.F.N = 0;
	m_R.F.H = 0;
	return;
}
void __time_critical_func(CZ80MsxDos::op_IN_E_memC)()
{
	m_R.E = m_pIoSys->In(m_R.C);
	m_R.SetFlagByIN(m_R.E);
	return;
}
void __time_critical_func(CZ80MsxDos::op_OUT_memC_E)()
{
	m_pIoSys->Out(m_R.C, m_R.E);
	return;
}
void CZ80MsxDos::op_ADC_HL_DE()
{
	uint16_t v = m_R.GetHL();
	m_R.Add16Cy(&v, m_R.GetDE(), m_R.F.C);
	m_R.SetHL(v);
	return;
}
void CZ80MsxDos::op_LD_DE_memAD()
{
	z80memaddr_t ad = m_pMemSys->Read(m_R.PC++);
	ad |= static_cast<z80memaddr_t>(m_pMemSys->Read(m_R.PC++)) << 8;
	m_R.E = m_pMemSys->Read(ad+0);
	m_R.D = m_pMemSys->Read(ad+1);
	return;
}
void CZ80MsxDos::op_IM_2()
{
	m_IM = INTERRUPTMODE2;
	return;
}
void CZ80MsxDos::op_LD_A_R()
{
	m_R.A = m_R.R;
	//
	m_R.F.C = m_R.F.C;
	m_R.F.Z = (m_R.A==0)?1:0;
	m_R.F.PV = (m_bIFF2)?1:0;
	m_R.F.S = (m_R.A>>7)&0x01;
	m_R.F.N = 0;
	m_R.F.H = 0;
	return;
}
void __time_critical_func(CZ80MsxDos::op_IN_H_memC)()
{
	m_R.H = m_pIoSys->In(m_R.C);
	m_R.SetFlagByIN(m_R.H);
	return;
}
void __time_critical_func(CZ80MsxDos::op_OUT_memC_H)()
{
	m_pIoSys->Out(m_R.C, m_R.H);
	return;
}
void CZ80MsxDos::op_SBC_HL_HL()
{
	uint16_t v = m_R.GetHL();
	m_R.Sub16Cy(&v, v, m_R.F.C);
	m_R.SetHL(v);
	return;
}
void CZ80MsxDos::op_RRD()
{
	uint8_t mem = m_pMemSys->Read(m_R.GetHL());
	uint8_t temp = mem&0x0F;
	mem >>= 4;
	mem |= (m_R.A<<4) & 0xF0;
	m_R.A = (m_R.A&0xF0) | temp;
	//
	m_R.F.C = m_R.F.C;
	m_R.F.Z = (m_R.A==0)?1:0;
	m_R.F.PV = m_R.CheckParytyEven(m_R.A);
	m_R.F.S = (m_R.A>>7)&0x01;
	m_R.F.N = 0;
	m_R.F.H = 0;
	return;
}
void __time_critical_func(CZ80MsxDos::op_IN_L_memC)()
{
	m_R.L = m_pIoSys->In(m_R.C);
	m_R.SetFlagByIN(m_R.L);
	return;
}
void __time_critical_func(CZ80MsxDos::op_OUT_memC_L)()
{
	m_pIoSys->Out(m_R.C, m_R.L);
	return;
}
void CZ80MsxDos::op_ADC_HL_HL()
{
	uint16_t v = m_R.GetHL();
	m_R.Add16Cy(&v, v, m_R.F.C);
	m_R.SetHL(v);
	return;
}
void CZ80MsxDos::op_RLD()
{
	uint8_t mem = m_pMemSys->Read(m_R.GetHL());
	uint8_t temp = (mem>>4)&0x0F;
	mem |= (mem<<4) | (m_R.A&0x0F);
	m_R.A = (m_R.A&0xF0) | temp;
	//
	m_R.F.C = m_R.F.C;
	m_R.F.Z = (m_R.A==0)?1:0;
	m_R.F.PV = m_R.CheckParytyEven(m_R.A);
	m_R.F.S = (m_R.A>>7)&0x01;
	m_R.F.N = 0;
	m_R.F.H = 0;
	return;
}
//void CZ80MsxDos::op_IN_memC()
// {
// 	assert(false);
// }
void CZ80MsxDos::op_SBC_HL_SP()
{
	uint16_t v = m_R.GetHL();
	m_R.Sub16Cy(&v, m_R.SP, m_R.F.C);
	m_R.SetHL(v);
	return;
}
void CZ80MsxDos::op_LD_memAD_SP()
{
	z80memaddr_t ad = m_pMemSys->Read(m_R.PC++);
	ad |= static_cast<z80memaddr_t>(m_pMemSys->Read(m_R.PC++)) << 8;
	m_pMemSys->Write(ad+0, (m_R.SP>>0)&0xff);
	m_pMemSys->Write(ad+1, (m_R.SP>>8)&0xff);
	return;
}
void __time_critical_func(CZ80MsxDos::op_IN_A_memC)()
{
	m_R.A = m_pIoSys->In(m_R.C);
	m_R.SetFlagByIN(m_R.A);
	return;
}
void __time_critical_func(CZ80MsxDos::op_OUT_memC_A)()
{
	m_pIoSys->Out(m_R.C, m_R.A);
	return;
}
void CZ80MsxDos::op_ADC_HL_SP()
{
	uint16_t v = m_R.GetHL();
	m_R.Add16Cy(&v, m_R.SP, m_R.F.C);
	m_R.SetHL(v);
	return;
}
void CZ80MsxDos::op_LD_SP_memAD()
{
	z80memaddr_t ad = m_pMemSys->Read(m_R.PC++);
	ad |= static_cast<z80memaddr_t>(m_pMemSys->Read(m_R.PC++)) << 8;
	m_R.SP = m_pMemSys->Read(ad+0);
	m_R.SP |= static_cast<uint16_t>(m_pMemSys->Read(ad+1)) << 8;
	return;
}
void CZ80MsxDos::op_LDI()
{
	uint16_t hl = m_R.GetHL();
	uint16_t de = m_R.GetDE();
	uint16_t bc = m_R.GetBC();
	uint8_t v = m_pMemSys->Read(hl);
	m_pMemSys->Write(de, v);
	m_R.SetHL(hl+1);
	m_R.SetDE(de+1);
	m_R.SetBC(--bc);
	m_R.F.PV = (bc==0)?0:1;
	m_R.F.N = 0;
	m_R.F.H = 0;
	return;
}
void CZ80MsxDos::op_CPI()
{
	uint8_t a = m_R.A;
	uint16_t hl = m_R.GetHL();
	uint8_t v = m_pMemSys->Read(hl);
	uint16_t bc = m_R.GetBC();
	a -= v;
	m_R.SetHL(++hl);
	m_R.SetBC(--bc);
	//
	m_R.F.C = m_R.F.C;
	m_R.F.Z = (a==0)?1:0;
	m_R.F.PV = (bc==0)?0:1;
	m_R.F.S = (a>>7)&0x01;
	m_R.F.N = 1;
	m_R.H = ((m_R.A&0x0F)<(v&0x0F))?1:0; // ビット4への桁借りが生じたか
	return;
}
void __time_critical_func(CZ80MsxDos::op_INI)()
{
	uint16_t hl = m_R.GetHL();
	uint8_t v = m_pIoSys->In(m_R.C);
	m_pMemSys->Write(hl, v);
	m_R.SetHL(hl+1);
	m_R.B--;
	//
	m_R.F.C = m_R.F.C;
	m_R.F.Z = (m_R.B==0)?1:0;
	m_R.F.N = 1;
	return;
}
void __time_critical_func(CZ80MsxDos::op_OUTI)()
{
	uint16_t hl = m_R.GetHL();
	uint8_t v = m_pMemSys->Read(hl);
	m_pIoSys->Out(m_R.C, v);
	m_R.SetHL(hl+1);
	m_R.B--;
	//
	m_R.F.C = m_R.F.C;
	m_R.F.Z = (m_R.B==0)?1:0;
	m_R.F.N = 1;
	return;
}
void CZ80MsxDos::op_LDD()
{
	uint16_t hl = m_R.GetHL();
	uint16_t de = m_R.GetDE();
	uint16_t bc = m_R.GetBC();
	uint8_t v = m_pMemSys->Read(hl);
	m_pMemSys->Write(de, v);
	m_R.SetHL(hl-1);
	m_R.SetDE(de-1);
	m_R.SetBC(--bc);
	m_R.F.PV = (bc==0)?0:1;
	m_R.F.N = 0;
	m_R.F.H = 0;
	return;
}
void CZ80MsxDos::op_CPD()
{
	uint8_t a = m_R.A;
	uint16_t hl = m_R.GetHL();
	uint8_t v = m_pMemSys->Read(hl);
	uint16_t bc = m_R.GetBC();
	a -= v;
	m_R.SetHL(--hl);
	m_R.SetBC(--bc);
	//
	m_R.F.C = m_R.F.C;
	m_R.F.Z = (a==0)?1:0;
	m_R.F.PV = (bc==0)?0:1;
	m_R.F.S = (a>>7)&0x01;
	m_R.F.N = 1;
	m_R.H = ((m_R.A&0x0F)<(v&0x0F))?1:0; // ビット4への桁借りが生じたか
	return;
}
void __time_critical_func(CZ80MsxDos::op_IND)()
{
	uint16_t hl = m_R.GetHL();
	uint8_t v = m_pIoSys->In(m_R.C);
	m_pMemSys->Write(hl, v);
	m_R.SetHL(hl-1);
	m_R.B--;
	//
	m_R.F.C = m_R.F.C;
	m_R.F.Z = (m_R.B==0)?1:0;
	m_R.F.N = 1;
	return;
}
void __time_critical_func(CZ80MsxDos::op_OUTD)()
{
	uint16_t hl = m_R.GetHL();
	uint8_t v = m_pMemSys->Read(hl);
	m_pIoSys->Out(m_R.C, v);
	m_R.SetHL(hl-1);
	m_R.B--;
	//
	m_R.F.C = m_R.F.C;
	m_R.F.Z = (m_R.B==0)?1:0;
	m_R.F.N = 1;
	return;
}
void __time_critical_func(CZ80MsxDos::op_LDIR())
{
	uint16_t hl = m_R.GetHL();
	uint16_t de = m_R.GetDE();
	uint16_t bc = m_R.GetBC();
	do{
		uint8_t v = m_pMemSys->Read(hl);
		m_pMemSys->Write(de, v);
		++hl, ++de, --bc;
	} while(bc != 0);
	m_R.SetHL(hl);
	m_R.SetDE(de);
	m_R.SetBC(bc);
	m_R.F.PV = 0;
	m_R.F.N = 0;
	m_R.F.H = 0;
	return;
}
void __time_critical_func(CZ80MsxDos::op_CPIR())
{
	uint8_t a = m_R.A;
	uint16_t hl = m_R.GetHL();
	uint16_t bc = m_R.GetBC();
	do{
		uint8_t v = m_pMemSys->Read(hl);
		++hl, --bc;
		if(v == a ){
			m_R.SetHL(hl);
			m_R.SetBC(bc);
			m_R.F.Z = 1;
			m_R.F.PV = 1;
			m_R.F.S = 0;
			m_R.F.N = 1;
			m_R.F.H = 0;
			return;
		}
	}while(bc!=0);
	//
	m_R.SetHL(hl);
	m_R.SetBC(bc);
	//
	m_R.F.Z = 0;
	m_R.F.PV = 0;
	m_R.F.N = 1;
	return;
}
void __time_critical_func(CZ80MsxDos::op_INIR())
{
	uint16_t hl = m_R.GetHL();
	do{
		uint8_t v = m_pIoSys->In(m_R.C);
		m_pMemSys->Write(hl, v);
		++hl,m_R.B--;
	}while(m_R.B!=0);
	//
	m_R.SetHL(hl);
	m_R.F.Z = 1;
	m_R.F.N = 1;
	return;
}
void __time_critical_func(CZ80MsxDos::op_OTIR())
{
	uint16_t hl = m_R.GetHL();
	do{
		uint8_t v = m_pMemSys->Read(hl);
		m_pIoSys->Out(m_R.C, v);
		++hl, m_R.B--;
	}while(m_R.B!=0);
	m_R.SetHL(hl);
	m_R.F.Z = 1;
	m_R.F.N = 1;
	return;
}
void __time_critical_func(CZ80MsxDos::op_LDDR())
{
	uint16_t hl = m_R.GetHL();
	uint16_t de = m_R.GetDE();
	uint16_t bc = m_R.GetBC();
	do{
		uint8_t v = m_pMemSys->Read(hl);
		m_pMemSys->Write(de, v);
		--hl, --de, --bc;
	} while(bc != 0);
	m_R.SetHL(hl);
	m_R.SetDE(de);
	m_R.SetBC(bc);
	m_R.F.PV = 0;
	m_R.F.N = 0;
	m_R.F.H = 0;
	return;
}
void __time_critical_func(CZ80MsxDos::op_CPDR())
{
	uint8_t a = m_R.A;
	uint16_t hl = m_R.GetHL();
	uint16_t bc = m_R.GetBC();
	do{
		uint8_t v = m_pMemSys->Read(hl);
		--hl, --bc;
		if (v == a) {
			m_R.SetHL(hl);
			m_R.SetBC(bc);
			m_R.F.Z = 1;
			m_R.F.PV = 1;
			m_R.F.S = 0;
			m_R.F.N = 1;
			m_R.F.H = 0;
			return;
		}
	}while(bc!=0);
	//
	m_R.SetHL(hl);
	m_R.SetBC(bc);
	//
	m_R.F.Z = 0;
	m_R.F.PV = 0;
	m_R.F.N = 1;
	return;
}
void __time_critical_func(CZ80MsxDos::op_INDR())
{
	uint16_t hl = m_R.GetHL();
	do{
		uint8_t v = m_pIoSys->In(m_R.C);
		m_pMemSys->Write(hl, v);
		--hl,m_R.B--;
	}while(m_R.B!=0);
	//
	m_R.SetHL(hl);
	m_R.F.Z = 1;
	m_R.F.N = 1;
	return;
}
void __time_critical_func(CZ80MsxDos::op_OUTR())
{
	uint16_t hl = m_R.GetHL();
	do{
		uint8_t v = m_pMemSys->Read(hl);
		m_pIoSys->Out(m_R.C, v);
		--hl, m_R.B--;
	}while(m_R.B!=0);
	m_R.SetHL(hl);
	m_R.F.Z = 1;
	m_R.F.N = 1;
	return;
}

// extended4 for IY
void CZ80MsxDos::op_ADD_IY_BC()
{
	m_R.Add16(&m_R.IY, m_R.GetBC());
	return;
}
void CZ80MsxDos::op_ADD_IY_DE()
{
	m_R.Add16(&m_R.IY, m_R.GetDE());
	return;
}
void CZ80MsxDos::op_LD_IY_ad()
{
	m_R.IY = m_pMemSys->Read(m_R.PC++);
	m_R.IY |= static_cast<uint16_t>(m_pMemSys->Read(m_R.PC++)) << 8;
	return;
}
void CZ80MsxDos::op_LD_memAD_IY()
{
	uint16_t ad = m_pMemSys->Read(m_R.PC++);
	ad |= static_cast<z80memaddr_t>(m_pMemSys->Read(m_R.PC++)) << 8;
	//
	m_pMemSys->Write(ad+0, (m_R.IY>>0)&0xff);
	m_pMemSys->Write(ad+1, (m_R.IY>>8)&0xff);
	return;
}
void CZ80MsxDos::op_INC_IY()
{
	++m_R.IY;
	return;
}
void CZ80MsxDos::op_ADD_IY_IY()
{
	m_R.Add16(&m_R.IY, m_R.IY);
	return;
}
void CZ80MsxDos::op_LD_IY_memAD()
{
	uint16_t ad = m_pMemSys->Read(m_R.PC++);
	ad |= static_cast<z80memaddr_t>(m_pMemSys->Read(m_R.PC++)) << 8;
	//
	m_R.IY = m_pMemSys->Read(ad + 0);
	m_R.IY |= static_cast<uint16_t>(m_pMemSys->Read(ad + 1)) << 8;
	return;
}
void CZ80MsxDos::op_DEC_IY()
{
	--m_R.IY;
	return;
}
void CZ80MsxDos::op_INC_memIYpV()
{
	uint16_t ad = m_R.IY + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	m_R.Inc8(&v);
	m_pMemSys->Write(ad, v);
	return;
}
void CZ80MsxDos::op_DEC_memIYpV()
{
	uint16_t ad = m_R.IY + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	m_R.Dec8(&v);
	m_pMemSys->Write(ad, v);
	return;
}
void CZ80MsxDos::op_LD_memIYpV_v()
{
	uint16_t ad = m_R.IY + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(m_R.PC++);
	m_pMemSys->Write(ad, v);
	return;
}
void CZ80MsxDos::op_ADD_IY_SP()
{
	m_R.Add16(&m_R.IY, m_R.SP);
	return;
}
void CZ80MsxDos::op_LD_B_IYH()	// undoc.Z80
{
	m_R.B = (m_R.IY >> 8 ) & 0xFF;
	return;
}
void CZ80MsxDos::op_LD_B_IYL()	// undoc.Z80
{
	m_R.B = m_R.IY & 0xFF;
	return;
}
void CZ80MsxDos::op_LD_B_memIYpV()
{
	uint16_t ad = m_R.IY + m_pMemSys->Read(m_R.PC++);
	m_R.B = m_pMemSys->Read(ad);
	return;
}
void CZ80MsxDos::op_LD_C_IYH()	// undoc.Z80
{
	m_R.C = (m_R.IY >> 8) & 0xFF;
	return;
}
void CZ80MsxDos::op_LD_C_IYL()	// undoc.Z80
{
	m_R.C = m_R.IY & 0xFF;
	return;
}
void CZ80MsxDos::op_LD_C_memIYpV()
{
	uint16_t ad = m_R.IY + m_pMemSys->Read(m_R.PC++);
	m_R.C = m_pMemSys->Read(ad);
	return;
}
void CZ80MsxDos::op_LD_D_memIYpV()
{
	uint16_t ad = m_R.IY + m_pMemSys->Read(m_R.PC++);
	m_R.D = m_pMemSys->Read(ad);
	return;
}
void CZ80MsxDos::op_LD_E_memIYpV()
{
	uint16_t ad = m_R.IY + m_pMemSys->Read(m_R.PC++);
	m_R.E = m_pMemSys->Read(ad);
	return;
}
void CZ80MsxDos::op_LD_IYH_E()	// undoc.Z80
{
	m_R.IY = (m_R.IY & 0xFF) | (static_cast<uint16_t>(m_R.E) << 8);
	return;
}
void CZ80MsxDos::op_LD_H_memIYpV()
{
	uint16_t ad = m_R.IY + m_pMemSys->Read(m_R.PC++);
	m_R.H = m_pMemSys->Read(ad);
	return;
}
void CZ80MsxDos::op_LD_IYL_IYH()	// undoc.Z80	// blueMSXの逆アセンブラだと LD_IYL,H と表示されるがそれは間違いっぽい
{
	m_R.IY = (m_R.IY & 0xFF00) | ((m_R.IY >> 8) & 0xFF);
	return;
}
void CZ80MsxDos::op_LD_IYL_IYL()	// undoc.Z80
{
	// do nothing
	return;
}
void CZ80MsxDos::op_LD_L_memIYpV()
{
	uint16_t ad = m_R.IY + m_pMemSys->Read(m_R.PC++);
	m_R.L = m_pMemSys->Read(ad);
	return;
}
void CZ80MsxDos::op_LD_IYL_A()	// undoc.Z80
{
	m_R.IY = (m_R.IY & 0xFF00) | m_R.A;
	return;
}
void CZ80MsxDos::op_LD_memIYpV_B()
{
	uint16_t ad = m_R.IY + m_pMemSys->Read(m_R.PC++);
	m_pMemSys->Write(ad, m_R.B);
	return;
}
void CZ80MsxDos::op_LD_memIYpV_C()
{
	uint16_t ad = m_R.IY + m_pMemSys->Read(m_R.PC++);
	m_pMemSys->Write(ad, m_R.C);
	return;
}
void CZ80MsxDos::op_LD_memIYpV_D()
{
	uint16_t ad = m_R.IY + m_pMemSys->Read(m_R.PC++);
	m_pMemSys->Write(ad, m_R.D);
	return;
}
void CZ80MsxDos::op_LD_memIYpV_E()
{
	uint16_t ad = m_R.IY + m_pMemSys->Read(m_R.PC++);
	m_pMemSys->Write(ad, m_R.E);
	return;
}
void CZ80MsxDos::op_LD_memIYpV_H()
{
	uint16_t ad = m_R.IY + m_pMemSys->Read(m_R.PC++);
	m_pMemSys->Write(ad, m_R.H);
	return;
}
void CZ80MsxDos::op_LD_memIYpV_L()
{
	uint16_t ad = m_R.IY + m_pMemSys->Read(m_R.PC++);
	m_pMemSys->Write(ad, m_R.L);
	return;
}
void CZ80MsxDos::op_LD_memIYpV_A()
{
	uint16_t ad = m_R.IY + m_pMemSys->Read(m_R.PC++);
	m_pMemSys->Write(ad, m_R.A);
	return;
}
void CZ80MsxDos::op_LD_A_IYH()	// undoc.Z80
{
	m_R.A = static_cast<uint8_t>((m_R.IY>>8)&0xFF);
	return;
}
void CZ80MsxDos::op_LD_A_IYL()	// undoc.Z80
{
	m_R.A = static_cast<uint8_t>(m_R.IY&0xFF);
	return;
}
void CZ80MsxDos::op_LD_A_memIYpV()
{
	uint16_t ad = m_R.IY + m_pMemSys->Read(m_R.PC++);
	m_R.A = m_pMemSys->Read(ad);
	return;
}
void CZ80MsxDos::op_ADD_A_memIYpV()
{
	uint16_t ad = m_R.IY + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	m_R.Add8(&m_R.A, v);
	return;
}
void CZ80MsxDos::op_ADC_A_memIYpV()
{
	uint16_t ad = m_R.IY + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	m_R.Add8Cy(&m_R.A, v, m_R.F.C);
	return;
}
void CZ80MsxDos::op_SUB_memIYpV()
{
	uint16_t ad = m_R.IY + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	m_R.Sub8(&m_R.A, v);
	return;
}
void CZ80MsxDos::op_SBC_A_memIYpV()
{
	uint16_t ad = m_R.IY + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	m_R.Sub8Cy(&m_R.A, v, m_R.F.C);
	return;
}
void CZ80MsxDos::op_AND_memIYpV()
{
	uint16_t ad = m_R.IY + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	m_R.And8(&m_R.A, v);
	return;
}
void CZ80MsxDos::op_XOR_memIYpV()
{
	uint16_t ad = m_R.IY + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	m_R.Xor8(&m_R.A, v);
	return;
}
void CZ80MsxDos::op_OR_memIYpV()
{
	uint16_t ad = m_R.IY + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	m_R.Or8(&m_R.A, v);
	return;
}
void CZ80MsxDos::op_CP_memIYpV()
{
	uint16_t ad = m_R.IY + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	uint8_t tempA = m_R.A;
	m_R.Sub8(&tempA, v);
	return;
}
void CZ80MsxDos::op_EXTENDED_4IY2()
{
	// このマシンコードは、DDh+CBh+nn+vv の形式になっていて、
	// データ部nnがコードの途中に位置していることに注意する
	// このメソッドが呼ばれた時点で、DDh+CBhまではデコードされているからPCの位置はnnを示している。
	// 子メソッドを呼び出す時はこの位置を維持する。子メソッドから戻ったら06hの分のPC++を行う。
	uint8_t vv = m_pMemSys->Read(m_R.PC+1);
	auto pFunc = m_Op.Extended4IY2[vv];
	(this->*pFunc)();
	m_R.PC++;
	return;
}
void CZ80MsxDos::op_POP_IY()
{
	m_R.IY = m_pMemSys->Read(m_R.SP++);
	m_R.IY |= static_cast<uint16_t>(m_pMemSys->Read(m_R.SP++)) << 8;
	return;
}
void CZ80MsxDos::op_EX_memSP_IY()
{
	uint8_t iyl = (m_R.IY >> 0) & 0xFF;
	uint8_t iyh = (m_R.IY >> 8) & 0xFF;
	m_R.IY = m_pMemSys->Read(m_R.SP+0);
	m_R.IY |= static_cast<uint16_t>(m_pMemSys->Read(m_R.SP+1)) < 8;;
	m_pMemSys->Write(m_R.SP+0, iyl);
	m_pMemSys->Write(m_R.SP+1, iyh);
	return;
}
void CZ80MsxDos::op_PUSH_IY()
{
	m_pMemSys->Write(--m_R.SP, (m_R.IY>>8)&0xFF);
	m_pMemSys->Write(--m_R.SP, (m_R.IY>>0)&0xFF);
	return;
}
void CZ80MsxDos::op_JP_memIY()
{
	m_R.PC = m_R.IY;	// IYの値そのものがアドレス値である
	return;
}
void CZ80MsxDos::op_LD_SP_IY()
{
	m_R.SP = m_R.IY;
	return;
}

// extended4 for IY - 2
void CZ80MsxDos::op_RLC_memVpIY()
{
	z80memaddr_t ad = m_R.IY + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	m_R.Rlc8(&v);
	m_pMemSys->Write(ad, v);
	return;
}
void CZ80MsxDos::op_RRC_memVpIY()
{
	z80memaddr_t ad = m_R.IY + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	m_R.Rrc8(&v);
	m_pMemSys->Write(ad, v);
	return;
}
void CZ80MsxDos::op_RL_memVpIY()
{
	z80memaddr_t ad = m_R.IY + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	m_R.Rl8(&v);
	m_pMemSys->Write(ad, v);
	return;
}
void CZ80MsxDos::op_RR_memVpIY()
{
	z80memaddr_t ad = m_R.IY + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	m_R.Rr8(&v);
	m_pMemSys->Write(ad, v);
	return;
}
void CZ80MsxDos::op_SLA_memVpIY()
{
	z80memaddr_t ad = m_R.IY + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	m_R.Sla8(&v);
	m_pMemSys->Write(ad, v);
	return;
}
void CZ80MsxDos::op_SRA_memVpIY()
{
	z80memaddr_t ad = m_R.IY + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	m_R.Sra8(&v);
	m_pMemSys->Write(ad, v);
	return;
}
void CZ80MsxDos::op_SRL_memVpIY()
{
	z80memaddr_t ad = m_R.IY + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	m_R.Srl8(&v);
	m_pMemSys->Write(ad, v);
	return;
}
void CZ80MsxDos::op_BIT_0_memVpIY()
{
	z80memaddr_t ad = m_R.IY + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	m_R.F.Z = (v & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_1_memVpIY()
{
	z80memaddr_t ad = m_R.IY + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	m_R.F.Z = ((v>>1) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_2_memVpIY()
{
	z80memaddr_t ad = m_R.IY + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	m_R.F.Z = ((v>>2) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_3_memVpIY()
{
	z80memaddr_t ad = m_R.IY + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	m_R.F.Z = ((v>>3) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_4_memVpIY()
{
	z80memaddr_t ad = m_R.IY + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	m_R.F.Z = ((v>>4) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_5_memVpIY()
{
	z80memaddr_t ad = m_R.IY + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	m_R.F.Z = ((v>>5) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_6_memVpIY()
{
	z80memaddr_t ad = m_R.IY + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	m_R.F.Z = ((v>>6) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_BIT_7_memVpIY()
{
	z80memaddr_t ad = m_R.IY + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	m_R.F.Z = ((v>>7) & 0x01) ^ 0x01;
	m_R.F.N = 0, m_R.F.H = 1;
	return;
}
void CZ80MsxDos::op_RES_0_memVpIY()
{
	z80memaddr_t ad = m_R.IY + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	v &= ((0x01 << 0) ^ 0xFF);
	m_pMemSys->Write(ad, v);
	return;
}
void CZ80MsxDos::op_RES_1_memVpIY()
{
	z80memaddr_t ad = m_R.IY + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	v &= ((0x01 << 1) ^ 0xFF);
	m_pMemSys->Write(ad, v);
	return;
}
void CZ80MsxDos::op_RES_2_memVpIY()
{
	z80memaddr_t ad = m_R.IY + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	v &= ((0x01 << 2) ^ 0xFF);
	m_pMemSys->Write(ad, v);
	return;
}
void CZ80MsxDos::op_RES_3_memVpIY()
{
	z80memaddr_t ad = m_R.IY + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	v &= ((0x01 << 3) ^ 0xFF);
	m_pMemSys->Write(ad, v);
	return;
}
void CZ80MsxDos::op_RES_4_memVpIY()
{
	z80memaddr_t ad = m_R.IY + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	v &= ((0x01 << 4) ^ 0xFF);
	m_pMemSys->Write(ad, v);
	return;
}
void CZ80MsxDos::op_RES_5_memVpIY()
{
	z80memaddr_t ad = m_R.IY + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	v &= ((0x01 << 5) ^ 0xFF);
	m_pMemSys->Write(ad, v);
	return;
}
void CZ80MsxDos::op_RES_6_memVpIY()
{
	z80memaddr_t ad = m_R.IY + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	v &= ((0x01 << 6) ^ 0xFF);
	m_pMemSys->Write(ad, v);
	return;
}
void CZ80MsxDos::op_RES_7_memVpIY()
{
	z80memaddr_t ad = m_R.IY + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	v &= ((0x01 << 7) ^ 0xFF);
	m_pMemSys->Write(ad, v);
	return;
}
void CZ80MsxDos::op_SET_0_memVpIY()
{
	z80memaddr_t ad = m_R.IY + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	v |= 0x01 << 0;
	m_pMemSys->Write(ad, v);
	return;
}
void CZ80MsxDos::op_SET_1_memVpIY()
{
	z80memaddr_t ad = m_R.IY + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	v |= 0x01 << 1;
	m_pMemSys->Write(ad, v);
	return;
}
void CZ80MsxDos::op_SET_2_memVpIY()
{
	z80memaddr_t ad = m_R.IY + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	v |= 0x01 << 2;
	m_pMemSys->Write(ad, v);
	return;
}
void CZ80MsxDos::op_SET_3_memVpIY()
{
	z80memaddr_t ad = m_R.IY + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	v |= 0x01 << 3;
	m_pMemSys->Write(ad, v);
	return;
}
void CZ80MsxDos::op_SET_4_memVpIY()
{
	z80memaddr_t ad = m_R.IY + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	v |= 0x01 << 4;
	m_pMemSys->Write(ad, v);
	return;
}
void CZ80MsxDos::op_SET_5_memVpIY()
{
	z80memaddr_t ad = m_R.IY + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	v |= 0x01 << 5;
	m_pMemSys->Write(ad, v);
	return;
}
void CZ80MsxDos::op_SET_6_memVpIY()
{
	z80memaddr_t ad = m_R.IY + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	v |= 0x01 << 6;
	m_pMemSys->Write(ad, v);
	return;
}
void CZ80MsxDos::op_SET_7_memVpIY()
{
	z80memaddr_t ad = m_R.IY + m_pMemSys->Read(m_R.PC++);
	uint8_t v = m_pMemSys->Read(ad);
	v |= 0x01 << 7;
	m_pMemSys->Write(ad, v);
	return;
}


