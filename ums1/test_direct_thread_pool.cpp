#include "stdafx.h"


#ifdef USE_DIRECT_THREAD_POOL
#ifdef USE_TEST_WEBSERVER
bool test_direct_thread_pool() {//success is true, failure is false

	int sockErr;

#ifdef USE_USER_MODE_SCHEDULING
	BOOL success;
	dwTlsID=TlsAlloc();
	completionPort=makeCompletionPort();
#endif

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
	
#ifdef USE_USER_MODE_SCHEDULING
	if(completionPort!=addCompletionHandle(completionPort, (HANDLE)sListen, CKACCEPT)){
		DebugBreak();
	};
#endif

#ifdef USE_NEW_THREAD_POOL
	InitializeThreadpoolEnvironment(&tEnvrion);
	pMainPool=CreateThreadpool(NULL);
	pCleanup=CreateThreadpoolCleanupGroup();
	SetThreadpoolCallbackCleanupGroup(&tEnvrion, pCleanup, 0);
	SetThreadpoolCallbackPool(&tEnvrion, pMainPool);

	pListen=CreateThreadpoolIo((HANDLE)sListen, acceptingIoThreadProc, 0, &tEnvrion);
#endif

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
#ifdef USE_USER_MODE_SCHEDULING
		addAcceptSocket(sListen);
#endif
#ifdef USE_NEW_THREAD_POOL
		addAcceptSocket(sListen);
#endif
	}
#ifdef USE_USER_MODE_SCHEDULING
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
		SchedulerHandlers[n]=CreateThread(0, 0, SchedulerProc, (LPVOID)n, 0, 0);
	}

	WaitForMultipleObjects(numSchedulers, SchedulerHandlers, TRUE, INFINITE);
	for(int n=0; n<numSchedulers; n++){
		CloseHandle(SchedulerHandlers[n]);
	}
#endif
#ifdef USE_NEW_THREAD_POOL
	OutputDebugString(TEXT("CloseThreadpoolCleanupGroupMembers waiting\n"));
	SleepEx(INFINITE, TRUE);
	CloseThreadpoolCleanupGroupMembers(pCleanup, FALSE, NULL);
	OutputDebugString(TEXT("CloseThreadpoolCleanupGroupMembers done\n"));
#endif
	
	return false;
}
#endif

HANDLE dtp_cp{INVALID_HANDLE_VALUE};

VOID CALLBACK dtp_apc_fs_stat(ULONG_PTR dwParam) {
//wprintf(L"dtp_apc_fs_stat\n");
//HANDLE handle = CreateFileW(L".", FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,  NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
//CloseHandle(handle);
	fs_stat();
}

DWORD __stdcall dtp_test_fs() {
	OVERLAPPED_ENTRY lpCompletionPortEntries[1];
	ULONG ulNumEntriesRemoved{0};

	for(;;) {
		SetLastError(ERROR_SUCCESS);
		if(GetQueuedCompletionStatusEx(dtp_cp, lpCompletionPortEntries, ARRAYSIZE(lpCompletionPortEntries), &ulNumEntriesRemoved, INFINITE, TRUE)) {
			if(lpCompletionPortEntries[0].lpCompletionKey == 2) {
				fs_stat();
			} else if(lpCompletionPortEntries[0].lpCompletionKey == 1) {
				return 1;
			}
		} else {
			auto lastError = GetLastError();
			switch(lastError) {
			case ERROR_ABANDONED_WAIT_0:
				continue;
			default:
				wprintf(L"lastError dec:%d, hex:%x\n", lastError, lastError);
			}
			return 1;
		}
	}
	return 0;
}

bool benchmark_dtp_fs_stat() {
	for(int r = 0; r < numBenchmarkRepetitions; r++) {
		dtp_cp = makeCompletionPort();

		std::array<HANDLE, numProcessors> threadHandles{{INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE}};
		for(auto &nThread: threadHandles) {
			nThread = CreateThread(nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(dtp_test_fs), nullptr, 0, nullptr);
		}

		auto start = __rdtsc();
		for(int i = 0; i < numRequests; i++) {
		//QueueUserAPC(dtp_apc_fs_stat, threadHandles[i % numDirectThreads], 0);
			PostQueuedCompletionStatus(dtp_cp, 0, 2, nullptr);
		}

		for(int i = 0; i < numProcessors; i++) {
		//QueueUserAPC(dtp_apc_fs_stat, threadHandles[i % numDirectThreads], 0);
			PostQueuedCompletionStatus(dtp_cp, 0, 1, nullptr);
		}

		if(threadHandles.size() <= MAXIMUM_WAIT_OBJECTS) {
			WaitForMultipleObjects(static_cast<DWORD>(threadHandles.size()), threadHandles.data(), TRUE, INFINITE);
		} else {
			for(const auto &nThread: threadHandles) {
				WaitForSingleObject(nThread, INFINITE);
			}
		}
		CloseHandle(dtp_cp);

		auto finish = __rdtsc();
		wprintf(L"start to finish:                             %f seconds\n", static_cast<double_t>(finish - start) / 3300000000.0);
	}
	return false;
}
#endif//USE_DIRECT_THREAD_POOL
