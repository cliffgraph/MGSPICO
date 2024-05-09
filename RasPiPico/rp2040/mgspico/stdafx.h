#pragma once
#define GCC_NEW new
#define NULL_DELETE(p) 		{if(p!=nullptr){delete (p);(p)=nullptr;}}
#define NULL_DELETEARRAY(p) {if(p!=nullptr){delete[] (p);(p)=nullptr;}}
#ifdef NDEBUG
	#define DEBUG_BREAK		((void)0)
#else
	#ifdef _WIN32
		#define DEBUG_BREAK		DebugBreak()
	#elif __linux
		#define DEBUG_BREAK		assert(false)
	#else
		#define DEBUG_BREAK		((void)0)
	#endif
#endif

// __not_in_flash_func()ではIntelliSenseが働かないので、RAM_FUNCを使用する
#define RAM_FUNC __attribute__((noinline)) __attribute__((section(".time_critical")))
