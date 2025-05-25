set(BinName "playlib")

set(Z80Asm AILZ80ASM.exe)
set(Z80AsmOpt -bin -lst -lm simple -ts 4 -f)

add_custom_command(
	OUTPUT player.bin
	DEPENDS player.z80
	COMMAND cd ${CMAKE_CURRENT_SOURCE_DIR}
	COMMAND ${Z80Asm} -i player.z80 -o ${CMAKE_CURRENT_BINARY_DIR}/player.bin ${Z80AsmOpt}
)

set(ObjCpy arm-none-eabi-objcopy.exe)
set(ObjCpyOpt -v -I binary -O elf32-littlearm --rename-section .data=.rodata --set-section-flags .rodata=alloc,load,readonly )

add_custom_command(
	OUTPUT player.o
	DEPENDS player.bin
	COMMAND ${ObjCpy} ${ObjCpyOpt} player.bin ${CMAKE_CURRENT_BINARY_DIR}/player.o
)

add_library(
	${BinName}
	STATIC
	playlib.cpp
	player.o
)

target_include_directories(
	${BinName}
	PUBLIC
	${PROJECT_SOURCE_DIR}/playlib
)
