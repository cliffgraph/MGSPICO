set(BinName "mgspico")

add_compile_definitions(MGSPICO_1ST)

add_executable(
	${BinName}
	global.cpp
	mgspico.cpp
	timerobj.cpp
	sdfat.cpp
	MgsFiles.cpp
	CUTimeCount.cpp
	t_mgspico.cpp
	t_mmmspi.cpp
	ff/ff.c
	ff/ffsystem.c
	ff/ffunicode.c
	ff/mmc_pico_spi.c
	mgs/mgs_tools.cpp
	oled/oledssd1306.cpp
	HopStepZ/CHopStepZ.cpp
	HopStepZ/CMsxIoSystem.cpp
	HopStepZ/CMsxMemSlotSystem.cpp
	HopStepZ/CMsxMusic.cpp
	HopStepZ/CMsxVoidMemory.cpp
	HopStepZ/CMsxDummyMain.cpp
	HopStepZ/CPhysicalSlotDevice.cpp
	HopStepZ/CRam64k.cpp
	HopStepZ/CScc.cpp
	HopStepZ/CZ80MsxDos.cpp
	tgf/CTgfPlayer.cpp
	CReadFileStream.cpp
	MgspicoSettings.cpp
	vgm/CVgmPlayer.cpp
	../playlib/playlib.cpp
	)

include_directories(
	ff
	HopStepZ
	HopStepZ/muse
	HopStepZ/tools
	../playlib
	)

target_compile_options(
	${BinName}
	PUBLIC -O2 -Wall)

# pico_generate_pio_header(
# 	${BinName}
# 	${CMAKE_CURRENT_LIST_DIR}/mgspico.pio
# )

## enable usb output, disable uart output
pico_enable_stdio_usb(${BinName} 1)
pico_enable_stdio_uart(${BinName} 0)

target_link_libraries(
	${BinName}
	PRIVATE
	pico_stdlib
	playlib
#	hardware_pio
	 hardware_spi hardware_i2c pico_multicore hardware_flash)

# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(${BinName})

# add url via pico_set_program_url
#example_auto_set_url(${BinName})

