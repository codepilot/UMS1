// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

/*

path types

c:\something
\\?\c:\something
\\.\c:\something
\??\c:\something

*/

#include <Winsock2.h>
#include <ws2tcpip.h>
#include <Mswsock.h>
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Mswsock.lib")
#pragma comment(lib, "Synchronization.lib")
#include <Windows.h>
//#include <Winternl.h>
#include <cstdio>
#include <tchar.h>
#include <array>
#include <vector>
#include <functional>

//#define PROCESSOR_USE_THREAD_RING
#define PROCESSOR_USE_ATOMIC_LIST

#ifdef _DEBUG
#define ods(txt) {OutputDebugString(TEXT(txt));}
#else
#define ods(txt)
#endif

#ifdef ods
#undef ods
#define ods(txt)
#endif //ods

extern std::array<std::wstring, 3> reasons;

#ifdef _DEBUG
template<typename T>
T DebugLastError(const wchar_t *area, T &&success) {
	auto lastError = GetLastError();
	if(success && lastError == ERROR_SUCCESS) { return success; }
	wprintf(L"%s: %p lastError dec:%d hex:0x%x\n", area, (LPVOID)success, lastError, lastError);
	DebugBreak();
	return success;
}

#define macroDebugLastError(cmdLine) DebugLastError(L#cmdLine, (cmdLine));
#else
#define macroDebugLastError(cmdLine) (cmdLine);
#endif

#ifdef _DEBUG
template<typename T>
T DebugLastErrorExpected(DWORD expected, const wchar_t *area, T &&success) {
	auto lastError = GetLastError();
	if(!success && lastError == expected) {
		SetLastError(ERROR_SUCCESS);
		return success;
	}
	wprintf(L"%s: %p lastError dec:%d hex:0x%x\n", area, (LPVOID)success, lastError, lastError);
	DebugBreak();
	return success;
}
#define macroDebugLastErrorExpected(expectedError, cmdLine) DebugLastErrorExpected(expectedError, L#cmdLine, (cmdLine));
#else
#define macroDebugLastErrorExpected(expectedError, cmdLine) (cmdLine);
#endif

#include "NewThreadPoolApi/NewThreadPoolApi.h"
#include "OldThreadPoolApi/OldThreadPoolApi.h"
#include "DirectThreadPool.h"
#include "UserModeScheduling/UserModeScheduling.h"

// TODO: reference additional headers your program requires here

#define makeCompletionPort() CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 0)
#define addCompletionHandle(cp, fh, key) CreateIoCompletionPort(fh, cp, key, 0)

extern char *status200HelloWorld;
extern LPFN_DISCONNECTEX DisconnectEx;
extern SOCKET sListen;

#define numSchedulers 12
#define numThreadsPerScheduler 4
#define numSockets 256

#define numDirectThreads 6

extern DECLSPEC_NOINLINE void fs_stat();
enum class CPKey: ULONG_PTR { NO_KEY = 0, UMS_CP_QUIT, UMS_CP_FS_STAT };

__declspec(align(64)) extern __int64 volatile dequeuedRequestCount;
__declspec(align(64)) extern unsigned __int64 volatile firstRequestStarted;
__declspec(align(64)) extern unsigned __int64 volatile lastRequestFinished;
__declspec(align(64)) extern unsigned __int64 volatile WaitVariable;
__declspec(align(64)) extern unsigned __int64 CompareVariable;
__declspec(align(64)) extern unsigned __int64 UndesiredValue;


#define numBenchmarkRepetitions 4
#define numProcessors 6
#define numLogicalCoresPerProcessor 2
#define numThreadsPerProcessor 1
//#define numRequests 1
#define numRequests 100000

//#define USE_TEST_WEBSERVER


#define USE_USER_MODE_SCHEDULING
#define USE_NEW_THREAD_POOL
//#define USE_OLD_THREAD_POOL
#define USE_DIRECT_THREAD_POOL
#define USE_SYNCHRONOUS

#if 0
#if \
	(defined(USE_USER_MODE_SCHEDULING) && defined(USE_NEW_THREAD_POOL)) || \
	(defined(USE_USER_MODE_SCHEDULING) && defined(USE_OLD_THREAD_POOL)) || \
	(defined(USE_USER_MODE_SCHEDULING) && defined(USE_DIRECT_THREAD_POOL)) || \
	(defined(USE_NEW_THREAD_POOL) && defined(USE_OLD_THREAD_POOL)) || \
	(defined(USE_NEW_THREAD_POOL) && defined(USE_DIRECT_THREAD_POOL)) || \
	(defined(USE_OLD_THREAD_POOL) && defined(USE_DIRECT_THREAD_POOL)) || \
	( \
		(!defined(USE_USER_MODE_SCHEDULING)) && \
		(!defined(USE_NEW_THREAD_POOL)) && \
		(!defined(USE_OLD_THREAD_POOL)) && \
		(!defined(USE_DIRECT_THREAD_POOL)))
#error "Exactly one of USE_USER_MODE_SCHEDULING, USE_NEW_THREAD_POOL, USE_OLD_THREAD_POOL, USE_DIRECT_THREAD_POOL must be defined"
#endif
#endif

#ifdef USE_USER_MODE_SCHEDULING
//#ifdef USE_TEST_WEBSERVER
bool test_user_mode_scheduling();
//#endif
bool benchmark_ums_fs_stat();
#endif//USE_USER_MODE_SCHEDULING

#ifdef USE_NEW_THREAD_POOL
#ifdef USE_TEST_WEBSERVER
bool test_new_thread_pool();
#endif
bool benchmark_ntp_workQueue_fs_stat();
bool benchmark_ntp_trySubmit_fs_stat();
#endif//USE_NEW_THREAD_POOL

#ifdef USE_OLD_THREAD_POOL
#ifdef USE_TEST_WEBSERVER
bool test_old_thread_pool();
#endif
bool benchmark_otp_fs_stat();
#endif//USE_OLD_THREAD_POOL

#ifdef USE_DIRECT_THREAD_POOL
#ifdef USE_TEST_WEBSERVER
bool test_direct_thread_pool();
#endif
bool benchmark_dtp_fs_stat();
#endif//USE_DIRECT_THREAD_POOL

#ifdef USE_SYNCHRONOUS
bool benchmark_sync_fs_stat();
#endif