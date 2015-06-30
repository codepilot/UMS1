#pragma once

class WorkerThread {
public:
	Scheduler *scheduler{nullptr};
	HANDLE threadHandle{nullptr};
	PUMS_CONTEXT tContext{nullptr};

	void attachToProcessor(Processor *_proc, LPTHREAD_START_ROUTINE lpStartAddress, LPVOID lpParameter) {
		macroDebugLastError(CreateUmsThreadContext(&tContext));
		ThreadAttributeList threadAttributeList{1};
	//threadAttributeList.updateAttrib(0, UMS_CREATE_THREAD_ATTRIBUTES{UMS_VERSION, tContext, _scheduler->completionList.UmsCompletionList});
		threadAttributeList.updateAttrib(0, UMS_CREATE_THREAD_ATTRIBUTES{UMS_VERSION, tContext, _proc->completionList.UmsCompletionList});
		HANDLE rt1 = macroDebugLastError(CreateRemoteThreadEx(GetCurrentProcess(), NULL, 0, lpStartAddress, lpParameter, 0, threadAttributeList, NULL));
		if(!rt1){
			ods("CreateRemoteThreadEx Failure\n");
			DebugBreak();
			return;
		}else{
			macroDebugLastError(CloseHandle(rt1));
		}
	}

	~WorkerThread() {
		if(!scheduler) { return ; }
		wprintf(L"~WorkerThread(%p) start\n", this);
		wprintf(L"~WorkerThread(%p) finish\n", this);
	}
};
