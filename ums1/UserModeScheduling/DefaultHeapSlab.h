#pragma once

template<typename T>
class PrivateHeap {
public:
	HANDLE heap{nullptr};
	PrivateHeap(HANDLE _heap = nullptr): heap{_heap} { }
	~PrivateHeap() {
		if(heap) {
			macroDebugLastError(HeapDestroy(heap));
			heap = nullptr;
		}
	}
	class Slab {
	public:
		HANDLE heap{nullptr};
		T address{nullptr};
		size_t byteCount{0};
		size_t AllocedByteCount{0};
		Slab(): heap{nullptr}, byteCount{0}, AllocedByteCount{0}, address{nullptr} { }
		Slab(HANDLE _heap): heap{_heap}, byteCount{0}, AllocedByteCount{0}, address{nullptr} { }
		Slab(HANDLE _heap, size_t _byteCount): heap{_heap}, byteCount{_byteCount}, AllocedByteCount{_byteCount + 4}, address{nullptr} {
			address = macroDebugLastError(reinterpret_cast<T>(HeapAlloc(heap, HEAP_GENERATE_EXCEPTIONS|HEAP_ZERO_MEMORY, AllocedByteCount)));
		}
		void realloc(size_t _byteCount) {
			byteCount = _byteCount;
			AllocedByteCount = _byteCount + 4;
			if(address) {
			//SecureZeroMemory(reinterpret_cast<uint8_t *>(address) + oldByteCount, _byteCount);
				address = macroDebugLastError(reinterpret_cast<T>(HeapReAlloc(heap, HEAP_GENERATE_EXCEPTIONS|HEAP_ZERO_MEMORY, reinterpret_cast<LPVOID>(address), AllocedByteCount)));
				return;
			} else{
				address = macroDebugLastError(reinterpret_cast<T>(HeapAlloc(heap, HEAP_GENERATE_EXCEPTIONS|HEAP_ZERO_MEMORY, AllocedByteCount)));
			}
		}

		template<typename T1>
		void append(T1 _address, size_t _byteCount) {
			size_t oldByteCount = byteCount;
			realloc(byteCount + _byteCount);
			CopyMemory(reinterpret_cast<uint8_t *>(address) + oldByteCount, _address, _byteCount);
		}

		void free() {
			if(address) {
				macroDebugLastError(HeapFree(heap, 0, reinterpret_cast<LPVOID>(address)));
				address=nullptr;
			}
			byteCount = 0;
			AllocedByteCount = 0;
		}
		~Slab() {
			free();
		}
	};
	Slab alloc() { return {heap}; }
	Slab alloc(size_t _byteCount) { return {heap, _byteCount}; }
	T allocPtr(size_t _byteCount) {
		return macroDebugLastError(reinterpret_cast<T>(HeapAlloc(heap, HEAP_GENERATE_EXCEPTIONS|HEAP_ZERO_MEMORY, _byteCount)));
	}
};

template<typename T>
class PrivateHeapSingleThread: public PrivateHeap<T> {
public:
	PrivateHeapSingleThread(): PrivateHeap{HeapCreate(HEAP_GENERATE_EXCEPTIONS | HEAP_NO_SERIALIZE, 0, 0)} { macroDebugLastError(heap); }
};

template<typename T>
class PrivateHeapMultiThread: public PrivateHeap<T> {
public:
	PrivateHeapMultiThread(): PrivateHeap{HeapCreate(HEAP_GENERATE_EXCEPTIONS, 0, 0)} { macroDebugLastError(heap); }
};

template<typename T>
class DefaultHeapSlab {
public:
	T address{nullptr};
	size_t byteCount{0};
	size_t AllocedByteCount{0};
	DefaultHeapSlab(): byteCount{0}, AllocedByteCount{0}, address{nullptr} { }
	DefaultHeapSlab(size_t _byteCount): byteCount{_byteCount}, AllocedByteCount{_byteCount + 4}, address{nullptr} {
		address = macroDebugLastError(reinterpret_cast<T>(HeapAlloc(GetProcessHeap(), HEAP_GENERATE_EXCEPTIONS|HEAP_ZERO_MEMORY, AllocedByteCount)));
	}
	void realloc(size_t _byteCount) {
		byteCount = _byteCount;
		AllocedByteCount = _byteCount + 4;
		if(address) {
		//SecureZeroMemory(reinterpret_cast<uint8_t *>(address) + oldByteCount, _byteCount);
			address = macroDebugLastError(reinterpret_cast<T>(HeapReAlloc(GetProcessHeap(), HEAP_GENERATE_EXCEPTIONS|HEAP_ZERO_MEMORY, reinterpret_cast<LPVOID>(address), AllocedByteCount)));
			return;
		} else{
			address = macroDebugLastError(reinterpret_cast<T>(HeapAlloc(GetProcessHeap(), HEAP_GENERATE_EXCEPTIONS|HEAP_ZERO_MEMORY, AllocedByteCount)));
		}
	}

	template<typename T1>
	void append(T1 _address, size_t _byteCount) {
		size_t oldByteCount = byteCount;
		realloc(byteCount + _byteCount);
		CopyMemory(reinterpret_cast<uint8_t *>(address) + oldByteCount, _address, _byteCount);
	}

	void free() {
		if(address) {
			macroDebugLastError(HeapFree(GetProcessHeap(), 0, reinterpret_cast<LPVOID>(address)));
			address=nullptr;
		}
		byteCount = 0;
		AllocedByteCount = 0;
	}
	~DefaultHeapSlab() {
		free();
	}
};
