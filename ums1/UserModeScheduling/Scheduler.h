#pragma once

template<const int _processorsPerScheduler, const int _threadsPerProcessor>
class Scheduler {
public:
	class Processor;

	#include "ThreadAttributeList.h"
	#include "WorkerThread.h"
	#include "WorkerThreadVector.h"
	#include "SchedulerThread.h"
	#include "CompletionList.h"
	#include "StartupInfo.h"
	#include "CritialSectionWrapper.h"
	#include "ThreadContextQueue.h"

	class ContextYielded {
	public:
		PUMS_CONTEXT ctx{nullptr};
		PVOID param{nullptr};
		ContextYielded(PUMS_CONTEXT _ctx = nullptr, PVOID _param = nullptr): ctx{_ctx}, param{_param} { }
	};

	#include "AtomicSinglyLinkedList.h"

	#include "Processor.h"

	std::array<Processor, _processorsPerScheduler> processors;
	std::array<HANDLE, _processorsPerScheduler> schedulerEvents;
	std::array<HANDLE, _processorsPerScheduler> processorHandles;
	class SuspendedContext {
	public:
		PUMS_CONTEXT ctx;
		int procNum;
	};
	AtomicSinglyLinkedList<SuspendedContext> suspendList;
	__declspec(align(64)) __int64 volatile numTotalTerminatedThreads{0};
	__declspec(align(64)) __int64 volatile numTotalSuspendedThreads{0};
	std::array<Thread, 1> dequeueThreads;
	std::array<Thread, 1> unsuspendThreads;

	Scheduler() {
		for(int i = 0; i < _processorsPerScheduler; i++) {
			processors[i].procNum = i;
			schedulerEvents[i] = processors[i].completionList.completionEvent;
		}
		for(auto &t: dequeueThreads) {
			t.create(this, staticDequeueProc);
		}
		for(auto &t: unsuspendThreads) {
			t.create(this, staticUnsuspendProc);
		}
	}

	static DWORD WINAPI staticUnsuspendProc(LPVOID lpParameter) {
		SetLastError(0);
		Scheduler *sched{reinterpret_cast<Scheduler *>(lpParameter)};
		macroDebugLastError(1);
		return sched->unsuspendProc();
	}

	DWORD unsuspendProc() {
		while(numTotalTerminatedThreads < _processorsPerScheduler * _threadsPerProcessor) {
			auto suspendItem = suspendList.waitPopForwardItem();
			if(!suspendItem) {
				break;
			} else if(!suspendItem->val.ctx) {
				return 0;
			} else if(Processor::isThreadTerminated(suspendItem->val.ctx)) {
				_InterlockedAdd64(&processors[suspendItem->val.procNum].numTerminatedThreads, 1);
				_InterlockedAdd64(&numTotalTerminatedThreads, 1);
			} else if (Processor::isThreadSuspended(suspendItem->val.ctx)) {
				suspendList.pushForwardItem({suspendItem->val.ctx,  suspendItem->val.procNum});
			} else {
				processors[suspendItem->val.procNum].processorWorkList.pushForwardItem({suspendItem->val.ctx, 0});
			}
		}
		return 0;
	}

	static DWORD WINAPI staticDequeueProc(LPVOID lpParameter) {
		SetLastError(0);
		Scheduler *sched{reinterpret_cast<Scheduler *>(lpParameter)};
		macroDebugLastError(1);
		return sched->dequeueProc();
	}

	DWORD dequeueProc() {
		macroDebugLastError(1);
		while(numTotalTerminatedThreads < _processorsPerScheduler * _threadsPerProcessor) {
			macroDebugLastError(1);
			auto waitSTS = WaitForMultipleObjectsEx(_processorsPerScheduler, schedulerEvents.data(), false, INFINITE, TRUE);
			macroDebugLastError(1);
			if((waitSTS >= WAIT_OBJECT_0) && (waitSTS < WAIT_OBJECT_0 + MAXIMUM_WAIT_OBJECTS)) {
				auto procNum = waitSTS - WAIT_OBJECT_0;
				PUMS_CONTEXT threadList[_threadsPerProcessor + 1]{0};
				int threadCount{0};
				macroDebugLastError(1);
				macroDebugLastError(DequeueUmsCompletionListItems(processors[procNum].completionList.UmsCompletionList, INFINITE, &threadList[0]));
				if(!threadList[threadCount]) {
					DebugBreak();
					continue;
				}
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
#ifdef PROCESSOR_USE_THREAD_RING
				for(int i = 0; i < threadCount; i++) {
					if(Processor::isThreadTerminated(threadList[i])) {
						processors[procNum].writeTerminatedThread(threadList[i]);
					} else if (Processor::isThreadSuspended(threadList[i])) {
						processors[procNum].writeSuspendThread(threadList[i]);
					} else {
						processors[procNum].writeReadyThread(threadList[i]);
					}
				}
#endif

#ifdef PROCESSOR_USE_ATOMIC_LIST
				for(int i = 0; i < threadCount; i++) {
					if(Processor::isThreadTerminated(threadList[i])) {
						_InterlockedAdd64(&processors[procNum].numTerminatedThreads, 1);
						_InterlockedAdd64(&numTotalTerminatedThreads, 1);
						processors[procNum].processorWorkList.pushForwardItem({0, 0});
					//processors[procNum].terminatedList.pushForwardItem(std::move(threadList[i]));
					} else if (Processor::isThreadSuspended(threadList[i])) {
						suspendList.pushForwardItem({threadList[i], procNum});
					} else {
						processors[procNum].processorWorkList.pushForwardItem({threadList[i], 0});
					}
				}
#endif
				continue;
			}
			if((waitSTS >= WAIT_ABANDONED) && (waitSTS < WAIT_ABANDONED + MAXIMUM_WAIT_OBJECTS)) {
				auto procNum = waitSTS - WAIT_ABANDONED;
				continue;
			}
			if(waitSTS == WAIT_TIMEOUT) {
				continue;
			}
			if(waitSTS == WAIT_FAILED) {
				DWORD lastError = GetLastError();
				DebugBreak();
				return 1;
			} else if(waitSTS == WAIT_FAILED) {
				DWORD lastError = GetLastError();
				DebugBreak();
				return 1;
			}
		}
		return 0;
	}

	void addThreads(LPTHREAD_START_ROUTINE lpStartAddress, LPVOID lpParameter) {
		for(auto &p: processors) {
			p.addThreads(lpStartAddress, lpParameter);
		}
	}

	void addThreadsToScheduler(int proc, LPTHREAD_START_ROUTINE lpStartAddress, LPVOID lpParameter) {
		processors[proc].addThreads(lpStartAddress, lpParameter);
	}

	void start() {
		for(auto &p: processors) {
			p.start(this);
		}
		for(int i = 0; i < _processorsPerScheduler; i++) {
			processorHandles[i] = processors[i].processorThread.threadHandle;
		}
	}

#if 0
	void wait() {
		for(auto &p: processors) {
			p.processorThread.wait();
		}
	}
#else
	bool tryWait(DWORD dwMilliseconds=0ui32) {
		DWORD waitStatus{WaitForMultipleObjects(static_cast<DWORD>(processorHandles.size()), processorHandles.data(), TRUE, dwMilliseconds)};

		if((waitStatus >= WAIT_OBJECT_0) && (waitStatus < (WAIT_OBJECT_0 + MAXIMUM_WAIT_OBJECTS))) {
			return true;
		} else if (waitStatus == WAIT_TIMEOUT) {
			return false;
		} else {
			DebugBreak();
			return false;
		}
	}
	void wait() {
		DWORD waitStatus{WaitForMultipleObjects(static_cast<DWORD>(processorHandles.size()), processorHandles.data(), TRUE, INFINITE)};

		if((waitStatus >= WAIT_OBJECT_0) && (waitStatus < (WAIT_OBJECT_0 + MAXIMUM_WAIT_OBJECTS))) {
		} else {
			DebugBreak();
		}
		for(auto &t: dequeueThreads) {
			t.sendAPC();
		}
		for(auto &t: dequeueThreads) {
			t.wait();
		}
		for(auto &t: unsuspendThreads) {
			suspendList.pushForwardItem({0, 0});
		}
		for(auto &t: unsuspendThreads) {
			t.wait();
		}

		
	}
#endif

	~Scheduler() {
		wait();
	//OutputDebugStringW(L"~Scheduler()\n");
	}

};
