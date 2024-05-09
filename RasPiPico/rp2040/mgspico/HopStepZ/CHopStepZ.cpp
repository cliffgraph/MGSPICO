#include "../def_gpio.h"
#include "../CUTimeCount.h"
#include "CZ80MsxDos.h"
#include "CMsxMemSlotSystem.h"
#include "CMsxIoSystem.h"
#include "CRam64k.h"
#include "CMsxMusic.h"
//#include "CScc.h"
#include "CPhysicalSlotDevice.h"
#include "CMsxDummyMain.h"
#include "CHopStepZ.h"

CHopStepZ::CHopStepZ()
{
	m_pSlot = nullptr;
	m_pIo = nullptr;
	m_pCpu = nullptr;
	m_pRam64 = nullptr;
	m_pFm = nullptr;
	m_pPhy = nullptr;
//	m_pScc = nullptr;
	m_pDumyMain = nullptr;
	return;
}
CHopStepZ::~CHopStepZ()
{
	NULL_DELETE(m_pCpu);
	NULL_DELETE(m_pRam64);
	NULL_DELETE(m_pFm);
	NULL_DELETE(m_pPhy);
	NULL_DELETE(m_pSlot);
	NULL_DELETE(m_pIo);
//	NULL_DELETE(m_pScc);
	NULL_DELETE(m_pDumyMain);
	return;
}

void CHopStepZ::Setup(const bool bForceOpll, const bool bKINROU5)
{
	m_pSlot = GCC_NEW CMsxMemSlotSystem();
	m_pIo = GCC_NEW CMsxIoSystem();
	m_pIo->JoinObject(m_pIo);
	m_pIo->JoinObject(m_pSlot);
	// slot#0 : ---
	m_pDumyMain = GCC_NEW CMsxDummyMain();
	m_pSlot->JoinObject(0, m_pDumyMain);
	// slot#1 : PhysicalSlot
	auto pPhySD = GCC_NEW CPhysicalSlotDevice();
	pPhySD->Setup();
	m_pPhy = pPhySD;
	m_pSlot->JoinObject(1, m_pPhy);
	m_pIo->JoinObject(m_pPhy);
	// m_pScc = GCC_NEW CScc();
	// m_pSlot->JoinObject(1, m_pScc);
	// slot#2 : fm-bios
	if( bForceOpll )
	{
		m_pFm = GCC_NEW CMsxMusic();
		m_pSlot->JoinObject(2, m_pFm);
		m_pIo->JoinObject(m_pFm);
	}
	// slot#3 : RAM
	m_pRam64 = GCC_NEW CRam64k(0xc9);
	m_pSlot->JoinObject(3, m_pRam64);
	m_pIo->JoinObject(m_pRam64);

	// CPU&DOS
	m_pCpu = GCC_NEW CZ80MsxDos();
	m_pCpu->SetSubSystem(m_pSlot, m_pIo);	

	// メモリセットアップ
	for( int t = 0; t < 0xf0; ++t)
		m_pSlot->Write(t, 0x00);
	// ページnのRAMのスロットアドレス
	m_pSlot->Write(0xF341, 0x83);	// Page.0
	m_pSlot->Write(0xF342, 0x83);	// Page.1
	m_pSlot->Write(0xF343, 0x83);	// Page.2
	m_pSlot->Write(0xF344, 0x83);	// Page.3
	// 拡張スロットの状態
	m_pSlot->Write(0xFCC1, 0x00);	// SLOT.0 + MAIN-ROMのスロットアドレス
	m_pSlot->Write(0xFCC2, 0x80);	// SLOT.1
	m_pSlot->Write(0xFCC3, 0x00);	// SLOT.2
	m_pSlot->Write(0xFCC4, 0x00);	// SLOT.3
	m_pSlot->Write(0xFFF7, m_pSlot->Read(0xFCC1));	// 0xFCC1と同一の内容を書く
	// FDCの作業領域らしいがMGSDRVがここ読み込んで何か書き換えを行っていたので、
	// DOS起動直後の値を再現しておく
	m_pSlot->Write(0xF349, 0xbb);
	m_pSlot->Write(0xF34A, 0xe7);
	//
	m_pSlot->Write(0xFF3E, 0xc9);	// H.NEWS
	m_pSlot->Write(0xFD9F, 0xc9);	// H.TIMI
	return;
}

void CHopStepZ::GetSubSystems(CMsxMemSlotSystem **pSlot, CZ80MsxDos **pCpu)
{
	*pSlot = m_pSlot;
	*pCpu = m_pCpu;
	return;
}

uint8_t CHopStepZ::ReadMemory(const z80memaddr_t addr) const
{
	return m_pSlot->Read(addr);
}

uint16_t CHopStepZ::ReadMemoryWord(const z80memaddr_t addr) const
{
	return m_pSlot->ReadWord(addr);
}

void CHopStepZ::WriteMemory(const z80memaddr_t addr, const uint8_t b)
{
	m_pSlot->Write(addr, b);
	return;
}

void CHopStepZ::WriteMemory(const z80memaddr_t addr, const uint8_t *pSrc, const uint32_t bsize)
{
	m_pSlot->BinaryTo(addr, pSrc, bsize);
	return;
}

const CZ80Regs &CHopStepZ::GetCurrentRegs() const
{
	return m_pCpu->GetCurrentRegs();
}

void CHopStepZ::Run(const z80memaddr_t startAddr, const z80memaddr_t stackAddr, bool *pStop)
{
	m_pSlot->Write(0x0006, (stackAddr>>0)&0xff);
	m_pSlot->Write(0x0007, (stackAddr>>8)&0xff);
	m_pCpu->Push16(0x0000);
	m_pCpu->ResetCpu(startAddr, stackAddr);

	while( m_pCpu->GetPC() != 0 && (pStop==nullptr||!*pStop)) {
	 	m_pCpu->Execution();
	}
	return;
}

void CHopStepZ::RunStage1(const z80memaddr_t startAddr, const z80memaddr_t stackAddr)
{
	m_pSlot->Write(0x0006, (stackAddr>>0)&0xff);
	m_pSlot->Write(0x0007, (stackAddr>>8)&0xff);
	m_pCpu->Push16(0x0000);
	m_pCpu->ResetCpu(startAddr, stackAddr);
	return;
}

RAM_FUNC bool CHopStepZ::RunStage2()
{
	if( m_pCpu->GetPC() == 0 )
		return false;
 	m_pCpu->Execution();
	return true;
}


