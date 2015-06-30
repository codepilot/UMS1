#pragma once

#if 0

template<typename T>
class AtomicSinglyLinkedList {
public:
#if 0
  InitializeSListHead	Initializes the head of a singly linked list.
RtlInitializeSListHead	Initializes the head of a singly linked list. Applications should call InitializeSListHead instead.

  InterlockedFlushSList	Flushes the entire list of items in a singly linked list.
RtlInterlockedFlushSList	Flushes the entire list of items in a singly linked list. Applications should call InterlockedFlushSList instead.

  InterlockedPopEntrySList	Removes an item from the front of a singly linked list.
RtlInterlockedPopEntrySList	Removes an item from the front of a singly linked list. Applications should call InterlockedPopEntrySList instead.

  InterlockedPushEntrySList	Inserts an item at the front of a singly linked list.
RtlInterlockedPushEntrySList	Inserts an item at the front of a singly linked list. Applications should call InterlockedPushEntrySList instead.

InterlockedPushListSList		Inserts a singly-linked list at the front of another singly linked list.

InterlockedPushListSListEx	Inserts a singly-linked list at the front of another singly linked list. Access to the lists is synchronized on a multiprocessor system. This version of the method does not use the __fastcall calling convention

  QueryDepthSList	Retrieves the number of entries in the specified singly linked list.
RtlQueryDepthSList	Retrieves the number of entries in the specified singly linked list. Applications should call QueryDepthSList instead.

RtlFirstEntrySList	Retrieves the first entry in a singly linked list.

#endif
	__declspec(align(64)) SLIST_HEADER headerForwardPath;
	__declspec(align(64)) SLIST_HEADER headerFreePath;
	__declspec(align(64)) SLIST_HEADER headerReturnPath;

	typedef class Entry {
	public:
		SLIST_ENTRY entry{nullptr};
		T val;
		Entry(T &&_val): val{std::move(_val)} { }
	} *PEntry;
	PrivateHeapSingleThread<PEntry> heapEntries;

	AtomicSinglyLinkedList() {
		InitializeSListHead(&headerForwardPath);
		InitializeSListHead(&headerReturnPath);
		InitializeSListHead(&headerFreePath);
	}

	void pushForwardItem(T && val) {
		PEntry pEntry = heapEntries.allocPtr(sizeof(Entry));
		InterlockedPushEntrySList(&headerForwardPath, &pEntry->entry);
	}

	void pushReturnItem(PEntry pEntry) {
		InterlockedPushEntrySList(&headerReturnPath, &pEntry->entry);
	}

	PVOID pushForwardList() {
	}

	PVOID pushReturnList() {
	}

	PEntry tryPopForwardItem() {
		auto ret = reinterpret_cast<PEntry>(InterlockedPopEntrySList(&headerForwardPath));
		return ret;
	}

	PVOID tryPopReturnItem() {
		InterlockedPopEntrySList
		return nullptr;
	}

	Entry *waitPopForwardItem() {
		for(;;) {
			auto ret = tryPopForwardItem();
			if(ret) {
				return ret;
			} else {
				DebugBreak();
				continue;
			}
		}
	}

	PVOID waitPopReturnItem() {
		return nullptr;
	}

	PVOID tryPopForwardList() {
		InterlockedFlushSList
		return nullptr;
	}

	PVOID tryPopReturnList() {
		InterlockedFlushSList
		return nullptr;
	}

	PVOID waitPopForwardList() {
		return nullptr;
	}

	PVOID waitPopReturnList() {
		return nullptr;
	}
};

#else

template<typename T>
class AtomicSinglyLinkedList {
public:
	typedef class Entry {
	public:
		Entry *next{nullptr};
		T val;
		Entry() { }
		Entry(T &&_val): val{std::move(_val)} { }
	} *PEntry;


	class Head {
	public:
		__declspec(align(64)) volatile PEntry next{nullptr}; //LOW
		volatile __int64 seq{0}; //HIGH

		__forceinline void pushEntries(PEntry firstEntry, PEntry lastEntry) {
			lastEntry->next = nullptr;
			__int64 ExchangeLow{reinterpret_cast<__int64>(firstEntry)};
			__int64 ExchangeHigh{1};
			__int64 result[2]{0, 0};
			for(;;) {
				auto ret = _InterlockedCompareExchange128((volatile LONG64 *)&next, ExchangeHigh, ExchangeLow, result);
				if(ret) {
					return;
				} else {
					lastEntry->next = (PEntry)result[0];
					ExchangeHigh = result[1] + 1;
					continue;
				}
			}
		}

		__forceinline void pushEntry(PEntry entry) {
			pushEntries(entry, entry);
		}

		__forceinline PEntry popList() {
			return (PEntry)_InterlockedExchange64((volatile LONG64 *)&next, 0);
		}

		__forceinline bool itemsAvailable() {
			return cachedPopEntries || next;
		}
	};

	class HeadPrivatePop {
	public:
		__declspec(align(64)) volatile PEntry next{nullptr}; //LOW
		volatile __int64 seq{0}; //HIGH
		__declspec(align(64)) PEntry cachedPopEntries{nullptr};
		__declspec(align(64)) PEntry cachedPushEntriesFirst{nullptr};
		PEntry cachedPushEntriesLast{nullptr};

		__forceinline void cachedPushEntries(PEntry firstEntry, PEntry lastEntry) {
			if((cachedPushEntriesFirst && !cachedPushEntriesLast) || (!cachedPushEntriesFirst && cachedPushEntriesLast)) {
				DebugBreak();
			}
			if(cachedPushEntriesFirst) {
				lastEntry->next = cachedPushEntriesFirst;
				cachedPushEntriesFirst = firstEntry;
			} else {
				cachedPushEntriesFirst = firstEntry;
				cachedPushEntriesLast = lastEntry;
			}
		}

		__forceinline void cachedPushEntry(PEntry entry) {
			cachedPushEntries(entry, entry);
		}

		__forceinline void flushCachedPushEntries() {
			if(!cachedPushEntriesFirst) {
				return;
			}
			pushEntries(cachedPushEntriesFirst, cachedPushEntriesLast);
			cachedPushEntriesFirst = nullptr;
			cachedPushEntriesLast = nullptr;
		}

		__forceinline void pushEntries(PEntry firstEntry, PEntry lastEntry) {
			lastEntry->next = nullptr;
			__int64 ExchangeLow{reinterpret_cast<__int64>(firstEntry)};
			__int64 ExchangeHigh{1};
			__int64 result[2]{0, 0};
			for(;;) {
				auto ret = _InterlockedCompareExchange128((volatile LONG64 *)&next, ExchangeHigh, ExchangeLow, result);
				if(ret) {
					return;
				} else {
					lastEntry->next = (PEntry)result[0];
					ExchangeHigh = result[1] + 1;
					continue;
				}
			}
		}

		__forceinline void pushEntry(PEntry entry) {
			pushEntries(entry, entry);
		}

		__forceinline PEntry popList() {
			return (PEntry)_InterlockedExchange64((volatile LONG64 *)&next, 0);
		}

		__forceinline PEntry popEntry() {
			PEntry ret{nullptr};
			if(cachedPopEntries) {
				ret = cachedPopEntries;
				cachedPopEntries = ret->next;
				return ret;
			}
			cachedPopEntries = popList();
			if(cachedPopEntries) {
				ret = cachedPopEntries;
				cachedPopEntries = ret->next;
				return ret;
			}
			return nullptr;
		}

		__forceinline bool itemsAvailable() {
			return cachedPopEntries || next;
		}
	};

//PrivateHeapSingleThread<PEntry> heapEntries;
	PrivateHeapMultiThread<PEntry> heapEntries;
	__declspec(align(64)) HeadPrivatePop headerForwardPath;
	__declspec(align(64)) HeadPrivatePop headerAlternatePath;
//__declspec(align(64)) HeadPrivatePop headerFreePath;
//__declspec(align(64)) HeadPrivatePop headerReturnPath;

	__forceinline AtomicSinglyLinkedList() {
	}

	__forceinline bool forwardItemsAvailable() {
		return headerForwardPath.itemsAvailable();
	}

	__forceinline bool alternateItemsAvailable() {
		return headerAlternatePath.itemsAvailable();
	}

	__forceinline void flushPushForwordItems() {
		headerForwardPath.flushCachedPushEntries();
		WakeByAddressAll((PVOID)(&headerForwardPath));
	}

	__forceinline void flushPushAlternateItems() {
		headerAlternatePath.flushCachedPushEntries();
		WakeByAddressAll((PVOID)(&headerAlternatePath));
	}

	__forceinline void cachedPushForwardItem(T && val) {
		PEntry pEntry = heapEntries.allocPtr(sizeof(Entry));
		pEntry->val = val;
		headerForwardPath.cachedPushEntry(pEntry);
//	InterlockedPushEntrySList(&headerForwardPath, &pEntry->entry);
	}

	__forceinline void cachedPushAlternateItem(T && val) {
		PEntry pEntry = heapEntries.allocPtr(sizeof(Entry));
		pEntry->val = val;
		headerAlternatePath.cachedPushEntry(pEntry);
//	InterlockedPushEntrySList(&headerForwardPath, &pEntry->entry);
	}

	__forceinline void pushAlternateItem(T && val) {
		PEntry pEntry = heapEntries.allocPtr(sizeof(Entry));
		pEntry->val = val;
		headerAlternatePath.pushEntry(pEntry);
		WakeByAddressAll((PVOID)(&headerAlternatePath));
	}

	__forceinline void pushForwardItem(T && val) {
		PEntry pEntry = heapEntries.allocPtr(sizeof(Entry));
		pEntry->val = val;
		headerForwardPath.pushEntry(pEntry);
		WakeByAddressAll((PVOID)(&headerForwardPath));
//	InterlockedPushEntrySList(&headerForwardPath, &pEntry->entry);
	}

#if 0
	__forceinline void pushReturnItem(PEntry pEntry) {
		InterlockedPushEntrySList(&headerReturnPath, &pEntry->entry);
	}

	__forceinline PVOID pushForwardList() {
	}

	__forceinline PVOID pushReturnList() {
	}
#endif
#if 1
	__forceinline PEntry tryPopForwardItem() {
		auto ret = headerForwardPath.popEntry();
//	auto ret = reinterpret_cast<PEntry>(InterlockedPopEntrySList(&headerForwardPath));
		return ret;
	}
	__forceinline PEntry tryPopAlternateItem() {
		auto ret = headerAlternatePath.popEntry();
//	auto ret = reinterpret_cast<PEntry>(InterlockedPopEntrySList(&headerForwardPath));
		return ret;
	}
#endif
#if 0
	__forceinline PVOID tryPopReturnItem() {
		InterlockedPopEntrySList
		return nullptr;
	}

#endif
#if 1
	__forceinline __declspec(noinline) Entry *waitPopForwardItem() {
		for(;;) {
			auto ret = tryPopForwardItem();
			if(ret) {
				return ret;
			} else {
				__int64 CompareVariable{0};
				WaitOnAddress(&headerForwardPath, &CompareVariable, 8, INFINITE);
				continue;
			}
		}
	}
#endif
#if 0
	__forceinline PVOID waitPopReturnItem() {
		return nullptr;
	}

	__forceinline PVOID tryPopForwardList() {
		InterlockedFlushSList
		return nullptr;
	}

	__forceinline PVOID tryPopReturnList() {
		InterlockedFlushSList
		return nullptr;
	}

	__forceinline PVOID waitPopForwardList() {
		return nullptr;
	}

	__forceinline PVOID waitPopReturnList() {
		return nullptr;
	}
#endif
};

#endif