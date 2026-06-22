#include <windows.h>
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
            TCHAR szFileName[MAX_PATH];

			DragQueryFile(hDrop, 0, szFileName, ARRAYSIZE(szFileName));
            if ( hFile && hFile != INVALID_HANDLE_VALUE )
                CloseHandle(hFile);
			hFile = private_OpenFile(hDlg, hTabCtrl, hEditFile, szFileName, sizeof(szFileName), hInstance, &hTooltip);
			DragFinish(hDrop);
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
			        BROWSEINFO bi;
				    LPITEMIDLIST lpIdList;				

                    if ( hFile && hFile != INVALID_HANDLE_VALUE )
                        CloseHandle(hFile);
				    RtlZeroMemory(&bi, sizeof(bi));
				    dwFileNameLength = MAX_PATH * 1024 * sizeof(TCHAR);
					if (lpstrFileName != NULL)
						LocalFree(lpstrFileName);
				    lpstrFileName = (LPTSTR)LocalAlloc(LPTR, dwFileNameLength);
  				
				    bi.hwndOwner = hDlg;
				    bi.lpszTitle = ResStr(IDS_BROWSE_TITLE);
				    bi.ulFlags = BIF_BROWSEINCLUDEFILES | BIF_BROWSEFORCOMPUTER | BIF_RETURNONLYFSDIRS | BIF_STATUSTEXT | BIF_RETURNFSANCESTORS;
                    SHGetFolderLocation(hDlg, CSIDL_DRIVES, nullptr, 0, (LPITEMIDLIST *)&bi.pidlRoot);
				    lpIdList = SHBrowseForFolder(&bi);
                    if (lpIdList != NULL) {
#if defined(__cplusplus)
			            IMalloc *comMalloc;

					    SHGetPathFromIDList(lpIdList, lpstrFileName);
					    if ( SUCCEEDED( SHGetMalloc(&comMalloc) ) ) {
					        comMalloc->Free(lpIdList);
					        comMalloc->Release();
					    }
#else
						SHGetPathFromIDList(lpIdList, lpstrFileName);
						CoTaskMemFree(lpIdList);
#endif
                        private_EnumerateStream(lpstrFileName);
					    hFile = private_OpenFile(hDlg, hTabCtrl, hEditFile, lpstrFileName, dwFileNameLength, hInstance, &hTooltip);					
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
		case WM_CLOSE: {
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
