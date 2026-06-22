#include <windows.h>
#include <ole2.h>
#include <commctrl.h>
#include <shlobj.h>
#include <strsafe.h>
#include "common.h"
#include "file_basic_info.h"
#include "file_standart_info.h"
#include "file_stream_info.h"
#include "file_id_info.h"
#include "file_exif_info.h"
#include "resource.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")

static struct tagButtonCtrl {
	WORD		wId;
	WORD		wResourceId;
	HIMAGELIST	hImageList;
	WORD		wAlign;
} g_ButtonCtrl [] = {
	{ IDC_OPENFILE,	IDI_FILEOPEN,	NULL, BUTTON_IMAGELIST_ALIGN_CENTER }
};

static struct tagTabCtrl {	
	FILE_INFO_BY_HANDLE_CLASS FileInfoClass;
	UINT                      nCaptionID;
	LPTSTR                    lpstrWindow;
	HWND                      hWnd;
	DLGPROC                   pfnProc;
} g_TabInfoCtrl [] = {
    { FileBasicInfo, IDS_TAB_BASIC, TEXT("FILE_BASIC_INFO"), NULL, fbi_WindowHandler },
    { FileStandardInfo, IDS_TAB_STANDARD, TEXT("FILE_STANDART_INFO"), NULL, fsi_WindowHandler },
    { FileStreamInfo, IDS_TAB_STREAMS, TEXT("FILE_STREAM_INFO"), NULL, fssi_WindowHandler },
    //{ FileNameInfo, TEXT("NameInfo"), TEXT(""), nullptr, nullptr },
    //{ FileCompressionInfo, TEXT("CompressionInfo"), TEXT(""), nullptr, nullptr },
    //{ FileAttributeTagInfo, TEXT("AttributeTagInfo"), TEXT(""), nullptr, nullptr },
    //{ FileIdBothDirectoryInfo, TEXT("IdBothDirectoryInfo"), TEXT(""), nullptr, nullptr },
    //{ FileIdBothDirectoryRestartInfo, TEXT("IdBothDirectoryRestartInfo"), TEXT(""), nullptr, nullptr },
    //{ FileDispositionInfo, TEXT("FileDispositionInfo"), TEXT(""), nullptr, nullptr },
    //{ FileRenameInfo, TEXT("FileRenameInfo"), TEXT(""), nullptr, nullptr },
    //{ FileAllocationInfo, TEXT("FileAllocationInfo"), TEXT(""), nullptr, nullptr },
    //{ FileEndOfFileInfo, TEXT("FileEndOfFileInfo"), TEXT(""), nullptr, nullptr },
    //{ FileIoPriorityHintInfo, TEXT("FileIoPriorityHintInfo"), TEXT(""), nullptr, nullptr }
#if (_WIN32_WINNT >= _WIN32_WINNT_WIN8)
    //,
    //{ FileStorageInfo, TEXT("FileStorageInfo"), TEXT(""), nullptr, nullptr },
    //{ FileAlignmentInfo, TEXT("FileAlignmentInfo"), TEXT(""), nullptr, nullptr },
    { FileIdInfo, IDS_TAB_ID, TEXT("FILE_ID_INFO"), NULL, fii_WindowHandler }//,
    //{ FileIdExtdDirectoryInfo, TEXT("FileIdExtdDirectoryInfo"), TEXT(""), nullptr, nullptr },
    //{ FileIdExtdDirectoryRestartInfo, TEXT("FileIdExtdDirectoryRestartInfo"), TEXT(""), nullptr, nullptr }
#endif
    ,{ MaximumFileInfoByHandleClass, IDS_TAB_EXIF, TEXT("FILE_EXIF_INFO"), NULL, fxi_WindowHandler }
};


/* ---------- Recent files (task #8) --------------------------------- */
#define RECENT_MAX      10
#define ID_RECENT_BASE  (ID_RECENT_FILE_BASE)
static const TCHAR g_szRecentKey[] = TEXT("Software\\FileInfo\\RecentFiles");

static VOID private_AddRecentFile(LPCTSTR lpszPath);
static VOID private_RebuildRecentMenu(HWND hDlg);
static VOID private_OpenRecentFile(HWND hDlg, HWND hTabCtrl, HWND hEditFile,
    HANDLE *phFile, LPTSTR *plpstrFileName, DWORD *pdwLen,
    HINSTANCE hInst, HWND *phTooltip, UINT uRecentIdx);

/* ---------- Long path helper (task #6) ------------------------------ */
static VOID
private_MakeLongPath(LPCTSTR lpszIn, LPTSTR lpszOut, DWORD cchOut)
{
    if (lstrlen(lpszIn) >= MAX_PATH && lpszIn[0] != TEXT('\\')) {
        StringCchPrintf(lpszOut, cchOut, TEXT("\\\\?\\%s"), lpszIn);
    } else {
        StringCchCopy(lpszOut, cchOut, lpszIn);
    }
}

static const TCHAR g_szShellKey[]    = TEXT("Software\\Classes\\*\\shell\\fileinfo");
static const TCHAR g_szShellCmdKey[] = TEXT("Software\\Classes\\*\\shell\\fileinfo\\command");

static BOOL
private_IsShellIntegrated(VOID)
{
    HKEY hKey;
    BOOL bResult = (RegOpenKeyEx(HKEY_CURRENT_USER, g_szShellKey, 0, KEY_READ, &hKey) == ERROR_SUCCESS);
    if (bResult) RegCloseKey(hKey);
    return bResult;
}

static BOOL
private_RegisterShell(HWND hDlg, HINSTANCE hInstance)
{
    TCHAR szExePath[MAX_PATH];
    TCHAR szCmd[MAX_PATH + 10];
    HKEY  hKey;
    LONG  lRet;

    GetModuleFileName((HMODULE)hInstance, szExePath, ARRAYSIZE(szExePath));
    StringCchPrintf(szCmd, ARRAYSIZE(szCmd), TEXT("\"%s\" \"%%1\""), szExePath);

    lRet = RegCreateKeyEx(HKEY_CURRENT_USER, g_szShellKey, 0, NULL,
                          REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL);
    if (lRet != ERROR_SUCCESS) { common_ShowError(hDlg, ResStr(IDS_ERR_REG_CREATE)); return FALSE; }
    RegSetValueEx(hKey, NULL, 0, REG_SZ, (BYTE*)TEXT("Open in FileInfo"),
                  (lstrlen(TEXT("Open in FileInfo")) + 1) * sizeof(TCHAR));
    RegCloseKey(hKey);

    lRet = RegCreateKeyEx(HKEY_CURRENT_USER, g_szShellCmdKey, 0, NULL,
                          REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL);
    if (lRet != ERROR_SUCCESS) { common_ShowError(hDlg, ResStr(IDS_ERR_REG_CREATE_CMD)); return FALSE; }
    RegSetValueEx(hKey, NULL, 0, REG_SZ, (BYTE*)szCmd, (lstrlen(szCmd) + 1) * sizeof(TCHAR));
    RegCloseKey(hKey);
    return TRUE;
}

static VOID
private_UnregisterShell(VOID)
{
    RegDeleteKey(HKEY_CURRENT_USER, g_szShellCmdKey);
    RegDeleteKey(HKEY_CURRENT_USER, g_szShellKey);
}

static LPTSTR g_lpstrCmdLineFile = NULL;

static const TCHAR g_szRegKey[] = TEXT("Software\\FileInfo");

static VOID
private_SaveWindowPos(HWND hDlg) {
    HKEY hKey;
    RECT rc;
    if (!GetWindowRect(hDlg, &rc)) return;
    if (RegCreateKeyEx(HKEY_CURRENT_USER, g_szRegKey, 0, NULL,
                       REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        DWORD val;
        val = (DWORD)rc.left; RegSetValueEx(hKey, TEXT("WinX"), 0, REG_DWORD, (LPBYTE)&val, sizeof(val));
        val = (DWORD)rc.top;  RegSetValueEx(hKey, TEXT("WinY"), 0, REG_DWORD, (LPBYTE)&val, sizeof(val));
        RegCloseKey(hKey);
    }
}

static BOOL
private_RestoreWindowPos(HWND hDlg) {
    HKEY  hKey;
    BOOL  bOk = FALSE;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, g_szRegKey, 0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS) {
        DWORD x, y, cb = sizeof(DWORD);
        if (RegQueryValueEx(hKey, TEXT("WinX"), NULL, NULL, (LPBYTE)&x, &cb) == ERROR_SUCCESS &&
            RegQueryValueEx(hKey, TEXT("WinY"), NULL, NULL, (LPBYTE)&y, &cb) == ERROR_SUCCESS) {
            /* Clamp to virtual screen so the window is never off-screen */
            RECT rc; GetWindowRect(hDlg, &rc);
            int w = rc.right - rc.left, h = rc.bottom - rc.top;
            int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
            int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
            int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
            int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
            int nx = ((int)x < vx) ? vx : ((int)x > vx + vw - w) ? vx + vw - w : (int)x;
            int ny = ((int)y < vy) ? vy : ((int)y > vy + vh - h) ? vy + vh - h : (int)y;
            SetWindowPos(hDlg, NULL, nx, ny, 0, 0,
                         SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            bOk = TRUE;
        }
        RegCloseKey(hKey);
    }
    return bOk;
}

INT_PTR CALLBACK
MainDialog(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
BOOL
SetDebugStatusForCurentProc(VOID);
BOOL
IsElevated(VOID);

int WINAPI
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	InitCommonControls();
    SetDebugStatusForCurentProc();
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    {
        int nArgs;
        LPWSTR *szArgList = CommandLineToArgvW(GetCommandLineW(), &nArgs);
        if (szArgList && nArgs > 1) {
            int cch = lstrlenW(szArgList[1]) + 1;
            g_lpstrCmdLineFile = (LPTSTR)LocalAlloc(LPTR, cch * sizeof(TCHAR));
            if (g_lpstrCmdLineFile)
                StringCchCopyW(g_lpstrCmdLineFile, cch, szArgList[1]);
        }
        if (szArgList) LocalFree(szArgList);
    }
    DialogBoxParam(hInstance, TEXT("MAINDIALOG"), NULL, MainDialog, (LPARAM)hInstance);
    if (g_lpstrCmdLineFile) { LocalFree(g_lpstrCmdLineFile); g_lpstrCmdLineFile = NULL; }
	return EXIT_SUCCESS;
}

static VOID CALLBACK
private_InitButtonImageList(HWND hDlg, HINSTANCE hInstance) {
	UINT				uIndex;
	BUTTON_IMAGELIST	bil;

	RtlSecureZeroMemory( &bil, sizeof(bil) );
	bil.margin.left = 0;
	bil.margin.top = 0;
	bil.margin.right = 0;
	bil.margin.bottom = 0;
	for ( uIndex = 0; uIndex < sizeof(g_ButtonCtrl)/sizeof(g_ButtonCtrl[0]); ++uIndex ) {
		g_ButtonCtrl[uIndex].hImageList = ImageList_Create( 16, 16, ILC_COLOR32, 1, 1 );
		ImageList_ReplaceIcon( g_ButtonCtrl[uIndex].hImageList, -1, LoadIcon( hInstance, MAKEINTRESOURCE(g_ButtonCtrl[uIndex].wResourceId) ) );
		bil.himl = g_ButtonCtrl[uIndex].hImageList;
		bil.uAlign = g_ButtonCtrl[uIndex].wAlign;
		Button_SetImageList( GetDlgItem( hDlg, g_ButtonCtrl[uIndex].wId ), &bil );
	}
}

static VOID CALLBACK
private_InitTabInfoCtrl(HWND hTabCtrl, HINSTANCE hInstance) {
	TCITEM tci;
	RECT rect;
	int iTab;
	TCHAR szCap[128];

	GetClientRect(hTabCtrl, &rect);
	RtlZeroMemory(&tci, sizeof(tci));
	tci.mask = TCIF_TEXT;
	for (iTab = 0; iTab < sizeof(g_TabInfoCtrl)/sizeof(g_TabInfoCtrl[0]); ++iTab) {
		LoadString(hInstance, g_TabInfoCtrl[iTab].nCaptionID, szCap, ARRAYSIZE(szCap));
		tci.pszText = szCap;
		TabCtrl_InsertItem(hTabCtrl, iTab, &tci);
        if (g_TabInfoCtrl[iTab].hWnd == NULL) {
			g_TabInfoCtrl[iTab].hWnd = CreateDialogParam(
				hInstance, 
				g_TabInfoCtrl[iTab].lpstrWindow, 
				hTabCtrl, 
				g_TabInfoCtrl[iTab].pfnProc, 
                (LPARAM)hInstance
			);
			MoveWindow( g_TabInfoCtrl[iTab].hWnd, rect.left + 2, rect.top + 21, rect.right - 5, rect.bottom - 24, TRUE);
			ShowWindow( g_TabInfoCtrl[iTab].hWnd, SW_HIDE );
		}
	}
	ShowWindow( g_TabInfoCtrl[0].hWnd, SW_SHOW );
}


HWND 
CreateToolTip(int iCtrlId, HWND hDlg, HINSTANCE hInstance, PTSTR pszText) {
  HWND hwndTool;
  HWND hwndTip;
  TOOLINFO toolInfo = { 0 };

  if (!iCtrlId || !hDlg || !pszText) {
    return FALSE;
  }
  hwndTool = GetDlgItem(hDlg, iCtrlId);
  hwndTip = CreateWindowEx(0, TOOLTIPS_CLASS, NULL,
                            WS_POPUP |TTS_ALWAYSTIP | TTS_BALLOON,
                            CW_USEDEFAULT, CW_USEDEFAULT,
                            CW_USEDEFAULT, CW_USEDEFAULT,
                            hDlg, NULL,
                            hInstance, NULL);
  
  if (!hwndTool || !hwndTip) {
      return (HWND)NULL;
  }                              
                            
  
  toolInfo.cbSize = sizeof(toolInfo);
  toolInfo.hwnd = hDlg;
  toolInfo.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
  toolInfo.uId = (UINT_PTR)hwndTool;
  toolInfo.lpszText = pszText;
  SendMessage(hwndTip, TTM_ADDTOOL, 0, (LPARAM)&toolInfo);

  return hwndTip;
}

HWND 
CreateToolTipForRect(HWND hwndParent, HINSTANCE hInstance, LPCTSTR lpstrText) {
	TOOLINFO ti = { 0 };
    HWND hwndTT = CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS, NULL, 
                                 WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP, 
                                 CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 
                                 hwndParent, nullptr, hInstance, nullptr);

    SetWindowPos(hwndTT, HWND_TOPMOST, 0, 0, 0, 0, 
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
   
    
    ti.cbSize   = sizeof(TOOLINFO);
    ti.uFlags   = TTF_SUBCLASS;
    ti.hwnd     = hwndParent;
    ti.hinst    = hInstance;
    ti.lpszText = (LPTSTR)lpstrText;
    
    GetClientRect (hwndParent, &ti.rect);
    SendMessage(hwndTT, TTM_ADDTOOL, 0, (LPARAM) (LPTOOLINFO) &ti);	
		return hwndTT;
}

/* IDropTarget -- OLE drag-drop incl. taskbar (task #7) */
class CDropTarget : public IDropTarget {
public:
    HWND m_hWnd;
    LONG m_cRef;
    CDropTarget() : m_hWnd(NULL), m_cRef(1) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppv) override {
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IDropTarget)) {
            *ppv = this; AddRef(); return S_OK;
        }
        *ppv = nullptr; return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef()  override { return InterlockedIncrement(&m_cRef); }
    ULONG STDMETHODCALLTYPE Release() override { return InterlockedDecrement(&m_cRef); }
    HRESULT STDMETHODCALLTYPE DragEnter(IDataObject*, DWORD, POINTL, DWORD *pe) override
        { *pe = DROPEFFECT_COPY; return S_OK; }
    HRESULT STDMETHODCALLTYPE DragOver(DWORD, POINTL, DWORD *pe) override
        { *pe = DROPEFFECT_COPY; return S_OK; }
    HRESULT STDMETHODCALLTYPE DragLeave() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE Drop(IDataObject *pDO, DWORD, POINTL, DWORD *pe) override {
        FORMATETC fe = { CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
        STGMEDIUM stg;
        *pe = DROPEFFECT_NONE;
        if (SUCCEEDED(pDO->GetData(&fe, &stg))) {
            HDROP hDrop = (HDROP)GlobalLock(stg.hGlobal);
            if (hDrop) {
                PostMessage(m_hWnd, WM_DROPFILES, (WPARAM)hDrop, 0);
                GlobalUnlock(stg.hGlobal);
                *pe = DROPEFFECT_COPY;
            }
        }
        return S_OK;
    }
};
static CDropTarget g_DropTarget;

static VOID CALLBACK
private_SetFileHandle(HWND hDlg, HWND hTabCtrl, HANDLE hFile, LPCTSTR lpcstrFileName) {
	int iCurTab;

	iCurTab = TabCtrl_GetCurSel(hTabCtrl);
	if ( iCurTab >= 0 && iCurTab < sizeof(g_TabInfoCtrl)/sizeof(g_TabInfoCtrl[0]) ) {
		if (g_TabInfoCtrl[iCurTab].hWnd) {
			SendMessage(g_TabInfoCtrl[iCurTab].hWnd, WM_SETFILE_HANDLE, (WPARAM)NULL, (LPARAM)hFile);
			SendMessage(g_TabInfoCtrl[iCurTab].hWnd, WM_SETFILE_NAME,   (WPARAM)NULL, (LPARAM)lpcstrFileName);
		}
	}
}

static VOID
private_EnumerateStream(LPCTSTR lpstrFileName) {
	WIN32_FIND_STREAM_DATA fsd;
	HANDLE hStream;

	hStream = FindFirstStreamW(lpstrFileName, FindStreamInfoStandard, &fsd, 0);
	if ( hStream == INVALID_HANDLE_VALUE )
		return;
	do {
        (void)0;
	} while (FindNextStreamW(hStream, &fsd));
	FindClose(hStream);
}

static HANDLE
private_OpenFile(HWND hDlg, HWND hTabCtrl, HWND hEditFile, LPCTSTR lpcstrFileName, DWORD dwFileNameLength, HINSTANCE hInstance, HWND *hTooltip) {
  SECURITY_ATTRIBUTES sa;
  HANDLE hFile;

  RtlZeroMemory(&sa, sizeof(sa));
  if ( !common_CreateSecurityAttributes(&sa) ) {
		common_ShowError(hDlg, ResStr(IDS_ERR_SECURITY));
		SetWindowText(hEditFile, TEXT(""));
		return INVALID_HANDLE_VALUE;
	}	
	{
		DWORD dwFileAttributes;
		/** Check if file read only */
                
		dwFileAttributes = GetFileAttributes(lpcstrFileName);
		if ( dwFileAttributes & FILE_ATTRIBUTE_READONLY ) {
		  LPTSTR lpMessage;
		  DWORD dwMessageLength = dwFileNameLength;
		  int iRet;

		  dwMessageLength += 1024 * sizeof(TCHAR);
		  lpMessage = (LPTSTR)LocalAlloc(LPTR, dwMessageLength);
		  StringCchPrintf( lpMessage, 
			  dwMessageLength, 
			  ResStr(IDS_WARN_READONLY_MSG), 
			  lpcstrFileName );
	      iRet = MessageBox(hDlg, lpMessage, ResStr(IDS_WARN_READONLY_TITLE), MB_YESNO | MB_ICONQUESTION);
		  if ( iRet != IDYES ) {
			  LocalFree(lpMessage);
			  common_FreeSecurityAttributes(&sa);
			  return INVALID_HANDLE_VALUE;
		  }
		  dwFileAttributes &= ~FILE_ATTRIBUTE_READONLY;
		  if ( !SetFileAttributes( lpcstrFileName, dwFileAttributes ) ) {
			  common_ShowError(hDlg, ResStr(IDS_ERR_SETATTR));
			  LocalFree(lpMessage);
			  common_FreeSecurityAttributes(&sa);
			  return INVALID_HANDLE_VALUE;
		  }
		  LocalFree(lpMessage);
		}
	}
	hFile = CreateFile(lpcstrFileName,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		&sa, OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
	if (hFile == INVALID_HANDLE_VALUE && GetLastError() == ERROR_SHARING_VIOLATION) {
		/* Fallback: read-only when another process holds an exclusive write lock */
		hFile = CreateFile(lpcstrFileName,
			GENERIC_READ,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			&sa, OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
	}
	if ( hFile == INVALID_HANDLE_VALUE ) {
		common_ShowError(hDlg, ResStr(IDS_ERR_CREATEFILE));
		SetWindowText(hEditFile, TEXT(""));
        private_SetFileHandle(hDlg, hTabCtrl, nullptr, lpcstrFileName);
	} else {
		private_EnumerateStream(lpcstrFileName);
		SetWindowText(hEditFile, lpcstrFileName);
        if ((*hTooltip) != nullptr)
		    DestroyWindow( (*hTooltip) );
	    (*hTooltip) = CreateToolTipForRect(hEditFile, hInstance, lpcstrFileName);
		private_SetFileHandle(hDlg, hTabCtrl, hFile, lpcstrFileName);
	}
	common_FreeSecurityAttributes(&sa);
  return hFile;
}

/* ---------- Recent files implementation (task #8) ------------------- */

static VOID
private_AddRecentFile(LPCTSTR lpszPath)
{
    HKEY  hKey;
    TCHAR szExisting[MAX_PATH * 4];
    DWORD cb, i;
    if (RegCreateKeyEx(HKEY_CURRENT_USER, g_szRecentKey, 0, NULL,
            REG_OPTION_NON_VOLATILE, KEY_READ | KEY_WRITE, NULL, &hKey, NULL) != ERROR_SUCCESS)
        return;
    for (i = RECENT_MAX - 1; i > 0; --i) {
        TCHAR szSrc[8], szDst[8];
        StringCchPrintf(szSrc, ARRAYSIZE(szSrc), TEXT("%u"), i - 1);
        StringCchPrintf(szDst, ARRAYSIZE(szDst), TEXT("%u"), i);
        cb = sizeof(szExisting);
        if (RegQueryValueEx(hKey, szSrc, NULL, NULL, (LPBYTE)szExisting, &cb) == ERROR_SUCCESS)
            RegSetValueEx(hKey, szDst, 0, REG_SZ, (LPBYTE)szExisting, cb);
        else
            RegDeleteValue(hKey, szDst);
    }
    RegSetValueEx(hKey, TEXT("0"), 0, REG_SZ, (LPBYTE)lpszPath,
                  (lstrlen(lpszPath) + 1) * sizeof(TCHAR));
    RegCloseKey(hKey);
}

static VOID
private_RebuildRecentMenu(HWND hDlg)
{
    HMENU hSys = GetSystemMenu(hDlg, FALSE);
    HKEY  hKey;
    UINT  i;
    for (i = 0; i < RECENT_MAX; ++i)
        RemoveMenu(hSys, ID_RECENT_BASE + i, MF_BYCOMMAND);
    RemoveMenu(hSys, ID_RECENT_BASE - 1, MF_BYCOMMAND);
    if (RegOpenKeyEx(HKEY_CURRENT_USER, g_szRecentKey, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return;
    AppendMenu(hSys, MF_SEPARATOR, ID_RECENT_BASE - 1, NULL);
    for (i = 0; i < RECENT_MAX; ++i) {
        TCHAR szName[8], szPath[MAX_PATH * 4], szLabel[MAX_PATH * 4 + 4];
        DWORD cb = sizeof(szPath);
        StringCchPrintf(szName, ARRAYSIZE(szName), TEXT("%u"), i);
        if (RegQueryValueEx(hKey, szName, NULL, NULL, (LPBYTE)szPath, &cb) != ERROR_SUCCESS)
            break;
        StringCchPrintf(szLabel, ARRAYSIZE(szLabel), TEXT("&%u  %s"), (i + 1) % 10, szPath);
        AppendMenu(hSys, MF_STRING, ID_RECENT_BASE + i, szLabel);
    }
    RegCloseKey(hKey);
}

static VOID
private_OpenRecentFile(HWND hDlg, HWND hTabCtrl, HWND hEditFile,
    HANDLE *phFile, LPTSTR *plpstrFileName, DWORD *pdwLen,
    HINSTANCE hInst, HWND *phTooltip, UINT uRecentIdx)
{
    HKEY  hKey;
    TCHAR szPath[MAX_PATH * 4], szName[8];
    DWORD cb = sizeof(szPath);
    StringCchPrintf(szName, ARRAYSIZE(szName), TEXT("%u"), uRecentIdx);
    if (RegOpenKeyEx(HKEY_CURRENT_USER, g_szRecentKey, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return;
    if (RegQueryValueEx(hKey, szName, NULL, NULL, (LPBYTE)szPath, &cb) == ERROR_SUCCESS) {
        if (*phFile && *phFile != INVALID_HANDLE_VALUE) CloseHandle(*phFile);
        *phFile = INVALID_HANDLE_VALUE;
        *pdwLen = (lstrlen(szPath) + 1) * sizeof(TCHAR) + 1024;
        if (*plpstrFileName) LocalFree(*plpstrFileName);
        *plpstrFileName = (LPTSTR)LocalAlloc(LPTR, *pdwLen);
        if (*plpstrFileName) {
            StringCchCopy(*plpstrFileName, *pdwLen / sizeof(TCHAR), szPath);
            private_EnumerateStream(*plpstrFileName);
            *phFile = private_OpenFile(hDlg, hTabCtrl, hEditFile, *plpstrFileName,
                                       *pdwLen, hInst, phTooltip);
            if (*phFile != INVALID_HANDLE_VALUE) {
                private_AddRecentFile(szPath);
                private_RebuildRecentMenu(hDlg);
            }
        }
    }
    RegCloseKey(hKey);
}

/* ---------- File comparison (task #9) ------------------------------- */

static VOID
private_CompareFiles(HWND hDlg, HINSTANCE hInst, LPCTSTR lpszFile1)
{
    IFileOpenDialog *pDlg2 = nullptr;
    if (!lpszFile1) return;
    if (SUCCEEDED(CoCreateInstance(__uuidof(FileOpenDialog), nullptr,
                                   CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pDlg2)))) {
        DWORD dwOpts;
        pDlg2->GetOptions(&dwOpts);
        pDlg2->SetOptions(
            dwOpts | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST | FOS_DONTADDTORECENT);
        pDlg2->SetTitle(ResStr(IDS_COMPARE_WITH));
        if (SUCCEEDED(pDlg2->Show(hDlg))) {
            IShellItem *pItem2 = nullptr;
            if (SUCCEEDED(pDlg2->GetResult(&pItem2))) {
                LPWSTR pszPath2 = nullptr;
                if (SUCCEEDED(pItem2->GetDisplayName(SIGDN_FILESYSPATH, &pszPath2))) {
                    FILE_BASIC_INFO fbi1, fbi2;
                    FILE_STANDARD_INFO fsi1, fsi2;
                    HANDLE h1, h2;
                    TCHAR szResult[4096];
                    szResult[0] = 0;
                    h1 = CreateFile(lpszFile1, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE,
                                    NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
                    h2 = CreateFile(pszPath2, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE,
                                    NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
                    if (h1 != INVALID_HANDLE_VALUE && h2 != INVALID_HANDLE_VALUE) {
                        ZeroMemory(&fbi1, sizeof(fbi1)); ZeroMemory(&fbi2, sizeof(fbi2));
                        ZeroMemory(&fsi1, sizeof(fsi1)); ZeroMemory(&fsi2, sizeof(fsi2));
                        GetFileInformationByHandleEx(h1, FileBasicInfo,    &fbi1, sizeof(fbi1));
                        GetFileInformationByHandleEx(h2, FileBasicInfo,    &fbi2, sizeof(fbi2));
                        GetFileInformationByHandleEx(h1, FileStandardInfo, &fsi1, sizeof(fsi1));
                        GetFileInformationByHandleEx(h2, FileStandardInfo, &fsi2, sizeof(fsi2));
                        if (fsi1.EndOfFile.QuadPart != fsi2.EndOfFile.QuadPart) {
                            TCHAR szLine[512];
                            StringCchPrintf(szLine, ARRAYSIZE(szLine),
                                TEXT("Size: %lld  vs  %lld\r\n"),
                                fsi1.EndOfFile.QuadPart, fsi2.EndOfFile.QuadPart);
                            StringCchCat(szResult, ARRAYSIZE(szResult), szLine);
                        }
                        if (fbi1.FileAttributes != fbi2.FileAttributes) {
                            TCHAR szLine[256];
                            StringCchPrintf(szLine, ARRAYSIZE(szLine),
                                TEXT("Attributes: 0x%X  vs  0x%X\r\n"),
                                fbi1.FileAttributes, fbi2.FileAttributes);
                            StringCchCat(szResult, ARRAYSIZE(szResult), szLine);
                        }
                        if (fbi1.LastWriteTime.QuadPart != fbi2.LastWriteTime.QuadPart)
                            StringCchCat(szResult, ARRAYSIZE(szResult), TEXT("LastWriteTime: differs\r\n"));
                        if (fbi1.CreationTime.QuadPart != fbi2.CreationTime.QuadPart)
                            StringCchCat(szResult, ARRAYSIZE(szResult), TEXT("CreationTime: differs\r\n"));
                        if (fsi1.NumberOfLinks != fsi2.NumberOfLinks) {
                            TCHAR szLine[256];
                            StringCchPrintf(szLine, ARRAYSIZE(szLine),
                                TEXT("HardLinks: %u  vs  %u\r\n"),
                                fsi1.NumberOfLinks, fsi2.NumberOfLinks);
                            StringCchCat(szResult, ARRAYSIZE(szResult), szLine);
                        }
                    }
                    if (h1 != INVALID_HANDLE_VALUE) CloseHandle(h1);
                    if (h2 != INVALID_HANDLE_VALUE) CloseHandle(h2);
                    if (szResult[0] == 0) {
                        MessageBox(hDlg, ResStr(IDS_COMPARE_EQUAL), ResStr(IDS_COMPARE_TITLE), MB_OK | MB_ICONINFORMATION);
                    } else {
                        LPTSTR szMsg = NULL;
                        int nChars = _scwprintf(ResStr(IDS_COMPARE_RESULT), szResult) + 1;
                        szMsg = (LPTSTR)HeapAlloc(GetProcessHeap(), 0, nChars * sizeof(TCHAR));
                        if (szMsg) swprintf_s(szMsg, nChars, ResStr(IDS_COMPARE_RESULT), szResult);
                        MessageBox(hDlg, szMsg ? szMsg : szResult, ResStr(IDS_COMPARE_TITLE), MB_OK | MB_ICONINFORMATION);
                        HeapFree(GetProcessHeap(), 0, szMsg);
                    }
                    CoTaskMemFree(pszPath2);
                }
                pItem2->Release();
            }
        }
        pDlg2->Release();
    }
}

INT_PTR CALLBACK
MainDialog(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static HINSTANCE hInstance = NULL;
    static HANDLE    hFile = NULL;
    static HWND      hTooltip = NULL;
    static HWND      hEditFile = NULL;
    static HWND      hTabCtrl = NULL;
    static HWND      hRestartAsAdministrator = NULL;
	static LPTSTR    lpstrFileName = NULL;
	static DWORD     dwFileNameLength = 0;
	switch ( uMsg ) {
		case WM_INITDIALOG: {
		  hInstance = (HINSTANCE)lParam;
		  SetWindowText(hDlg, ResStr(IDS_APP_TITLE));
		  SendMessage(hDlg, WM_SETICON, ICON_BIG,
		      (LPARAM)LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APP)));
		  SendMessage(hDlg, WM_SETICON, ICON_SMALL,
		      (LPARAM)LoadImage(hInstance, MAKEINTRESOURCE(IDI_APP),
		                        IMAGE_ICON, GetSystemMetrics(SM_CXSMICON),
		                        GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR));
		  hEditFile = GetDlgItem(hDlg, IDC_EDITFILE);
		  hTabCtrl = GetDlgItem(hDlg, IDC_INFOTAB);
          hRestartAsAdministrator = GetDlgItem(hDlg, IDC_RESTART_AS_ADMINISTARTOR);
		  private_InitButtonImageList(hDlg, hInstance);
		  private_InitTabInfoCtrl(hTabCtrl, hInstance);
		  CreateToolTipForRect( GetDlgItem(hDlg, IDC_OPENFILE), hInstance, ResStr(IDS_TOOLTIP_OPEN_FILE));
		  CreateToolTipForRect( GetDlgItem(hDlg, IDC_OPENDIRECTORY), hInstance, ResStr(IDS_TOOLTIP_OPEN_DIR));
#if (NTDDI_VERSION >= NTDDI_VISTA)
          Button_SetElevationRequiredState(hRestartAsAdministrator, TRUE);
          EnableWindow(hRestartAsAdministrator, !IsElevated());
#endif
          CheckDlgButton(hDlg, IDC_SHELL_INTEGRATE,
              private_IsShellIntegrated() ? BST_CHECKED : BST_UNCHECKED);
          if (g_lpstrCmdLineFile) {
              dwFileNameLength = (lstrlen(g_lpstrCmdLineFile) + 1) * sizeof(TCHAR) + 1024;
              if (lpstrFileName) LocalFree(lpstrFileName);
              lpstrFileName = (LPTSTR)LocalAlloc(LPTR, dwFileNameLength);
              if (lpstrFileName) {
                  StringCchCopy(lpstrFileName, dwFileNameLength / sizeof(TCHAR), g_lpstrCmdLineFile);
                  hFile = private_OpenFile(hDlg, hTabCtrl, hEditFile, lpstrFileName,
                                           dwFileNameLength, hInstance, &hTooltip);
              }
          }
		  private_RestoreWindowPos(hDlg);
		  /* Recent files menu (task #8) */
		  private_RebuildRecentMenu(hDlg);
		  /* OLE drag-drop (task #7) */
		  g_DropTarget.m_hWnd = hDlg;
		  OleInitialize(nullptr);
		  RegisterDragDrop(hDlg, &g_DropTarget);
		  /* Compare with... in system menu (task #9) */
		  { HMENU hSys = GetSystemMenu(hDlg, FALSE);
		    if (hSys) AppendMenu(hSys, MF_STRING, IDM_COMPARE_FILE, ResStr(IDS_COMPARE_WITH));
		  }
		  return TRUE;
		}
		case WM_NOTIFY: {
			LPNMHDR lpnmHdr = (LPNMHDR)lParam;
			switch ( lpnmHdr->code ) {
		    case TCN_SELCHANGE: {
				int iCurTab = TabCtrl_GetCurSel(lpnmHdr->hwndFrom);
				int iTab;

				for (iTab = 0; iTab < sizeof(g_TabInfoCtrl)/sizeof(g_TabInfoCtrl[0]); ++iTab) {
					ShowWindow( g_TabInfoCtrl[iTab].hWnd, SW_HIDE );
				}
				ShowWindow( g_TabInfoCtrl[iCurTab].hWnd, SW_SHOW );
				if (iCurTab >= 0 && iCurTab < sizeof(g_TabInfoCtrl) / sizeof(g_TabInfoCtrl[0])) {
					SendMessage(g_TabInfoCtrl[iCurTab].hWnd, WM_SETFILE_HANDLE, (WPARAM)NULL, (LPARAM)hFile);
					SendMessage(g_TabInfoCtrl[iCurTab].hWnd, WM_SETFILE_NAME, (WPARAM)NULL, (LPARAM)lpstrFileName);
				}
				break;
			  }
			}
			break;
		}
		case WM_DROPFILES: {
			HDROP hDrop = (HDROP)wParam;
            TCHAR szDropped[MAX_PATH * 4];
            TCHAR szLong[MAX_PATH * 4 + 8];

			szDropped[0] = 0;
			DragQueryFile(hDrop, 0, szDropped, ARRAYSIZE(szDropped));
			private_MakeLongPath(szDropped, szLong, ARRAYSIZE(szLong));
            if ( hFile && hFile != INVALID_HANDLE_VALUE )
                CloseHandle(hFile);
			dwFileNameLength = (lstrlen(szLong) + 1) * sizeof(TCHAR) + 1024;
			if (lpstrFileName) LocalFree(lpstrFileName);
			lpstrFileName = (LPTSTR)LocalAlloc(LPTR, dwFileNameLength);
			if (lpstrFileName) {
				StringCchCopy(lpstrFileName, dwFileNameLength / sizeof(TCHAR), szLong);
				hFile = private_OpenFile(hDlg, hTabCtrl, hEditFile, lpstrFileName, dwFileNameLength, hInstance, &hTooltip);
				if (hFile != INVALID_HANDLE_VALUE) {
					private_AddRecentFile(szDropped);
					private_RebuildRecentMenu(hDlg);
				}
			}
			DragFinish(hDrop);
			break;
		}
		case WM_KEYDOWN: {
			/* Task #14: F5 refreshes file info for currently open file */
			if ((int)wParam == VK_F5 && hFile && hFile != INVALID_HANDLE_VALUE) {
				private_SetFileHandle(hDlg, hTabCtrl, hFile, lpstrFileName);
			}
			break;
		}
		case WM_COMMAND: {
			WORD wNotifyId = HIWORD(wParam);
			WORD wCtrlId = LOWORD(wParam);
			
			switch ( wCtrlId ) {
		        case IDM_CREATE_STREAM:
		        case IDM_VIEW_STREAM: {
					    /** Send message to children */
			        int iCurTab = TabCtrl_GetCurSel(hTabCtrl);
				    if ( iCurTab >= 0 && g_TabInfoCtrl[iCurTab].hWnd ) {
				        SendMessage(g_TabInfoCtrl[iCurTab].hWnd, WM_COMMAND, wParam, lParam);
				    }
				    break;
			    }
		        case IDC_OPENFILE: {
                    IFileOpenDialog *pDlg = nullptr;
                    if ( hFile && hFile != INVALID_HANDLE_VALUE )
                        CloseHandle(hFile);
                    hFile = INVALID_HANDLE_VALUE;
                    if (SUCCEEDED(CoCreateInstance(__uuidof(FileOpenDialog), nullptr,
                                                  CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pDlg)))) {
                        DWORD dwOpts;
                        pDlg->GetOptions(&dwOpts);
                        pDlg->SetOptions(dwOpts | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST
                                                | FOS_PATHMUSTEXIST | FOS_DONTADDTORECENT);
                        pDlg->SetTitle(ResStr(IDS_BROWSE_TITLE));
                        if (SUCCEEDED(pDlg->Show(hDlg))) {
                            IShellItem *pItem = nullptr;
                            if (SUCCEEDED(pDlg->GetResult(&pItem))) {
                                LPWSTR pszPath = nullptr;
                                if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszPath))) {
                                    dwFileNameLength = (lstrlen(pszPath) + 1) * sizeof(TCHAR) + 1024;
                                    if (lpstrFileName) LocalFree(lpstrFileName);
                                    lpstrFileName = (LPTSTR)LocalAlloc(LPTR, dwFileNameLength);
                                    if (lpstrFileName) {
                                        StringCchCopy(lpstrFileName, dwFileNameLength / sizeof(TCHAR), pszPath);
                                        private_EnumerateStream(lpstrFileName);
                                        hFile = private_OpenFile(hDlg, hTabCtrl, hEditFile,
                                                                  lpstrFileName, dwFileNameLength,
                                                                  hInstance, &hTooltip);
                                        if (hFile != INVALID_HANDLE_VALUE) {
                                            private_AddRecentFile(lpstrFileName);
                                            private_RebuildRecentMenu(hDlg);
                                        }
                                    }
                                    CoTaskMemFree(pszPath);
                                }
                                pItem->Release();
                            }
                        }
                        pDlg->Release();
                    }
                    break;
                }
                case IDC_RESTART_AS_ADMINISTARTOR: {
                    TCHAR szPathBuffer[MAX_PATH * 4];
                    DWORD dwSize = ARRAYSIZE(szPathBuffer);
                    DWORD dwRet;

                    dwRet = GetModuleFileName((HMODULE)hInstance, szPathBuffer, dwSize);
                    if (dwRet > 0 && dwRet < dwSize) {
                        szPathBuffer[dwRet] = (TCHAR)0x00;

                        ShellExecute(NULL, TEXT("runas"), szPathBuffer, NULL, NULL, SW_SHOWNORMAL);
                        SendMessage(hDlg, WM_CLOSE, (WPARAM)NULL, (LPARAM)NULL);
                    }
                    break;
                }
                case IDC_SHELL_INTEGRATE: {
                    if (IsDlgButtonChecked(hDlg, IDC_SHELL_INTEGRATE) == BST_CHECKED) {
                        if (!private_RegisterShell(hDlg, hInstance))
                            CheckDlgButton(hDlg, IDC_SHELL_INTEGRATE, BST_UNCHECKED);
                    } else {
                        private_UnregisterShell();
                    }
                    break;
                }
			}
		    break;
		}
		case WM_MOVE: {
			private_SaveWindowPos(hDlg);
			break;
		}
		case WM_SYSCOMMAND: {
			UINT uCmd = (UINT)(wParam & 0xFFF0);
			if (uCmd == IDM_COMPARE_FILE) {
				private_CompareFiles(hDlg, hInstance, lpstrFileName);
				break;
			}
			if (wParam >= ID_RECENT_BASE && wParam < ID_RECENT_BASE + RECENT_MAX) {
				private_OpenRecentFile(hDlg, hTabCtrl, hEditFile,
					&hFile, &lpstrFileName, &dwFileNameLength,
					hInstance, &hTooltip, (UINT)(wParam - ID_RECENT_BASE));
				break;
			}
			return DefWindowProc(hDlg, uMsg, wParam, lParam);
		}
		case WM_CLOSE: {
			RevokeDragDrop(hDlg);
			OleUninitialize();
			if (hFile != NULL && hFile != INVALID_HANDLE_VALUE) {
				CloseHandle(hFile);
			}
            hFile = NULL;
			if (lpstrFileName != NULL)
				LocalFree(lpstrFileName);
			lpstrFileName = NULL;
			EndDialog(hDlg, 0);
			return TRUE;
		}
	}
	return FALSE;
}

BOOL 
SetDebugStatusForCurentProc(VOID)
{
    HANDLE           hToken;
    LUID             DebugValue;
    TOKEN_PRIVILEGES tkp;
    BOOL             bRet;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        return FALSE;
    }

    if (!LookupPrivilegeValue((LPTSTR)NULL, SE_DEBUG_NAME, &DebugValue)) {
        return FALSE;
    }
    tkp.PrivilegeCount = 1;
    tkp.Privileges[0].Luid = DebugValue;
    tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    bRet = AdjustTokenPrivileges(hToken, FALSE, &tkp, sizeof(TOKEN_PRIVILEGES), (PTOKEN_PRIVILEGES)NULL, (PDWORD)NULL);
    return bRet;
}

BOOL 
IsElevated(VOID) 
{
    BOOL fRet = FALSE;
    HANDLE hToken = NULL;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION Elevation;
        DWORD cbSize = sizeof(TOKEN_ELEVATION);
        if (GetTokenInformation(hToken, TokenElevation, &Elevation, sizeof(Elevation), &cbSize)) {
            fRet = Elevation.TokenIsElevated;
        }
    }
    if (hToken) {
        CloseHandle(hToken);
    }
    return fRet;
}
