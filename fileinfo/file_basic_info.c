#include <windows.h>
#include "common.h"
#include "file_basic_info.h"
#include "resource.h"

VOID CALLBACK 
fbi_EnableControls(HWND hDlg, BOOL bStatus) {
	UINT uCtrlId;

	for (uCtrlId = 1010; uCtrlId <= 1034; ++uCtrlId) {
		EnableWindow( GetDlgItem(hDlg, uCtrlId), bStatus );
	}
}

DWORD CALLBACK 
fbi_GetFileAttributes(HWND hDlg) {
  DWORD dwResult = 0;

  if ( IsDlgButtonChecked(hDlg, IDC_ATTRIBUTE_ARCHIVE) == BST_CHECKED ) {
	  dwResult |= FILE_ATTRIBUTE_ARCHIVE;
  }
  if ( IsDlgButtonChecked(hDlg, IDC_ATTRIBUTE_HIDE) == BST_CHECKED ) {
	  dwResult |= FILE_ATTRIBUTE_HIDDEN;
  }
  if ( IsDlgButtonChecked(hDlg, IDC_ATTRIBUTE_NORMAL) == BST_CHECKED ) {
	  dwResult |= FILE_ATTRIBUTE_NORMAL;
  }
  if ( IsDlgButtonChecked(hDlg, IDC_ATTRIBUTE_INDEXED) == BST_CHECKED ) {
	  dwResult |= FILE_ATTRIBUTE_NOT_CONTENT_INDEXED;
  }
  if ( IsDlgButtonChecked(hDlg, IDC_ATTRIBUTE_OFFLINE) == BST_CHECKED ) {
	  dwResult |= FILE_ATTRIBUTE_OFFLINE;
  }
  if ( IsDlgButtonChecked(hDlg, IDC_ATTRIBUTE_READONLY) == BST_CHECKED ) {
	  dwResult |= FILE_ATTRIBUTE_READONLY;
  }
  if ( IsDlgButtonChecked(hDlg, IDC_ATTRIBUTE_SYSTEM) == BST_CHECKED ) {
	  dwResult |= FILE_ATTRIBUTE_SYSTEM;
  }
  if ( IsDlgButtonChecked(hDlg, IDC_ATTRIBUTE_TEMPORARY) == BST_CHECKED ) {
	  dwResult |= FILE_ATTRIBUTE_TEMPORARY;
  }
  if ( IsDlgButtonChecked(hDlg, IDC_ATTRIBUTE_COMPRESSED) == BST_CHECKED ) {
	  dwResult |= FILE_ATTRIBUTE_COMPRESSED;
  }
  if ( IsDlgButtonChecked(hDlg, IDC_ATTRIBUTE_DIRECTORY) == BST_CHECKED ) {
	  dwResult |= FILE_ATTRIBUTE_DIRECTORY;
  }
  if ( IsDlgButtonChecked(hDlg, IDC_ATTRIBUTE_CRYPTED) == BST_CHECKED ) {
	  dwResult |= FILE_ATTRIBUTE_ENCRYPTED;
  }
  if (IsDlgButtonChecked(hDlg, IDC_ATTRIBUTE_SPARSE) == BST_CHECKED) {
      dwResult |= FILE_ATTRIBUTE_SPARSE_FILE;
  }
  //FILE_ATTRIBUTE_SPARSE_FILE
  return dwResult;
}

VOID CALLBACK 
fbi_SetFileAttributes(HWND hDlg, DWORD dwFileAttributes) {
  CheckDlgButton(hDlg, IDC_ATTRIBUTE_ARCHIVE, dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE ? BST_CHECKED : BST_UNCHECKED);
  CheckDlgButton(hDlg, IDC_ATTRIBUTE_HIDE, dwFileAttributes & FILE_ATTRIBUTE_HIDDEN ? BST_CHECKED : BST_UNCHECKED);
  CheckDlgButton(hDlg, IDC_ATTRIBUTE_NORMAL, dwFileAttributes & FILE_ATTRIBUTE_NORMAL ? BST_CHECKED : BST_UNCHECKED);
  CheckDlgButton(hDlg, IDC_ATTRIBUTE_INDEXED, dwFileAttributes & FILE_ATTRIBUTE_NOT_CONTENT_INDEXED ? BST_CHECKED : BST_UNCHECKED);
  CheckDlgButton(hDlg, IDC_ATTRIBUTE_OFFLINE, dwFileAttributes & FILE_ATTRIBUTE_OFFLINE ? BST_CHECKED : BST_UNCHECKED);
  CheckDlgButton(hDlg, IDC_ATTRIBUTE_READONLY, dwFileAttributes & FILE_ATTRIBUTE_READONLY ? BST_CHECKED : BST_UNCHECKED);
  CheckDlgButton(hDlg, IDC_ATTRIBUTE_SYSTEM, dwFileAttributes & FILE_ATTRIBUTE_SYSTEM ? BST_CHECKED : BST_UNCHECKED);
  CheckDlgButton(hDlg, IDC_ATTRIBUTE_TEMPORARY, dwFileAttributes & FILE_ATTRIBUTE_TEMPORARY ? BST_CHECKED : BST_UNCHECKED);
  CheckDlgButton(hDlg, IDC_ATTRIBUTE_COMPRESSED, dwFileAttributes & FILE_ATTRIBUTE_COMPRESSED ? BST_CHECKED : BST_UNCHECKED);
  CheckDlgButton(hDlg, IDC_ATTRIBUTE_DIRECTORY, dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ? BST_CHECKED : BST_UNCHECKED);
  CheckDlgButton(hDlg, IDC_ATTRIBUTE_CRYPTED, dwFileAttributes & FILE_ATTRIBUTE_ENCRYPTED ? BST_CHECKED : BST_UNCHECKED);
  CheckDlgButton(hDlg, IDC_ATTRIBUTE_SPARSE, dwFileAttributes & FILE_ATTRIBUTE_SPARSE_FILE ? BST_CHECKED : BST_UNCHECKED);
}

static VOID CALLBACK
private_SetTimeDate(HWND hDlg, UINT uDateCtrlId, UINT uTimeCtrlId, PLARGE_INTEGER lpTimeDate) {
	SYSTEMTIME stime;
	FILETIME   ftime;
    FILETIME ltime;
	HWND hDateWnd = GetDlgItem(hDlg, uDateCtrlId);
	HWND hTimeWnd = GetDlgItem(hDlg, uTimeCtrlId);

	ftime.dwHighDateTime = lpTimeDate->HighPart;
	ftime.dwLowDateTime = lpTimeDate->LowPart;
    FileTimeToLocalFileTime(&ftime, &ltime);
	FileTimeToSystemTime(&ltime, &stime);
	DateTime_SetSystemtime(hDateWnd, GDT_VALID, &stime);
	DateTime_SetSystemtime(hTimeWnd, GDT_VALID, &stime);
}

static VOID CALLBACK
private_GetTimeDate(HWND hDlg, UINT uDateCtrlId, UINT uTimeCtrlId, PLARGE_INTEGER lpTimeDate) {
  SYSTEMTIME stime;
  SYSTEMTIME sdate;
  FILETIME ltime;
  FILETIME ftime;
  HWND hTime = GetDlgItem(hDlg, uTimeCtrlId);
  HWND hDate = GetDlgItem(hDlg, uDateCtrlId);

  RtlZeroMemory(&stime, sizeof(stime));
  RtlZeroMemory(&sdate, sizeof(sdate));
  DateTime_GetSystemtime(hTime, &stime);
  DateTime_GetSystemtime(hDate, &sdate);

  stime.wDay = sdate.wDay;
  stime.wDayOfWeek = sdate.wDayOfWeek;
  stime.wMonth = sdate.wMonth;
  stime.wYear = sdate.wYear;
  SystemTimeToFileTime(&stime, &ltime);
  LocalFileTimeToFileTime(&ltime, &ftime);
  lpTimeDate->HighPart = ftime.dwHighDateTime;
  lpTimeDate->LowPart = ftime.dwLowDateTime;
}

static VOID CALLBACK
private_SetFileBasicInformation(HWND hDlg, PFILE_BASIC_INFO pfbi, HANDLE hFile) {
	fbi_EnableControls(hDlg, FALSE);
    if (hFile != NULL && hFile != INVALID_HANDLE_VALUE) {
		BOOL bResult = GetFileInformationByHandleEx( hFile, FileBasicInfo, pfbi, sizeof(FILE_BASIC_INFO));
		if ( bResult ) {
			fbi_SetFileAttributes(hDlg, pfbi->FileAttributes);
		}
		EnableWindow(GetDlgItem(hDlg, IDC_LOCKER), TRUE);
		CheckDlgButton(hDlg, IDC_LOCKER, BST_UNCHECKED);
        
		private_SetTimeDate(hDlg, IDC_BFI_CREATION_TIME_DATE, IDC_BFI_CREATION_TIME_TIME, &pfbi->CreationTime);
		private_SetTimeDate(hDlg, IDC_BFI_CHANGE_TIME_DATE, IDC_BFI_CHANGE_TIME_TIME, &pfbi->ChangeTime);
		private_SetTimeDate(hDlg, IDC_BFI_LAST_ACCESS_TIME_DATE, IDC_BFI_LAST_ACCESS_TIME_TIME, &pfbi->LastAccessTime);
		private_SetTimeDate(hDlg, IDC_BFI_LAST_WRITE_TIME_DATE, IDC_BFI_LAST_WRITE_TIME_TIME, &pfbi->LastWriteTime);
	}
}

static VOID CALLBACK
private_UpdateButtonState(HWND hDlg, PFILE_BASIC_INFO pfbi) {
	LARGE_INTEGER lint;
	BOOL bEnable = pfbi->FileAttributes != fbi_GetFileAttributes(hDlg);
	if ( !bEnable ) {
		/** If change date time */
        private_GetTimeDate(hDlg, IDC_BFI_CREATION_TIME_DATE, IDC_BFI_CREATION_TIME_TIME, &lint);
		bEnable = (bEnable == TRUE || lint.QuadPart != pfbi->CreationTime.QuadPart);
		private_GetTimeDate(hDlg, IDC_BFI_CHANGE_TIME_DATE, IDC_BFI_CHANGE_TIME_TIME, &lint);
		bEnable = (bEnable == TRUE || lint.QuadPart != pfbi->ChangeTime.QuadPart);
		private_GetTimeDate(hDlg, IDC_BFI_LAST_ACCESS_TIME_DATE, IDC_BFI_LAST_ACCESS_TIME_TIME, &lint);
		bEnable = (bEnable == TRUE || lint.QuadPart != pfbi->LastAccessTime.QuadPart);
		private_GetTimeDate(hDlg, IDC_BFI_LAST_WRITE_TIME_DATE, IDC_BFI_LAST_WRITE_TIME_TIME, &lint);
		bEnable = (bEnable == TRUE || lint.QuadPart != pfbi->LastWriteTime.QuadPart);
	}
    EnableWindow(GetDlgItem(hDlg, IDC_FILEINFO_RESTORE), bEnable && IsDlgButtonChecked(hDlg, IDC_LOCKER) == BST_CHECKED);
	EnableWindow(GetDlgItem(hDlg, IDC_FILEINFO_SAVE), bEnable && IsDlgButtonChecked(hDlg, IDC_LOCKER) == BST_CHECKED);
}

INT_PTR CALLBACK 
fbi_WindowHandler(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
static FILE_BASIC_INFO fbi;
static HANDLE hFile = NULL;
  switch ( uMsg ) {
  	case WM_INITDIALOG: {
		fbi_EnableControls(hDlg, FALSE);
		RtlZeroMemory(&fbi, sizeof(fbi));
		return TRUE;
    }
	case WM_SETFILE_HANDLE: {
		hFile = (HANDLE)lParam;
		private_SetFileBasicInformation(hDlg, &fbi, hFile);
		private_UpdateButtonState(hDlg, &fbi);
		break;
	}
	case WM_NOTIFY: {
		LPNMHDR lpnmHdr = (LPNMHDR)lParam;
		switch ( lpnmHdr->code ) {
	      case DTN_DATETIMECHANGE: {
			  private_UpdateButtonState(hDlg, &fbi);
			  break;
		  }
		}
		break;
	}
    case WM_COMMAND: {
	  WORD wNotifyId = HIWORD(wParam);
	  WORD wCtrlId = LOWORD(wParam);
	  switch ( wNotifyId ) {
	    case BN_CLICKED: {
			if ( wCtrlId == IDC_LOCKER ) {
				fbi_EnableControls(hDlg, IsDlgButtonChecked(hDlg, IDC_LOCKER) == BST_CHECKED);
				if ( hFile && hFile != INVALID_HANDLE_VALUE )
					EnableWindow(GetDlgItem(hDlg, IDC_LOCKER), TRUE);
				EnableWindow(GetDlgItem(hDlg, IDC_FILEINFO_RESTORE), FALSE);
				EnableWindow(GetDlgItem(hDlg, IDC_FILEINFO_SAVE), FALSE);
			} else {
				private_UpdateButtonState(hDlg, &fbi);
			}
			break;
		}
	  }
	  switch ( wCtrlId ) {
	    case IDC_FILEINFO_SAVE: {
			DWORD dwFileAttributes;

			dwFileAttributes = fbi_GetFileAttributes(hDlg);
			private_GetTimeDate(hDlg, IDC_BFI_CREATION_TIME_DATE, IDC_BFI_CREATION_TIME_TIME, &fbi.CreationTime);
			private_GetTimeDate(hDlg, IDC_BFI_CHANGE_TIME_DATE, IDC_BFI_CHANGE_TIME_TIME, &fbi.ChangeTime);
			private_GetTimeDate(hDlg, IDC_BFI_LAST_ACCESS_TIME_DATE, IDC_BFI_LAST_ACCESS_TIME_TIME, &fbi.LastAccessTime);
			private_GetTimeDate(hDlg, IDC_BFI_LAST_WRITE_TIME_DATE, IDC_BFI_LAST_WRITE_TIME_TIME, &fbi.LastWriteTime);
	        
			fbi.FileAttributes = dwFileAttributes;
			if ( !SetFileInformationByHandle( hFile, FileBasicInfo, &fbi, sizeof(fbi) ) ) {
				common_ShowError(hDlg, TEXT("SetFileInformationByHandle"));
			}
            private_SetFileBasicInformation(hDlg, &fbi, hFile);
			private_UpdateButtonState(hDlg, &fbi);
			break;
		}
		case IDC_FILEINFO_RESTORE: {
		    fbi_SetFileAttributes(hDlg, fbi.FileAttributes);
		    private_UpdateButtonState(hDlg, &fbi);
		    break;
		}
	  }
	  break;
	}
	case WM_RESETFILE_HANDLE: {
        hFile = NULL;
		fbi_EnableControls(hDlg, FALSE);
		break;
	}
  }
  return FALSE;
}
