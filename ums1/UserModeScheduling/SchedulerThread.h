#pragma once

class Thread {
public:
	HANDLE threadHandle{nullptr};
	Thread() {
	}
	BOOL create(LPVOID lpParameter, LPTHREAD_START_ROUTINE lpStartAddress) {
		macroDebugLastError(threadHandle = CreateThread(nullptr, 0, static_cast<LPTHREAD_START_ROUTINE>(lpStartAddress), (LPVOID)lpParameter, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr));
		return 1;
	}

	DWORD wait(DWORD dwMilliseconds=INFINITE) {
		if(!threadHandle) {
			return 1;
		}
	//return macroDebugLastError(WaitForSingleObject(threadHandle, INFINITE));
		return WaitForSingleObject(threadHandle, INFINITE);
	}

	BOOL terminate(DWORD exitCode) {
		auto ret = TerminateThread(threadHandle, exitCode); threadHandle = nullptr;
		return ret;
	}

	static VOID NTAPI nullAPC(ULONG_PTR Parameter) {
	}

	DWORD sendAPC(PAPCFUNC pfnAPC = nullAPC, ULONG_PTR dwData = 0) {
		auto ret = QueueUserAPC(pfnAPC, threadHandle, dwData);
		SetLastError(ERROR_SUCCESS);
		return ret;
	}

	~Thread() {
		wait();
	//wprintf(L"~SchedulerThread()\n");
		if(threadHandle) {
			macroDebugLastError(CloseHandle(threadHandle));
		}
	}
};