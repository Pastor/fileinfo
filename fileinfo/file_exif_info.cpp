#include "common.h"
#include "file_exif_info.h"
#include "resource.h"

INT_PTR CALLBACK 
fxi_WindowHandler(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
static HANDLE hFile = nullptr;
static LPTSTR lpstrFileName = nullptr;
  switch ( uMsg ) {
  	case WM_INITDIALOG: {
		return TRUE;
    }
	case WM_SETFILE_HANDLE: {
		hFile = (HANDLE)lParam;
		if ( hFile && hFile != INVALID_HANDLE_VALUE ) {
		}
		break;
	}
	case WM_SETFILE_NAME: {
		int size = lstrlen((LPCTSTR)lParam);
		if (lpstrFileName != nullptr) {
			LocalFree(lpstrFileName);
		}
		lpstrFileName = (LPTSTR)LocalAlloc(LPTR, size * sizeof(TCHAR));
		StringCchCopy(lpstrFileName, size, (LPCTSTR)lParam);
		break;
	}
	case WM_RESETFILE_HANDLE: {
        hFile = nullptr;
		if (lpstrFileName != nullptr) {
			LocalFree(lpstrFileName);
		}
		lpstrFileName = nullptr;
		break;
	}
	case WM_CLOSE:
	case WM_DESTROY: {
		if (lpstrFileName != nullptr) {
			LocalFree(lpstrFileName);
		}
		lpstrFileName = nullptr;
		break;
	}
  }
  return FALSE;
}