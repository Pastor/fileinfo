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
				  /* Task #5: additional FILE_INFO_BY_HANDLE_CLASS data */
				  {
					TCHAR szExtra[512]; szExtra[0] = 0;
					/* FileCompressionInfo */
					{ FILE_COMPRESSION_INFO fci;
					  if (GetFileInformationByHandleEx(hFile, FileCompressionInfo, &fci, sizeof(fci))) {
						if (fci.CompressedFileSize.QuadPart > 0 && fci.CompressedFileSize.QuadPart < fsi.EndOfFile.QuadPart) {
							double ratio = fsi.EndOfFile.QuadPart > 0 ?
								(1.0 - (double)fci.CompressedFileSize.QuadPart / fsi.EndOfFile.QuadPart) * 100.0 : 0.0;
							TCHAR szLine[256];
							StringCchPrintf(szLine, ARRAYSIZE(szLine),
								ResStr(IDS_ADD_COMPR_FMT), fci.CompressedFileSize.QuadPart, ratio);
							StringCchCat(szExtra, ARRAYSIZE(szExtra), szLine);
							StringCchCat(szExtra, ARRAYSIZE(szExtra), TEXT("\r\n"));
						}
					  }
					}
					/* FileStorageInfo */
					{ FILE_STORAGE_INFO fsti;
					  if (GetFileInformationByHandleEx(hFile, FileStorageInfo, &fsti, sizeof(fsti))) {
						if (fsti.LogicalBytesPerSector != fsti.PhysicalBytesPerSectorForPerformance) {
							TCHAR szLine[256];
							StringCchPrintf(szLine, ARRAYSIZE(szLine),
								ResStr(IDS_ADD_STORAGE_FMT),
								fsti.PhysicalBytesPerSectorForPerformance,
								fsti.LogicalBytesPerSector);
							StringCchCat(szExtra, ARRAYSIZE(szExtra), szLine);
							StringCchCat(szExtra, ARRAYSIZE(szExtra), TEXT("\r\n"));
						}
					  }
					}
					/* FileAlignmentInfo */
					{ FILE_ALIGNMENT_INFO fai;
					  if (GetFileInformationByHandleEx(hFile, FileAlignmentInfo, &fai, sizeof(fai))) {
						if (fai.AlignmentRequirement > 0) {
							TCHAR szLine[256];
							StringCchPrintf(szLine, ARRAYSIZE(szLine),
								ResStr(IDS_ADD_ALIGN_FMT), fai.AlignmentRequirement);
							StringCchCat(szExtra, ARRAYSIZE(szExtra), szLine);
							StringCchCat(szExtra, ARRAYSIZE(szExtra), TEXT("\r\n"));
						}
					  }
					}
					/* FileAttributeTagInfo (reparse) */
					{ FILE_ATTRIBUTE_TAG_INFO fati;
					  if (GetFileInformationByHandleEx(hFile, FileAttributeTagInfo, &fati, sizeof(fati))) {
						if (fati.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
							TCHAR szLine[256];
							StringCchPrintf(szLine, ARRAYSIZE(szLine),
								ResStr(IDS_ADD_REPARSE_FMT), fati.ReparseTag);
							StringCchCat(szExtra, ARRAYSIZE(szExtra), szLine);
							StringCchCat(szExtra, ARRAYSIZE(szExtra), TEXT("\r\n"));
						}
					  }
					}
				if (szExtra[0]) {
					/* Show via tooltip on the AllocationSize control */
					HWND hCtrl = GetDlgItem(hDlg, IDC_ALLOCATED_SIZE);
					if (hCtrl) SetWindowText(hCtrl, szExtra);
				}
			  }
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