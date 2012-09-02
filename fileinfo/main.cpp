#include <windows.h>
#include <commctrl.h>
#include "resource.h"

#pragma comment(lib, "comctl32.lib")

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
	{ FileBasicInfo,                  TEXT("Основная"),                    TEXT("FILE_BASIC_INFO"), NULL, NULL },
	{ FileStandardInfo,               TEXT("Стандартная"),                 TEXT(""), NULL, NULL },
	{ FileNameInfo,                   TEXT("NameInfo"),                    TEXT(""), NULL, NULL },
	{ FileStreamInfo,                 TEXT("StreamInfo"),                  TEXT(""), NULL, NULL },
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
				NULL
			);
			MoveWindow( g_TabInfoCtrl[iTab].hWnd, rect.left + 2, rect.top + 21, rect.right - 5, rect.bottom - 24, TRUE);
			ShowWindow( g_TabInfoCtrl[iTab].hWnd, SW_HIDE );
		}
	}
	ShowWindow( g_TabInfoCtrl[0].hWnd, SW_SHOW );
}


HWND 
CreateToolTip(int iCtrlId, HWND hDlg, HINSTANCE hInstance, PTSTR pszText) {
  if (!iCtrlId || !hDlg || !pszText) {
      return FALSE;
  }
  HWND hwndTool = GetDlgItem(hDlg, iCtrlId);
  HWND hwndTip = CreateWindowEx(NULL, TOOLTIPS_CLASS, NULL,
                            WS_POPUP |TTS_ALWAYSTIP | TTS_BALLOON,
                            CW_USEDEFAULT, CW_USEDEFAULT,
                            CW_USEDEFAULT, CW_USEDEFAULT,
                            hDlg, NULL, 
                            hInstance, NULL);
  
 if (!hwndTool || !hwndTip) {
     return (HWND)NULL;
 }                              
                            
  TOOLINFO toolInfo = { 0 };
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
    HWND hwndTT = CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS, NULL, 
                                 WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP, 
                                 CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 
                                 hwndParent, NULL, hInstance, NULL);

    SetWindowPos(hwndTT, HWND_TOPMOST, 0, 0, 0, 0, 
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
   
    TOOLINFO ti = { 0 };
    ti.cbSize   = sizeof(TOOLINFO);
    ti.uFlags   = TTF_SUBCLASS;
    ti.hwnd     = hwndParent;
    ti.hinst    = hInstance;
    ti.lpszText = lpstrText;
    
    GetClientRect (hwndParent, &ti.rect);
    SendMessage(hwndTT, TTM_ADDTOOL, 0, (LPARAM) (LPTOOLINFO) &ti);	
		return hwndTT;
}



INT_PTR CALLBACK
MainDialog(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
static HINSTANCE hInstance = NULL;
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
					break;
				}
			}
			break;
		}
		case WM_COMMAND: {
			WORD wNotifyId = HIWORD(wParam);
			WORD wCtrlId = LOWORD(wParam);
			switch ( wCtrlId ) {
		    case IDC_OPENFILE: {
					OPENFILENAME ofn;
					LPTSTR lpstrFileName;
					DWORD  dwFileNameLength;

					dwFileNameLength = MAX_PATH * 1024 * sizeof(TCHAR);
					lpstrFileName = (LPTSTR)LocalAlloc(LPTR, dwFileNameLength);
					RtlZeroMemory(lpstrFileName, dwFileNameLength);
					RtlZeroMemory(&ofn, sizeof(ofn));
					ofn.lStructSize = sizeof(ofn);
					ofn.hwndOwner = hDlg;
					ofn.hInstance = hInstance;
					ofn.lpstrFile = lpstrFileName;
					ofn.nMaxFile = dwFileNameLength;
					if ( GetOpenFileName(&ofn) ) {
						SetWindowText(hEditFile, lpstrFileName);
						if ( hTooltip != NULL )
							DestroyWindow(hTooltip);
						hTooltip = CreateToolTipForRect(hEditFile, hInstance, lpstrFileName);
					}
					LocalFree(lpstrFileName);
					
					break;
				}
			}
			break;
		}
		case WM_CLOSE: {
			EndDialog(hDlg, NULL);
			return TRUE;
		}
	}
	return FALSE;
}
