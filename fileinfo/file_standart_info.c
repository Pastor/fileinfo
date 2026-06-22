#include "common.h"
#include "file_standart_info.h"
#include "resource.h"
#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")

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
			  /* Task #10: format sizes with human-readable suffix */
			  {
				TCHAR szHR[32];
				StrFormatByteSize64(fsi.AllocationSize.QuadPart, szHR, ARRAYSIZE(szHR));
				SetDlgItemText(hDlg, IDC_ALLOCATED_SIZE, szHR);
				StrFormatByteSize64(fsi.EndOfFile.QuadPart, szHR, ARRAYSIZE(szHR));
				SetDlgItemText(hDlg, IDC_ENDOF_FILE, szHR);
			  }
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
				/* Task #17: NTFS compression ratio (AllocationSize vs EndOfFile) */
				if (fsi.EndOfFile.QuadPart > 0 &&
				    fsi.AllocationSize.QuadPart < fsi.EndOfFile.QuadPart) {
					double ratio = (1.0 - (double)fsi.AllocationSize.QuadPart /
					               fsi.EndOfFile.QuadPart) * 100.0;
					TCHAR szLine[256];
					StringCchPrintf(szLine, ARRAYSIZE(szLine),
					    ResStr(IDS_ADD_COMPR_FMT),
					    fsi.AllocationSize.QuadPart, ratio);
					StringCchCat(szExtra, ARRAYSIZE(szExtra), szLine);
					StringCchCat(szExtra, ARRAYSIZE(szExtra), TEXT("\r\n"));
				}
				/* Show extra info in IDC_EXTRA_INFO, enable control if non-empty */
				{ HWND hExtra = GetDlgItem(hDlg, IDC_EXTRA_INFO);
				  if (hExtra) {
					SetWindowText(hExtra, szExtra[0] ? szExtra : TEXT(""));
					EnableWindow(hExtra, szExtra[0] ? TRUE : FALSE);
				  }
				}
			  }
			} else {
				bClear = TRUE;
			}
		} else {
			bClear = TRUE;
		}

		if ( bClear ) {
			SetDlgItemText( hDlg, IDC_ALLOCATED_SIZE, TEXT("0") );
			SetDlgItemText( hDlg, IDC_ENDOF_FILE, TEXT("0") );
			SetDlgItemInt( hDlg, IDC_NUMBER_OF_LINKS, 0, FALSE );
			CheckDlgButton( hDlg, IDC_IS_DIRECTORY, BST_UNCHECKED );
			CheckDlgButton( hDlg, IDC_DELETED_PENDING, BST_UNCHECKED );
			SetDlgItemText( hDlg, IDC_EXTRA_INFO, TEXT("") );
			EnableWindow( GetDlgItem(hDlg, IDC_EXTRA_INFO), FALSE );
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