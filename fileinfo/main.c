#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <strsafe.h>
#include "common.h"
#include "file_basic_info.h"
#include "file_standart_info.h"
#include "file_stream_info.h"
#include "resource.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "advapi32.lib")

static struct tagButtonCtrl {
	WORD				wId;
	WORD				wResourceId;
	HIMAGELIST	hImageList;
	WORD				wAlign;
} g_ButtonCtrl [] = {
	{ IDC_OPENFILE,	IDI_FILEOPEN,	NULL, BUTTON_IMAGELIST_ALIGN_CENTER }
};

static struct tagTabCtrl {	
	FILE_INFO_BY_HANDLE_CLASS FileInfoClass;
	LPTSTR                    lpstrCaption;
	LPTSTR                    lpstrWindow;
	HWND                      hWnd;
	DLGPROC                   pfnProc;
} g_TabInfoCtrl [] = {
	{ FileBasicInfo,                  TEXT("Основная"),                    TEXT("FILE_BASIC_INFO"),    NULL, fbi_WindowHandler },
	{ FileStandardInfo,               TEXT("Стандартная"),                 TEXT("FILE_STANDART_INFO"), NULL, fsi_WindowHandler },
	{ FileStreamInfo,                 TEXT("Потоки"),                      TEXT("FILE_STREAM_INFO"),   NULL, fssi_WindowHandler },
	{ FileNameInfo,                   TEXT("NameInfo"),                    TEXT(""), NULL, NULL },
	{ FileCompressionInfo,            TEXT("CompressionInfo"),             TEXT(""), NULL, NULL },
	{ FileAttributeTagInfo,           TEXT("AttributeTagInfo"),            TEXT(""), NULL, NULL },
	{ FileIdBothDirectoryInfo,        TEXT("IdBothDirectoryInfo"),         TEXT(""), NULL, NULL },
	{ FileIdBothDirectoryRestartInfo, TEXT("IdBothDirectoryRestartInfo"),  TEXT(""), NULL, NULL },
	{ FileDispositionInfo,            TEXT("FileDispositionInfo"),         TEXT(""), NULL, NULL },
	{ FileRenameInfo,                 TEXT("FileRenameInfo"),              TEXT(""), NULL, NULL },
	{ FileAllocationInfo,             TEXT("FileAllocationInfo"),          TEXT(""), NULL, NULL },
	{ FileEndOfFileInfo,              TEXT("FileEndOfFileInfo"),           TEXT(""), NULL, NULL },
	{ FileIoPriorityHintInfo,         TEXT("FileIoPriorityHintInfo"),      TEXT(""), NULL, NULL },
	{ MaximumFileInfoByHandleClass,   TEXT("MaximumFileInfoByHandleClass"),TEXT(""), NULL, NULL }
};

INT_PTR CALLBACK
MainDialog(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

int WINAPI
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	InitCommonControls();
	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	DialogBoxParam(hInstance, TEXT("MAINDIALOG"), NULL, MainDialog, (LPARAM)hInstance);
	return EXIT_SUCCESS;
}

static VOID CALLBACK
private_InitButtonImageList(HWND hDlg, HINSTANCE hInstance) {
	UINT							uIndex;
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

	GetClientRect(hTabCtrl, &rect);
	RtlZeroMemory(&tci, sizeof(tci));
	tci.mask = TCIF_TEXT;
	for (iTab = 0; iTab < sizeof(g_TabInfoCtrl)/sizeof(g_TabInfoCtrl[0]); ++iTab) {
		tci.pszText = g_TabInfoCtrl[iTab].lpstrCaption;
		TabCtrl_InsertItem(hTabCtrl, iTab, &tci);
		if ( g_TabInfoCtrl[iTab].hWnd == NULL ) {
			g_TabInfoCtrl[iTab].hWnd = CreateDialogParam(
				hInstance, 
				g_TabInfoCtrl[iTab].lpstrWindow, 
				hTabCtrl, 
				g_TabInfoCtrl[iTab].pfnProc, 
				(LPARAM)NULL
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
CreateToolTipForRect(HWND hwndParent, HINSTANCE hInstance, LPTSTR lpstrText) {
	TOOLINFO ti = { 0 };
    HWND hwndTT = CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS, NULL, 
                                 WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP, 
                                 CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 
                                 hwndParent, NULL, hInstance, NULL);

    SetWindowPos(hwndTT, HWND_TOPMOST, 0, 0, 0, 0, 
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
   
    
    ti.cbSize   = sizeof(TOOLINFO);
    ti.uFlags   = TTF_SUBCLASS;
    ti.hwnd     = hwndParent;
    ti.hinst    = hInstance;
    ti.lpszText = lpstrText;
    
    GetClientRect (hwndParent, &ti.rect);
    SendMessage(hwndTT, TTM_ADDTOOL, 0, (LPARAM) (LPTOOLINFO) &ti);	
		return hwndTT;
}

static VOID CALLBACK
private_SetFileHandle(HWND hDlg, HWND hTabCtrl, HANDLE hFile) {
	int iCurTab;

	iCurTab = TabCtrl_GetCurSel(hTabCtrl);
	if ( iCurTab >= 0 && iCurTab < sizeof(g_TabInfoCtrl)/sizeof(g_TabInfoCtrl[0]) ) {
		if ( g_TabInfoCtrl[iCurTab].hWnd )
			SendMessage( g_TabInfoCtrl[iCurTab].hWnd, WM_SETFILE_HANDLE, (WPARAM)NULL, (LPARAM)hFile );
	}
}

static VOID
private_EnumerateStream(LPTSTR lpstrFileName) {
	/**WIN32_FIND_STREAM_DATA fsd;
	HANDLE hStream;

	hStream = FindFirstStreamW(lpstrFileName, FindStreamInfoStandard, &fsd, 0);
	if ( hStream == INVALID_HANDLE_VALUE )
		return;
	do {
	} while (FindNextStreamW(hStream, &fsd));
	FindClose(hStream);*/
}

INT_PTR CALLBACK
MainDialog(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
static HINSTANCE hInstance = NULL;
static HANDLE hFile = NULL;
static HWND hTooltip = NULL;
static HWND hEditFile = NULL;
static HWND hTabCtrl = NULL;
	switch ( uMsg ) {
		case WM_INITDIALOG: {
		  hInstance = (HINSTANCE)lParam;
		  SetWindowText(hDlg, TEXT("Информация о файле"));
		  hEditFile = GetDlgItem(hDlg, IDC_EDITFILE);
		  hTabCtrl = GetDlgItem(hDlg, IDC_INFOTAB);
		  private_InitButtonImageList(hDlg, hInstance);
		  private_InitTabInfoCtrl(hTabCtrl, hInstance);
		  CreateToolTipForRect( GetDlgItem(hDlg, IDC_OPENFILE), hInstance, TEXT("Открыть файл"));
		  CreateToolTipForRect( GetDlgItem(hDlg, IDC_OPENDIRECTORY), hInstance, TEXT("Открыть директорию"));
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
				if ( iCurTab >= 0 && iCurTab < sizeof(g_TabInfoCtrl)/sizeof(g_TabInfoCtrl[0]) )
				  SendMessage( g_TabInfoCtrl[iCurTab].hWnd, WM_SETFILE_HANDLE, (WPARAM)NULL, (LPARAM)hFile );
				break;
			  }
			}
			break;
		}
		case WM_DROPFILES: {
			HDROP hDrop = (HDROP)wParam;
      TCHAR szFileName[MAX_PATH * sizeof(TCHAR)];

			DragQueryFile(hDrop, 0, szFileName, sizeof(szFileName));
			MessageBox(hDlg, szFileName, NULL, MB_OK);
			DragFinish(hDrop);
			break;
		}
		case WM_COMMAND: {
			LPTSTR lpstrFileName;
			DWORD  dwFileNameLength;
			BOOL bOpenDirectory = FALSE;
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
				//LPSHELLFOLDER pDesktopFolder;
				//ULONG chEaten;
				//DWORD dwAttributes;
				

				RtlZeroMemory(&bi, sizeof(bi));
				dwFileNameLength = MAX_PATH * 1024 * sizeof(TCHAR);
				lpstrFileName = (LPTSTR)LocalAlloc(LPTR, dwFileNameLength);
				//GetModuleFileName(hInstance, lpstrFileName, dwFileNameLength);
				//GetCurrentDirectory(dwFileNameLength, lpstrFileName);
				//SHGetDesktopFolder(&pDesktopFolder);
				//pDesktopFolder->lpVtbl->ParseDisplayName(pDesktopFolder, hDlg, NULL, lpstrFileName, &chEaten, &bi.pidlRoot, &dwAttributes);
				
				bi.hwndOwner = hDlg;
				bi.lpszTitle = TEXT("Выбор директории или файла");
				bi.ulFlags = BIF_BROWSEINCLUDEFILES | BIF_BROWSEFORCOMPUTER | BIF_RETURNONLYFSDIRS | BIF_STATUSTEXT | BIF_RETURNFSANCESTORS;
				lpIdList = SHBrowseForFolder(&bi);
				if ( lpIdList != NULL && SUCCEEDED(lpIdList) ) {
					SECURITY_ATTRIBUTES sa;
					IMalloc *comMalloc;

					SHGetPathFromIDList(lpIdList, lpstrFileName);
					if ( SUCCEEDED( SHGetMalloc(&comMalloc) ) ) {
					  comMalloc->lpVtbl->Free(comMalloc, lpIdList);
					  comMalloc->lpVtbl->Release(comMalloc);
					}
					RtlZeroMemory(&sa, sizeof(sa));
					if ( !common_CreateSecurityAttributes(&sa) ) {
						common_ShowError(hDlg, TEXT("Create secutiry attributes"));
						SetWindowText(hEditFile, TEXT(""));
						break;
					}	
					{
						DWORD dwFileAttributes;
						/** Check if file read only */
                        
						dwFileAttributes = GetFileAttributes(lpstrFileName);
						if ( dwFileAttributes & FILE_ATTRIBUTE_READONLY ) {
						  LPTSTR lpMessage;
						  DWORD dwMessageLength = dwFileNameLength;
						  int iRet;

						  dwMessageLength += 1024 * sizeof(TCHAR);
						  lpMessage = (LPTSTR)LocalAlloc(LPTR, dwMessageLength);
						  StringCchPrintf( lpMessage, 
							  dwMessageLength, 
							  TEXT("Файл \"%s\" защищен от записи.\r\nСнять защиту и открыть?"), 
							  lpstrFileName );
					      iRet = MessageBox(hDlg, lpMessage, TEXT("Предупреждение"), MB_YESNO | MB_ICONQUESTION);
						  if ( iRet != IDYES ) {
							  LocalFree(lpstrFileName);
							  LocalFree(lpMessage);
							  common_FreeSecurityAttributes(&sa);
							  return FALSE;
						  }
						  dwFileAttributes &= ~FILE_ATTRIBUTE_READONLY;
						  if ( !SetFileAttributes( lpstrFileName, dwFileAttributes ) ) {
							  common_ShowError(hDlg, TEXT("SetFileAttributes"));
							  LocalFree(lpstrFileName);
							  LocalFree(lpMessage);
							  common_FreeSecurityAttributes(&sa);
							  return FALSE;
						  }
						  LocalFree(lpMessage);
						}
					}
					hFile = CreateFile(lpstrFileName, 
						GENERIC_READ | GENERIC_WRITE, 
						FILE_SHARE_READ/* | FILE_SHARE_WRITE*/, 
						&sa, 
						OPEN_EXISTING, 
						FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS, 
						NULL);
					if ( hFile == INVALID_HANDLE_VALUE ) {
						common_ShowError(hDlg, TEXT("CreateFile"));
						SetWindowText(hEditFile, TEXT(""));
						private_SetFileHandle(hDlg, hTabCtrl, NULL);
					} else {
						private_EnumerateStream(lpstrFileName);
						SetWindowText(hEditFile, lpstrFileName);
					  if ( hTooltip != NULL )
						  DestroyWindow(hTooltip);
					  hTooltip = CreateToolTipForRect(hEditFile, hInstance, lpstrFileName);
						private_SetFileHandle(hDlg, hTabCtrl, hFile);
					}
					common_FreeSecurityAttributes(&sa);
				}
				LocalFree(lpstrFileName);
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
			EndDialog(hDlg, 0);
			return TRUE;
		}
	}
	return FALSE;
}
