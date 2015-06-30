#pragma once

template<typename T, const int queueCount>
class ThreadContextQueue {
	std::vector<T> threadVector;
	CritialSectionWrapper cs;
public:
	ThreadContextQueue() {
		macroDebugLastError(1);
		threadVector.reserve(queueCount);
		macroDebugLastError(1);
	}

	bool isInside(T item) {
		bool ret;
		cs.enter([this, &ret, item]{
			if(!threadVector.size()) { ret = false; return; }
			ret = std::find(threadVector.begin(), threadVector.end(), item) != threadVector.end();
		});
		return ret;
	}

	uint64_t itemsAvailable() {
		uint64_t ret{0};
		cs.enter([this, &ret]{
			//macroDebugLastError(1);
			ret = threadVector.size();
			//macroDebugLastError(1);
#if 0
			if(ret > queueCount) {
				DebugBreak();
			}
#endif
		});
		return ret;
	}

	bool isFull() {
		macroDebugLastError(1);
		if(itemsAvailable() == queueCount) {
			macroDebugLastError(1);
			return true;
		} else {
			macroDebugLastError(1);
			return false;
		}
	}

	void write(T item) {
	//if(threadVector.size() >= queueCount) {
	//	DebugBreak();
	//}
		cs.enter([this, item]{
			macroDebugLastError(1);
			threadVector.push_back(item);
			macroDebugLastError(1);
		});
	}

	T tryRead() {
		T ret{0};
		cs.tryEnter([this, &ret]{
			if(!threadVector.size()) {
				return;
			}
			macroDebugLastError(1);
			ret = threadVector.back();
			macroDebugLastError(1);
			threadVector.pop_back();
			macroDebugLastError(1);
		});
		return ret;
	}

	T read() {
		T ret{0};
		cs.enter([this, &ret]{
			if(!threadVector.size()) {
				return;
			}
			macroDebugLastError(1);
			ret = threadVector.back();
			macroDebugLastError(1);
			threadVector.pop_back();
			macroDebugLastError(1);
		});
		return ret;
	}

//~ThreadContextQueue() {
//	macroDebugLastError(HeapFree(GetProcessHeap(), 0, ring)); ring = nullptr;
//}
};