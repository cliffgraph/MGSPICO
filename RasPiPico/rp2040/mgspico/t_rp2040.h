#pragma once

// 1us wait.
inline void t_wait1us()
{
	// no good
	// sleep_us(1), busy_wait_us(1)
	// no good
	// { uint32_t t = time_us_32(); while(t == time_us_32()); }

	// 125MHz動作時、下記のコードでbusy_wait_usを使用するより安定した1usのウェイトを生成できた
	// ループ回数は実機で動作させ実測して決定した
	// この方法の情報源：https://forums.raspberrypi.com/viewtopic.php?t=304922
	for(int t = 0; t < 25; t++) {
	    __asm volatile ("nop":);
	}
	return;
}

inline void t_wait700ns()
{
	for(int t = 0; t < 16; t++) {
	    __asm volatile ("nop":);
	}
	return;
}

inline void t_wait100ns()
{
	for(int t = 0; t < 7; t++) {
	    __asm volatile ("nop":);
	}
	return;
}
