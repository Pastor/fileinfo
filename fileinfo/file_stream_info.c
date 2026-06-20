#include "common.h"
#include "file_stream_info.h"
#include "resource.h"

static struct tagTransferColumn {
	INT		iId;
	LPTSTR	lpName;
	INT		iWidth;
	INT		iMask;
} g_ListViewColumn [] = {
	{ 0,	TEXT("�"),	      30,  LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM | LVCF_FMT },
	{ 1,	TEXT("���"),	  100, LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM | LVCF_FMT },
	{ 2,	TEXT("������"),	  75,  LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM | LVCF_FMT },
	{ 3,	TEXT("��������"), 80,  LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM | LVCF_FMT }
};


static VOID CALLBACK
private_InitListView(HWND hListView) {
	INT			iCount;
	LVCOLUMN	lvColumn;

    for ( iCount = 0; iCount < sizeof(g_ListViewColumn)/sizeof(g_ListViewColumn[0]); ++iCount ) {
	    ZeroMemory( &lvColumn, sizeof(lvColumn) );
	    lvColumn.mask = g_ListViewColumn[iCount].iMask;
	    lvColumn.iSubItem = g_ListViewColumn[iCount].iId;
	    lvColumn.pszText = g_ListViewColumn[iCount].lpName;
	    lvColumn.cx = g_ListViewColumn[iCount].iWidth;
	    lvColumn.fmt = LVCFMT_CENTER;
	    ListView_InsertColumn( hListView, iCount, &lvColumn );
    }
    ListView_SetExtendedListViewStyleEx( hListView, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT );
    ListView_SetExtendedListViewStyleEx( hListView, LVS_EX_GRIDLINES, LVS_EX_GRIDLINES );
    ListView_SetExtendedListViewStyleEx( hListView, LVS_EX_SINGLEROW, LVS_EX_SINGLEROW );
}

static LPTSTR CALLBACK
private_GetStreamName(HANDLE hFile, LPCTSTR pPrefix) {
	PFILE_NAME_INFO pfni;
	DWORD           dwFileStructureLength = sizeof(FILE_NAME_INFO) + (MAX_PATH * sizeof(TCHAR));
	LPTSTR          lpstrFileName = NULL;
	DWORD           dwPrefixLength =  pPrefix == NULL ? 0 : lstrlen(pPrefix);


	pfni = LocalAlloc(LPTR, dwFileStructureLength);
	if (pfni == NULL)
		return NULL;
	RtlZeroMemory(pfni, dwFileStructureLength);
	if ( GetFileInformationByHandleEx( hFile, FileNameInfo, pfni, dwFileStructureLength) ) {
		/* FileNameLength is in bytes; allocate extra for ":" + prefix + null */
		DWORD cbFileName = pfni->FileNameLength + (dwPrefixLength + 4) * sizeof(TCHAR);
		DWORD cchFileName = cbFileName / sizeof(TCHAR);
		lpstrFileName = (LPTSTR)LocalAlloc(LPTR, cbFileName);
		if (lpstrFileName != NULL) {
			StringCchCopy(lpstrFileName, cchFileName, pfni->FileName);
			if (dwPrefixLength > 0) {
				StringCchCat(lpstrFileName, cchFileName, TEXT(":"));
				StringCchCat(lpstrFileName, cchFileName, pPrefix);
			}
		}
	}
	LocalFree(pfni);
    return lpstrFileName;
}


INT_PTR CALLBACK
fssi_WindowHandler(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static PFILE_STREAM_INFO pfsi = NULL;
    static DWORD     dwPfsiSize = 0;
    static HANDLE    hFile = NULL;
    static HWND      hListView = NULL;
    static HWND      hCreateButton = NULL;
	static HINSTANCE hInstance = NULL;
    switch ( uMsg ) {
  	    case WM_INITDIALOG: {
			hInstance = (HINSTANCE)lParam;
            hCreateButton = GetDlgItem(hDlg, IDC_CREATE_STREAM);
			hListView = GetDlgItem(hDlg, IDC_STREAM_LIST);
			dwPfsiSize = 4 * 1024;
			pfsi = (PFILE_STREAM_INFO)LocalAlloc(LPTR, dwPfsiSize);
			private_InitListView(hListView);
		    return TRUE;
        }
		case WM_COMMAND: {
			switch ( LOWORD(wParam) ) {
				case IDM_VIEW_STREAM: {
					LPTSTR lpstrFileName;
					INT iSelected;
					

					iSelected = ListView_GetSelectionMark(hListView);
					if ( iSelected >= 0 ) {
						TCHAR szName[MAX_PATH + 64];
						ListView_GetItemText(hListView, iSelected, 1, szName, ARRAYSIZE(szName));
						lpstrFileName = private_GetStreamName(hFile, szName);
						if ( lpstrFileName ) {
                            MessageBox(hDlg, lpstrFileName, NULL, 0);
							LocalFree(lpstrFileName);
						}
					}
					break;
				}
                case IDC_CREATE_STREAM:
				case IDM_CREATE_STREAM: {
					LPTSTR lpstrFileName;

					lpstrFileName = private_GetStreamName(hFile, TEXT(""));
					if ( lpstrFileName ) {
						LocalFree(lpstrFileName);
					}
					break;
				}
			}
			break;
		}
		case WM_NOTIFY: {
			LPNMHDR hdr = (LPNMHDR)lParam;
			switch (hdr->code) { 
		        case NM_RCLICK: {
					/** Or WM_MENUCOMMAND */
					if ( hdr->idFrom == IDC_STREAM_LIST ) {
						HMENU hStreamMenu;
			            BOOL bView = FALSE;
                        UINT uFlags;
						POINT p;

						GetCursorPos(&p);
			            uFlags = MF_BYPOSITION | MF_STRING | MF_POPUP;
			            bView = ListView_GetSelectedCount(hListView) != 0;
			            if ( !bView )
                            uFlags |= MF_DISABLED;
						if ( ListView_GetItemCount(hListView) == 0 )
							break;
						hStreamMenu = CreatePopupMenu();
			            AppendMenu(hStreamMenu, uFlags, IDM_VIEW_STREAM,   TEXT("����������"));
                        AppendMenu(hStreamMenu, MF_BYPOSITION | MF_STRING, IDM_CREATE_STREAM, TEXT("�������"));
                        SetForegroundWindow(hListView);
                        TrackPopupMenu(hStreamMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, p.x, p.y, 0, hListView, NULL);
			            DestroyMenu(hStreamMenu);
					}
					break;
			    }
				case NM_CLICK: {
					if (hdr->idFrom == IDC_STREAM_LIST) {
						INT iSelected;


						iSelected = ListView_GetSelectionMark(hListView);
						if (iSelected >= 0) {
							TCHAR szName[MAX_PATH + 64];

							ListView_GetItemText(hListView, iSelected, 1, szName, ARRAYSIZE(szName));
							CreateToolTip(IDC_STREAM_LIST, hDlg, hInstance, szName);
						}
					}
					break;
				}
			}
			break;
		}
	  case WM_SETFILE_HANDLE: {
		  BOOL bClear;
		  hFile = (HANDLE)lParam;
		  if ( hFile && hFile != INVALID_HANDLE_VALUE ) {
			  BOOL bResult;
			  bClear = FALSE;
			  /* Grow the buffer until the API is satisfied (ERROR_MORE_DATA). */
			  for (;;) {
				  bResult = GetFileInformationByHandleEx(hFile, FileStreamInfo, pfsi, dwPfsiSize);
				  if (bResult)
					  break;
				  if (GetLastError() != ERROR_MORE_DATA)
					  break;
				  dwPfsiSize *= 2;
				  LocalFree(pfsi);
				  pfsi = (PFILE_STREAM_INFO)LocalAlloc(LPTR, dwPfsiSize);
				  if (!pfsi) {
					  bResult = FALSE;
					  break;
				  }
			  }
			  if ( bResult ) {
					LVITEM	lvItem;
					TCHAR   szSubBuffer[1024];
					INT nIndex;
                    DWORD dwOffset;
					PFILE_STREAM_INFO pInfo;

					dwOffset = 0;
					nIndex = 0;
                    pInfo = pfsi;
					ListView_DeleteAllItems(hListView);
					ZeroMemory( &lvItem, sizeof(lvItem) );
	                lvItem.mask = LVIF_TEXT | LVIF_PARAM | LVIF_STATE; 
	                lvItem.state = 0; 
	                lvItem.stateMask = 0;
					while (TRUE) {
						lvItem.iItem = nIndex;
		                lvItem.iSubItem = 0;
						StringCchPrintf( szSubBuffer, sizeof(szSubBuffer)/sizeof(szSubBuffer[0]), TEXT("%d"), nIndex);
		                lvItem.pszText = szSubBuffer;
		                lvItem.cchTextMax = lstrlen( szSubBuffer );
		                ListView_InsertItem( hListView, &lvItem );
                        /** Name */
						//StringCchPrintf( szSubBuffer, sizeof(szSubBuffer), TEXT("%*.*S"), pInfo->StreamNameLength / 2, pInfo->StreamNameLength / 2, pInfo->StreamName );
						StringCchPrintf( szSubBuffer, sizeof(szSubBuffer) / sizeof(szSubBuffer[0]), TEXT("%s"), pInfo->StreamName );
						ListView_SetItemText( hListView, nIndex, 1, szSubBuffer );
						/** Size */
						StringCchPrintf( szSubBuffer, sizeof(szSubBuffer) / sizeof(szSubBuffer[0]), TEXT("%I64u"), pInfo->StreamSize.QuadPart);
						ListView_SetItemText( hListView, nIndex, 2, szSubBuffer );
						/** Allocated size */
						StringCchPrintf( szSubBuffer, sizeof(szSubBuffer) / sizeof(szSubBuffer[0]), TEXT("%I64u"), pInfo->StreamAllocationSize.QuadPart);
						ListView_SetItemText( hListView, nIndex, 3, szSubBuffer );
						++nIndex;
						if ( !pInfo->NextEntryOffset )
							break;
						pInfo = (PFILE_STREAM_INFO)( (LPBYTE)pInfo + pInfo->NextEntryOffset );
					}
			  } else {
				  bClear = TRUE;
			  }
		  } else {
			  bClear = TRUE;
		  }

		  if ( bClear ) {
			  ListView_DeleteAllItems(hListView);
		  }

          EnableWindow(hCreateButton, hFile != NULL && hFile != INVALID_HANDLE_VALUE);
		  break;
	  }
	  case WM_RESETFILE_HANDLE: {
          hFile = NULL;
		  break;
	  }
	  case WM_CLOSE:
	  case WM_DESTROY: {
		  LocalFree(pfsi);
		  break;
	  }
  }
  return FALSE;
}