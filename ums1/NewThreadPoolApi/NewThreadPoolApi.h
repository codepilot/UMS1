#pragma once

/*

	pool         <one to many>  environment
	group        <one to many>  environment
	environment	 <one to many>  queue(sync, work, trySubmit, timer, io)


	environment can have one
		setCleanupGroup
		setRaceDll
		setPool
		setPriority
		setLongFunction
		setActivationContext
		setNoActivationContext
		setFinalizationCallback
		setPersistent
*/

namespace NewThreadPoolApi {
	class TpCallBack {
		PTP_CALLBACK_INSTANCE instance{nullptr};
	public:
		__forceinline TpCallBack(PTP_CALLBACK_INSTANCE _instance): instance{_instance} {
		}
		__forceinline BOOL mayRunLong() {
			return CallbackMayRunLong(instance);
		}
		__forceinline void dissociate() {
			DisassociateCurrentThreadFromCallback(instance);
		}
		__forceinline void onReturnFreeLibrary(HMODULE mod) {
			FreeLibraryWhenCallbackReturns(instance, mod);
		}
		__forceinline void onReturnLeaveCriticalSection(PCRITICAL_SECTION pcs) {
			LeaveCriticalSectionWhenCallbackReturns(instance, pcs);
		}
		__forceinline void onReturnReleaseMutex(HANDLE mut) {
			ReleaseMutexWhenCallbackReturns(instance, mut);
		}
		__forceinline void onReturnReleaseSemaphone(HANDLE sem, DWORD crel) {
			ReleaseSemaphoreWhenCallbackReturns(instance, sem, crel);
		}
		__forceinline void onReturnSetEvent(HANDLE evt) {
			SetEventWhenCallbackReturns(instance, evt);
		}
	};

	class WaitQueue {
		PTP_WAIT instance{nullptr};
		bool partOfCleanupGroup{false};
	public:
		__forceinline WaitQueue(PTP_CALLBACK_ENVIRON pEnv=nullptr, bool _partOfCleanupGroup=false): instance{nullptr}, partOfCleanupGroup{_partOfCleanupGroup} {
			instance = macroDebugLastError(CreateThreadpoolWait(staticCB, this, pEnv));
		}

		__forceinline WaitQueue(PTP_WAIT _instance): instance{_instance}, partOfCleanupGroup{true} {
		}

		__forceinline ~WaitQueue() {
			if(!partOfCleanupGroup) {
				CloseThreadpoolWait(instance);
			}
		}

		static void __stdcall staticCB(PTP_CALLBACK_INSTANCE pInstance, PVOID pContext, PTP_WAIT pWait, TP_WAIT_RESULT WaitResult) {
			reinterpret_cast<WaitQueue *>(pContext)->cb(pInstance, WaitResult);
		}

		__forceinline void cb(TpCallBack tpcb, TP_WAIT_RESULT WaitResult) {
		}

		__forceinline void set(HANDLE h, PFILETIME pftTimeout) { /* wait handle on handle limited by timeout */
			SetThreadpoolWait(instance, h, pftTimeout);
		} 
		__forceinline void set(HANDLE h) { /* no timeout */
			SetThreadpoolWait(instance, h, nullptr);
		} 
		__forceinline void set(PFILETIME pftTimeout) { /* only timeout */
			SetThreadpoolWait(instance, nullptr, pftTimeout);
		}
		__forceinline void waitAll() {
			WaitForThreadpoolWaitCallbacks(instance, false);
		}
		__forceinline void waitCancelPending() {
			WaitForThreadpoolWaitCallbacks(instance, true);
		}
	};
	class WorkQueue {
		PTP_WORK instance;
		bool partOfCleanupGroup{false};
	public:
		__forceinline WorkQueue(PTP_CALLBACK_ENVIRON pEnv, bool _partOfCleanupGroup, PTP_WORK_CALLBACK pfnwk, PVOID pv): instance{nullptr}, partOfCleanupGroup{_partOfCleanupGroup} {
			instance = macroDebugLastError(CreateThreadpoolWork(pfnwk, pv, pEnv));
		}
		__forceinline WorkQueue(PTP_CALLBACK_ENVIRON pEnv=nullptr, bool _partOfCleanupGroup=false): instance{nullptr}, partOfCleanupGroup{_partOfCleanupGroup} {
			instance = macroDebugLastError(CreateThreadpoolWork(staticCB, this, pEnv));
		}
		__forceinline WorkQueue(PTP_WORK _instance): instance{_instance}, partOfCleanupGroup{true} {
		}
		__forceinline ~WorkQueue() {
			if(!partOfCleanupGroup) {
				CloseThreadpoolWork(instance);
			}
		}

		static void __stdcall staticCB(PTP_CALLBACK_INSTANCE pInstance, PVOID pContext, PTP_WORK pWork) {
			reinterpret_cast<WorkQueue *>(pContext)->cb(pInstance);
		}

		__forceinline void cb(TpCallBack tpcb) {
		}

		__forceinline void submit() {
			SubmitThreadpoolWork(instance);
		}

		__forceinline static BOOL trySubmit(PTP_SIMPLE_CALLBACK pfns, PVOID pv, PTP_CALLBACK_ENVIRON pcbe) {
			return macroDebugLastError(TrySubmitThreadpoolCallback(pfns, pv, pcbe));
		}

		__forceinline void waitAll() {
			WaitForThreadpoolWorkCallbacks(instance, false);
		}
		__forceinline void waitCancelPending() {
			WaitForThreadpoolWorkCallbacks(instance, true);
		}
	};

	class TimerQueue {
		PTP_TIMER instance{nullptr};
		bool partOfCleanupGroup{false};
	public:
		__forceinline TimerQueue(PTP_CALLBACK_ENVIRON pEnv=nullptr, bool _partOfCleanupGroup=false): instance{nullptr}, partOfCleanupGroup{_partOfCleanupGroup} {
			instance = macroDebugLastError(CreateThreadpoolTimer(staticCB, this, pEnv));
		}
		__forceinline TimerQueue(PTP_TIMER _instance): instance{_instance}, partOfCleanupGroup{true} {
		}
		__forceinline ~TimerQueue() {
			if(!partOfCleanupGroup) {
				CloseThreadpoolTimer(instance);
			}
		}

		static void __stdcall staticCB(PTP_CALLBACK_INSTANCE pInstance, PVOID pContext, PTP_TIMER pTimer) {
			reinterpret_cast<TimerQueue *>(pContext)->cb(pInstance);
		}

		__forceinline void cb(TpCallBack tpcb) {
		}

		__forceinline BOOL isSet() const {
			return IsThreadpoolTimerSet(instance);
		}
		__forceinline void set(PFILETIME pftDueTime, DWORD msPeriod, DWORD msWindowLength) {
			SetThreadpoolTimer(instance, pftDueTime, msPeriod, msWindowLength);
		}

		__forceinline void waitAll() {
			WaitForThreadpoolTimerCallbacks(instance, false);
		}
		__forceinline void waitCancelPending() {
			WaitForThreadpoolTimerCallbacks(instance, true);
		}
	};

	class IoQueue {
		PTP_IO instance{nullptr};
		bool partOfCleanupGroup{false};
	public:
		__forceinline IoQueue(HANDLE fl, PTP_CALLBACK_ENVIRON pEnv=nullptr, bool _partOfCleanupGroup=false): instance{nullptr}, partOfCleanupGroup{_partOfCleanupGroup} {
			instance = macroDebugLastError(CreateThreadpoolIo(fl, staticCB, this, nullptr));
		}
		__forceinline IoQueue(PTP_IO _instance): instance{_instance}, partOfCleanupGroup{true} {
		}
		__forceinline ~IoQueue() {
			if(!partOfCleanupGroup) {
				CloseThreadpoolIo(instance);
			}
		}

		static void __stdcall staticCB(PTP_CALLBACK_INSTANCE pInstance, PVOID pContext, PVOID pOverlapped, ULONG IoResult, ULONG_PTR NumberOfBytesTransferred, PTP_IO pIo) {
			reinterpret_cast<IoQueue *>(pContext)->cb(pInstance, pOverlapped, IoResult, NumberOfBytesTransferred);
		}

		__forceinline void cb(TpCallBack tpcb, PVOID pOverlapped, ULONG IoResult, ULONG_PTR NumberOfBytesTransferred) {
		}

		__forceinline void cancel() {
			CancelThreadpoolIo(instance);
		}
		__forceinline void start() {
			StartThreadpoolIo(instance);
		}
		__forceinline void waitAll() {
			WaitForThreadpoolIoCallbacks(instance, false);
		}
		__forceinline void waitCancelPending() {
			WaitForThreadpoolIoCallbacks(instance, true);
		}
	};

	class Pool {
		PTP_POOL instance{nullptr};
	public:
		operator PTP_POOL() { return instance; }
		__forceinline Pool(): instance{nullptr} {
			instance = macroDebugLastError(CreateThreadpool(nullptr));
		}
		__forceinline ~Pool() {
			CloseThreadpool(instance);
		}
		__forceinline void setMax(DWORD cthrdMost) {
			SetThreadpoolThreadMaximum(instance, cthrdMost);
		}
		__forceinline BOOL setMin(DWORD cthrdMic) {
			return macroDebugLastError(SetThreadpoolThreadMinimum(instance, cthrdMic));
		}
	};
	class Group {
		PTP_CLEANUP_GROUP instance{nullptr};
	public:
		operator PTP_CLEANUP_GROUP() { return instance; }
		__forceinline Group(): instance{nullptr} {
			instance = macroDebugLastError(CreateThreadpoolCleanupGroup());
		}
		__forceinline void closeMembers(BOOL fCancelPendingCallbacks=false, PVOID pvCleanupContext=nullptr) {
			CloseThreadpoolCleanupGroupMembers(instance, fCancelPendingCallbacks, pvCleanupContext);
		}
		__forceinline ~Group() {
			closeMembers();
			CloseThreadpoolCleanupGroup(instance);
		}
	};
	class Environment {
		TP_CALLBACK_ENVIRON env;
	public:
		operator PTP_CALLBACK_ENVIRON() { return &env;}
		__forceinline Environment() {
			TpInitializeCallbackEnviron(&env);
		}
		__forceinline Environment(PTP_CLEANUP_GROUP pCleanupGroup, PTP_POOL pPool) {
			TpInitializeCallbackEnviron(&env);
			TpSetCallbackCleanupGroup(&env, pCleanupGroup, nullptr);
			TpSetCallbackThreadpool(&env, pPool);
		}
		__forceinline ~Environment() {
			TpDestroyCallbackEnviron(&env);
		}
		__forceinline void setCleanupGroup(PTP_CLEANUP_GROUP pCleanupGroup, PTP_CLEANUP_GROUP_CANCEL_CALLBACK pCleanupGroupCancelCallback = nullptr) /* only one */ {
			TpSetCallbackCleanupGroup(&env, pCleanupGroup, pCleanupGroupCancelCallback);
		}
		__forceinline void setRaceDll(PVOID DllHandle) /* only one */ {
			TpSetCallbackRaceWithDll(&env, DllHandle);
		}
		__forceinline void setPool(PTP_POOL pPool) /* only one */ {
			TpSetCallbackThreadpool(&env, pPool);
		}
		__forceinline void setPriority(TP_CALLBACK_PRIORITY Priority) {
			TpSetCallbackPriority(&env, Priority);
		}
		__forceinline void setLongFunction() {
			TpSetCallbackLongFunction(&env);
		}
		__forceinline void setActivationContext(struct _ACTIVATION_CONTEXT *ActivationContext) {
			TpSetCallbackActivationContext(&env, ActivationContext);
		}
		__forceinline void setNoActivationContext() {
			TpSetCallbackNoActivationContext(&env);
		}
		__forceinline void setFinalizationCallback(PTP_SIMPLE_CALLBACK  pFinalizationCallback) {
			TpSetCallbackFinalizationCallback(&env, pFinalizationCallback);
		}
		__forceinline void setPersistent() {
			TpSetCallbackPersistent(&env);
		}
	};

	class PrivatePoolEnvironment {
		NewThreadPoolApi::Pool pool;
		NewThreadPoolApi::Group group;
		NewThreadPoolApi::Environment env;
	public:
		PrivatePoolEnvironment(): pool{}, group{}, env{group, pool} {

		}
		__forceinline void setMax(DWORD cthrdMost) {
			SetThreadpoolThreadMaximum(pool, cthrdMost);
		}
		__forceinline BOOL setMin(DWORD cthrdMic) {
			return macroDebugLastError(SetThreadpoolThreadMinimum(pool, cthrdMic));
		}

		__forceinline void closeMembers(BOOL fCancelPendingCallbacks=false, PVOID pvCleanupContext=nullptr) {
			CloseThreadpoolCleanupGroupMembers(group, fCancelPendingCallbacks, pvCleanupContext);
		}

		__forceinline void setRaceDll(PVOID DllHandle) /* only one */ {
			TpSetCallbackRaceWithDll(env, DllHandle);
		}
		__forceinline void setPriority(TP_CALLBACK_PRIORITY Priority) {
			TpSetCallbackPriority(env, Priority);
		}
		__forceinline void setLongFunction() {
			TpSetCallbackLongFunction(env);
		}
		__forceinline void setActivationContext(struct _ACTIVATION_CONTEXT *ActivationContext) {
			TpSetCallbackActivationContext(env, ActivationContext);
		}
		__forceinline void setNoActivationContext() {
			TpSetCallbackNoActivationContext(env);
		}
		__forceinline void setFinalizationCallback(PTP_SIMPLE_CALLBACK  pFinalizationCallback) {
			TpSetCallbackFinalizationCallback(env, pFinalizationCallback);
		}
		__forceinline BOOL trySubmit(PTP_SIMPLE_CALLBACK pfns, PVOID pv) {
			return macroDebugLastError(TrySubmitThreadpoolCallback(pfns, pv, env));
		}

		template<typename T> __forceinline BOOL trySubmit(T &&pfns) {
			return macroDebugLastError(TrySubmitThreadpoolCallback(static_cast<PTP_SIMPLE_CALLBACK>(pfns), nullptr, env));
		}

		template<typename T> __forceinline BOOL trySubmit(PVOID pv, T &&pfns) {
			return macroDebugLastError(TrySubmitThreadpoolCallback(static_cast<PTP_SIMPLE_CALLBACK>(pfns), pv, env));
		}

		__forceinline WaitQueue createWaitQueue() { return {env, true}; }
		__forceinline WorkQueue createWorkQueue(PTP_WORK_CALLBACK pfnwk, PVOID pv) { return {env, true, pfnwk, pv}; }
		template<typename T> __forceinline WorkQueue createWorkQueue(T &&pfnwk) { return {env, true, static_cast<PTP_WORK_CALLBACK>(pfnwk), nullptr}; }
		template<typename T> __forceinline WorkQueue createWorkQueue(PVOID pv, T &&pfnwk) { return {env, true, static_cast<PTP_WORK_CALLBACK>(pfnwk), pv}; }
		__forceinline WorkQueue createWorkQueue() { return {env, true}; }
		__forceinline TimerQueue createTimerQueue() { return {env, true}; }
		__forceinline IoQueue createIoQueue(HANDLE fl) { return {fl, env, true}; }
	};
};