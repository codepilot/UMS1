#include "stdafx.h"

std::array<std::wstring, 3> reasons{{L"UmsSchedulerStartup", L"UmsSchedulerThreadBlocked", L"UmsSchedulerThreadYield"}};

#ifdef USE_USER_MODE_SCHEDULING
HANDLE listEvents[numSchedulers];
PUMS_COMPLETION_LIST completionList[numSchedulers];
UMS_SCHEDULER_STARTUP_INFO SchedulerStartupInfo[numSchedulers];
DWORD WINAPI UmsWorkerThreadProc(LPVOID lpParameter);
VOID NTAPI UmsSchedulerProc(UMS_SCHEDULER_REASON Reason, ULONG_PTR ActivationPayload, PVOID SchedulerParam);
HANDLE completionPort=NULL;
DWORD dwTlsID;

typedef enum _COMPLETION_KEY {
	CKACCEPT = 0,
	CKRECV,
	CKSEND
} COMPLETION_KEY, *PCOMPLETION_KEY;

typedef enum _OVERLAPPED_ACCEPT_STATE {
	OAS_RECV = 0,
	OAS_SEND,
	OAS_CLOSE
} OVERLAPPED_ACCEPT_STATE, *POVERLAPPED_ACCEPT_STATE;

typedef struct _OVERLAPPED_ACCEPT {
	OVERLAPPED overlapped;
	SOCKET s;
	OVERLAPPED_ACCEPT_STATE oas;
	DWORD bufSize;
    LPVOID acceptBuf;
} OVERLAPPED_ACCEPT, *LPOVERLAPPED_ACCEPT;

DWORD WINAPI UmsWorkerThreadProc(LPVOID lpParameter) {
	ods("*****UmsWorkerThreadProcStart\n");
	for(;;){
		ods("waiting start\n");
		BOOL success;
		const int maxEntries=1;
		OVERLAPPED_ENTRY oe[maxEntries];
		ULONG numRemoved=0;
		success=GetQueuedCompletionStatusEx(completionPort, oe, maxEntries, &numRemoved, INFINITE, false);
		if(!success){
			DebugBreak();
		}
		for(ULONG i=0; i<numRemoved; i++){
			if(CKACCEPT==oe[i].lpCompletionKey){
				ods("CKACCEPT\n");
				LPOVERLAPPED_ACCEPT ola=(LPOVERLAPPED_ACCEPT)oe[i].lpOverlapped;
				DWORD numRead=0;
				SecureZeroMemory(oe[i].lpOverlapped, sizeof(OVERLAPPED));
				ola->oas=OAS_RECV;
				success=ReadFile((HANDLE)ola->s, ola->acceptBuf, ola->bufSize, &numRead, oe[i].lpOverlapped);
				if(!success && GetLastError()!=ERROR_IO_PENDING){
					int wsaError=WSAGetLastError();
					DebugBreak();
				}
			}else if(CKRECV==oe[i].lpCompletionKey){
				LPOVERLAPPED_ACCEPT ola=(LPOVERLAPPED_ACCEPT)oe[i].lpOverlapped;
				if(OAS_RECV==ola->oas){
					ods("OAS_RECV\n");
					DWORD numWritten=0;
					SecureZeroMemory(oe[i].lpOverlapped, sizeof(OVERLAPPED));
					DWORD sizeofSend=sizeof(status200HelloWorld)-1;
					CopyMemory(ola->acceptBuf, status200HelloWorld, sizeofSend);
					ola->oas=OAS_SEND;
					success=WriteFile((HANDLE)ola->s, ola->acceptBuf, sizeofSend, &numWritten, oe[i].lpOverlapped);
					if(!success && GetLastError()!=ERROR_IO_PENDING){
						int wsaError=WSAGetLastError();
						DebugBreak();
					}
				}else if(OAS_SEND==ola->oas){
					ods("OAS_SEND\n");
					SecureZeroMemory(oe[i].lpOverlapped, sizeof(OVERLAPPED));
					ola->oas=OAS_CLOSE;
					success=DisconnectEx(ola->s, oe[i].lpOverlapped, TF_REUSE_SOCKET, 0);
					if(!success && GetLastError()!=ERROR_IO_PENDING){
						int wsaError=WSAGetLastError();
						DebugBreak();
					}
				}else if(OAS_CLOSE==ola->oas){
					ods("OAS_CLOSE\n");
					DWORD acceptBufSize=max(0, ola->bufSize-sizeof(sockaddr_in)-16-sizeof(sockaddr_in)-16);
					acceptBufSize=0;
					success=AcceptEx(sListen, ola->s, ola->acceptBuf, acceptBufSize, sizeof(sockaddr_in)+16, sizeof(sockaddr_in)+16, 0, &ola->overlapped);
					if(!success && WSAGetLastError()!=ERROR_IO_PENDING){
						DebugBreak();
					}
				}else{
					DebugBreak();
				}
			}else{
				DebugBreak();
			}
		}
		ods("waiting end\n");
	}
	ods("*****UmsWorkerThreadProcEnd\n");
	return 0;
}

typedef struct _THREAD_LIST_ITEM {
	_THREAD_LIST_ITEM *next;
	PUMS_CONTEXT thread;
} THREAD_LIST_ITEM, *PTHREAD_LIST_ITEM;

PTHREAD_LIST_ITEM threadList[numSchedulers]={0};
UINT numThreadsAlive[numSchedulers]={0};
UINT numThreadsInList[numSchedulers]={0};

void addThreadToList(DWORD n, PUMS_CONTEXT thread){
	PTHREAD_LIST_ITEM threadListHead=(PTHREAD_LIST_ITEM)HeapAlloc(GetProcessHeap(), HEAP_GENERATE_EXCEPTIONS|HEAP_ZERO_MEMORY, sizeof(THREAD_LIST_ITEM));
	threadListHead->next=threadList[n];
	threadListHead->thread=thread;
	threadList[n]=threadListHead;
	numThreadsInList[n]++;
}

PUMS_CONTEXT removeThreadFromList(DWORD n){
	PTHREAD_LIST_ITEM threadListHead=threadList[n];
	PUMS_CONTEXT nThread=threadListHead->thread;
	threadList[n]=threadListHead->next;
	HeapFree(GetProcessHeap(), 0, threadListHead);
	numThreadsInList[n]--;
	if(!threadList[n]){
		ods("!threadList\n");
	}
	return nThread;
}

VOID NTAPI UmsSchedulerProc(UMS_SCHEDULER_REASON Reason, ULONG_PTR ActivationPayload, PVOID SchedulerParam){
	DWORD id=(DWORD)TlsGetValue(dwTlsID);
	//ods("UmsSchedulerProc\n"));
	if(UmsSchedulerStartup==Reason){
		ods("UmsSchedulerStartup\n");
	}else if(UmsSchedulerThreadBlocked==Reason){
		ods("UmsSchedulerThreadBlocked\n");
		if(ActivationPayload&1){
			ods("blocked by system call\n");
		}else{
			ods("blocked by trap(ex. hard page fault), interrupt(ex. apc)\n");
		}
	}else if(UmsSchedulerThreadYield==Reason){
		ods("UmsSchedulerThreadYield\n");
		addThreadToList(id, (PUMS_CONTEXT)ActivationPayload);
		//	ActivationPayload=UMS Thread Context yielded
		//	SchedulerParam=UmsThreadYield.param
	}
	BOOL success;
	for(;;){
		if(!numThreadsAlive[id]){
			break;
			return;
		}
		PUMS_CONTEXT UmsThreadList=0;
		if(!numThreadsInList[id]){
			DWORD waitSTS=WaitForMultipleObjects(numSchedulers, listEvents, false, INFINITE);
			if(waitSTS==WAIT_FAILED){
				DebugBreak();
			}
			DWORD n=waitSTS-WAIT_OBJECT_0;
			ods("WaitingForDequeue\n");
			success=DequeueUmsCompletionListItems(completionList[n], INFINITE, &UmsThreadList);
			if(UmsThreadList){
				//ods("DequeueUmsCompletionListItems Success\n"));
				for(;;){
					addThreadToList(id, UmsThreadList);

					ods("GetNextUmsListItem\n");
					UmsThreadList=GetNextUmsListItem(UmsThreadList);
					if(!UmsThreadList){break;}
				}
			}else if(success){
			}else{
				ods("DequeueUmsCompletionListItems Fail\n");
			}
		}
		if(threadList[id]){
			while(threadList[id]){
				PUMS_CONTEXT nThread=removeThreadFromList(id);
				ULONG retLen=0;
				BOOLEAN isSuspended=0, isTerminated=0;
				QueryUmsThreadInformation(nThread, UmsThreadIsSuspended, &isSuspended, sizeof(BOOLEAN), &retLen);
				if(isSuspended){
					ods("isSuspended\n");
				}
				QueryUmsThreadInformation(nThread, UmsThreadIsTerminated, &isTerminated, sizeof(BOOLEAN), &retLen);
				if(isTerminated){
					ods("isTerminated\n");
				}
				if(isTerminated){
					success=DeleteUmsThreadContext(nThread);
					if(!success){
						DebugBreak();
					}
					numThreadsAlive[id]--;
					if(!numThreadsAlive[id]){
						ods("!numThreadsAlive\n");
					}
				}else{
					for(;;){
						ods("ExecuteUmsThread\n");
						success=ExecuteUmsThread(nThread);
						if(!success){
							ods("ExecuteUmsThread Fail\n");
						}else{
							ods("ExecuteUmsThread Success\n");
						}
					}
				}
			}
		}else{
			ods("threadList==NULL\n");
		}
	}
	ods("UmsSchedulerProc returning\n");
	return;
}

void addThread(UINT n, LPTHREAD_START_ROUTINE lpStartAddress, LPVOID lpParameter){
	BOOL success;
	SIZE_T lpSize=0;
	PUMS_CONTEXT tContext;
	success=CreateUmsThreadContext(&tContext);
	if(success){SetLastError(0);}
	LPPROC_THREAD_ATTRIBUTE_LIST lpAttributeList=NULL;
	success=InitializeProcThreadAttributeList(lpAttributeList, 1, 0, &lpSize);
	lpAttributeList=(LPPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(GetProcessHeap(), HEAP_GENERATE_EXCEPTIONS|HEAP_ZERO_MEMORY, lpSize);
	success=InitializeProcThreadAttributeList(lpAttributeList, 1, 0, &lpSize);
	if(success){SetLastError(0);}
	UMS_CREATE_THREAD_ATTRIBUTES threadAttributes;
	threadAttributes.UmsVersion=UMS_VERSION;
	threadAttributes.UmsContext=tContext;
	threadAttributes.UmsCompletionList=completionList[n];
	success=UpdateProcThreadAttribute(lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_UMS_THREAD, &threadAttributes, sizeof(threadAttributes), NULL, NULL);
	if(success){SetLastError(0);}


	HANDLE rt1=CreateRemoteThreadEx(GetCurrentProcess(), NULL, 0, &UmsWorkerThreadProc, lpParameter, STACK_SIZE_PARAM_IS_A_RESERVATION, lpAttributeList, NULL);

	DeleteProcThreadAttributeList(lpAttributeList);
	HeapFree(GetProcessHeap(), 0, lpAttributeList); lpAttributeList=NULL;

	if(!rt1){
		ods("CreateRemoteThreadEx Failure\n");
		DebugBreak();
		return;
	}else{
		//ods("CreateRemoteThreadEx Success\n"));
		success=CloseHandle(rt1);
		if(success){SetLastError(0);}
	}
}

DWORD WINAPI SchedulerProc(LPVOID lpParameter){
	BOOL success;
	TlsSetValue(dwTlsID, lpParameter);
	success=EnterUmsSchedulingMode(&SchedulerStartupInfo[(UINT)lpParameter]);
	if(!success){
		DebugBreak();
	}else{
		SetLastError(0);
	}
	success=DeleteUmsCompletionList(completionList[(UINT)lpParameter]);
	if(!success){
		DebugBreak();
	}else{
		SetLastError(0);
	}
	return 0;
}

void umsAddAcceptSocket(SOCKET sListen) {
	BOOL success;
	SOCKET sAccept=WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
	if(INVALID_SOCKET==sAccept){
		DebugBreak();
	}
	if(completionPort!=addCompletionHandle(completionPort, (HANDLE)sAccept, CKRECV)){
		DebugBreak();
	};
	DWORD nBufSize=max(4096, (sizeof(sockaddr_in)+16)*2);
	LPOVERLAPPED_ACCEPT ola=(LPOVERLAPPED_ACCEPT)HeapAlloc(GetProcessHeap(), HEAP_GENERATE_EXCEPTIONS|HEAP_ZERO_MEMORY, sizeof(OVERLAPPED_ACCEPT)+nBufSize);
	ola->acceptBuf=(UINT8*)ola+sizeof(OVERLAPPED_ACCEPT);
	ola->s=sAccept;
	ola->bufSize=nBufSize;
	DWORD acceptBufSize=max(0, ola->bufSize-sizeof(sockaddr_in)-16-sizeof(sockaddr_in)-16);
	acceptBufSize=0;
	success=AcceptEx(sListen, ola->s, ola->acceptBuf, acceptBufSize, sizeof(sockaddr_in)+16, sizeof(sockaddr_in)+16, 0, &ola->overlapped);
	if(!success && WSAGetLastError()!=ERROR_IO_PENDING){
		DebugBreak();
	}
}

//#ifdef USE_TEST_WEBSERVER
bool test_user_mode_scheduling() {//success is true, failure is false
	int sockErr;

	BOOL success;
	dwTlsID=TlsAlloc();
	completionPort=makeCompletionPort();

	WSADATA lpWSAData={0};
	sockErr=WSAStartup(0x202, &lpWSAData);
	sListen=WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
	if(INVALID_SOCKET==sListen){
		DebugBreak();
	}
	GUID GuidDisconnectEx = WSAID_DISCONNECTEX;
	DWORD dwBytes;
	WSAIoctl(sListen, SIO_GET_EXTENSION_FUNCTION_POINTER,
             &GuidDisconnectEx, sizeof (GuidDisconnectEx), 
             &DisconnectEx, sizeof (DisconnectEx), 
             &dwBytes, NULL, NULL);
	
	if(completionPort!=addCompletionHandle(completionPort, (HANDLE)sListen, CKACCEPT)){
		DebugBreak();
	};

	sockaddr_in service={0};
	service.sin_family=AF_INET;
	service.sin_port=htons(8080);

	sockErr=bind(sListen, (SOCKADDR *) &service, sizeof(service));
	if(SOCKET_ERROR==sockErr){
		DebugBreak();
	}
	sockErr=listen(sListen, SOMAXCONN);
	if(SOCKET_ERROR==sockErr){
		DebugBreak();
	}
	for(int i=0; i<numSockets; i++){
		umsAddAcceptSocket(sListen);
	}
	for(int n=0; n<numSchedulers; n++){
		success=CreateUmsCompletionList(&completionList[n]);
		if(!success){
			DebugBreak();
		}else{SetLastError(0);}

		success=GetUmsCompletionListEvent(completionList[n], &listEvents[n]);
		SchedulerStartupInfo[n].CompletionList=completionList[n];
		SchedulerStartupInfo[n].UmsVersion=UMS_VERSION;
		SchedulerStartupInfo[n].SchedulerParam=(LPVOID)n;
		SchedulerStartupInfo[n].SchedulerProc=UmsSchedulerProc;

		if(!success){
			DebugBreak();
		}else{SetLastError(0);}

		for(int i=0; i<numThreadsPerScheduler; i++){
			addThread(n, &UmsWorkerThreadProc, (LPVOID)i);
			numThreadsAlive[n]++;
		}
	}
	HANDLE SchedulerHandlers[numSchedulers]={0};
	for(int n=0; n<numSchedulers; n++){
		SchedulerHandlers[n]=CreateThread(0, 0, SchedulerProc, (LPVOID)n, STACK_SIZE_PARAM_IS_A_RESERVATION, 0);
	}

	WaitForMultipleObjects(numSchedulers, SchedulerHandlers, TRUE, INFINITE);
	for(int n=0; n<numSchedulers; n++){
		CloseHandle(SchedulerHandlers[n]);
	}

	return false;
}
//#endif





HANDLE umsCompletionPort{nullptr};

#ifdef _DEBUG
//#define _DEBUG_MORE
#endif

DWORD WINAPI WorkerThreadProcYield(LPVOID lpParameter) {
	decltype(UndesiredValue) CapturedValue;
	auto ctx = GetCurrentUmsThread();
#ifdef _DEBUG_MORE
	uint64_t messageCount{0ui64};
#endif
	CapturedValue = WaitVariable;
	while (CapturedValue == UndesiredValue) {
		WaitOnAddress(&WaitVariable, &CompareVariable, sizeof(WaitVariable), INFINITE);
		CapturedValue = WaitVariable;
	}

	CPKey currentKey{CPKey::NO_KEY};
	for(;;) {
		UmsThreadYield(&currentKey);

		switch(currentKey) {
		case CPKey::UMS_CP_QUIT:
#ifdef _DEBUG_MORE
			wprintf(L"%p: threadProc Finished, messageCount: %I64u\n", ctx, messageCount);
#endif
			return 0;

		case CPKey::UMS_CP_FS_STAT:
		{
			auto retVal = _InterlockedAdd64(&dequeuedRequestCount, 1);
			if(retVal == 1) {
				firstRequestStarted = __rdtsc();
			}
			fs_stat();
		//Sleep(0);
			if(retVal == numRequests) {
				lastRequestFinished = __rdtsc();
			}
		}
			break;

		default:
			DebugBreak();
			break;
		}
	}

	return 0;

}
#undef _DEBUG_MORE

DWORD WINAPI WorkerThreadProc(LPVOID lpParameter) {
//WaitForSingleObject(wakeupEvent, INFINITE);
	decltype(UndesiredValue) CapturedValue;

	CapturedValue = WaitVariable;
	while (CapturedValue == UndesiredValue) {
		WaitOnAddress(&WaitVariable, &CompareVariable, sizeof(WaitVariable), INFINITE);
		CapturedValue = WaitVariable;
	}

	HANDLE threadCompletionPort{reinterpret_cast<HANDLE>(lpParameter)};
#ifdef _DEBUG_MORE
	wprintf(L"%p: threadProc...\n", GetCurrentUmsThread());
	uint64_t messageCount{0ui64};
#endif

	for(;;) {
		OVERLAPPED_ENTRY lpCompletionPortEntries[1]{{0}};
		ULONG ulNumEntriesRemoved{0};
		SetLastError(ERROR_SUCCESS);

#ifdef _DEBUG_MORE
		wprintf(L"%p: GetQueuedCompletionStatusEx...\n", GetCurrentUmsThread());
#endif
		if(!GetQueuedCompletionStatusEx(threadCompletionPort, lpCompletionPortEntries, 1, &ulNumEntriesRemoved, 100, TRUE)) {
			DWORD lastError{GetLastError()};
			switch(lastError) {
			case WAIT_TIMEOUT:
#ifdef _DEBUG_MORE
				wprintf(L"%p: GetQueuedCompletionStatusEx WAIT_TIMEOUT\n", GetCurrentUmsThread());
#endif
				continue;
			default:
				macroDebugLastError(0);
			}
		}
		
#ifdef _DEBUG_MORE
		wprintf(L"%p: GetQueuedCompletionStatusEx got %I64u\n", GetCurrentUmsThread(), ulNumEntriesRemoved);
		messageCount += ulNumEntriesRemoved;
#endif

		auto retVal = _InterlockedAdd64(&dequeuedRequestCount, ulNumEntriesRemoved);
		switch(static_cast<CPKey>(lpCompletionPortEntries[0].lpCompletionKey)) {
		case CPKey::UMS_CP_QUIT:
#ifdef _DEBUG_MORE
			wprintf(L"%p: threadProc Finished, messageCount: %I64u\n", GetCurrentUmsThread(), messageCount);
#endif
			return 0;

		case CPKey::UMS_CP_FS_STAT:
			if(retVal == 1) {
				firstRequestStarted = __rdtsc();
			}
			fs_stat();
			if(retVal == numRequests) {
				lastRequestFinished = __rdtsc();
			}
			break;
		}
	}

	return 0;
}


bool benchmark_ums_fs_stat() {
//#define makeCompletionPort() CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 0)
//#define addCompletionHandle(cp, fh, key) CreateIoCompletionPort(fh, cp, key, 0)

	for(int i = 0; i < numBenchmarkRepetitions; i++) {
	//HANDLE cp = makeCompletionPort();
	//wakeupEvent = CreateEvent(nullptr, false, false, nullptr);
		WaitVariable = 0;
		CompareVariable = 0;
		UndesiredValue = 0;

		std::array<HANDLE, numProcessors> cpa;
		for(auto &cpn: cpa) {
			cpn = makeCompletionPort();
		}
		dequeuedRequestCount = 0;
		firstRequestStarted = 0;
		lastRequestFinished = 0;

		UserModeScheduling::Scheduler<numProcessors, numThreadsPerProcessor> scheduler;
		//scheduler.addThreads(WorkerThreadProc, cp);
		for(int n = 0; n < numProcessors; n++) {
		//scheduler.addThreadsToScheduler(n, WorkerThreadProc, cpa[n]);
			scheduler.addThreadsToScheduler(n, WorkerThreadProcYield, cpa[n]);
		}
		scheduler.start();

		Sleep(100);


		for(int n = 0; n < numRequests; n++) {
		//PostQueuedCompletionStatus(cp, 0, static_cast<ULONG_PTR>(CPKey::UMS_CP_FS_STAT), 0);
			PostQueuedCompletionStatus(cpa[n % numProcessors], 0, static_cast<ULONG_PTR>(CPKey::UMS_CP_FS_STAT), 0);
		//scheduler.processors[n % numProcessors].processorWorkList.pushAlternateItem((LPVOID)(CPKey::UMS_CP_FS_STAT));
			scheduler.processors[n % numProcessors].processorWorkList.cachedPushAlternateItem({0, (LPVOID)(CPKey::UMS_CP_FS_STAT)});
		}

		for(auto &cpn: cpa) {
			for(int n = 0; n < numThreadsPerProcessor; n++) {
			//PostQueuedCompletionStatus(cp, 0, static_cast<ULONG_PTR>(CPKey::UMS_CP_QUIT), 0);
				PostQueuedCompletionStatus(cpn, 0, static_cast<ULONG_PTR>(CPKey::UMS_CP_QUIT), 0);
			}
		}

#if 0
		for(int p = 0; p < numProcessors; p++) {
			for(int n = 0; n < numThreadsPerProcessor; n++) {
				scheduler.processors[p].processorWorkList.cachedPushAlternateItem({0, (LPVOID)(CPKey::UMS_CP_QUIT)});
			}
		}
#endif
		for(int p = 0; p < numProcessors; p++) {
			scheduler.processors[p].processorWorkList.flushPushAlternateItems();
		}

#if 0
		for(int p = 0; p < numProcessors; p++) {
			for(;;) {
				scheduler.processors[p].processorWorkList.waitPopAlternateItem();
			}
		}
#endif

		auto start = __rdtsc();
		WaitVariable = 1;
		WakeByAddressAll((PVOID)(&WaitVariable));
	//SetEvent(wakeupEvent);

#if 1
		bool tryWaitStatus{false};
		do {
			tryWaitStatus = scheduler.tryWait(10);
#if _DEBUG_MORE
			wprintf(L"tryWaitStatus: %d, ", tryWaitStatus);
			wprintf(L"dequeuedRequestCount: %I64u\n", dequeuedRequestCount);
#endif
		}while(!tryWaitStatus && dequeuedRequestCount < numRequests);

		for(int p = 0; p < numProcessors; p++) {
			for(int n = 0; n < numThreadsPerProcessor; n++) {
				scheduler.processors[p].processorWorkList.cachedPushForwardItem({0, (LPVOID)(CPKey::UMS_CP_QUIT)});
			}
		}
		for(int p = 0; p < numProcessors; p++) {
			scheduler.processors[p].processorWorkList.flushPushForwordItems();
		}

#elif 0
		bool tryWaitStatus{false};
		do {
		//wprintf(L"scheduler.tryWait(30000ms)...\n");
			tryWaitStatus = scheduler.tryWait(1000);
		//for(int p = 0; p < numProcessors; p++) {
		//	scheduler.processors[p].processorWorkList.pushAlternateItem((LPVOID)(CPKey::UMS_CP_QUIT));
		//}
		//wprintf(L"scheduler.tryWait(30000ms): %s\n", tryWaitStatus?L"true":L"false");
		}while(!tryWaitStatus);
#else
		scheduler.wait();
#endif
		auto finish = __rdtsc();
#if 0
		wprintf(L"start to firstRequestStarted:                %I64u\n", firstRequestStarted - start);
		wprintf(L"firstRequestStarted to lastRequestFinished:  %I64u\n", lastRequestFinished - firstRequestStarted);
		wprintf(L"lastRequestFinished to finish:  %I64u\n", finish - lastRequestFinished);
		wprintf(L"start to finish: %I64u\n", finish - start);
#else
	//wprintf(L"dequeuedRequestCount: %I64u\n", dequeuedRequestCount);
	//wprintf(L"start to firstRequestStarted:                %f seconds\n", static_cast<double_t>(firstRequestStarted - start) / 3300000000.0);
	//wprintf(L"firstRequestStarted to lastRequestFinished:  %f seconds\n", static_cast<double_t>(lastRequestFinished - firstRequestStarted) / 3300000000.0);
	//wprintf(L"lastRequestFinished to finish:               %f seconds\n", static_cast<double_t>(finish - lastRequestFinished) / 3300000000.0);
		wprintf(L"start to finish:                             %f seconds\n", static_cast<double_t>(finish - start) / 3300000000.0);
#endif
	//CloseHandle(cp);
		for(auto &cpn: cpa) { CloseHandle(cpn); }
	//CloseHandle(wakeupEvent);
	}
	return false;
}
#endif//USE_USER_MODE_SCHEDULING
