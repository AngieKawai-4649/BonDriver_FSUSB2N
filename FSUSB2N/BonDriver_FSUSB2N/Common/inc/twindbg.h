#pragma once

#ifdef _DEBUG
#include <stdio.h>
#define DBG_INFO(...) do{ char d_buff[128]; \
	_snprintf_s(d_buff,128, __VA_ARGS__); \
	::OutputDebugStringA(d_buff);}while(0)
#define TRACE(...) do{ wchar_t d_buff[128]; \
	_snwprintf_s(d_buff,128, __VA_ARGS__); \
	::OutputDebugStringW(d_buff);}while(0)
#else
#define DBG_INFO(...) void()
#define TRACE(...) void()
#endif

#define DBGOUT DBG_INFO
