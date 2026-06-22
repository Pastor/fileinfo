#include <windows.h>
#include <aclapi.h>
#pragma comment(lib, "advapi32.lib")
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
  /* Task #15: ReparsePoint checkbox (IDC_CHECK12) */
  if (IsDlgButtonChecked(hDlg, IDC_CHECK12) == BST_CHECKED) {
      dwResult |= FILE_ATTRIBUTE_REPARSE_POINT;
  }
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
  /* Task #15 */
  CheckDlgButton(hDlg, IDC_CHECK12, dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT ? BST_CHECKED : BST_UNCHECKED);
}

static VOID CALLBACK
private_SetTimeDate(HWND hDlg, UINT uDateCtrlId, UINT uTimeCtrlId, PLARGE_INTEGER lpTimeDate) {
	SYSTEMTIME stime;
	FILETIME   ftime;
	HWND hDateWnd = GetDlgItem(hDlg, uDateCtrlId);
	HWND hTimeWnd = GetDlgItem(hDlg, uTimeCtrlId);

	ftime.dwHighDateTime = lpTimeDate->HighPart;
	ftime.dwLowDateTime = lpTimeDate->LowPart;
    /* Task #11: DST-correct conversion (FileTimeToLocalFileTime ignores historical DST) */
    { SYSTEMTIME stUTC;
      FileTimeToSystemTime(&ftime, &stUTC);
      SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &stime);
    }
	DateTime_SetSystemtime(hDateWnd, GDT_VALID, &stime);
	DateTime_SetSystemtime(hTimeWnd, GDT_VALID, &stime);
}

static VOID CALLBACK
private_GetTimeDate(HWND hDlg, UINT uDateCtrlId, UINT uTimeCtrlId, PLARGE_INTEGER lpTimeDate) {
  SYSTEMTIME stime;
  SYSTEMTIME sdate;
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
  /* Task #11: DST-correct reverse conversion */
  { SYSTEMTIME stUTC;
    TzSpecificLocalTimeToSystemTime(NULL, &stime, &stUTC);
    SystemTimeToFileTime(&stUTC, &ftime);
  }
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
		/* Task #16: show file owner */
		{
			PSECURITY_DESCRIPTOR pSD = NULL;
			PSID pOwnerSid = NULL;
			if (GetSecurityInfo(hFile, SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION,
			                    &pOwnerSid, NULL, NULL, NULL, &pSD) == ERROR_SUCCESS) {
				TCHAR szOwner[128] = TEXT(""), szDomain[128] = TEXT("");
				DWORD dwOwner = ARRAYSIZE(szOwner), dwDomain = ARRAYSIZE(szDomain);
				SID_NAME_USE use;
				if (LookupAccountSid(NULL, pOwnerSid, szOwner, &dwOwner,
				                     szDomain, &dwDomain, &use)) {
					TCHAR szFull[260];
					if (szDomain[0])
						StringCchPrintf(szFull, ARRAYSIZE(szFull), TEXT("%s\\%s"), szDomain, szOwner);
					else
						StringCchCopy(szFull, ARRAYSIZE(szFull), szOwner);
					SetDlgItemText(hDlg, IDC_FILE_OWNER, szFull);
				}
				LocalFree(pSD);
			}
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
			/* Mutual exclusion: Normal vs all others; Encrypted vs Compressed */
			if (wCtrlId == IDC_ATTRIBUTE_NORMAL &&
			    IsDlgButtonChecked(hDlg, IDC_ATTRIBUTE_NORMAL) == BST_CHECKED) {
			    /* Normal is set -> clear every other attribute */
			    UINT id;
			    for (id = 1018; id <= 1029; ++id)
			        if (id != IDC_ATTRIBUTE_NORMAL)
			            CheckDlgButton(hDlg, id, BST_UNCHECKED);
			} else if (wCtrlId != IDC_ATTRIBUTE_NORMAL && wCtrlId >= 1018 && wCtrlId <= 1030 &&
			           IsDlgButtonChecked(hDlg, wCtrlId) == BST_CHECKED) {
			    /* Any non-Normal attribute set -> clear Normal */
			    CheckDlgButton(hDlg, IDC_ATTRIBUTE_NORMAL, BST_UNCHECKED);
			    /* Encrypted and Compressed are mutually exclusive */
			    if (wCtrlId == IDC_ATTRIBUTE_CRYPTED)
			        CheckDlgButton(hDlg, IDC_ATTRIBUTE_COMPRESSED, BST_UNCHECKED);
			    else if (wCtrlId == IDC_ATTRIBUTE_COMPRESSED)
			        CheckDlgButton(hDlg, IDC_ATTRIBUTE_CRYPTED, BST_UNCHECKED);
			}
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
			EnableWindow(GetDlgItem(hDlg, IDC_FILEINFO_SAVE), FALSE);

			dwFileAttributes = fbi_GetFileAttributes(hDlg);
			private_GetTimeDate(hDlg, IDC_BFI_CREATION_TIME_DATE, IDC_BFI_CREATION_TIME_TIME, &fbi.CreationTime);
			private_GetTimeDate(hDlg, IDC_BFI_CHANGE_TIME_DATE, IDC_BFI_CHANGE_TIME_TIME, &fbi.ChangeTime);
			private_GetTimeDate(hDlg, IDC_BFI_LAST_ACCESS_TIME_DATE, IDC_BFI_LAST_ACCESS_TIME_TIME, &fbi.LastAccessTime);
			private_GetTimeDate(hDlg, IDC_BFI_LAST_WRITE_TIME_DATE, IDC_BFI_LAST_WRITE_TIME_TIME, &fbi.LastWriteTime);
	        
			fbi.FileAttributes = dwFileAttributes;
			/* Task #12: confirm before writing timestamps */
			if (MessageBox(hDlg, ResStr(IDS_SAVE_CONFIRM_MSG), ResStr(IDS_SAVE_CONFIRM_TITLE),
			               MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES) {
				EnableWindow(GetDlgItem(hDlg, IDC_FILEINFO_SAVE), TRUE);
				break;
			}
			if ( !SetFileInformationByHandle( hFile, FileBasicInfo, &fbi, sizeof(fbi) ) ) {
				common_ShowError(hDlg, ResStr(IDS_ERR_SETFILEINFO));
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
