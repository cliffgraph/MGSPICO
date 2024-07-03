set(BinName "mgspico_2")
set(Srcd "../mgspico")

add_compile_definitions(MGS_MUSE_MACHINA)

add_executable(
	${BinName}
	${Srcd}/global.cpp
	${Srcd}/mgspico.cpp
	${Srcd}/timerobj.cpp
	${Srcd}/sdfat.cpp
	${Srcd}/MgsFiles.cpp
	${Srcd}/CUTimeCount.cpp
	${Srcd}/t_mgspico.cpp
	${Srcd}/ff/ff.c
	${Srcd}/ff/ffsystem.c
	${Srcd}/ff/ffunicode.c
	${Srcd}/ff/mmc_pico_spi.c
	${Srcd}/mgs/mgs_tools.cpp
	${Srcd}/oled/oledssd1306.cpp
	${Srcd}/HopStepZ/CHopStepZ.cpp
	${Srcd}/HopStepZ/CMsxIoSystem.cpp
	${Srcd}/HopStepZ/CMsxMemSlotSystem.cpp
	${Srcd}/HopStepZ/CMsxMusic.cpp
	${Srcd}/HopStepZ/CMsxVoidMemory.cpp
	${Srcd}/HopStepZ/CMsxDummyMain.cpp
	${Srcd}/HopStepZ/CPhysicalSlotDevice.cpp
	${Srcd}/HopStepZ/CRam64k.cpp
	${Srcd}/HopStepZ/CScc.cpp
	${Srcd}/HopStepZ/CZ80MsxDos.cpp
	${Srcd}/tgf/CTgfPlayer.cpp
	${Srcd}/CReadFileStream.cpp
	${Srcd}/MgspicoSettings.cpp
	${Srcd}/vgm/CVgmPlayer.cpp
	${Srcd}/t_si5351.cpp
	)

include_directories(
	${Srcd}/ff
	${Srcd}/HopStepZ
	${Srcd}/HopStepZ/muse
	${Srcd}/HopStepZ/tools
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
#	hardware_pio
	 hardware_spi hardware_i2c pico_multicore hardware_flash)

# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(${BinName})

# add url via pico_set_program_url
#example_auto_set_url(${BinName})

