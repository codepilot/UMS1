#pragma once

template<typename T, const int maxSize>
class PrivateStack {
public:
	T arr[maxSize];
	int size{0};
	void push(T &&val) {
		arr[size++] = val;
	}
	bool pop(T &ret) {
		if(!size) {
			return false;
		}
		
		size--;
		ret = std::move(arr[size]);
		return true;
	}
};

class Processor {
public:
	Scheduler *scheduler{nullptr};
	CompletionList completionList;
#ifdef _DEBUG_MORE
	DebugStream debugStreamInst;
#endif

#ifdef PROCESSOR_USE_THREAD_RING
	ThreadContextQueue<PUMS_CONTEXT, _threadsPerProcessor> readyThreadRing;
	ThreadContextQueue<ContextYielded, _threadsPerProcessor> yieldThreadRing;
	ThreadContextQueue<PUMS_CONTEXT, _threadsPerProcessor> suspendThreadRing;
//ThreadContextQueue<PUMS_CONTEXT, _threadsPerProcessor> terminatedThreadRing;
	AtomicSinglyLinkedList<LPVOID> processorWorkList;
#endif

#ifdef PROCESSOR_USE_ATOMIC_LIST
//AtomicSinglyLinkedList<PUMS_CONTEXT> readyList;
//AtomicSinglyLinkedList<ContextYielded> yieldList;
	PrivateStack<ContextYielded, _threadsPerProcessor> yieldStack;
//AtomicSinglyLinkedList<PUMS_CONTEXT> suspendList;
//AtomicSinglyLinkedList<PUMS_CONTEXT> terminatedList;
	AtomicSinglyLinkedList<ContextYielded> processorWorkList;
//AtomicSinglyLinkedList<ContextYielded> processorOldWorkList;
#endif

	WorkerThreadVector workerThreads;
	StartupInfo startupInfo;
	Thread processorThread;
	int procNum;
	__declspec(align(64)) __int64 volatile numTerminatedThreads{0};
	__declspec(align(64)) __int64 volatile numSuspendedThreads{0};
#ifdef _DEBUG
	int
		pathA{0}, pathB{0}, pathC{0}, pathD{0}, pathE{0}, pathF{0}, pathG{0}, pathH{0},
		pathI{0}, pathJ{0}, pathK{0}, pathL{0}, pathM{0}, pathN{0}, pathO{0}, pathP{0},
		pathQ{0}, pathR{0};
#endif

	Processor(): /*readyThreadRing{}, yieldThreadRing{}, suspendThreadRing{}, terminatedThreadRing{},*/ completionList{}, workerThreads{} { }
#ifdef _DEBUG
	~Processor() {
		wprintf(L"procNum(%2d) A:%d B:%d C:%d D:%d E:%d F:%d G:%d H:%d I:%d J:%d K:%d L:%d M:%d N:%d O:%d P:%d Q:%d R:%d\n", procNum,
		pathA, pathB, pathC, pathD, pathE, pathF, pathG, pathH,
		pathI, pathJ, pathK, pathL, pathM, pathN, pathO, pathP,
		pathQ, pathR);
	}
#endif
#ifdef _DEBUG_MORE
	bool checkRings(PUMS_CONTEXT item) {
		return true;
		if(readyThreadRing.isInside(item)) {
			debugStreamInst << L"procNum(" << procNum << L") " << L"readyThreadRing.isInside(" << item << L"): true"; debugStreamInst.flushLineBreak();
			return false;
		} else if(yieldThreadRing.isInside(item)) {
			debugStreamInst << L"procNum(" << procNum << L") " << L"yieldThreadRing.isInside(" << item << L"): true"; debugStreamInst.flushLineBreak();
			return false;
		} else if(suspendThreadRing.isInside(item)) {
			debugStreamInst << L"procNum(" << procNum << L") " << L"suspendThreadRing.isInside(" << item << L"): true"; debugStreamInst.flushLineBreak();
			return false;
		/*} else if(terminatedThreadRing.isInside(item)) {
			debugStreamInst << L"procNum(" << procNum << L") " << L"terminatedThreadRing.isInside(" << item << L"): true"; debugStreamInst.flushLineBreak();
			return false; */
		} else {
			return true;
		}
	}
#endif //_DEBUG

#ifdef PROCESSOR_USE_THREAD_RING
	void writeReadyThread(PUMS_CONTEXT item) {
#ifdef _DEBUG_MORE
		debugStreamInst << L"procNum(" << procNum << L") " << L"writeReadyThread(" << item << L") "; debugStreamInst.flushLine();
		if(checkRings(item)) {
#endif //_DEBUG
			readyThreadRing.write(item);
#ifdef _DEBUG_MORE
		}
		debugStreamInst << L"procNum(" << procNum << L") writeReadyThread " << L"itemsAvailable() = " << readyThreadRing.itemsAvailable(); debugStreamInst.flushLine();
#endif //_DEBUG
	}

	void writeYieldThread(PUMS_CONTEXT item, PVOID param) {
#ifdef _DEBUG_MORE
		debugStreamInst << L"procNum(" << procNum << L") " << L"writeYieldThread(" << item << L")";
		debugStreamInst.flushLine();
		if(checkRings(item)) {
#endif //_DEBUG
			yieldThreadRing.write({item, param});
#ifdef _DEBUG_MORE
		}
		debugStreamInst << L"procNum(" << procNum << L") writeYieldThread " << L"itemsAvailable() = " << yieldThreadRing.itemsAvailable();
		debugStreamInst.flushLine();
#endif //_DEBUG
	}

	void writeSuspendThread(PUMS_CONTEXT item) {
		numSuspendedThreads++;
		_InterlockedAdd64(&scheduler->numTotalSuspendedThreads, 1);
#ifdef _DEBUG_MORE
		debugStreamInst << L"procNum(" << procNum << L") " << L"writeSuspendThread(" << item << L") "; debugStreamInst.flushLine();
		if(checkRings(item)) {
#endif //_DEBUG
			suspendThreadRing.write(item);
#ifdef _DEBUG_MORE
		}
		debugStreamInst << L"procNum(" << procNum << L") writeSuspendThread " << L"itemsAvailable() = " << suspendThreadRing.itemsAvailable(); debugStreamInst.flushLine();
#endif //_DEBUG
	}

	void writeTerminatedThread(PUMS_CONTEXT item) {
		numTerminatedThreads++;
		_InterlockedAdd64(&scheduler->numTotalTerminatedThreads, 1);
#if 0
#ifdef _DEBUG
		debugStreamInst << L"procNum(" << procNum << L") " << L"writeTerminatedThread(" << item << L") ";
		debugStreamInst.flush();
		if(checkRings(item)) {
#endif //_DEBUG
			terminatedThreadRing.write(item);
#ifdef _DEBUG
		}
		debugStreamInst << L"procNum(" << procNum << L") " << L"itemsAvailable() = " << terminatedThreadRing.itemsAvailable() << L"\n";
		debugStreamInst.flush();
#endif //_DEBUG
#endif
	}

	PUMS_CONTEXT tryReadReadyThread() {
		auto ret = readyThreadRing.tryRead();
#ifdef _DEBUG_MORE
		debugStreamInst << L"procNum(" << procNum << L") " << L"tryReadReadyThread(): " << ret;
		debugStreamInst.flushLine();
#endif
		return ret;
	}

	PUMS_CONTEXT readReadyThread() {
		auto ret = readyThreadRing.read();
#ifdef _DEBUG_MORE
		debugStreamInst << L"procNum(" << procNum << L") " << L"readReadyThread(): " << ret;
		debugStreamInst.flushLine();
#endif
		return ret;
	}

	ContextYielded readYieldThread() {
		auto ret = yieldThreadRing.read();
#ifdef _DEBUG_MORE
		debugStreamInst << L"procNum(" << procNum << L") " << L"readYieldThread(): " << ret << L"\n";
		debugStreamInst.flush();
#endif
		return ret;
	}

	ContextYielded tryReadYieldThread() {
		auto ret = yieldThreadRing.tryRead();
#ifdef _DEBUG_MORE
		debugStreamInst << L"procNum(" << procNum << L") " << L"tryReadYieldThread(): " << ret;
		debugStreamInst.flushLine();
#endif
		return ret;
	}

	PUMS_CONTEXT tryReadSuspendThread() {
		auto ret = suspendThreadRing.tryRead();
#ifdef _DEBUG_MORE
		debugStreamInst << L"procNum(" << procNum << L") " << L"tryReadSuspendThread(): " << ret;
		debugStreamInst.flushLine();
#endif
		return ret;
	}


	PUMS_CONTEXT readSuspendThread() {
		auto ret = suspendThreadRing.read();
#ifdef _DEBUG_MORE
		debugStreamInst << L"procNum(" << procNum << L") " << L"readSuspendThread(): " << ret << L"\n";
		debugStreamInst.flush();
#endif
		return ret;
	}

	PUMS_CONTEXT readTerminatedThread() {
		auto ret = terminatedThreadRing.read();
#ifdef _DEBUG_MORE
		debugStreamInst << L"procNum(" << procNum << L") " << L"readTerminatedThread(): " << ret << L"\n";
		debugStreamInst.flush();
#endif
		return ret;
	}
#endif

	void addThreads(LPTHREAD_START_ROUTINE lpStartAddress, LPVOID lpParameter) {
		workerThreads.addThreads(this, _threadsPerProcessor, lpStartAddress, lpParameter);
	}

	static DWORD WINAPI staticProcessorProc(LPVOID lpParameter) {
		Processor *proc{reinterpret_cast<Processor *>(lpParameter)};

	//SetThreadIdealProcessor(GetCurrentThread(), proc->procNum);
		SetLastError(ERROR_SUCCESS);
		if(_processorsPerScheduler == 12) {
			macroDebugLastError(SetThreadAffinityMask(GetCurrentThread(), static_cast<DWORD_PTR>(1) << static_cast<DWORD_PTR>(proc->procNum)));
		} else if (_processorsPerScheduler == 6) {
			macroDebugLastError(SetThreadAffinityMask(GetCurrentThread(), static_cast<DWORD_PTR>(1) << static_cast<DWORD_PTR>(proc->procNum * 2)));
		}

		UMS_SCHEDULER_STARTUP_INFO startupInfo{
			UMS_VERSION,
			proc->completionList.UmsCompletionList,
			staticUmsSchedulerProc,
			proc};

		macroDebugLastError(EnterUmsSchedulingMode(&startupInfo));
		return 0;
	}

	void start(Scheduler *_scheduler) {
		scheduler = _scheduler;
		processorThread.create(this, staticProcessorProc);
	}

	bool waitForBlockingThreads(DWORD WaitTimeOut) {
		return false;
		PUMS_CONTEXT threadList[_threadsPerProcessor + 1]{0};
		int threadCount{0};


#if 1
#ifdef _DEBUG_MORE
		debugStreamInst << L"procNum(" << procNum << L") " << L"before DequeueUmsCompletionListItems"; debugStreamInst.flushLine();
		debugStreamInst << L"procNum(" << procNum << L") " << L"numTotalTerminatedThreads: " << scheduler->numTotalTerminatedThreads << L" numTerminatedThreads: " << numTerminatedThreads; debugStreamInst.flushLine();
#endif
		macroDebugLastError(DequeueUmsCompletionListItems(scheduler->processors[procNum].completionList.UmsCompletionList, WaitTimeOut, &threadList[0]));
		if(!threadList[threadCount]) {
			return false;
		}
#elif 1
		DWORD waitSTS{0};
#ifdef _DEBUG_MORE
		debugStreamInst << L"procNum(" << procNum << L") " << L"before WaitForSingleObject"; debugStreamInst.flushLine();
#endif
		waitSTS = WaitForSingleObject(scheduler->schedulerEvents[procNum], 1000);
		switch(waitSTS) {
		case WAIT_OBJECT_0 + 0x00:
#ifdef _DEBUG_MORE
			debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + " << procNum; debugStreamInst.flushLine();
#endif
			break;
		case WAIT_ABANDONED + 0x00:
#ifdef _DEBUG_MORE
			debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x00"; debugStreamInst.flushLine();
#endif
			DebugBreak();
			break;
		case WAIT_TIMEOUT:
#ifdef _DEBUG_MORE
			debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_TIMEOUT"; debugStreamInst.flushLine();
#endif
			return;

		case WAIT_FAILED:
#ifdef _DEBUG_MORE
			debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_FAILED"; debugStreamInst.flushLineBreak();
#endif
			break;

		default:
			DebugBreak();
		}

#ifdef _DEBUG // freezes without sometimes
		debugStreamInst << L"procNum(" << procNum << L") " << L"before DequeueUmsCompletionListItems"; debugStreamInst.flushLine();
#endif
	//macroDebugLastError(DequeueUmsCompletionListItems(completionList.UmsCompletionList, INFINITE, &threadList[threadCount]));
		macroDebugLastError(DequeueUmsCompletionListItems(scheduler->processors[procNum].completionList.UmsCompletionList, 0, &threadList[0]));
		if(!threadList[threadCount]) {
			return;
		}
#elif 0
		debugStreamInst << L"procNum(" << procNum << L") " << L"before WaitForMultipleObjects"; debugStreamInst.flushLine();
		waitSTS = WaitForMultipleObjects(_processorsPerScheduler, scheduler->schedulerEvents.data(), false, 10);

#if 1
		switch(waitSTS) {
		case WAIT_OBJECT_0 + 0x00: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x00"; debugStreamInst.flushLine(); break;
		case WAIT_OBJECT_0 + 0x01: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x01"; debugStreamInst.flushLine(); break;
		case WAIT_OBJECT_0 + 0x02: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x02"; debugStreamInst.flushLine(); break;
		case WAIT_OBJECT_0 + 0x03: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x03"; debugStreamInst.flushLine(); break;
		case WAIT_OBJECT_0 + 0x04: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x04"; debugStreamInst.flushLine(); break;
		case WAIT_OBJECT_0 + 0x05: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x05"; debugStreamInst.flushLine(); break;
		case WAIT_OBJECT_0 + 0x06: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x06"; debugStreamInst.flushLine(); break;
		case WAIT_OBJECT_0 + 0x07: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x07"; debugStreamInst.flushLine(); break;
		case WAIT_OBJECT_0 + 0x08: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x08"; debugStreamInst.flushLine(); break;
		case WAIT_OBJECT_0 + 0x09: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x09"; debugStreamInst.flushLine(); break;
		case WAIT_OBJECT_0 + 0x0A: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x0A"; debugStreamInst.flushLine(); break;
		case WAIT_OBJECT_0 + 0x0B: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x0B"; debugStreamInst.flushLine(); break;
		case WAIT_OBJECT_0 + 0x0C: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x0C"; debugStreamInst.flushLine(); break;
		case WAIT_OBJECT_0 + 0x0D: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x0D"; debugStreamInst.flushLine(); break;
		case WAIT_OBJECT_0 + 0x0E: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x0E"; debugStreamInst.flushLine(); break;
		case WAIT_OBJECT_0 + 0x0F: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x0F"; debugStreamInst.flushLine(); break;

		case WAIT_OBJECT_0 + 0x10: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x10"; debugStreamInst.flushLine(); break;
		case WAIT_OBJECT_0 + 0x11: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x11"; debugStreamInst.flushLine(); break;
		case WAIT_OBJECT_0 + 0x12: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x12"; debugStreamInst.flushLine(); break;
		case WAIT_OBJECT_0 + 0x13: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x13"; debugStreamInst.flushLine(); break;
		case WAIT_OBJECT_0 + 0x14: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x14"; debugStreamInst.flushLine(); break;
		case WAIT_OBJECT_0 + 0x15: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x15"; debugStreamInst.flushLine(); break;
		case WAIT_OBJECT_0 + 0x16: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x16"; debugStreamInst.flushLine(); break;
		case WAIT_OBJECT_0 + 0x17: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x17"; debugStreamInst.flushLine(); break;
		case WAIT_OBJECT_0 + 0x18: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x18"; debugStreamInst.flushLine(); break;
		case WAIT_OBJECT_0 + 0x19: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x19"; debugStreamInst.flushLine(); break;
		case WAIT_OBJECT_0 + 0x1A: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x1A"; debugStreamInst.flushLine(); break;
		case WAIT_OBJECT_0 + 0x1B: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x1B"; debugStreamInst.flushLine(); break;
		case WAIT_OBJECT_0 + 0x1C: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x1C"; debugStreamInst.flushLine(); break;
		case WAIT_OBJECT_0 + 0x1D: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x1D"; debugStreamInst.flushLine(); break;
		case WAIT_OBJECT_0 + 0x1E: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x1E"; debugStreamInst.flushLine(); break;
		case WAIT_OBJECT_0 + 0x1F: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x1F"; debugStreamInst.flushLine(); break;

		case WAIT_OBJECT_0 + 0x20: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x20"; debugStreamInst.flushLine(); break;
		case WAIT_OBJECT_0 + 0x21: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x21"; debugStreamInst.flushLine(); break;
		case WAIT_OBJECT_0 + 0x22: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x22"; debugStreamInst.flushLine(); break;
		case WAIT_OBJECT_0 + 0x23: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x23"; debugStreamInst.flushLine(); break;
		case WAIT_OBJECT_0 + 0x24: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x24"; debugStreamInst.flushLine(); break;
		case WAIT_OBJECT_0 + 0x25: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x25"; debugStreamInst.flushLine(); break;
		case WAIT_OBJECT_0 + 0x26: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x26"; debugStreamInst.flushLine(); break;
		case WAIT_OBJECT_0 + 0x27: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x27"; debugStreamInst.flushLine(); break;
		case WAIT_OBJECT_0 + 0x28: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x28"; debugStreamInst.flushLine(); break;
		case WAIT_OBJECT_0 + 0x29: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x29"; debugStreamInst.flushLine(); break;
		case WAIT_OBJECT_0 + 0x2A: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x2A"; debugStreamInst.flushLine(); break;
		case WAIT_OBJECT_0 + 0x2B: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x2B"; debugStreamInst.flushLine(); break;
		case WAIT_OBJECT_0 + 0x2C: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x2C"; debugStreamInst.flushLine(); break;
		case WAIT_OBJECT_0 + 0x2D: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x2D"; debugStreamInst.flushLine(); break;
		case WAIT_OBJECT_0 + 0x2E: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x2E"; debugStreamInst.flushLine(); break;
		case WAIT_OBJECT_0 + 0x2F: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x2F"; debugStreamInst.flushLine(); break;

		case WAIT_OBJECT_0 + 0x30: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x30"; debugStreamInst.flushLine(); break;
		case WAIT_OBJECT_0 + 0x31: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x31"; debugStreamInst.flushLine(); break;
		case WAIT_OBJECT_0 + 0x32: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x32"; debugStreamInst.flushLine(); break;
		case WAIT_OBJECT_0 + 0x33: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x33"; debugStreamInst.flushLine(); break;
		case WAIT_OBJECT_0 + 0x34: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x34"; debugStreamInst.flushLine(); break;
		case WAIT_OBJECT_0 + 0x35: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x35"; debugStreamInst.flushLine(); break;
		case WAIT_OBJECT_0 + 0x36: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x36"; debugStreamInst.flushLine(); break;
		case WAIT_OBJECT_0 + 0x37: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x37"; debugStreamInst.flushLine(); break;
		case WAIT_OBJECT_0 + 0x38: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x38"; debugStreamInst.flushLine(); break;
		case WAIT_OBJECT_0 + 0x39: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x39"; debugStreamInst.flushLine(); break;
		case WAIT_OBJECT_0 + 0x3A: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x3A"; debugStreamInst.flushLine(); break;
		case WAIT_OBJECT_0 + 0x3B: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x3B"; debugStreamInst.flushLine(); break;
		case WAIT_OBJECT_0 + 0x3C: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x3C"; debugStreamInst.flushLine(); break;
		case WAIT_OBJECT_0 + 0x3D: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x3D"; debugStreamInst.flushLine(); break;
		case WAIT_OBJECT_0 + 0x3E: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x3E"; debugStreamInst.flushLine(); break;
		case WAIT_OBJECT_0 + 0x3F: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_OBJECT_0 + 0x3F"; debugStreamInst.flushLine(); break;

		case WAIT_ABANDONED + 0x00: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x00"; debugStreamInst.flushLine(); DebugBreak(); break;
		case WAIT_ABANDONED + 0x01: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x01"; debugStreamInst.flushLine(); DebugBreak(); break;
		case WAIT_ABANDONED + 0x02: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x02"; debugStreamInst.flushLine(); DebugBreak(); break;
		case WAIT_ABANDONED + 0x03: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x03"; debugStreamInst.flushLine(); DebugBreak(); break;
		case WAIT_ABANDONED + 0x04: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x04"; debugStreamInst.flushLine(); DebugBreak(); break;
		case WAIT_ABANDONED + 0x05: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x05"; debugStreamInst.flushLine(); DebugBreak(); break;
		case WAIT_ABANDONED + 0x06: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x06"; debugStreamInst.flushLine(); DebugBreak(); break;
		case WAIT_ABANDONED + 0x07: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x07"; debugStreamInst.flushLine(); DebugBreak(); break;
		case WAIT_ABANDONED + 0x08: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x08"; debugStreamInst.flushLine(); DebugBreak(); break;
		case WAIT_ABANDONED + 0x09: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x09"; debugStreamInst.flushLine(); DebugBreak(); break;
		case WAIT_ABANDONED + 0x0A: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x0A"; debugStreamInst.flushLine(); DebugBreak(); break;
		case WAIT_ABANDONED + 0x0B: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x0B"; debugStreamInst.flushLine(); DebugBreak(); break;
		case WAIT_ABANDONED + 0x0C: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x0C"; debugStreamInst.flushLine(); DebugBreak(); break;
		case WAIT_ABANDONED + 0x0D: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x0D"; debugStreamInst.flushLine(); DebugBreak(); break;
		case WAIT_ABANDONED + 0x0E: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x0E"; debugStreamInst.flushLine(); DebugBreak(); break;
		case WAIT_ABANDONED + 0x0F: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x0F"; debugStreamInst.flushLine(); DebugBreak(); break;

		case WAIT_ABANDONED + 0x10: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x10"; debugStreamInst.flushLine(); DebugBreak(); break;
		case WAIT_ABANDONED + 0x11: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x11"; debugStreamInst.flushLine(); DebugBreak(); break;
		case WAIT_ABANDONED + 0x12: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x12"; debugStreamInst.flushLine(); DebugBreak(); break;
		case WAIT_ABANDONED + 0x13: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x13"; debugStreamInst.flushLine(); DebugBreak(); break;
		case WAIT_ABANDONED + 0x14: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x14"; debugStreamInst.flushLine(); DebugBreak(); break;
		case WAIT_ABANDONED + 0x15: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x15"; debugStreamInst.flushLine(); DebugBreak(); break;
		case WAIT_ABANDONED + 0x16: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x16"; debugStreamInst.flushLine(); DebugBreak(); break;
		case WAIT_ABANDONED + 0x17: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x17"; debugStreamInst.flushLine(); DebugBreak(); break;
		case WAIT_ABANDONED + 0x18: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x18"; debugStreamInst.flushLine(); DebugBreak(); break;
		case WAIT_ABANDONED + 0x19: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x19"; debugStreamInst.flushLine(); DebugBreak(); break;
		case WAIT_ABANDONED + 0x1A: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x1A"; debugStreamInst.flushLine(); DebugBreak(); break;
		case WAIT_ABANDONED + 0x1B: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x1B"; debugStreamInst.flushLine(); DebugBreak(); break;
		case WAIT_ABANDONED + 0x1C: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x1C"; debugStreamInst.flushLine(); DebugBreak(); break;
		case WAIT_ABANDONED + 0x1D: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x1D"; debugStreamInst.flushLine(); DebugBreak(); break;
		case WAIT_ABANDONED + 0x1E: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x1E"; debugStreamInst.flushLine(); DebugBreak(); break;
		case WAIT_ABANDONED + 0x1F: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x1F"; debugStreamInst.flushLine(); DebugBreak(); break;

		case WAIT_ABANDONED + 0x20: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x20"; debugStreamInst.flushLine(); DebugBreak(); break;
		case WAIT_ABANDONED + 0x21: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x21"; debugStreamInst.flushLine(); DebugBreak(); break;
		case WAIT_ABANDONED + 0x22: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x22"; debugStreamInst.flushLine(); DebugBreak(); break;
		case WAIT_ABANDONED + 0x23: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x23"; debugStreamInst.flushLine(); DebugBreak(); break;
		case WAIT_ABANDONED + 0x24: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x24"; debugStreamInst.flushLine(); DebugBreak(); break;
		case WAIT_ABANDONED + 0x25: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x25"; debugStreamInst.flushLine(); DebugBreak(); break;
		case WAIT_ABANDONED + 0x26: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x26"; debugStreamInst.flushLine(); DebugBreak(); break;
		case WAIT_ABANDONED + 0x27: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x27"; debugStreamInst.flushLine(); DebugBreak(); break;
		case WAIT_ABANDONED + 0x28: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x28"; debugStreamInst.flushLine(); DebugBreak(); break;
		case WAIT_ABANDONED + 0x29: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x29"; debugStreamInst.flushLine(); DebugBreak(); break;
		case WAIT_ABANDONED + 0x2A: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x2A"; debugStreamInst.flushLine(); DebugBreak(); break;
		case WAIT_ABANDONED + 0x2B: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x2B"; debugStreamInst.flushLine(); DebugBreak(); break;
		case WAIT_ABANDONED + 0x2C: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x2C"; debugStreamInst.flushLine(); DebugBreak(); break;
		case WAIT_ABANDONED + 0x2D: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x2D"; debugStreamInst.flushLine(); DebugBreak(); break;
		case WAIT_ABANDONED + 0x2E: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x2E"; debugStreamInst.flushLine(); DebugBreak(); break;
		case WAIT_ABANDONED + 0x2F: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x2F"; debugStreamInst.flushLine(); DebugBreak(); break;

		case WAIT_ABANDONED + 0x30: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x30"; debugStreamInst.flushLine(); DebugBreak(); break;
		case WAIT_ABANDONED + 0x31: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x31"; debugStreamInst.flushLine(); DebugBreak(); break;
		case WAIT_ABANDONED + 0x32: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x32"; debugStreamInst.flushLine(); DebugBreak(); break;
		case WAIT_ABANDONED + 0x33: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x33"; debugStreamInst.flushLine(); DebugBreak(); break;
		case WAIT_ABANDONED + 0x34: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x34"; debugStreamInst.flushLine(); DebugBreak(); break;
		case WAIT_ABANDONED + 0x35: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x35"; debugStreamInst.flushLine(); DebugBreak(); break;
		case WAIT_ABANDONED + 0x36: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x36"; debugStreamInst.flushLine(); DebugBreak(); break;
		case WAIT_ABANDONED + 0x37: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x37"; debugStreamInst.flushLine(); DebugBreak(); break;
		case WAIT_ABANDONED + 0x38: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x38"; debugStreamInst.flushLine(); DebugBreak(); break;
		case WAIT_ABANDONED + 0x39: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x39"; debugStreamInst.flushLine(); DebugBreak(); break;
		case WAIT_ABANDONED + 0x3A: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x3A"; debugStreamInst.flushLine(); DebugBreak(); break;
		case WAIT_ABANDONED + 0x3B: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x3B"; debugStreamInst.flushLine(); DebugBreak(); break;
		case WAIT_ABANDONED + 0x3C: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x3C"; debugStreamInst.flushLine(); DebugBreak(); break;
		case WAIT_ABANDONED + 0x3D: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x3D"; debugStreamInst.flushLine(); DebugBreak(); break;
		case WAIT_ABANDONED + 0x3E: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x3E"; debugStreamInst.flushLine(); DebugBreak(); break;
		case WAIT_ABANDONED + 0x3F: debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED + 0x3F"; debugStreamInst.flushLine(); DebugBreak(); break;

			debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_ABANDONED"; debugStreamInst.flushLineBreak();
			break;

		case WAIT_TIMEOUT:
			debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_TIMEOUT"; debugStreamInst.flushLine();
			return;

		case WAIT_FAILED:
			debugStreamInst << L"procNum(" << procNum << L") " << L"WAIT_FAILED"; debugStreamInst.flushLineBreak();
			break;

		default:
			DebugBreak();
		}
#endif
		debugStreamInst << L"procNum(" << procNum << L") " << L"before DequeueUmsCompletionListItems"; debugStreamInst.flushLine();
	//macroDebugLastError(DequeueUmsCompletionListItems(completionList.UmsCompletionList, INFINITE, &threadList[threadCount]));
		macroDebugLastError(DequeueUmsCompletionListItems(scheduler->processors[waitSTS].completionList.UmsCompletionList, 0, &threadList[0]));
		if(!threadList[threadCount]) {
			return;
		}
#endif


#ifdef _DEBUG_MORE // freezes without sometimes
		debugStreamInst << L"procNum(" << procNum << L") " << L"dequeued: ";
#endif
		for(;;) {
			threadCount++;
			auto nextThread = GetNextUmsListItem(threadList[threadCount - 1]);
			if(nextThread) {
				if(threadCount >= _threadsPerProcessor) {
					threadCount += 0;
				}
				threadList[threadCount] = nextThread;
			} else {
				break;
			}
		}

#ifdef _DEBUG_MORE // freezes without sometimes
		for(int i = 0; i < threadCount; i++) {
			debugStreamInst << threadList[i] << L" ";
		}
		debugStreamInst.flushLine();
#endif

		for(int i = 0; i < threadCount; i++) {
			if(isThreadTerminated(threadList[i])) {
				writeTerminatedThread(threadList[i]);
			} else if (isThreadSuspended(threadList[i])) {
				writeSuspendThread(threadList[i]);
			} else {
				writeReadyThread(threadList[i]);
			}
		}
		return true;
	}

	static bool isThreadTerminated(PUMS_CONTEXT execThread) {
		BOOLEAN isTerminated{0};
		ULONG retLenB{0};
		macroDebugLastError(QueryUmsThreadInformation(execThread, UmsThreadIsTerminated, &isTerminated, sizeof(isTerminated), &retLenB));
		return isTerminated?true:false;
	}

	static bool isThreadSuspended(PUMS_CONTEXT execThread) {
		BOOLEAN isSuspended{0};
		ULONG retLenB{0};
		macroDebugLastError(QueryUmsThreadInformation(execThread, UmsThreadIsSuspended, &isSuspended, sizeof(isSuspended), &retLenB));
		return isSuspended?true:false;
	}

	void tryExecuting(PUMS_CONTEXT execThread) {
		BOOLEAN isSuspended{0};
		BOOLEAN isTerminated{0};
		ULONG retLenA{0};
		ULONG retLenB{0};

#ifdef _DEBUG_MORE
		debugStreamInst << L"procNum(" << procNum << L") " << L"tryExecuting(" << execThread << L")..."; debugStreamInst.flushLine();
#endif

		macroDebugLastError(1);
		macroDebugLastError(QueryUmsThreadInformation(execThread, UmsThreadIsSuspended, &isSuspended, sizeof(isSuspended), &retLenA));
		macroDebugLastError(QueryUmsThreadInformation(execThread, UmsThreadIsTerminated, &isTerminated, sizeof(isTerminated), &retLenB));

		if(isSuspended){
			writeSuspendThread(execThread);

#ifdef _DEBUG_MORE
			debugStreamInst << L"procNum(" << procNum << L") " << L"writeSuspendThread(" << execThread << L"); " << L"suspendThreadRing.itemsAvailable() = " << suspendThreadRing.itemsAvailable(); debugStreamInst.flushLine();
#endif

			macroDebugLastError(1);
		} else if(isTerminated){
			macroDebugLastError(DeleteUmsThreadContext(execThread));
			writeTerminatedThread(execThread);

#ifdef _DEBUG_MORE
			debugStreamInst << L"procNum(" << procNum << L") " << L"writeTerminatedThread(" << execThread << L"); "  << L"scheduler->numTerminatedThreads = " << scheduler->numTerminatedThreads; debugStreamInst.flushLine();
#endif

			macroDebugLastError(1);
		} else {
#ifdef _DEBUG_MORE
			debugStreamInst << L"procNum(" << procNum << L") " << L"ExecuteUmsThread(" << execThread << L")"; debugStreamInst.flushLine();
#endif

			macroDebugLastError(1);
			SetLastError(ERROR_SUCCESS);
			ExecuteUmsThread(execThread);
			auto lastError = GetLastError();
			SetLastError(ERROR_SUCCESS);
#ifdef _DEBUG_MORE
			debugStreamInst << L"procNum(" << procNum << L") " << L"ExecuteUmsThread(" << execThread << L") Fail, lastError = " << static_cast<uint32_t>(lastError); debugStreamInst.flushLine();
#endif


		//writeReadyThread(execThread);
#ifdef _DEBUG_MORE
			debugStreamInst << L"procNum(" << procNum << L") " << L"readyThreadRing.itemsAvailable() = " << readyThreadRing.itemsAvailable(); debugStreamInst.flushLine();
#endif

		//macroDebugLastError(1);
		}
	}


	VOID UmsSchedulerProc(UMS_SCHEDULER_REASON Reason, ULONG_PTR ActivationPayload, PVOID SchedulerParam) {
		switch(Reason) {
			case UMS_SCHEDULER_REASON::UmsSchedulerStartup:
				break;
			case UMS_SCHEDULER_REASON::UmsSchedulerThreadBlocked:
				break;
			case UMS_SCHEDULER_REASON::UmsSchedulerThreadYield:
#ifdef PROCESSOR_USE_THREAD_RING
				writeYieldThread(reinterpret_cast<PUMS_CONTEXT>(ActivationPayload), SchedulerParam);
#endif
#ifdef PROCESSOR_USE_ATOMIC_LIST
//				yieldList.pushForwardItem({reinterpret_cast<PUMS_CONTEXT>(ActivationPayload), SchedulerParam});
#ifdef _DEBUG
				pathO++;
#endif
				if(processorWorkList.alternateItemsAvailable()) {
#ifdef _DEBUG
					pathP++;
#endif
					auto workItem = processorWorkList.tryPopAlternateItem();
					*((LPVOID *)SchedulerParam) = workItem->val.param;

					macroDebugLastError(1);
					SetLastError(ERROR_SUCCESS);
					ExecuteUmsThread(reinterpret_cast<PUMS_CONTEXT>(ActivationPayload));
#ifdef _DEBUG
					pathQ++;
#endif
					auto lastError = GetLastError();
					SetLastError(ERROR_SUCCESS);
					yieldStack.push({reinterpret_cast<PUMS_CONTEXT>(ActivationPayload), SchedulerParam});
					processorWorkList.pushForwardItem({0, workItem->val.param});
				} else {
#ifdef _DEBUG
					pathR++;
#endif
					yieldStack.push({reinterpret_cast<PUMS_CONTEXT>(ActivationPayload), SchedulerParam});
				}
#endif
				break;
		}

#ifdef _DEBUG_MORE
		debugStreamInst << L"procNum(" << procNum << L") " << L"UmsSchedulerProc(Reason(" << Reason << L"):" << reasons[Reason].c_str() << L", ActivationPayload:" << ActivationPayload << L", SchedulerParam:" << SchedulerParam << L", GetCurrentUmsThread():" << GetCurrentUmsThread() << L")";
		debugStreamInst.flushLine();
#endif

	//while(scheduler->numTotalTerminatedThreads < static_cast<__int64>(_processorsPerScheduler * _threadsPerProcessor)) {
		while(numTerminatedThreads < static_cast<__int64>(_threadsPerProcessor)) {
#ifdef _DEBUG_MORE
			debugStreamInst << L"procNum(" << procNum << L") " << L"scheduler->numTerminatedThreads " << scheduler->numTerminatedThreads; debugStreamInst.flushLine();
#endif

#ifdef PROCESSOR_USE_THREAD_RING
			PUMS_CONTEXT ctx{nullptr};
			if(yieldThreadRing.itemsAvailable() && (processorWorkList.headerForwardPath.cachedPopEntries || processorWorkList.headerForwardPath.next)) {
				auto workItem = processorWorkList.tryPopForwardItem();
				if(workItem) {
					auto ctxYielded = tryReadYieldThread();
					if(ctxYielded.ctx) {
						*((LPVOID *)ctxYielded.param) = workItem->val;
						tryExecuting(ctxYielded.ctx);
					}
				}
			} else if((yieldThreadRing.itemsAvailable() + numTerminatedThreads) == _threadsPerProcessor) {
				auto workItem = processorWorkList.waitPopForwardItem();
				auto ctxYielded = tryReadYieldThread();
				if(ctxYielded.ctx) {
					*((LPVOID *)ctxYielded.param) = workItem->val;
					tryExecuting(ctxYielded.ctx);
				}
			//processorWorkList.pushFreeItem(workItem);
			} else if(ctx = tryReadReadyThread()) {
				tryExecuting(ctx);
			} else if(!waitForBlockingThreads(suspendThreadRing.itemsAvailable()?0:100)) {
				if(ctx = tryReadSuspendThread()) {
					tryExecuting(ctx);
				}
			}
#endif

#ifdef PROCESSOR_USE_ATOMIC_LIST
			auto workItem = processorWorkList.waitPopForwardItem();
			if(workItem->val.ctx && !workItem->val.param) {//readyThread
				if(isThreadTerminated(workItem->val.ctx)) {
					_InterlockedAdd64(&numTerminatedThreads, 1);
					_InterlockedAdd64(&scheduler->numTotalTerminatedThreads, 1);
#ifdef _DEBUG
					pathA++;
#endif
					continue;
				} else if (Processor::isThreadSuspended(workItem->val.ctx)) {
					scheduler->suspendList.pushForwardItem({workItem->val.ctx, procNum});
#ifdef _DEBUG
					pathB++;
#endif
					continue;
				} else {
					macroDebugLastError(1);
					SetLastError(ERROR_SUCCESS);
#ifdef _DEBUG
					pathC++;
#endif
					ExecuteUmsThread(workItem->val.ctx);
#ifdef _DEBUG
					pathD++;
#endif
				//processorWorkList.pushForwardItem({suspendItem->val.ctx, 0});
					auto lastError = GetLastError();
					switch(lastError) {
						case ERROR_INVALID_PARAMETER:
#ifdef _DEBUG
							pathE++;
#endif
							SetLastError(ERROR_SUCCESS);
							continue;
						default:
#ifdef _DEBUG
							pathF++;
#endif
							DebugBreak();
					}
#ifdef _DEBUG
					pathG++;
#endif
					SetLastError(ERROR_SUCCESS);
				//scheduler->suspendList.pushForwardItem({workItem->val.ctx, procNum});
				}

			} else if(!workItem->val.ctx && workItem->val.param) {//workItem for yielded thread
			//auto ctxYielded = yieldList.tryPopForwardItem();
				ContextYielded ctxYielded;
#ifdef _DEBUG
				pathH++;
#endif
				if(!yieldStack.pop(ctxYielded)) {
					processorWorkList.pushAlternateItem({0, workItem->val.param});
#ifdef _DEBUG
					pathI++;
#endif
					continue;
				} else {
					*((LPVOID *)ctxYielded.param) = workItem->val.param;
#ifdef _DEBUG
					pathJ++;
#endif
					if(isThreadTerminated(ctxYielded.ctx)) {
						_InterlockedAdd64(&numTerminatedThreads, 1);
						_InterlockedAdd64(&scheduler->numTotalTerminatedThreads, 1);
#ifdef _DEBUG
						pathK++;
#endif
						continue;
					} else if (Processor::isThreadSuspended(ctxYielded.ctx)) {
						scheduler->suspendList.pushForwardItem({ctxYielded.ctx, procNum});
#ifdef _DEBUG
						pathL++;
#endif
						continue;
					} else {
						macroDebugLastError(1);
						SetLastError(ERROR_SUCCESS);
#ifdef _DEBUG
						pathM++;
#endif
						ExecuteUmsThread(ctxYielded.ctx);
#ifdef _DEBUG
						pathN++;
#endif
						auto lastError = GetLastError();
						SetLastError(ERROR_SUCCESS);
					//yieldList.pushForwardItem(std::move(ctxYielded));
						processorWorkList.pushForwardItem({0, workItem->val.param});
						continue;
					}
				}
			} else {
				continue;
			}
#endif

#if 0 //PROCESSOR_USE_ATOMIC_LIST
			if(processorWorkList.forwardItemsAvailable() && yieldList.forwardItemsAvailable()) {
				auto workItem = processorWorkList.tryPopForwardItem();
				auto ctxYielded = yieldList.tryPopForwardItem();
				if(!workItem) {
					DebugBreak();
				}
				if(!ctxYielded->val.ctx) {
					DebugBreak();
				}
				*((LPVOID *)ctxYielded->val.param) = workItem->val;

				macroDebugLastError(1);
				SetLastError(ERROR_SUCCESS);
				ExecuteUmsThread(ctxYielded->val.ctx);
				auto lastError = GetLastError();
				SetLastError(ERROR_SUCCESS);
				suspendList.pushForwardItem(std::move(ctxYielded));
				continue;
			}

			if(readyList.forwardItemsAvailable()) {
				auto ctxReady = readyList.tryPopForwardItem();
				macroDebugLastError(1);
				SetLastError(ERROR_SUCCESS);
				ExecuteUmsThread(ctxReady->val);
				auto lastError = GetLastError();
				SetLastError(ERROR_SUCCESS);
				suspendList.pushForwardItem(std::move(ctxReady));
				continue;
			}

			if(suspendList.forwardItemsAvailable()) {
				auto ctxSuspend = suspendList.tryPopForwardItem();
				macroDebugLastError(1);
				SetLastError(ERROR_SUCCESS);
				ExecuteUmsThread(ctxSuspend->val);
				auto lastError = GetLastError();
				SetLastError(ERROR_SUCCESS);
				suspendList.pushForwardItem(std::move(ctxSuspend));
				continue;
			}
#endif
		}

#ifdef _DEBUG_MORE
		debugStreamInst << L"procNum(" << procNum << L") " << L"scheduler->numTerminatedThreads " << scheduler->numTerminatedThreads;
		debugStreamInst.flushLine();
		debugStreamInst << L"procNum(" << procNum << L") " << L"Processor finished";
		debugStreamInst.flushLine();
#endif
	}

	static VOID NTAPI staticUmsSchedulerProc(UMS_SCHEDULER_REASON Reason, ULONG_PTR ActivationPayload, PVOID SchedulerParam) {
		static __declspec(thread) Processor *proc{nullptr};

		SetLastError(ERROR_SUCCESS);
		macroDebugLastError(1);
		if(Reason == UMS_SCHEDULER_REASON::UmsSchedulerStartup) {
			proc = reinterpret_cast<decltype(proc)>(SchedulerParam);
		}
		proc->UmsSchedulerProc(Reason, ActivationPayload, SchedulerParam);
	}
};