#include "common.h"
#include <tchar.h>
#include <commdlg.h>
#include <shlobj.h>
#include "file_stream_info.h"
#include "resource.h"

#pragma comment(lib, "comdlg32.lib")

/* Private message: wParam=1 confirm (Enter), wParam=0 cancel (Esc/KillFocus) */
#define WM_STREAM_NAME_DONE  (WM_APP + 100)

/* State for the inline stream-name editor */
static WNDPROC pfnOldStreamNameEditProc = NULL;
static INT     iStreamNameEditItem      = -1;

static struct tagTransferColumn {
	INT		iId;
	UINT	nNameID;
	INT		iWidth;
	INT		iMask;
} g_ListViewColumn [] = {
	{ 0,	IDS_COL_NUM,	      30,  LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM | LVCF_FMT },
	{ 1,	IDS_COL_NAME,	  100, LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM | LVCF_FMT },
	{ 2,	IDS_COL_SIZE,	  75,  LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM | LVCF_FMT },
	{ 3,	IDS_COL_ALLOC, 80,  LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM | LVCF_FMT }
};


static VOID CALLBACK
private_InitListView(HWND hListView) {
	INT			iCount;
	LVCOLUMN	lvColumn;

    for ( iCount = 0; iCount < sizeof(g_ListViewColumn)/sizeof(g_ListViewColumn[0]); ++iCount ) {
	    ZeroMemory( &lvColumn, sizeof(lvColumn) );
	    lvColumn.mask = g_ListViewColumn[iCount].iMask;
	    lvColumn.iSubItem = g_ListViewColumn[iCount].iId;
	    lvColumn.pszText = (LPTSTR)ResStr(g_ListViewColumn[iCount].nNameID);
	    lvColumn.cx = g_ListViewColumn[iCount].iWidth;
	    lvColumn.fmt = LVCFMT_CENTER;
	    ListView_InsertColumn( hListView, iCount, &lvColumn );
    }
    ListView_SetExtendedListViewStyleEx( hListView, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT );
    ListView_SetExtendedListViewStyleEx( hListView, LVS_EX_GRIDLINES, LVS_EX_GRIDLINES );
    ListView_SetExtendedListViewStyleEx( hListView, LVS_EX_SINGLEROW, LVS_EX_SINGLEROW );
}

/* ------------------------------------------------------------------ */
/* Inline stream-name edit control                                      */
/* ------------------------------------------------------------------ */

/* Subclassed WndProc: intercepts Enter / Escape / KillFocus. */
static LRESULT CALLBACK
StreamNameEditProc(HWND hEdit, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    HWND hDlg = (HWND)(LONG_PTR)GetWindowLongPtr(hEdit, GWLP_USERDATA);

    switch (uMsg) {
    case WM_GETDLGCODE:
        return DLGC_WANTALLKEYS;
    case WM_KEYDOWN:
        if (wParam == VK_RETURN) {
            PostMessage(hDlg, WM_STREAM_NAME_DONE, 1, 0);
            return 0;
        }
        if (wParam == VK_ESCAPE) {
            PostMessage(hDlg, WM_STREAM_NAME_DONE, 0, 0);
            return 0;
        }
        break;
    case WM_KILLFOCUS:
        PostMessage(hDlg, WM_STREAM_NAME_DONE, 0, 0);
        break;
    }
    return CallWindowProc(pfnOldStreamNameEditProc, hEdit, uMsg, wParam, lParam);
}

/* Destroy the inline edit control safely. */
static VOID
private_DestroyStreamEdit(HWND *phEdit)
{
    if (*phEdit) {
        DestroyWindow(*phEdit);
        *phEdit = NULL;
        pfnOldStreamNameEditProc = NULL;
    }
}

/* Insert a blank row, create an EDIT control over the Name cell,
   subclass it, and give it focus. */
static VOID
private_BeginStreamCreate(HWND hDlg, HWND hListView, HINSTANCE hInst,
                          HWND *phStreamNameEdit)
{
    INT   iCount;
    RECT  rc;
    HWND  hEdit;
    LVITEM lvi;

    if (*phStreamNameEdit)   /* already editing */
        return;

    iCount = ListView_GetItemCount(hListView);

    ZeroMemory(&lvi, sizeof(lvi));
    lvi.mask    = LVIF_TEXT;
    lvi.iItem   = iCount;
    lvi.pszText = TEXT("");
    ListView_InsertItem(hListView, &lvi);
    ListView_SetItemText(hListView, iCount, 1, TEXT(""));
    ListView_EnsureVisible(hListView, iCount, FALSE);

    /* Position the edit over column 1 of the new row */
    ListView_GetSubItemRect(hListView, iCount, 1, LVIR_BOUNDS, &rc);

    hEdit = CreateWindowEx(
        0, WC_EDIT, TEXT(""),
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        rc.left, rc.top,
        rc.right  - rc.left,
        rc.bottom - rc.top,
        hListView, NULL, hInst, NULL);

    if (!hEdit) {
        ListView_DeleteItem(hListView, iCount);
        return;
    }

    SetWindowLongPtr(hEdit, GWLP_USERDATA, (LONG_PTR)hDlg);
    pfnOldStreamNameEditProc = (WNDPROC)(LONG_PTR)
        SetWindowLongPtr(hEdit, GWLP_WNDPROC, (LONG_PTR)StreamNameEditProc);

    iStreamNameEditItem = iCount;
    *phStreamNameEdit   = hEdit;
    SendMessage(hEdit, WM_SETFONT, (WPARAM)SendMessage(hListView, WM_GETFONT, 0, 0), TRUE);
    SetFocus(hEdit);
}

/* ================================================================== */
/* Task #18: Background I/O thread with IProgressDialog               */
/* ================================================================== */

typedef struct {
    TCHAR    szSrc[MAX_PATH * 2];
    TCHAR    szDst[MAX_PATH * 2];
    LONGLONG llTotal;
    LONGLONG llDone;
    DWORD    dwError;
    HANDLE   hCancel;
} STREAM_IO_PARAMS;

static DWORD WINAPI
private_StreamCopyThread(LPVOID lpParam)
{
    STREAM_IO_PARAMS *p = (STREAM_IO_PARAMS *)lpParam;
    HANDLE hSrc, hDst;
    BYTE   buf[64 * 1024];
    DWORD  dwRead, dwWritten;
    LARGE_INTEGER liSize;

    hSrc = CreateFile(p->szSrc, GENERIC_READ, FILE_SHARE_READ,
                      NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hSrc == INVALID_HANDLE_VALUE) { p->dwError = GetLastError(); return 1; }

    hDst = CreateFile(p->szDst, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                      NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hDst == INVALID_HANDLE_VALUE) {
        p->dwError = GetLastError(); CloseHandle(hSrc); return 1;
    }

    liSize.QuadPart = 0;
    GetFileSizeEx(hSrc, &liSize);
    p->llTotal = liSize.QuadPart;
    p->llDone  = 0;
    p->dwError = ERROR_SUCCESS;

    while (p->dwError == ERROR_SUCCESS
           && WaitForSingleObject(p->hCancel, 0) == WAIT_TIMEOUT
           && ReadFile(hSrc, buf, sizeof(buf), &dwRead, NULL)
           && dwRead > 0) {
        if (!WriteFile(hDst, buf, dwRead, &dwWritten, NULL) || dwWritten != dwRead)
            p->dwError = GetLastError();
        else
            p->llDone += dwRead;
    }
    if (WaitForSingleObject(p->hCancel, 0) != WAIT_TIMEOUT)
        p->dwError = ERROR_CANCELLED;

    CloseHandle(hDst);
    CloseHandle(hSrc);
    return p->dwError == ERROR_SUCCESS ? 0 : 1;
}

static BOOL
private_RunStreamCopy(HWND hParent, LPCTSTR szSrc, LPCTSTR szDst)
{
    IProgressDialog *pPD = NULL;
    STREAM_IO_PARAMS params;
    HANDLE hThread, hCancel;
    BOOL bOk;

    ZeroMemory(&params, sizeof(params));
    StringCchCopy(params.szSrc, ARRAYSIZE(params.szSrc), szSrc);
    StringCchCopy(params.szDst, ARRAYSIZE(params.szDst), szDst);

    hCancel = CreateEvent(NULL, TRUE, FALSE, NULL);
    params.hCancel = hCancel;

#ifdef __cplusplus
    if (SUCCEEDED(CoCreateInstance(CLSID_ProgressDialog, NULL, CLSCTX_INPROC_SERVER,
                                   IID_IProgressDialog, (void **)&pPD))) {
        pPD->SetTitle(ResStr(IDS_STREAM_CONFIRM_TITLE));
        pPD->SetLine(1, szSrc, FALSE, NULL);
        pPD->SetLine(2, szDst, FALSE, NULL);
        pPD->StartProgressDialog(hParent, NULL, PROGDLG_MODAL | PROGDLG_AUTOTIME, NULL);
    }
#else
    if (SUCCEEDED(CoCreateInstance(&CLSID_ProgressDialog, NULL, CLSCTX_INPROC_SERVER,
                                   &IID_IProgressDialog, (void **)&pPD))) {
        pPD->lpVtbl->SetTitle(pPD, ResStr(IDS_STREAM_CONFIRM_TITLE));
        pPD->lpVtbl->SetLine(pPD, 1, szSrc, FALSE, NULL);
        pPD->lpVtbl->SetLine(pPD, 2, szDst, FALSE, NULL);
        pPD->lpVtbl->StartProgressDialog(pPD, hParent,
            NULL, PROGDLG_MODAL | PROGDLG_AUTOTIME, NULL);
    }
#endif

    hThread = CreateThread(NULL, 0, private_StreamCopyThread, &params, 0, NULL);

    while (WaitForSingleObject(hThread, 100) == WAIT_TIMEOUT) {
        if (pPD) {
#ifdef __cplusplus
            if (pPD->HasUserCancelled())
                SetEvent(hCancel);
            if (params.llTotal > 0)
                pPD->SetProgress64(
                    (ULONGLONG)params.llDone, (ULONGLONG)params.llTotal);
#else
            if (pPD->lpVtbl->HasUserCancelled(pPD))
                SetEvent(hCancel);
            if (params.llTotal > 0)
                pPD->lpVtbl->SetProgress64(pPD,
                    (ULONGLONG)params.llDone, (ULONGLONG)params.llTotal);
#endif
        }
    }

    if (pPD) {
#ifdef __cplusplus
        pPD->StopProgressDialog();
        pPD->Release();
#else
        pPD->lpVtbl->StopProgressDialog(pPD);
        pPD->lpVtbl->Release(pPD);
#endif
    }
    CloseHandle(hThread);
    CloseHandle(hCancel);

    bOk = (params.dwError == ERROR_SUCCESS);
    if (!bOk && params.dwError != ERROR_CANCELLED)
        common_ShowError(hParent, ResStr(IDS_STREAM_ERR_WRITE));
    return bOk;
}

/* ================================================================== */

/* Write file contents to an NTFS alternate data stream.
   lpstrStreamName is the full stream name as listed in FILE_STREAM_INFO
   (e.g. ":backup:$DATA").  Shows an Open-file dialog and a confirmation
   message before writing.  Refreshes the stream list on success. */
static VOID
private_LoadFileToStream(HWND hDlg, LPCTSTR lpstrFilePath,
                         LPCTSTR lpstrStreamName, HANDLE hFile)
{
    TCHAR        szSource[MAX_PATH];
    TCHAR        szAdsPath[MAX_PATH * 2];
    LPTSTR       szConfirm = NULL;
    OPENFILENAME ofn;
    HANDLE       hSrc    = INVALID_HANDLE_VALUE;
    HANDLE       hStream = INVALID_HANDLE_VALUE;
    BYTE         buf[64 * 1024];
    DWORD        dwRead, dwWritten;
    BOOL         bOk = TRUE;

    /* Select the source file */
    ZeroMemory(&ofn, sizeof(ofn));
    szSource[0]       = TEXT('\0');
    ofn.lStructSize   = sizeof(ofn);
    ofn.hwndOwner     = hDlg;
    ofn.lpstrFile     = szSource;
    ofn.nMaxFile      = ARRAYSIZE(szSource);
    ofn.lpstrTitle    = ResStr(IDS_STREAM_OPEN_TITLE);
    ofn.lpstrFilter   = TEXT("Все файлы (*.*)\0*.*\0");
    ofn.Flags         = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (!GetOpenFileName(&ofn))
        return;

    /* Full ADS path */
    StringCchCopy(szAdsPath, ARRAYSIZE(szAdsPath), lpstrFilePath);
    StringCchCat (szAdsPath, ARRAYSIZE(szAdsPath), lpstrStreamName);

    /* Confirmation: allocate buffer sized to actual inputs */
    {
        int nChars = _scwprintf(ResStr(IDS_STREAM_CONFIRM_FMT),
                                szSource, lpstrStreamName) + 1;
        szConfirm = (LPTSTR)HeapAlloc(GetProcessHeap(), 0,
                                      (size_t)nChars * sizeof(TCHAR));
        if (szConfirm)
            swprintf_s(szConfirm, (size_t)nChars, ResStr(IDS_STREAM_CONFIRM_FMT),
                       szSource, lpstrStreamName);
    }
    if (!szConfirm || MessageBox(hDlg, szConfirm, ResStr(IDS_STREAM_CONFIRM_TITLE),
                                 MB_YESNO | MB_ICONQUESTION) != IDYES) {
        HeapFree(GetProcessHeap(), 0, szConfirm);
        return;
    }
    HeapFree(GetProcessHeap(), 0, szConfirm);
    szConfirm = NULL;

    /* Task #18: background thread + IProgressDialog */
    if (private_RunStreamCopy(hDlg, szSource, szAdsPath)) {
        MessageBox(hDlg, ResStr(IDS_STREAM_WRITE_OK),
                   ResStr(IDS_STREAM_WRITE_OK_TITLE), MB_OK | MB_ICONINFORMATION);
        SendMessage(hDlg, WM_SETFILE_HANDLE, 0, (LPARAM)hFile);
    }
    (void)hSrc; (void)hStream; (void)buf; (void)dwRead; (void)dwWritten; (void)bOk;
}

/* ------------------------------------------------------------------ */
/* Copy the contents of an NTFS stream to a regular file.
   lpstrFilePath — base file path (e.g. C:\dir\file.txt)
   lpstrStreamName — stream name as reported by FILE_STREAM_INFO
                     (e.g. ":mydata:$DATA")
   Shows a Save File dialog and writes stream bytes to the chosen file. */
static VOID CALLBACK
private_SaveStream(HWND hDlg, LPCTSTR lpstrFilePath, LPCTSTR lpstrStreamName)
{
    TCHAR        szAdsPath[MAX_PATH * 2];
    TCHAR        szSavePath[MAX_PATH];
    OPENFILENAME ofn;
    HANDLE       hStream  = INVALID_HANDLE_VALUE;
    HANDLE       hOutFile = INVALID_HANDLE_VALUE;
    BYTE         buf[64 * 1024];
    DWORD        dwRead, dwWritten;
    BOOL         bOk = TRUE;

    /* Build full ADS path: "C:\file.txt:streamname:$DATA" */
    StringCchCopy(szAdsPath, ARRAYSIZE(szAdsPath), lpstrFilePath);
    StringCchCat (szAdsPath, ARRAYSIZE(szAdsPath), lpstrStreamName);

    /* Show Save-As dialog */
    ZeroMemory(&ofn, sizeof(ofn));
    szSavePath[0]     = TEXT('\0');
    ofn.lStructSize   = sizeof(ofn);
    ofn.hwndOwner     = hDlg;
    ofn.lpstrFile     = szSavePath;
    ofn.nMaxFile      = ARRAYSIZE(szSavePath);
    ofn.lpstrTitle    = ResStr(IDS_STREAM_SAVE_TITLE);
    ofn.lpstrFilter   = TEXT("Все файлы (*.*)\0*.*\0");
    ofn.Flags         = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (!GetSaveFileName(&ofn))
        return; /* user cancelled */

    /* Task #18: background thread + IProgressDialog */
    if (private_RunStreamCopy(hDlg, szAdsPath, szSavePath))
        MessageBox(hDlg, ResStr(IDS_STREAM_SAVE_OK),
                   ResStr(IDS_STREAM_SAVE_OK_TITLE), MB_OK | MB_ICONINFORMATION);
    (void)hStream; (void)hOutFile; (void)buf; (void)dwRead; (void)dwWritten; (void)bOk;
}

static LPTSTR CALLBACK
private_GetStreamName(HANDLE hFile, LPCTSTR pPrefix) {
	PFILE_NAME_INFO pfni;
	DWORD           dwFileStructureLength = sizeof(FILE_NAME_INFO) + (MAX_PATH * sizeof(TCHAR));
	LPTSTR          lpstrFileName = NULL;
	DWORD           dwPrefixLength =  pPrefix == NULL ? 0 : lstrlen(pPrefix);


	pfni = (PFILE_NAME_INFO)LocalAlloc(LPTR, dwFileStructureLength);
	if (pfni == NULL)
		return NULL;
	RtlZeroMemory(pfni, dwFileStructureLength);
	if ( GetFileInformationByHandleEx( hFile, FileNameInfo, pfni, dwFileStructureLength) ) {
		/* FileNameLength is in bytes; allocate extra for ":" + prefix + null */
		DWORD cbFileName = pfni->FileNameLength + (dwPrefixLength + 4) * sizeof(TCHAR);
		DWORD cchFileName = cbFileName / sizeof(TCHAR);
		lpstrFileName = (LPTSTR)LocalAlloc(LPTR, cbFileName);
		if (lpstrFileName != NULL) {
			StringCchCopy(lpstrFileName, cchFileName, pfni->FileName);
			if (dwPrefixLength > 0) {
				StringCchCat(lpstrFileName, cchFileName, TEXT(":"));
				StringCchCat(lpstrFileName, cchFileName, pPrefix);
			}
		}
	}
	LocalFree(pfni);
    return lpstrFileName;
}


INT_PTR CALLBACK
fssi_WindowHandler(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static PFILE_STREAM_INFO pfsi = NULL;
    static DWORD     dwPfsiSize = 0;
    static HANDLE    hFile = NULL;
    static HWND      hListView = NULL;
    static HWND      hCreateButton = NULL;
    static HINSTANCE hInstance = NULL;
    static LPTSTR    lpstrFilePath = NULL;
    static HWND      hStreamNameEdit = NULL;
    switch ( uMsg ) {
  	    case WM_INITDIALOG: {
			hInstance = (HINSTANCE)lParam;
            hCreateButton = GetDlgItem(hDlg, IDC_CREATE_STREAM);
			hListView = GetDlgItem(hDlg, IDC_STREAM_LIST);
			dwPfsiSize = 4 * 1024;
			pfsi = (PFILE_STREAM_INFO)LocalAlloc(LPTR, dwPfsiSize);
			private_InitListView(hListView);
		    return TRUE;
        }
		case WM_COMMAND: {
			switch ( LOWORD(wParam) ) {
				case IDM_VIEW_STREAM: {
					INT iSelected = ListView_GetSelectionMark(hListView);
					if (iSelected >= 0 && lpstrFilePath) {
						TCHAR szStreamName[MAX_PATH + 64];
						TCHAR szAdsPath[MAX_PATH * 2];
						HANDLE hStream;
						BYTE  buf[65536];
						DWORD dwRead = 0;
						BOOL  bIsText = FALSE;
						LPTSTR lpContent = NULL;
						HWND hViewer;
						
						ListView_GetItemText(hListView, iSelected, 1, szStreamName, ARRAYSIZE(szStreamName));
						StringCchCopy(szAdsPath, ARRAYSIZE(szAdsPath), lpstrFilePath);
						StringCchCat(szAdsPath, ARRAYSIZE(szAdsPath), szStreamName);
						
						hStream = CreateFile(szAdsPath, GENERIC_READ, FILE_SHARE_READ,
						                    NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
						if (hStream == INVALID_HANDLE_VALUE) {
							common_ShowError(hDlg, ResStr(IDS_STREAM_ERR_OPEN));
							break;
						}
						ReadFile(hStream, buf, sizeof(buf) - 2, &dwRead, NULL);
						buf[dwRead] = 0; buf[dwRead + 1] = 0;
						CloseHandle(hStream);
						
						/* Detect UTF-16 LE BOM */
						if (dwRead >= 2 && buf[0] == 0xFF && buf[1] == 0xFE) {
							bIsText = TRUE;
							lpContent = (LPTSTR)(buf + 2);
						} else {
							DWORD i2; bIsText = TRUE;
							for (i2 = 0; i2 < dwRead && bIsText; ++i2) {
								BYTE bv = buf[i2];
								if (bv < 0x20 && bv != 0x09 && bv != 0x0A && bv != 0x0D)
									bIsText = FALSE;
							}
						}
						
						if (bIsText && !lpContent) {
							int cch2 = MultiByteToWideChar(CP_UTF8, 0, (LPCSTR)buf, dwRead, NULL, 0) + 1;
							lpContent = (LPTSTR)HeapAlloc(GetProcessHeap(), 0, cch2 * sizeof(TCHAR));
							if (lpContent) MultiByteToWideChar(CP_UTF8, 0, (LPCSTR)buf, dwRead, lpContent, cch2);
						}
						
						if (!bIsText) {
							DWORD nLines = (dwRead + 15) / 16;
							DWORD cchHex = nLines * 80 + 4;
							DWORD jh, offset;
							lpContent = (LPTSTR)HeapAlloc(GetProcessHeap(), 0, cchHex * sizeof(TCHAR));
							if (lpContent) {
								LPTSTR p = lpContent; *p = 0;
								for (offset = 0; offset < dwRead; offset += 16) {
									DWORD n = ((offset + 16) <= dwRead) ? 16 : (dwRead - offset);
									DWORD used = (DWORD)(p - lpContent);
									p += StringCchPrintf(p, cchHex - used, TEXT("%04X  "), offset);
									for (jh = 0; jh < 16; ++jh) {
										used = (DWORD)(p - lpContent);
										if (jh < n) p += StringCchPrintf(p, cchHex-used, TEXT("%02X "), buf[offset+jh]);
										else { used=(DWORD)(p-lpContent); StringCchPrintf(p,cchHex-used,TEXT("   ")); p+=3; }
									}
									*p++ = TEXT(' '); *p++ = TEXT(' ');
									for (jh = 0; jh < n; ++jh) {
										BYTE bv = buf[offset+jh];
										*p++ = (bv >= 0x20 && bv < 0x7F) ? (TCHAR)bv : TEXT('.');
									}
									*p++ = TEXT('\r'); *p++ = TEXT('\n');
								}
								*p = TEXT('\0');
							}
						}
						
						/* Show viewer dialog */
						hViewer = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_STREAM_VIEWER), hDlg, NULL);
						if (hViewer) {
							TCHAR szTitle[MAX_PATH + 64];
							HWND hEdit = GetDlgItem(hViewer, IDC_STREAM_VIEWER_EDIT);
							HWND hMode = GetDlgItem(hViewer, IDC_STREAM_VIEWER_MODE);
							StringCchPrintf(szTitle, ARRAYSIZE(szTitle), ResStr(IDS_STREAM_VIEW_TITLE), szStreamName);
							SetWindowText(hViewer, szTitle);
							if (hEdit && lpContent) SetWindowText(hEdit, lpContent);
							if (hMode) SetWindowText(hMode, bIsText ? ResStr(IDS_STREAM_MODE_TEXT) : ResStr(IDS_STREAM_MODE_HEX));
							ShowWindow(hViewer, SW_SHOW);
							{ MSG vmsg;
							  while (IsWindow(hViewer) && GetMessage(&vmsg, NULL, 0, 0)) {
								if (vmsg.hwnd == hViewer && vmsg.message == WM_COMMAND && LOWORD(vmsg.wParam) == IDCANCEL)
									DestroyWindow(hViewer);
								else { TranslateMessage(&vmsg); DispatchMessage(&vmsg); }
							  }
							}
						}
						if (lpContent && lpContent != (LPTSTR)(buf + 2))
							HeapFree(GetProcessHeap(), 0, lpContent);
					}
					break;
				}
				case IDM_SAVE_STREAM: {
					INT iSelected = ListView_GetSelectionMark(hListView);
					if (iSelected >= 0 && lpstrFilePath) {
						TCHAR szStreamName[MAX_PATH + 64];
						ListView_GetItemText(hListView, iSelected, 1,
						                    szStreamName, ARRAYSIZE(szStreamName));
						private_SaveStream(hDlg, lpstrFilePath, szStreamName);
					}
					break;
				}
				case IDM_DELETE_STREAM: {
					INT iSel = ListView_GetSelectionMark(hListView);
					if (iSel >= 0 && lpstrFilePath) {
						TCHAR szStreamName[MAX_PATH + 64];
						TCHAR szAdsPath[MAX_PATH * 2];
						LPTSTR szConfirm = NULL;
						int nDelChars;
						ListView_GetItemText(hListView, iSel, 1, szStreamName, ARRAYSIZE(szStreamName));
						nDelChars = _scwprintf(ResStr(IDS_STREAM_DEL_FMT), szStreamName) + 1;
						szConfirm = (LPTSTR)HeapAlloc(GetProcessHeap(), 0, (size_t)nDelChars * sizeof(TCHAR));
						if (szConfirm) swprintf_s(szConfirm, (size_t)nDelChars, ResStr(IDS_STREAM_DEL_FMT), szStreamName);
						if (!szConfirm || MessageBox(hDlg, szConfirm, ResStr(IDS_STREAM_DEL_TITLE), MB_YESNO | MB_ICONWARNING) == IDYES) {
							StringCchCopy(szAdsPath, ARRAYSIZE(szAdsPath), lpstrFilePath);
							StringCchCat(szAdsPath, ARRAYSIZE(szAdsPath), szStreamName);
							if (DeleteFile(szAdsPath)) {
								MessageBox(hDlg, ResStr(IDS_STREAM_DEL_OK), ResStr(IDS_STREAM_DEL_OK_TITLE), MB_OK | MB_ICONINFORMATION);
								SendMessage(hDlg, WM_SETFILE_HANDLE, 0, (LPARAM)hFile);
							} else {
								common_ShowError(hDlg, ResStr(IDS_STREAM_ERR_OPEN));
							}
						}
						HeapFree(GetProcessHeap(), 0, szConfirm);
					}
					break;
				}
				case IDM_COPY_SELECTION: {
					INT nItems = ListView_GetItemCount(hListView);
					INT nCols  = 4;
					INT ci, cj;
					HGLOBAL hMem = NULL;
					DWORD cch = 0;
					for (ci = 0; ci < nItems; ++ci) {
						if (ListView_GetItemState(hListView, ci, LVIS_SELECTED) & LVIS_SELECTED) {
							for (cj = 0; cj < nCols; ++cj) {
								TCHAR tmp[1024]; tmp[0]=0;
								ListView_GetItemText(hListView, ci, cj, tmp, ARRAYSIZE(tmp));
								cch += (DWORD)lstrlen(tmp) + 2;
							}
							cch += 3;
						}
					}
					if (cch == 0) break;
					cch += 4;
					hMem = GlobalAlloc(GMEM_MOVEABLE, cch * sizeof(TCHAR));
					if (!hMem) break;
					{ LPTSTR p = (LPTSTR)GlobalLock(hMem);
					  if (p) {
						p[0] = 0;
						for (ci = 0; ci < nItems; ++ci) {
							if (ListView_GetItemState(hListView, ci, LVIS_SELECTED) & LVIS_SELECTED) {
								for (cj = 0; cj < nCols; ++cj) {
									TCHAR tmp[1024]; tmp[0]=0;
									ListView_GetItemText(hListView, ci, cj, tmp, ARRAYSIZE(tmp));
									StringCchCat(p, cch, tmp);
									if (cj < nCols-1) StringCchCat(p, cch, TEXT("\t"));
								}
								StringCchCat(p, cch, TEXT("\r\n"));
							}
						}
						GlobalUnlock(hMem);
					  }
					}
					if (OpenClipboard(hDlg)) {
						EmptyClipboard();
						SetClipboardData(CF_UNICODETEXT, hMem);
						CloseClipboard();
					} else {
						GlobalFree(hMem);
					}
					break;
				}
                case IDC_CREATE_STREAM: {
                    /* Button: add blank row + inline name editor */
                    private_BeginStreamCreate(hDlg, hListView, hInstance,
                                             &hStreamNameEdit);
                    break;
                }
				case IDM_CREATE_STREAM: {
                    /* Context menu: write a file's contents to selected stream */
                    INT iSel = ListView_GetSelectionMark(hListView);
                    if (iSel < 0 || !lpstrFilePath) {
                        MessageBox(hDlg,
                            ResStr(IDS_STREAM_NO_SEL_MSG),
                            ResStr(IDS_STREAM_NO_SEL_TITLE), MB_OK | MB_ICONINFORMATION);
                        break;
                    }
                    {
                        TCHAR szStreamName[MAX_PATH + 64];
                        ListView_GetItemText(hListView, iSel, 1,
                                            szStreamName, ARRAYSIZE(szStreamName));
                        if (szStreamName[0] == TEXT('\0')) {
                            MessageBox(hDlg,
                                ResStr(IDS_STREAM_NO_SEL_MSG),
                                ResStr(IDS_STREAM_NO_SEL_TITLE), MB_OK | MB_ICONINFORMATION);
                            break;
                        }
                        private_LoadFileToStream(hDlg, lpstrFilePath,
                                                 szStreamName, hFile);
                    }
					break;
				}
			}
			break;
		}
		case WM_NOTIFY: {
			LPNMHDR hdr = (LPNMHDR)lParam;
			switch (hdr->code) { 
		        case NM_RCLICK: {
					/** Or WM_MENUCOMMAND */
					if ( hdr->idFrom == IDC_STREAM_LIST ) {
						HMENU hStreamMenu;
			            BOOL bView = FALSE;
                        UINT uFlags;
						POINT p;

						GetCursorPos(&p);
			            uFlags = MF_BYPOSITION | MF_STRING | MF_POPUP;
			            bView = ListView_GetSelectedCount(hListView) != 0;
			            if ( !bView )
                            uFlags |= MF_DISABLED;
						if ( ListView_GetItemCount(hListView) == 0 )
							break;
						hStreamMenu = CreatePopupMenu();
                        AppendMenu(hStreamMenu, uFlags, IDM_VIEW_STREAM,    ResStr(IDS_STREAM_MENU_VIEW));
                        AppendMenu(hStreamMenu, uFlags, IDM_SAVE_STREAM,    ResStr(IDS_STREAM_MENU_SAVE));
                        AppendMenu(hStreamMenu, uFlags, IDM_DELETE_STREAM,  ResStr(IDS_STREAM_MENU_DELETE));
                        AppendMenu(hStreamMenu, uFlags, IDM_COPY_SELECTION, ResStr(IDS_MENU_COPY));
                        AppendMenu(hStreamMenu, MF_SEPARATOR, 0, NULL);
                        AppendMenu(hStreamMenu, MF_BYPOSITION | MF_STRING,  IDM_CREATE_STREAM, ResStr(IDS_STREAM_MENU_CREATE));
                        SetForegroundWindow(hListView);
                        TrackPopupMenu(hStreamMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, p.x, p.y, 0, hListView, NULL);
			            DestroyMenu(hStreamMenu);
					}
					break;
			    }
				case NM_CLICK: {
					if (hdr->idFrom == IDC_STREAM_LIST) {
						INT iSelected;


						iSelected = ListView_GetSelectionMark(hListView);
						if (iSelected >= 0) {
							TCHAR szName[MAX_PATH + 64];

							ListView_GetItemText(hListView, iSelected, 1, szName, ARRAYSIZE(szName));
							CreateToolTip(IDC_STREAM_LIST, hDlg, hInstance, szName);
						}
					}
					break;
				}
			}
			break;
		}
      case WM_KEYDOWN: {
          if (GetAsyncKeyState(VK_CONTROL) & 0x8000) {
              if (wParam == 0x41) { /* Ctrl+A: select all */
                  INT n = ListView_GetItemCount(hListView), ka;
                  for (ka = 0; ka < n; ++ka)
                      ListView_SetItemState(hListView, ka, LVIS_SELECTED, LVIS_SELECTED);
              } else if (wParam == 0x43) { /* Ctrl+C: copy */
                  SendMessage(hDlg, WM_COMMAND, IDM_COPY_SELECTION, 0);
              }
          }
          break;
      }
      case WM_STREAM_NAME_DONE: {
          /* Posted by StreamNameEditProc when the user presses Enter (wParam=1)
             or Escape/KillFocus (wParam=0). */
          if (!hStreamNameEdit)   /* already handled (Enter then KillFocus) */
              break;

          if (wParam == 1) {
              /* Confirm: read the name and try to create the stream */
              TCHAR szName[MAX_PATH];
              GetWindowText(hStreamNameEdit, szName, ARRAYSIZE(szName));

              private_DestroyStreamEdit(&hStreamNameEdit);

              /* Task #13: validate ADS stream name */
              if (szName[0] == TEXT('$')) {
                  MessageBox(hDlg, ResStr(IDS_STREAM_INVALID_NAME),
                             ResStr(IDS_STREAM_NO_SEL_TITLE), MB_OK | MB_ICONWARNING);
                  if (iStreamNameEditItem >= 0) {
                      ListView_DeleteItem(hListView, iStreamNameEditItem);
                      iStreamNameEditItem = -1;
                  }
                  break;
              }
              {
                  static const TCHAR szForbidden[] = TEXT(":/\\*?\"<>|");
                  const TCHAR *p = szName, *q;
                  BOOL bBad = FALSE;
                  for (; *p && !bBad; ++p)
                      for (q = szForbidden; *q; ++q)
                          if (*p == *q) { bBad = TRUE; break; }
                  if (bBad) {
                      MessageBox(hDlg, ResStr(IDS_STREAM_INVALID_NAME),
                                 ResStr(IDS_STREAM_NO_SEL_TITLE), MB_OK | MB_ICONWARNING);
                      if (iStreamNameEditItem >= 0) {
                          ListView_DeleteItem(hListView, iStreamNameEditItem);
                          iStreamNameEditItem = -1;
                      }
                      break;
                  }
              }

              if (szName[0] == TEXT('\0')) {
                  MessageBox(hDlg,
                      ResStr(IDS_STREAM_EMPTY_NAME_MSG),
                      ResStr(IDS_STREAM_NO_SEL_TITLE), MB_OK | MB_ICONWARNING);
                  /* Remove the blank row we inserted */
                  if (iStreamNameEditItem >= 0) {
                      ListView_DeleteItem(hListView, iStreamNameEditItem);
                      iStreamNameEditItem = -1;
                  }
                  break;
              }

              /* Build full ADS path: file + ":" + name + ":$DATA" */
              if (lpstrFilePath) {
                  TCHAR szAdsPath[MAX_PATH * 2];
                  HANDLE hStream;
                  StringCchCopy(szAdsPath, ARRAYSIZE(szAdsPath), lpstrFilePath);
                  StringCchCat (szAdsPath, ARRAYSIZE(szAdsPath), TEXT(":"));
                  StringCchCat (szAdsPath, ARRAYSIZE(szAdsPath), szName);
                  StringCchCat (szAdsPath, ARRAYSIZE(szAdsPath), TEXT(":$DATA"));

                  hStream = CreateFile(szAdsPath,
                                       GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                       NULL, CREATE_NEW,
                                       FILE_ATTRIBUTE_NORMAL, NULL);
                  if (hStream != INVALID_HANDLE_VALUE) {
                      CloseHandle(hStream);
                      /* Refresh the stream list */
                      SendMessage(hDlg, WM_SETFILE_HANDLE, 0, (LPARAM)hFile);
                  } else {
                      common_ShowError(hDlg, ResStr(IDS_STREAM_ERR_CREATE));
                      if (iStreamNameEditItem >= 0) {
                          ListView_DeleteItem(hListView, iStreamNameEditItem);
                          iStreamNameEditItem = -1;
                      }
                  }
              } else {
                  /* lpstrFilePath is NULL - no file selected */
                  MessageBox(hDlg,
                      ResStr(IDS_STREAM_NO_FILE_MSG),
                      ResStr(IDS_STREAM_NO_SEL_TITLE), MB_OK | MB_ICONWARNING);
                  if (iStreamNameEditItem >= 0) {
                      ListView_DeleteItem(hListView, iStreamNameEditItem);
                      iStreamNameEditItem = -1;
                  }
              }
          } else {
              /* Cancel */
              private_DestroyStreamEdit(&hStreamNameEdit);
              if (iStreamNameEditItem >= 0) {
                  ListView_DeleteItem(hListView, iStreamNameEditItem);
                  iStreamNameEditItem = -1;
              }
          }
          break;
      }
	  case WM_SETFILE_HANDLE: {
		  BOOL bClear;
		  hFile = (HANDLE)lParam;
		  if ( hFile && hFile != INVALID_HANDLE_VALUE ) {
			  BOOL bResult;
			  bClear = FALSE;
			  /* Grow the buffer until the API is satisfied (ERROR_MORE_DATA). */
			  for (;;) {
				  bResult = GetFileInformationByHandleEx(hFile, FileStreamInfo, pfsi, dwPfsiSize);
				  if (bResult)
					  break;
				  if (GetLastError() != ERROR_MORE_DATA)
					  break;
				  dwPfsiSize *= 2;
				  LocalFree(pfsi);
				  pfsi = (PFILE_STREAM_INFO)LocalAlloc(LPTR, dwPfsiSize);
				  if (!pfsi) {
					  bResult = FALSE;
					  break;
				  }
			  }
			  if ( bResult ) {
					LVITEM	lvItem;
					TCHAR   szSubBuffer[1024];
					INT nIndex;
                    DWORD dwOffset;
					PFILE_STREAM_INFO pInfo;

					dwOffset = 0;
					nIndex = 0;
                    pInfo = pfsi;
					ListView_DeleteAllItems(hListView);
					ZeroMemory( &lvItem, sizeof(lvItem) );
	                lvItem.mask = LVIF_TEXT | LVIF_PARAM | LVIF_STATE; 
	                lvItem.state = 0; 
	                lvItem.stateMask = 0;
					while (TRUE) {
						lvItem.iItem = nIndex;
		                lvItem.iSubItem = 0;
						StringCchPrintf( szSubBuffer, sizeof(szSubBuffer)/sizeof(szSubBuffer[0]), TEXT("%d"), nIndex);
		                lvItem.pszText = szSubBuffer;
		                lvItem.cchTextMax = lstrlen( szSubBuffer );
		                ListView_InsertItem( hListView, &lvItem );
                        /** Name */
						//StringCchPrintf( szSubBuffer, sizeof(szSubBuffer), TEXT("%*.*S"), pInfo->StreamNameLength / 2, pInfo->StreamNameLength / 2, pInfo->StreamName );
						StringCchPrintf( szSubBuffer, sizeof(szSubBuffer) / sizeof(szSubBuffer[0]), TEXT("%s"), pInfo->StreamName );
						ListView_SetItemText( hListView, nIndex, 1, szSubBuffer );
						/** Size */
						StringCchPrintf( szSubBuffer, sizeof(szSubBuffer) / sizeof(szSubBuffer[0]), TEXT("%I64u"), pInfo->StreamSize.QuadPart);
						ListView_SetItemText( hListView, nIndex, 2, szSubBuffer );
						/** Allocated size */
						StringCchPrintf( szSubBuffer, sizeof(szSubBuffer) / sizeof(szSubBuffer[0]), TEXT("%I64u"), pInfo->StreamAllocationSize.QuadPart);
						ListView_SetItemText( hListView, nIndex, 3, szSubBuffer );
						++nIndex;
						if ( !pInfo->NextEntryOffset )
							break;
						pInfo = (PFILE_STREAM_INFO)( (LPBYTE)pInfo + pInfo->NextEntryOffset );
					}
			  } else {
				  bClear = TRUE;
			  }
		  } else {
			  bClear = TRUE;
		  }

		  if ( bClear ) {
			  ListView_DeleteAllItems(hListView);
		  }

          EnableWindow(hCreateButton, hFile != NULL && hFile != INVALID_HANDLE_VALUE);
		  break;
	  }
	  case WM_SETFILE_NAME: {
		  if (lpstrFilePath) {
			  LocalFree(lpstrFilePath);
			  lpstrFilePath = NULL;
		  }
		  if (lParam) {
			  int cch = lstrlen((LPCTSTR)lParam) + 1;
			  lpstrFilePath = (LPTSTR)LocalAlloc(LPTR, cch * sizeof(TCHAR));
			  if (lpstrFilePath)
				  StringCchCopy(lpstrFilePath, cch, (LPCTSTR)lParam);
		  }
		  break;
	  }
	  case WM_RESETFILE_HANDLE: {
          hFile = NULL;
		  if (lpstrFilePath) {
			  LocalFree(lpstrFilePath);
			  lpstrFilePath = NULL;
		  }
		  break;
	  }
	  case WM_CLOSE:
	  case WM_DESTROY: {
		  LocalFree(pfsi);
		  if (lpstrFilePath) {
			  LocalFree(lpstrFilePath);
			  lpstrFilePath = NULL;
		  }
		  break;
	  }
  }
  return FALSE;
}