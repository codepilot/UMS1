#pragma once

class ThreadAttributeList {
public:
	DefaultHeapSlab<LPPROC_THREAD_ATTRIBUTE_LIST> tab;
	ThreadAttributeList(DWORD dwAttributeCount) {
		SIZE_T lpSize=0;
		macroDebugLastErrorExpected(ERROR_INSUFFICIENT_BUFFER, InitializeProcThreadAttributeList(nullptr, dwAttributeCount, 0, &lpSize));
		tab.realloc(lpSize);
		macroDebugLastError(InitializeProcThreadAttributeList(tab.address, dwAttributeCount, 0, &tab.byteCount));
	}
	~ThreadAttributeList() {
		DeleteProcThreadAttributeList(tab.address);
	}
	operator LPPROC_THREAD_ATTRIBUTE_LIST() { return tab.address; }
	void updateAttrib(DWORD Attribute, UMS_CREATE_THREAD_ATTRIBUTES val) {
		macroDebugLastError(UpdateProcThreadAttribute(
			tab.address,
			Attribute,
			PROC_THREAD_ATTRIBUTE_UMS_THREAD,
			&val,
			sizeof(val),
			nullptr,
			nullptr));
	}
};
