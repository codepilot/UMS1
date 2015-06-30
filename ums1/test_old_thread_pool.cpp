#include "stdafx.h"


#ifdef USE_OLD_THREAD_POOL
#ifdef USE_TEST_WEBSERVER

bool test_old_thread_pool() {//success is true, failure is false
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


bool benchmark_otp_fs_stat() {
	return false;
}
#endif//USE_OLD_THREAD_POOL
