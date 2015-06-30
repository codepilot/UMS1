#pragma once
class DebugStream {
public:
//DefaultHeapSlab<wchar_t *> wbuf;
	PrivateHeapSingleThread<wchar_t *> ph;
	PrivateHeapSingleThread<wchar_t *>::Slab wbuf;
	HANDLE outStream{nullptr};

	DebugStream(): wbuf{ph.alloc()} {
		*this << L".\\logs\\DebugStream-" << GetTickCount64() << L"-" << this << L".log";
		outStream = macroDebugLastError(CreateFileW(wbuf.address, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
		wbuf.free();
	}

	~DebugStream() {
		macroDebugLastError(CloseHandle(outStream));
	}


	static uint64_t powerOfTen(uint64_t p) {
		uint64_t ret{1ui64};
		for(uint64_t i{0ui64}; i < p; i++) {
			ret *= 10ui64;
		}
		return ret;
	}

	static uint64_t powerOfSixteen(uint64_t p) {
		uint64_t ret{1ui64};
		for(uint64_t i{0ui64}; i < p; i++) {
			ret *= 16ui64;
		}
		return ret;
	}

	DebugStream& operator <<(int32_t ival) {
		macroDebugLastError(1);
		wchar_t valStr[20 + 3]{0};
		wchar_t *valPtr{valStr};
		uint64_t curDst{0ui64};
		uint64_t val;
		if(!ival) {
			return *this << L"0";
		} else if(ival < 0i64) {
			val = 0i64 - ival;
			valPtr++[0] = '-';
		} else {
			val = ival;
		}
		for(uint64_t i = 0; i < 20ui64; i++) {
			uint64_t curNum{powerOfTen(19ui64 - i)};
			if(curDst || val >= curNum) {
				valPtr[curDst++] = '0' + static_cast<wchar_t>(val / curNum);
				val %= curNum;
			}
		}
		return *this << valStr;
	}

	DebugStream& operator <<(int64_t ival) {
		macroDebugLastError(1);
		wchar_t valStr[20 + 3]{0};
		wchar_t *valPtr{valStr};
		uint64_t curDst{0ui64};
		uint64_t val;
		if(!ival) {
			return *this << L"0";
		} else if(ival < 0i64) {
			val = 0i64 - ival;
			valPtr++[0] = '-';
		} else {
			val = ival;
		}
		for(uint64_t i = 0; i < 20ui64; i++) {
			uint64_t curNum{powerOfTen(19ui64 - i)};
			if(curDst || val >= curNum) {
				valPtr[curDst++] = '0' + static_cast<wchar_t>(val / curNum);
				val %= curNum;
			}
		}
		return *this << valStr;
	}

	DebugStream& operator <<(uint32_t val) {
		macroDebugLastError(1);
		wchar_t valStr[20 + 3]{0};
		wchar_t *valPtr{valStr};
		uint64_t curDst{0ui64};
		if(!val) {
			return *this << L"0";
		}

		for(uint64_t i = 0; i < 20ui64; i++) {
			uint64_t curNum{powerOfTen(19ui64 - i)};
			if(curDst || val >= curNum) {
				valPtr[curDst++] = '0' + static_cast<wchar_t>(val / curNum);
				val %= curNum;
			}
		}
		return *this << valStr;
	}

	DebugStream& operator <<(uint64_t val) {
		macroDebugLastError(1);
		wchar_t valStr[20 + 3]{0};
		wchar_t *valPtr{valStr};
		uint64_t curDst{0ui64};
		if(!val) {
			return *this << L"0";
		}

		for(uint64_t i = 0; i < 20ui64; i++) {
			uint64_t curNum{powerOfTen(19ui64 - i)};
			if(curDst || val >= curNum) {
				valPtr[curDst++] = '0' + static_cast<wchar_t>(val / curNum);
				val %= curNum;
			}
		}
		return *this << valStr;
	}

	DebugStream& operator <<(const void *ptr) {
		macroDebugLastError(1);
		wchar_t valStr[19]{0};
		wchar_t *valPtr = valStr;
		int32_t curDst{0};
		uint64_t val{reinterpret_cast<decltype(val)>(ptr)};
		valPtr++[0] = '0';
		valPtr++[0] = 'x';

		for(int32_t i = 0; i < 16; i++) {
			uint64_t curNum{powerOfSixteen(15 - i)};
			if(curDst || val >= curNum) {
				uint8_t charVal{static_cast<uint8_t>(val / curNum)};
				val %= curNum;
				if(charVal > 9) {
					valPtr[curDst++] = 'A' + (charVal - 10);
				} else {
					valPtr[curDst++] = '0' + charVal ;
				}
			}
		}

		return *this << valStr;
	}

	DebugStream& operator <<(const wchar_t *val) {
		wbuf.append(val, wcslen(val) * 2);
		return *this;
	}

	void flushLine() {
		*this << L"\n";
	//OutputDebugStringW(wbuf.address);
		DWORD numBytesWritten{0};
		wchar_t *src{wbuf.address};
		char *dst{reinterpret_cast<char*>(wbuf.address)};
		for(int i = 0; i < (wbuf.byteCount >> 1); i++) {
			dst[i] = static_cast<char>(src[i]);
		}
	//macroDebugLastError(WriteFile(outStream, wbuf.address, wbuf.byteCount, &numBytesWritten, nullptr));
		macroDebugLastError(WriteFile(outStream, dst, static_cast<DWORD>(wbuf.byteCount >> 1), &numBytesWritten, nullptr));
		wbuf.free();
	}

	void flushLineBreak() {
		*this << L"\n";
	//OutputDebugStringW(wbuf.address);
		DWORD numBytesWritten{0};
		macroDebugLastError(WriteFile(outStream, wbuf.address, static_cast<DWORD>(wbuf.byteCount), &numBytesWritten, nullptr));
		DebugBreak();
		wbuf.free();
	}

//void flushToDebug() {
//	OutputDebugStringW(debugSteam.str().c_str()); macroDebugLastError(1);
//	macroDebugLastError(1);
//	debugSteam.str(std::wstring());
//}
};
