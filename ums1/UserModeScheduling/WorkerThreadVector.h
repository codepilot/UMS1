#pragma once

class WorkerThreadVector {
public:
	std::vector<WorkerThread> threads;
	WorkerThreadVector() {
	}
	void addThreads(Processor *_proc, size_t threadCount, LPTHREAD_START_ROUTINE lpStartAddress, LPVOID lpParameter) {
		threads.reserve(threadCount);
		threads.resize(threadCount);
		for(auto &nThread: threads) {
			nThread.attachToProcessor(_proc, lpStartAddress, lpParameter);
		}
	}
};
