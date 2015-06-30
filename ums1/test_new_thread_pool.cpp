#include "stdafx.h"

PTP_POOL pMainPool;
PTP_CLEANUP_GROUP pCleanup;
TP_CALLBACK_ENVIRON tEnvrion;
PTP_IO pListen;

typedef enum _OVERLAPPED_ACCEPT_STATE {
	OAS_RECV = 0,
	OAS_SEND,
	OAS_CLOSE
} OVERLAPPED_ACCEPT_STATE, *POVERLAPPED_ACCEPT_STATE;

typedef struct _OVERLAPPED_ACCEPT {
	OVERLAPPED overlapped;
	PTP_IO tp;
	SOCKET s;
	OVERLAPPED_ACCEPT_STATE oas;
	DWORD bufSize;
    LPVOID acceptBuf;
} OVERLAPPED_ACCEPT, *LPOVERLAPPED_ACCEPT;

VOID CALLBACK acceptingIoThreadProc(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PVOID Overlapped, ULONG IoResult, ULONG_PTR NumberOfBytesTransferred, PTP_IO Io) {
	ods("acceptingIoThreadProc start\n");
	BOOL success;
	LPOVERLAPPED_ACCEPT ola=(LPOVERLAPPED_ACCEPT)Overlapped;
	SecureZeroMemory(Overlapped, sizeof(OVERLAPPED));
	ola->oas=OAS_RECV;
	DWORD numRead=0;
	StartThreadpoolIo(ola->tp);
	success=ReadFile((HANDLE)ola->s, ola->acceptBuf, ola->bufSize, &numRead, &ola->overlapped);
	if(!success && GetLastError()!=ERROR_IO_PENDING){
		int wsaError=WSAGetLastError();
		DebugBreak();
	}
	ods("acceptingIoThreadProc return\n");
}

VOID CALLBACK workerIoThreadProc(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PVOID Overlapped, ULONG IoResult, ULONG_PTR NumberOfBytesTransferred, PTP_IO Io) {
	ods("workerIoThreadProc start\n");
	BOOL success;
	LPOVERLAPPED_ACCEPT ola=(LPOVERLAPPED_ACCEPT)Overlapped;
	if(OAS_RECV==ola->oas){
		ods("OAS_RECV\n");
		DWORD numWritten=0;
		SecureZeroMemory(Overlapped, sizeof(OVERLAPPED));
		DWORD sizeofSend=sizeof(status200HelloWorld)-1;
		CopyMemory(ola->acceptBuf, status200HelloWorld, sizeofSend);
		ola->oas=OAS_SEND;
		StartThreadpoolIo(Io);
		success=WriteFile((HANDLE)ola->s, ola->acceptBuf, sizeofSend, &numWritten, &ola->overlapped);
		if(!success && GetLastError()!=ERROR_IO_PENDING){
			int wsaError=WSAGetLastError();
			DebugBreak();
		}
	}else if(OAS_SEND==ola->oas){
		ods("OAS_SEND\n");
		SecureZeroMemory(Overlapped, sizeof(OVERLAPPED));
		ola->oas=OAS_CLOSE;
		StartThreadpoolIo(Io);
		success=DisconnectEx(ola->s, &ola->overlapped, TF_REUSE_SOCKET, 0);
		if(!success && GetLastError()!=ERROR_IO_PENDING){
			int wsaError=WSAGetLastError();
			DebugBreak();
		}
	}else if(OAS_CLOSE==ola->oas){
		ods("OAS_CLOSE\n");
		DWORD acceptBufSize=max(0, ola->bufSize-sizeof(sockaddr_in)-16-sizeof(sockaddr_in)-16);
		acceptBufSize=0;
		StartThreadpoolIo(pListen);
		success=AcceptEx(sListen, ola->s, ola->acceptBuf, acceptBufSize, sizeof(sockaddr_in)+16, sizeof(sockaddr_in)+16, 0, &ola->overlapped);
		if(!success && WSAGetLastError()!=ERROR_IO_PENDING){
			DebugBreak();
		}
	}else{
		DebugBreak();
	}
	ods("workerIoThreadProc return\n");
}

void ntpAddAcceptSocket(SOCKET sListen) {
	BOOL success;
	SOCKET sAccept=WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
	if(INVALID_SOCKET==sAccept){
		DebugBreak();
	}
	DWORD nBufSize=max(4096, (sizeof(sockaddr_in)+16)*2);
	LPOVERLAPPED_ACCEPT ola=(LPOVERLAPPED_ACCEPT)HeapAlloc(GetProcessHeap(), HEAP_GENERATE_EXCEPTIONS|HEAP_ZERO_MEMORY, sizeof(OVERLAPPED_ACCEPT)+nBufSize);
	ola->tp=CreateThreadpoolIo((HANDLE)sAccept, workerIoThreadProc, 0, &tEnvrion);
	ola->acceptBuf=(UINT8*)ola+sizeof(OVERLAPPED_ACCEPT);
	ola->s=sAccept;
	ola->bufSize=nBufSize;
	DWORD acceptBufSize=max(0, ola->bufSize-sizeof(sockaddr_in)-16-sizeof(sockaddr_in)-16);
	acceptBufSize=0;
	StartThreadpoolIo(pListen);
	success=AcceptEx(sListen, ola->s, ola->acceptBuf, acceptBufSize, sizeof(sockaddr_in)+16, sizeof(sockaddr_in)+16, 0, &ola->overlapped);
	if(!success && WSAGetLastError()!=ERROR_IO_PENDING){
		DebugBreak();
	}
}

#ifdef USE_NEW_THREAD_POOL
#ifdef USE_TEST_WEBSERVER
bool test_new_thread_pool() {//success is true, failure is false
	int sockErr;

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
	

	InitializeThreadpoolEnvironment(&tEnvrion);
	pMainPool=CreateThreadpool(NULL);
	pCleanup=CreateThreadpoolCleanupGroup();
	SetThreadpoolCallbackCleanupGroup(&tEnvrion, pCleanup, 0);
	SetThreadpoolCallbackPool(&tEnvrion, pMainPool);

	pListen=CreateThreadpoolIo((HANDLE)sListen, acceptingIoThreadProc, 0, &tEnvrion);

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
		ntpAddAcceptSocket(sListen);
	}
	OutputDebugString(TEXT("CloseThreadpoolCleanupGroupMembers waiting\n"));
	SleepEx(INFINITE, TRUE);
	CloseThreadpoolCleanupGroupMembers(pCleanup, FALSE, NULL);
	OutputDebugString(TEXT("CloseThreadpoolCleanupGroupMembers done\n"));
	
	return false;
}
#endif

VOID CALLBACK WorkCallback_fs_stat(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WORK Work) {

}

#if 0
__declspec(noinline) bool benchmark_ntp_fs_stat() {
	TP_CALLBACK_ENVIRON env;
	InitializeThreadpoolEnvironment(&env);

	PTP_POOL pool{nullptr};
	pool = CreateThreadpool(nullptr);
	SetThreadpoolThreadMaximum(pool, 48);
	SetThreadpoolThreadMinimum(pool, 12);

	PTP_CLEANUP_GROUP group = CreateThreadpoolCleanupGroup();
	SetThreadpoolCallbackPool(&env, pool);
	SetThreadpoolCallbackCleanupGroup(&env, group, nullptr);

	PTP_WORK work_fs_stat = CreateThreadpoolWork(WorkCallback_fs_stat, nullptr, &env);


	SubmitThreadpoolWork(work_fs_stat);
	WaitForThreadpoolWorkCallbacks(work_fs_stat, false);

	CloseThreadpoolWork(work_fs_stat);

	CloseThreadpool(pool);
	return false;
}
#else

__declspec(noinline) bool benchmark_ntp_workQueue_fs_stat() {
	NewThreadPoolApi::PrivatePoolEnvironment ppe;
	ppe.setMin(numProcessors);
	ppe.setMax(numProcessors);
	ppe.setPriority(TP_CALLBACK_PRIORITY::TP_CALLBACK_PRIORITY_HIGH);
	//auto waitQueue = ppe.createWaitQueue();
//auto workQueue = ppe.createWorkQueue(WorkCallback, nullptr);
	auto workQueue = ppe.createWorkQueue([](PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WORK Work) { fs_stat(); });
	//auto timerQueue = ppe.createTimerQueue();

	for(int r = 0; r < numBenchmarkRepetitions; r++) {
		auto start = __rdtsc();
		for(int i = 0; i < numRequests; i++) {
			workQueue.submit();
		}
		workQueue.waitAll();
		auto finish = __rdtsc();
		wprintf(L"start to finish:                             %f seconds\n", static_cast<double_t>(finish - start) / 3300000000.0);

	}
	return false;
}

__declspec(noinline) bool benchmark_ntp_trySubmit_fs_stat() {
	NewThreadPoolApi::PrivatePoolEnvironment ppe;
	ppe.setMin(numProcessors);
	ppe.setMax(numProcessors);
	ppe.setPriority(TP_CALLBACK_PRIORITY::TP_CALLBACK_PRIORITY_HIGH);
	for(int r = 0; r < numBenchmarkRepetitions; r++) {
		auto start = __rdtsc();
		for(int i = 0; i < numRequests; i++) {
			ppe.trySubmit(reinterpret_cast<PVOID>(i), [](PTP_CALLBACK_INSTANCE Instance, PVOID Context) { fs_stat(); });
		}
		ppe.closeMembers();
		auto finish = __rdtsc();
		wprintf(L"start to finish:                             %f seconds\n", static_cast<double_t>(finish - start) / 3300000000.0);

	}
	return false;
}
#endif

#endif