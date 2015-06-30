#pragma once

class StartupInfo: public UMS_SCHEDULER_STARTUP_INFO {
public:
	BOOL enter(PUMS_COMPLETION_LIST _completionList, PUMS_SCHEDULER_ENTRY_POINT _schedulerProc, PVOID _schedulerParam) {
		UmsVersion = UMS_VERSION;
		CompletionList = _completionList;
		SchedulerProc = _schedulerProc;
		SchedulerParam = _schedulerParam;
		BOOL ret{FALSE};
		macroDebugLastError(ret=EnterUmsSchedulingMode(this));
		return ret;
	}
};