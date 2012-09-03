#include "common.h"
#include "file_standart_info.h"
#include "resource.h"

INT_PTR CALLBACK 
fsi_WindowHandler(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
static FILE_STANDARD_INFO fsi;
static HANDLE hFile = NULL;
  switch ( uMsg ) {
  	case WM_INITDIALOG: {
		RtlZeroMemory(&fsi, sizeof(fsi));
		return TRUE;
    }
	case WM_SETFILE_HANDLE: {
		BOOL bClear;
		hFile = (HANDLE)lParam;
		if ( hFile && hFile != INVALID_HANDLE_VALUE ) {
			BOOL bResult;
			bClear = FALSE;
			bResult = GetFileInformationByHandleEx(hFile, FileStandardInfo, &fsi, sizeof(fsi));
			if ( bResult ) {
			  LPTSTR lpMsg;
			  DWORD dwMsgSize;

			  dwMsgSize = 1024 * sizeof(TCHAR);
			  lpMsg = (LPTSTR)LocalAlloc(LPTR, dwMsgSize);
			  StringCchPrintf( lpMsg, dwMsgSize, TEXT("%lld"), fsi.AllocationSize.QuadPart );
			  SetDlgItemText( hDlg, IDC_ALLOCATED_SIZE, lpMsg );
			  StringCchPrintf( lpMsg, dwMsgSize, TEXT("%lld"), fsi.EndOfFile.QuadPart );
			  SetDlgItemText( hDlg, IDC_ENDOF_FILE, lpMsg );
			  LocalFree(lpMsg);
			  SetDlgItemInt(hDlg, IDC_NUMBER_OF_LINKS, fsi.NumberOfLinks, FALSE );
			  CheckDlgButton( hDlg, IDC_IS_DIRECTORY, fsi.Directory ? BST_CHECKED : BST_UNCHECKED );
			  CheckDlgButton( hDlg, IDC_DELETED_PENDING, fsi.DeletePending ? BST_CHECKED : BST_UNCHECKED );
			} else {
				bClear = TRUE;
			}
		} else {
			bClear = TRUE;
		}

		if ( bClear ) {
			SetDlgItemInt( hDlg, IDC_ALLOCATED_SIZE, 0, FALSE );
			SetDlgItemInt( hDlg, IDC_ENDOF_FILE, 0, FALSE );
			SetDlgItemInt( hDlg, IDC_NUMBER_OF_LINKS, 0, FALSE );
			CheckDlgButton( hDlg, IDC_IS_DIRECTORY, BST_UNCHECKED );
			CheckDlgButton( hDlg, IDC_DELETED_PENDING, BST_UNCHECKED );
		}
		break;
	}
	case WM_RESETFILE_HANDLE: {
		hFile = NULL;
		break;
	}
  }
  return FALSE;
}