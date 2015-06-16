#include "stdafx.h"

#define USE_USER_MODE_SCHEDULING

#ifndef USE_USER_MODE_SCHEDULING
#define USE_NEW_THREAD_POOL
#endif

#define numSchedulers 16
#define numThreadsPerScheduler 16
#define numSockets 256



#ifdef USE_USER_MODE_SCHEDULING
HANDLE listEvents[numSchedulers];
PUMS_COMPLETION_LIST completionList[numSchedulers];
UMS_SCHEDULER_STARTUP_INFO SchedulerStartupInfo[numSchedulers];
DWORD WINAPI UmsWorkerThreadProc(LPVOID lpParameter);
VOID NTAPI UmsSchedulerProc(UMS_SCHEDULER_REASON Reason, ULONG_PTR ActivationPayload, PVOID SchedulerParam);
HANDLE completionPort=NULL;
DWORD dwTlsID;
#endif

#ifdef USE_NEW_THREAD_POOL
PTP_POOL pMainPool;
PTP_CLEANUP_GROUP pCleanup;
TP_CALLBACK_ENVIRON tEnvrion;
PTP_IO pListen;
#endif

LPFN_DISCONNECTEX DisconnectEx=0;
SOCKET sListen;

#ifdef _DEBUG
#define ods(txt) {OutputDebugString(TEXT(txt));}
#else
#define ods(txt)
#endif

//#define ods(txt)

char status200HelloWorld[]="HTTP/1.1 200 Ok\r\nContent-Length: 12\r\nConnection: close\r\n\r\nHello World!";
#ifdef USE_USER_MODE_SCHEDULING

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


#define makeCompletionPort() CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 0)
#define addCompletionHandle(cp, fh, key) CreateIoCompletionPort(fh, cp, key, 0)
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


	HANDLE rt1=CreateRemoteThreadEx(GetCurrentProcess(), NULL, 0, &UmsWorkerThreadProc, lpParameter, 0, lpAttributeList, NULL);

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

void addAcceptSocket(SOCKET sListen) {
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

#endif

#ifdef USE_NEW_THREAD_POOL

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

void addAcceptSocket(SOCKET sListen) {
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
#endif

BOOL SetPrivilege(
    HANDLE hToken,          // access token handle
    LPCTSTR lpszPrivilege,  // name of privilege to enable/disable
    BOOL bEnablePrivilege   // to enable or disable privilege
    ) 
{
    TOKEN_PRIVILEGES tp;
    LUID luid;

    if ( !LookupPrivilegeValue( 
            NULL,            // lookup privilege on local system
            lpszPrivilege,   // privilege to lookup 
            &luid ) )        // receives LUID of privilege
    {
        printf("LookupPrivilegeValue error: %u\n", GetLastError() ); 
        return FALSE; 
    }

    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    if (bEnablePrivilege)
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    else
        tp.Privileges[0].Attributes = 0;

    // Enable the privilege or disable all privileges.

    if ( !AdjustTokenPrivileges(
           hToken, 
           FALSE, 
           &tp, 
           sizeof(TOKEN_PRIVILEGES), 
           (PTOKEN_PRIVILEGES) NULL, 
           (PDWORD) NULL) )
    { 
          printf("AdjustTokenPrivileges error: %u\n", GetLastError() ); 
          return FALSE; 
    } 

    if (GetLastError() == ERROR_NOT_ALL_ASSIGNED)

    {
          printf("The token does not have the specified privilege. \n");
          return FALSE;
    } 

    return TRUE;
}

int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE, LPTSTR    lpCmdLine, int       nCmdShow) {
#if 0
	{
		HANDLE hToken;
		if(!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)){
			DebugBreak();
			return FALSE;
		}
		SetPrivilege(hToken, TEXT("SeLockMemoryPrivilege"), TRUE);
		CloseHandle(hToken);
	}
	SIZE_T largePageSize=GetLargePageMinimum();
	PVOID largePage=VirtualAlloc(NULL, largePageSize, MEM_RESERVE|MEM_COMMIT|MEM_LARGE_PAGES, PAGE_READWRITE);

	if(!largePage){
		DebugBreak();
	}
#endif
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
	
	return 0;
}