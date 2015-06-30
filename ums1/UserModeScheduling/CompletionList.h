#pragma once

class CompletionList {
public:
	PUMS_COMPLETION_LIST UmsCompletionList{nullptr};
	HANDLE completionEvent{nullptr};
	CompletionList() {
	//wprintf(L"CompletionList() start\n");
		macroDebugLastError(1);
		macroDebugLastError(CreateUmsCompletionList(&UmsCompletionList));
		macroDebugLastError(GetUmsCompletionListEvent(UmsCompletionList, &completionEvent));
	//wprintf(L"CompletionList() finish\n");
	}
	~CompletionList() {
	//wprintf(L"~CompletionList() start\n");

		macroDebugLastError(DeleteUmsCompletionList(UmsCompletionList));
	//wprintf(L"~CompletionList() finish\n");
	}
};