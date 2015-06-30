#pragma once

class OldThreadPoolApi {
	class Wait {
		BOOL WINAPI RegisterWaitForSingleObject(
  _Out_    PHANDLE             phNewWaitObject,
  _In_     HANDLE              hObject,
  _In_     WAITORTIMERCALLBACK Callback,
  _In_opt_ PVOID               Context,
  _In_     ULONG               dwMilliseconds,
  _In_     ULONG               dwFlags
);
		BOOL WINAPI UnregisterWaitEx(
  _In_     HANDLE WaitHandle,
  _In_opt_ HANDLE CompletionEvent
);
	};

	class TimerQueue {
		class Timer{
		};
//CreateTimerQueue
//CreateTimerQueueTimer
//ChangeTimerQueueTimer
//DeleteTimerQueueTimer
//DeleteTimerQueueEx
	};

	static BOOL work(LPTHREAD_START_ROUTINE Function, PVOID Context, ULONG Flags) {
		return QueueUserWorkItem(Function, Context, Flags);
	}

	static BOOL io(HANDLE FileHandle, LPOVERLAPPED_COMPLETION_ROUTINE Function, ULONG Flags) {
		return BindIoCompletionCallback(FileHandle, Function, Flags);
	}

};