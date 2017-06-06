#include "common.h"
#include "file_id_info.h"
#include "resource.h"

INT_PTR CALLBACK 
fii_WindowHandler(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
static FILE_ID_INFO fii;
static HANDLE hFile = nullptr;
  switch ( uMsg ) {
  	case WM_INITDIALOG: {
		RtlZeroMemory(&fii, sizeof(fii));
		return TRUE;
    }
	case WM_SETFILE_HANDLE: {
		BOOL bClear;
		hFile = (HANDLE)lParam;
		if ( hFile && hFile != INVALID_HANDLE_VALUE ) {
			BOOL bResult;
			bClear = FALSE;
			bResult = GetFileInformationByHandleEx(hFile, FileIdInfo, &fii, sizeof(fii));
			if ( bResult ) {
				INT i, offset;
				TCHAR szBuffer[1024];

				StringCchPrintf(szBuffer, sizeof(szBuffer) / sizeof(szBuffer[0]), TEXT("%I64u"), fii.VolumeSerialNumber);
				SetDlgItemText(hDlg, IDC_VOLUME_SERIAL_NUMBER, szBuffer);

				szBuffer[0] = (TCHAR)0;
				for (i = 0, offset = 0; i < sizeof(fii.FileId.Identifier) / sizeof(fii.FileId.Identifier[0]); offset += 2, i++) {
					StringCchPrintf(szBuffer + offset, (sizeof(szBuffer) / sizeof(szBuffer[0])) - offset, TEXT("%02X"), fii.FileId.Identifier[i]);
				}
				szBuffer[offset] = (TCHAR)0;
				SetDlgItemText(hDlg, IDC_FILE_UNIQUE_ID, szBuffer);
			} else {
				bClear = TRUE;
			}
		} else {
			bClear = TRUE;
		}

		if ( bClear ) {
			SetDlgItemText(hDlg, IDC_VOLUME_SERIAL_NUMBER, TEXT(""));
			SetDlgItemText(hDlg, IDC_FILE_UNIQUE_ID, TEXT(""));
		}
		break;
	}
	case WM_RESETFILE_HANDLE: {
        hFile = nullptr;
		break;
	}
  }
  return FALSE;
}