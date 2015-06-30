#include "stdafx.h"

LPFN_DISCONNECTEX DisconnectEx=0;
SOCKET sListen;

char *status200HelloWorld = "HTTP/1.1 200 Ok\r\nContent-Length: 12\r\nConnection: close\r\n\r\nHello World!";

#if 0
#include <km\Ntifs.h>
#include <km\Fltkernel.h>
#else

/*  START OF LIBUV SOURCE MATERIAL (SEE LIBUV-LICENSE)  */
namespace WinInternal {

typedef struct _FILE_BASIC_INFORMATION {
  LARGE_INTEGER CreationTime;
  LARGE_INTEGER LastAccessTime;
  LARGE_INTEGER LastWriteTime;
  LARGE_INTEGER ChangeTime;
  DWORD FileAttributes;
} FILE_BASIC_INFORMATION, *PFILE_BASIC_INFORMATION;

typedef struct _FILE_STANDARD_INFORMATION {
  LARGE_INTEGER AllocationSize;
  LARGE_INTEGER EndOfFile;
  ULONG         NumberOfLinks;
  BOOLEAN       DeletePending;
  BOOLEAN       Directory;
} FILE_STANDARD_INFORMATION, *PFILE_STANDARD_INFORMATION;

typedef struct _FILE_INTERNAL_INFORMATION {
  LARGE_INTEGER IndexNumber;
} FILE_INTERNAL_INFORMATION, *PFILE_INTERNAL_INFORMATION;

typedef struct _FILE_EA_INFORMATION {
  ULONG EaSize;
} FILE_EA_INFORMATION, *PFILE_EA_INFORMATION;

typedef struct _FILE_ACCESS_INFORMATION {
  ACCESS_MASK AccessFlags;
} FILE_ACCESS_INFORMATION, *PFILE_ACCESS_INFORMATION;

typedef struct _FILE_POSITION_INFORMATION {
  LARGE_INTEGER CurrentByteOffset;
} FILE_POSITION_INFORMATION, *PFILE_POSITION_INFORMATION;

typedef struct _FILE_MODE_INFORMATION {
  ULONG Mode;
} FILE_MODE_INFORMATION, *PFILE_MODE_INFORMATION;

typedef struct _FILE_ALIGNMENT_INFORMATION {
  ULONG AlignmentRequirement;
} FILE_ALIGNMENT_INFORMATION, *PFILE_ALIGNMENT_INFORMATION;

typedef struct _FILE_NAME_INFORMATION {
  ULONG FileNameLength;
  WCHAR FileName[1];
} FILE_NAME_INFORMATION, *PFILE_NAME_INFORMATION;

typedef struct _FILE_ALL_INFORMATION {
  FILE_BASIC_INFORMATION     BasicInformation;
  FILE_STANDARD_INFORMATION  StandardInformation;
  FILE_INTERNAL_INFORMATION  InternalInformation;
  FILE_EA_INFORMATION        EaInformation;
  FILE_ACCESS_INFORMATION    AccessInformation;
  FILE_POSITION_INFORMATION  PositionInformation;
  FILE_MODE_INFORMATION      ModeInformation;
  FILE_ALIGNMENT_INFORMATION AlignmentInformation;
  FILE_NAME_INFORMATION      NameInformation;
} FILE_ALL_INFORMATION, *PFILE_ALL_INFORMATION;

typedef struct _FILE_FS_VOLUME_INFORMATION {
  LARGE_INTEGER VolumeCreationTime;
  ULONG         VolumeSerialNumber;
  ULONG         VolumeLabelLength;
  BOOLEAN       SupportsObjects;
  WCHAR         VolumeLabel[1];
} FILE_FS_VOLUME_INFORMATION, *PFILE_FS_VOLUME_INFORMATION;

typedef struct _IO_STATUS_BLOCK {
  union {
    NTSTATUS Status;
    PVOID Pointer;
  } DUMMYUNIONNAME;
  ULONG_PTR Information;
} IO_STATUS_BLOCK, *PIO_STATUS_BLOCK;

typedef enum _FILE_INFORMATION_CLASS {
  FileDirectoryInformation = 1,
  FileFullDirectoryInformation,
  FileBothDirectoryInformation,
  FileBasicInformation,
  FileStandardInformation,
  FileInternalInformation,
  FileEaInformation,
  FileAccessInformation,
  FileNameInformation,
  FileRenameInformation,
  FileLinkInformation,
  FileNamesInformation,
  FileDispositionInformation,
  FilePositionInformation,
  FileFullEaInformation,
  FileModeInformation,
  FileAlignmentInformation,
  FileAllInformation,
  FileAllocationInformation,
  FileEndOfFileInformation,
  FileAlternateNameInformation,
  FileStreamInformation,
  FilePipeInformation,
  FilePipeLocalInformation,
  FilePipeRemoteInformation,
  FileMailslotQueryInformation,
  FileMailslotSetInformation,
  FileCompressionInformation,
  FileObjectIdInformation,
  FileCompletionInformation,
  FileMoveClusterInformation,
  FileQuotaInformation,
  FileReparsePointInformation,
  FileNetworkOpenInformation,
  FileAttributeTagInformation,
  FileTrackingInformation,
  FileIdBothDirectoryInformation,
  FileIdFullDirectoryInformation,
  FileValidDataLengthInformation,
  FileShortNameInformation,
  FileIoCompletionNotificationInformation,
  FileIoStatusBlockRangeInformation,
  FileIoPriorityHintInformation,
  FileSfioReserveInformation,
  FileSfioVolumeInformation,
  FileHardLinkInformation,
  FileProcessIdsUsingFileInformation,
  FileNormalizedNameInformation,
  FileNetworkPhysicalNameInformation,
  FileIdGlobalTxDirectoryInformation,
  FileIsRemoteDeviceInformation,
  FileAttributeCacheInformation,
  FileNumaNodeInformation,
  FileStandardLinkInformation,
  FileRemoteProtocolInformation,
  FileMaximumInformation
} FILE_INFORMATION_CLASS, *PFILE_INFORMATION_CLASS;
#endif

typedef enum _FS_INFORMATION_CLASS {
  FileFsVolumeInformation       = 1,
  FileFsLabelInformation        = 2,
  FileFsSizeInformation         = 3,
  FileFsDeviceInformation       = 4,
  FileFsAttributeInformation    = 5,
  FileFsControlInformation      = 6,
  FileFsFullSizeInformation     = 7,
  FileFsObjectIdInformation     = 8,
  FileFsDriverPathInformation   = 9,
  FileFsVolumeFlagsInformation  = 10,
  FileFsSectorSizeInformation   = 11
} FS_INFORMATION_CLASS, *PFS_INFORMATION_CLASS;

typedef NTSTATUS (NTAPI *sNtQueryInformationFile)
                 (HANDLE FileHandle,
                  PIO_STATUS_BLOCK IoStatusBlock,
                  PVOID FileInformation,
                  ULONG Length,
                  FILE_INFORMATION_CLASS FileInformationClass);

typedef NTSTATUS (NTAPI *sNtQueryVolumeInformationFile)
                 (HANDLE FileHandle,
                  PIO_STATUS_BLOCK IoStatusBlock,
                  PVOID FsInformation,
                  ULONG Length,
                  FS_INFORMATION_CLASS FsInformationClass);

//sRtlNtStatusToDosError pRtlNtStatusToDosError;
//sNtDeviceIoControlFile pNtDeviceIoControlFile;
sNtQueryInformationFile pNtQueryInformationFile;
//sNtSetInformationFile pNtSetInformationFile;
sNtQueryVolumeInformationFile pNtQueryVolumeInformationFile;
//sNtQueryDirectoryFile pNtQueryDirectoryFile;
//sNtQuerySystemInformation pNtQuerySystemInformation;
//#include <Winternl.h>
typedef BOOLEAN (WINAPI *sRtlTimeToSecondsSince1970) (_In_  PLARGE_INTEGER Time, _Out_ PULONG         ElapsedSeconds);

sRtlTimeToSecondsSince1970 pRtlTimeToSecondsSince1970;
};

__forceinline int fs__stat_handle(HANDLE handle) {
  WinInternal::FILE_ALL_INFORMATION file_info;
  WinInternal::FILE_FS_VOLUME_INFORMATION volume_info;
  NTSTATUS nt_status;
  WinInternal::IO_STATUS_BLOCK io_status;

  nt_status = WinInternal::pNtQueryInformationFile(handle,
                                      &io_status,
                                      &file_info,
                                      sizeof file_info,
                                      WinInternal::FileAllInformation);

  /* Buffer overflow (a warning status code) is expected here. */
	/*
  if (NT_ERROR(nt_status)) {
    SetLastError(pRtlNtStatusToDosError(nt_status));
    return -1;
  }
	*/

  nt_status = WinInternal::pNtQueryVolumeInformationFile(handle,
                                            &io_status,
                                            &volume_info,
                                            sizeof volume_info,
                                            WinInternal::FileFsVolumeInformation);

  /* Buffer overflow (a warning status code) is expected here. */
	/*
  if (io_status.Status == STATUS_NOT_IMPLEMENTED) {
    statbuf->st_dev = 0;
  } else if (NT_ERROR(nt_status)) {
    SetLastError(pRtlNtStatusToDosError(nt_status));
    return -1;
  } else {
    statbuf->st_dev = volume_info.VolumeSerialNumber;
  }
	*/

  /* Todo: st_mode should probably always be 0666 for everyone. We might also
   * want to report 0777 if the file is a .exe or a directory.
   *
   * Currently it's based on whether the 'readonly' attribute is set, which
   * makes little sense because the semantics are so different: the 'read-only'
   * flag is just a way for a user to protect against accidental deletion, and
   * serves no security purpose. Windows uses ACLs for that.
   *
   * Also people now use uv_fs_chmod() to take away the writable bit for good
   * reasons. Windows however just makes the file read-only, which makes it
   * impossible to delete the file afterwards, since read-only files can't be
   * deleted.
   *
   * IOW it's all just a clusterfuck and we should think of something that
   * makes slightly more sense.
   *
   * And uv_fs_chmod should probably just fail on windows or be a total no-op.
   * There's nothing sensible it can do anyway.
   */

  if (file_info.BasicInformation.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
    //statbuf->st_mode |= S_IFLNK;
    //if (fs__readlink_handle(handle, NULL, &statbuf->st_size) != 0)
      return -1;

  } else if (file_info.BasicInformation.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
    //statbuf->st_mode |= _S_IFDIR;
    //statbuf->st_size = 0;

  } else {
    //statbuf->st_mode |= _S_IFREG;
    //statbuf->st_size = file_info.StandardInformation.EndOfFile.QuadPart;
  }

  //if (file_info.BasicInformation.FileAttributes & FILE_ATTRIBUTE_READONLY)
    //statbuf->st_mode |= _S_IREAD | (_S_IREAD >> 3) | (_S_IREAD >> 6);
  //else
    //statbuf->st_mode |= (_S_IREAD | _S_IWRITE) | ((_S_IREAD | _S_IWRITE) >> 3) |
    //                    ((_S_IREAD | _S_IWRITE) >> 6);

  //FILETIME_TO_TIMESPEC(statbuf->st_atim, file_info.BasicInformation.LastAccessTime);
  //FILETIME_TO_TIMESPEC(statbuf->st_ctim, file_info.BasicInformation.ChangeTime);
  //FILETIME_TO_TIMESPEC(statbuf->st_mtim, file_info.BasicInformation.LastWriteTime);
  //FILETIME_TO_TIMESPEC(statbuf->st_birthtim, file_info.BasicInformation.CreationTime);

  //statbuf->st_ino = file_info.InternalInformation.IndexNumber.QuadPart;

  /* st_blocks contains the on-disk allocation size in 512-byte units. */
  //statbuf->st_blocks =
  //    file_info.StandardInformation.AllocationSize.QuadPart >> 9ULL;

  //statbuf->st_nlink = file_info.StandardInformation.NumberOfLinks;

  /* The st_blksize is supposed to be the 'optimal' number of bytes for reading
   * and writing to the disk. That is, for any definition of 'optimal' - it's
   * supposed to at least avoid read-update-write behavior when writing to the
   * disk.
   *
   * However nobody knows this and even fewer people actually use this value,
   * and in order to fill it out we'd have to make another syscall to query the
   * volume for FILE_FS_SECTOR_SIZE_INFORMATION.
   *
   * Therefore we'll just report a sensible value that's quite commonly okay
   * on modern hardware.
   */
  //statbuf->st_blksize = 2048;

  /* Todo: set st_flags to something meaningful. Also provide a wrapper for
   * chattr(2).
   */
  //statbuf->st_flags = 0;

  /* Windows has nothing sensible to say about these values, so they'll just
   * remain empty.
   */
  //statbuf->st_gid = 0;
  //statbuf->st_uid = 0;
  //statbuf->st_rdev = 0;
  //statbuf->st_gen = 0;

  return 0;
}

__forceinline void fs_stat() {
  HANDLE handle = CreateFileW(
		L".",
		FILE_READ_ATTRIBUTES,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		nullptr,
		OPEN_EXISTING,
		FILE_FLAG_BACKUP_SEMANTICS,
		nullptr);

	if(INVALID_HANDLE_VALUE == handle) {
		DebugBreak();
	}
	fs__stat_handle(handle);
	CloseHandle(handle);
}

/*  END OF LIBUV SOURCE MATERIAL (SEE LIBUV-LICENSE)  */

__declspec(align(64)) __int64 volatile dequeuedRequestCount{0};
__declspec(align(64)) unsigned __int64 volatile firstRequestStarted{0};
__declspec(align(64)) unsigned __int64 volatile lastRequestFinished{0};
__declspec(align(64)) unsigned __int64 volatile WaitVariable{0};
__declspec(align(64)) unsigned __int64 CompareVariable{0};
__declspec(align(64)) unsigned __int64 UndesiredValue{0};

__declspec(noinline) bool benchmark_sync_fs_stat() {
	for(int r = 0; r < numBenchmarkRepetitions; r++) {
		auto start = __rdtsc();
		for(int i = 0; i < numRequests; i++) {
			fs_stat();
		}
		auto finish = __rdtsc();
		wprintf(L"start to finish:                             %f seconds\n", static_cast<double_t>(finish - start) / 3300000000.0);
	}
	return false;
}

__declspec(noinline) int64_t getTsc1970epoch() {
//auto ftSpacing = KeQueryTimeIncrement();
	FILETIME ftNowPrecise{0,0};
	GetSystemTimePreciseAsFileTime(&ftNowPrecise);
	std::array<int64_t, 10000> ftList;
	std::array<int64_t, 10000> tscList;
	std::array<int64_t, 10000> ftSpacing;
	std::array<int64_t, 10000> tscSpacing;
	double_t averageFtSpacing{0};
	double_t averageTscSpacing{0};
	int64_t lastTick{*reinterpret_cast<volatile int64_t *>(0x7FFE0014ui32)};
	for(int i = 0; i < 10000; i++) {
		int64_t curTick;
		do { curTick = *reinterpret_cast<volatile int64_t *>(0x7FFE0014ui32); } while(curTick == lastTick);
		ftList[i] = curTick;
		tscList[i] = __rdtsc();
		lastTick = curTick;
	}
	for(int i = 1; i < 10000; i++) {
		ftSpacing[i] = ftList[i] - ftList[i - 1];
		averageFtSpacing += ftSpacing[i];
		tscSpacing[i] = tscList[i] - tscList[i - 1];
		averageTscSpacing += tscSpacing[i];
	}
	averageFtSpacing /= 9999.0;
	averageTscSpacing /= 9999.0;
	return 0;
}

//int64_t Tsc1970epoch{getTsc1970epoch()};

__forceinline int64_t fileTimeFromUTC() {
#if 1
	FILETIME sSystemTimeAsFileTime{0,0};
//GetSystemTimeAsFileTime(&sSystemTimeAsFileTime);
	uint64_t start = __rdtsc();
	uint64_t middle = __rdtsc();
//GetSystemTimePreciseAsFileTime(&sSystemTimeAsFileTime);
	uint64_t finish = __rdtsc();
	printf("start to finish = %I64u\n", finish - start);
	LARGE_INTEGER liSysFileTime{sSystemTimeAsFileTime.dwLowDateTime, sSystemTimeAsFileTime.dwHighDateTime};
	return liSysFileTime.QuadPart;
#else
	return *reinterpret_cast<volatile int64_t *>(0x7FFE0014ui32);
#endif
}

__forceinline int64_t fileTimeFromSystemTime() {
	SYSTEMTIME sysTime1970{1970,1,4,1,0,0,0,0};
	FILETIME fileType1970{0,0};
	macroDebugLastError(SystemTimeToFileTime(&sysTime1970, &fileType1970));
	LARGE_INTEGER liFileType1970{fileType1970.dwLowDateTime, fileType1970.dwHighDateTime};
	return liFileType1970.QuadPart;
}

const int64_t fileTimeEpoch1970{fileTimeFromSystemTime()};

__forceinline double_t msSince1970perf() {
	double_t retC = static_cast<double_t>(fileTimeFromUTC() - fileTimeEpoch1970) * 1.0e-4;
	return retC;
}

int64_t msSince1970() {
//FILETIME sSystemTimeAsFileTime{0,0};
//GetSystemTimeAsFileTime(&sSystemTimeAsFileTime);
//ULONG ulSecondsA{0};
//LARGE_INTEGER liSysFileTime{sSystemTimeAsFileTime.dwLowDateTime, sSystemTimeAsFileTime.dwHighDateTime};
	
//macroDebugLastError(pRtlTimeToSecondsSince1970(&liSysFileTime, &ulSecondsA));
//int64_t retA = static_cast<int64_t>(ulSecondsA) * 1000;

//SYSTEMTIME sysTime1970{1970,1,4,1,0,0,0,0};
//FILETIME fileType1970{0,0};
//macroDebugLastError(SystemTimeToFileTime(&sysTime1970, &fileType1970));
//LARGE_INTEGER liFileType1970{fileType1970.dwLowDateTime, fileType1970.dwHighDateTime};

//ULONG ulSecondsB{0};
//macroDebugLastError(pRtlTimeToSecondsSince1970(&liFileType1970, &ulSecondsB));
//int64_t retB = static_cast<int64_t>(ulSecondsB) * 1000;

	int64_t retC = (fileTimeFromUTC() - fileTimeEpoch1970) / 10000i64;
	return retC;
}

#if 0
__declspec(noinline) void smallestTSC_old1() {
	SetThreadAffinityMask(GetCurrentThread(), 1);
	uint64_t smallest = 0xFFFFFFFFFFFFFFFFui64;
	for(;;) {
		auto start = __rdtsc();
		auto finish = __rdtsc();
		auto diff = finish - start;
		if(diff < smallest) {
			smallest = diff;
			printf("smallest %I64u\n", smallest);
			if(smallest < 10) { break; }
		}
	}
}
__declspec(noinline) void smallestTSC_old2() {
	SetThreadAffinityMask(GetCurrentThread(), 1);
	int32_t smallest = 0x7FFFFFFFi32;
	for(;;) {
		int32_t start = __rdtsc();
		int32_t finish = __rdtsc();
		int32_t diff = finish - start;
		if(diff < smallest) {
			smallest = diff;
			printf("smallest %d\n", smallest);
			if(smallest < 10) { break; }
		}
	}
}
__declspec(noinline) void smallestTSC() {
//SetThreadAffinityMask(GetCurrentThread(), 1);
	uint64_t smallest = 0xFFFFFFFFFFFFFFFFui64;
	for(;;) {
		SwitchToThread();
		uint64_t start = __rdtsc();
		uint64_t finish = __rdtsc();
		uint64_t diff = finish - start;
		if(diff < smallest) {
			smallest = diff;
			printf("smallest %I64u\n", smallest);
			if(smallest < 10) { break; }
		}
	}
}
#endif

int __cdecl _tmain(int argc, _TCHAR* argv[]) {
//smallestTSC();
  HMODULE ntdll_module;
// HMODULE kernel32_module;

  ntdll_module = GetModuleHandleA("ntdll.dll");

	WinInternal::pNtQueryInformationFile = (WinInternal::sNtQueryInformationFile) GetProcAddress(ntdll_module, "NtQueryInformationFile");
  WinInternal::pNtQueryVolumeInformationFile = (WinInternal::sNtQueryVolumeInformationFile) GetProcAddress(ntdll_module, "NtQueryVolumeInformationFile");
	WinInternal::pRtlTimeToSecondsSince1970 = (WinInternal::sRtlTimeToSecondsSince1970) GetProcAddress(ntdll_module, "RtlTimeToSecondsSince1970");

	if(0) {
		std::array<double_t, 1000> counts;
		for(int i = 0; i < 1000; i++) {
			counts[i] = msSince1970perf();
		}
		for(int i = 0; i < 1000; i++) {
			printf("msSince1970perf():  %f\n", counts[i]);
		}
	}

#ifdef USE_SYNCHRONOUS
	wprintf(TEXT("benchmark_sync_fs_stat started\n"));
	benchmark_sync_fs_stat();
	wprintf(TEXT("benchmark_sync_fs_stat finished\n"));
#endif

#ifdef USE_NEW_THREAD_POOL
#ifdef USE_TEST_WEBSERVER
	wprintf(TEXT("test_new_thread_pool started\n"));
	test_new_thread_pool();
	wprintf(TEXT("test_new_thread_pool finished\n"));
#endif
	wprintf(TEXT("benchmark_ntp_workQueue_fs_stat started\n"));
	benchmark_ntp_workQueue_fs_stat();
	wprintf(TEXT("benchmark_ntp_workQueue_fs_stat finished\n"));

	wprintf(TEXT("benchmark_ntp_trySubmit_fs_stat started\n"));
	benchmark_ntp_trySubmit_fs_stat();
	wprintf(TEXT("benchmark_ntp_trySubmit_fs_stat finished\n"));

#endif

#ifdef USE_DIRECT_THREAD_POOL
#ifdef USE_TEST_WEBSERVER
	wprintf(TEXT("test_direct_thread_pool started\n"));
	test_direct_thread_pool();
	wprintf(TEXT("test_direct_thread_pool finished\n"));
#endif
	wprintf(TEXT("benchmark_dtp_fs_stat started\n"));
	benchmark_dtp_fs_stat();
	wprintf(TEXT("benchmark_dtp_fs_stat finished\n"));
#endif

#ifdef USE_USER_MODE_SCHEDULING
#ifdef USE_TEST_WEBSERVER
	wprintf(TEXT("test_user_mode_scheduling started\n"));
	test_user_mode_scheduling();
	wprintf(TEXT("test_user_mode_scheduling finished\n"));
#endif
	wprintf(TEXT("benchmark_ums_fs_stat started\n"));
	benchmark_ums_fs_stat();
	wprintf(TEXT("benchmark_ums_fs_stat finished\n"));
#endif

#ifdef USE_OLD_THREAD_POOL
#ifdef USE_TEST_WEBSERVER
	wprintf(TEXT("test_old_thread_pool started\n"));
	test_old_thread_pool();
	wprintf(TEXT("test_old_thread_pool finished\n"));
#endif
	wprintf(TEXT("benchmark_otp_fs_stat started\n"));
	benchmark_otp_fs_stat();
	wprintf(TEXT("benchmark_opt_fs_stat finished\n"));
#endif

#ifdef _DEBUG
	wprintf(L"Press any key to quit...");
	getchar();
#endif
	return 0;
}