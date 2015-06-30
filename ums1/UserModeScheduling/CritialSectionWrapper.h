#pragma once

//#define FAKE_CRITICAL_SECTION
class CritialSectionWrapper {
#ifndef FAKE_CRITICAL_SECTION
	CRITICAL_SECTION criticalSection;
#endif
public:
	CritialSectionWrapper() {
#ifndef FAKE_CRITICAL_SECTION
		InitializeCriticalSectionEx(&criticalSection, 4000, CRITICAL_SECTION_NO_DEBUG_INFO);
#endif
	}

	~CritialSectionWrapper() {
#ifndef FAKE_CRITICAL_SECTION
		DeleteCriticalSection(&criticalSection);
#endif
	}

	template<class T>
	bool tryEnter(const T callback) {
#ifndef FAKE_CRITICAL_SECTION
		OutputDebugStringW(L"TryEnterCriticalSection ...\n");
		if(TryEnterCriticalSection(&criticalSection)) {
			OutputDebugStringW(L"TryEnterCriticalSection true\n");
#endif
			callback();
#ifndef FAKE_CRITICAL_SECTION
			LeaveCriticalSection(&criticalSection);
			OutputDebugStringW(L"LeaveCriticalSection\n");
#endif
			return true;
#ifndef FAKE_CRITICAL_SECTION
		} else {
			OutputDebugStringW(L"TryEnterCriticalSection false\n");
			return false;
		}
#endif
	}

	template<class T>
	void enter(const T&& callback) {
#ifndef FAKE_CRITICAL_SECTION
		OutputDebugStringW(L"EnterCriticalSection\n");
		EnterCriticalSection(&criticalSection);
#endif
		callback();
#ifndef FAKE_CRITICAL_SECTION
		LeaveCriticalSection(&criticalSection);
		OutputDebugStringW(L"LeaveCriticalSection\n");
#endif
	}

};
