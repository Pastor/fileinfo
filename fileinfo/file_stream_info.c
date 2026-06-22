#include "common.h"
#include <tchar.h>
#include <commdlg.h>
#include "file_stream_info.h"
#include "resource.h"

#pragma comment(lib, "comdlg32.lib")

/* Private message: wParam=1 confirm (Enter), wParam=0 cancel (Esc/KillFocus) */
#define WM_STREAM_NAME_DONE  (WM_APP + 100)

/* State for the inline stream-name editor */
static WNDPROC pfnOldStreamNameEditProc = NULL;
static INT     iStreamNameEditItem      = -1;

static struct tagTransferColumn {
	INT		iId;
	UINT	nNameID;
	INT		iWidth;
	INT		iMask;
} g_ListViewColumn [] = {
	{ 0,	IDS_COL_NUM,	      30,  LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM | LVCF_FMT },
	{ 1,	IDS_COL_NAME,	  100, LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM | LVCF_FMT },
	{ 2,	IDS_COL_SIZE,	  75,  LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM | LVCF_FMT },
	{ 3,	IDS_COL_ALLOC, 80,  LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM | LVCF_FMT }
};


static VOID CALLBACK
private_InitListView(HWND hListView) {
	INT			iCount;
	LVCOLUMN	lvColumn;

    for ( iCount = 0; iCount < sizeof(g_ListViewColumn)/sizeof(g_ListViewColumn[0]); ++iCount ) {
	    ZeroMemory( &lvColumn, sizeof(lvColumn) );
	    lvColumn.mask = g_ListViewColumn[iCount].iMask;
	    lvColumn.iSubItem = g_ListViewColumn[iCount].iId;
	    lvColumn.pszText = (LPTSTR)ResStr(g_ListViewColumn[iCount].nNameID);
	    lvColumn.cx = g_ListViewColumn[iCount].iWidth;
	    lvColumn.fmt = LVCFMT_CENTER;
	    ListView_InsertColumn( hListView, iCount, &lvColumn );
    }
    ListView_SetExtendedListViewStyleEx( hListView, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT );
    ListView_SetExtendedListViewStyleEx( hListView, LVS_EX_GRIDLINES, LVS_EX_GRIDLINES );
    ListView_SetExtendedListViewStyleEx( hListView, LVS_EX_SINGLEROW, LVS_EX_SINGLEROW );
}

/* ------------------------------------------------------------------ */
/* Inline stream-name edit control                                      */
/* ------------------------------------------------------------------ */

/* Subclassed WndProc: intercepts Enter / Escape / KillFocus. */
static LRESULT CALLBACK
StreamNameEditProc(HWND hEdit, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    HWND hDlg = (HWND)(LONG_PTR)GetWindowLongPtr(hEdit, GWLP_USERDATA);

    switch (uMsg) {
    case WM_GETDLGCODE:
        return DLGC_WANTALLKEYS;
    case WM_KEYDOWN:
        if (wParam == VK_RETURN) {
            PostMessage(hDlg, WM_STREAM_NAME_DONE, 1, 0);
            return 0;
        }
        if (wParam == VK_ESCAPE) {
            PostMessage(hDlg, WM_STREAM_NAME_DONE, 0, 0);
            return 0;
        }
        break;
    case WM_KILLFOCUS:
        PostMessage(hDlg, WM_STREAM_NAME_DONE, 0, 0);
        break;
    }
    return CallWindowProc(pfnOldStreamNameEditProc, hEdit, uMsg, wParam, lParam);
}

/* Destroy the inline edit control safely. */
static VOID
private_DestroyStreamEdit(HWND *phEdit)
{
    if (*phEdit) {
        DestroyWindow(*phEdit);
        *phEdit = NULL;
        pfnOldStreamNameEditProc = NULL;
    }
}

/* Insert a blank row, create an EDIT control over the Name cell,
   subclass it, and give it focus. */
static VOID
private_BeginStreamCreate(HWND hDlg, HWND hListView, HINSTANCE hInst,
                          HWND *phStreamNameEdit)
{
    INT   iCount;
    RECT  rc;
    HWND  hEdit;
    LVITEM lvi;

    if (*phStreamNameEdit)   /* already editing */
        return;

    iCount = ListView_GetItemCount(hListView);

    ZeroMemory(&lvi, sizeof(lvi));
    lvi.mask    = LVIF_TEXT;
    lvi.iItem   = iCount;
    lvi.pszText = TEXT("");
    ListView_InsertItem(hListView, &lvi);
    ListView_SetItemText(hListView, iCount, 1, TEXT(""));
    ListView_EnsureVisible(hListView, iCount, FALSE);

    /* Position the edit over column 1 of the new row */
    ListView_GetSubItemRect(hListView, iCount, 1, LVIR_BOUNDS, &rc);

    hEdit = CreateWindowEx(
        0, WC_EDIT, TEXT(""),
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        rc.left, rc.top,
        rc.right  - rc.left,
        rc.bottom - rc.top,
        hListView, NULL, hInst, NULL);

    if (!hEdit) {
        ListView_DeleteItem(hListView, iCount);
        return;
    }

    SetWindowLongPtr(hEdit, GWLP_USERDATA, (LONG_PTR)hDlg);
    pfnOldStreamNameEditProc = (WNDPROC)(LONG_PTR)
        SetWindowLongPtr(hEdit, GWLP_WNDPROC, (LONG_PTR)StreamNameEditProc);

    iStreamNameEditItem = iCount;
    *phStreamNameEdit   = hEdit;
    SendMessage(hEdit, WM_SETFONT, (WPARAM)SendMessage(hListView, WM_GETFONT, 0, 0), TRUE);
    SetFocus(hEdit);
}

/* Write file contents to an NTFS alternate data stream.
   lpstrStreamName is the full stream name as listed in FILE_STREAM_INFO
   (e.g. ":backup:$DATA").  Shows an Open-file dialog and a confirmation
   message before writing.  Refreshes the stream list on success. */
static VOID
private_LoadFileToStream(HWND hDlg, LPCTSTR lpstrFilePath,
                         LPCTSTR lpstrStreamName, HANDLE hFile)
{
    TCHAR        szSource[MAX_PATH];
    TCHAR        szAdsPath[MAX_PATH * 2];
    LPTSTR       szConfirm = NULL;
    OPENFILENAME ofn;
    HANDLE       hSrc    = INVALID_HANDLE_VALUE;
    HANDLE       hStream = INVALID_HANDLE_VALUE;
    BYTE         buf[64 * 1024];
    DWORD        dwRead, dwWritten;
    BOOL         bOk = TRUE;

    /* Select the source file */
    ZeroMemory(&ofn, sizeof(ofn));
    szSource[0]       = TEXT('\0');
    ofn.lStructSize   = sizeof(ofn);
    ofn.hwndOwner     = hDlg;
    ofn.lpstrFile     = szSource;
    ofn.nMaxFile      = ARRAYSIZE(szSource);
    ofn.lpstrTitle    = ResStr(IDS_STREAM_OPEN_TITLE);
    ofn.lpstrFilter   = TEXT("Все файлы (*.*)\0*.*\0");
    ofn.Flags         = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (!GetOpenFileName(&ofn))
        return;

    /* Full ADS path */
    StringCchCopy(szAdsPath, ARRAYSIZE(szAdsPath), lpstrFilePath);
    StringCchCat (szAdsPath, ARRAYSIZE(szAdsPath), lpstrStreamName);

    /* Confirmation: allocate buffer sized to actual inputs */
    {
        int nChars = _scwprintf(ResStr(IDS_STREAM_CONFIRM_FMT),
                                szSource, lpstrStreamName) + 1;
        szConfirm = (LPTSTR)HeapAlloc(GetProcessHeap(), 0,
                                      (size_t)nChars * sizeof(TCHAR));
        if (szConfirm)
            swprintf_s(szConfirm, (size_t)nChars, ResStr(IDS_STREAM_CONFIRM_FMT),
                       szSource, lpstrStreamName);
    }
    if (!szConfirm || MessageBox(hDlg, szConfirm, ResStr(IDS_STREAM_CONFIRM_TITLE),
                                 MB_YESNO | MB_ICONQUESTION) != IDYES) {
        HeapFree(GetProcessHeap(), 0, szConfirm);
        return;
    }
    HeapFree(GetProcessHeap(), 0, szConfirm);
    szConfirm = NULL;

    /* Open source */
    hSrc = CreateFile(szSource,
                      GENERIC_READ, FILE_SHARE_READ,
                      NULL, OPEN_EXISTING,
                      FILE_ATTRIBUTE_NORMAL, NULL);
    if (hSrc == INVALID_HANDLE_VALUE) {
        common_ShowError(hDlg, ResStr(IDS_STREAM_ERR_SRC_OPEN));
        return;
    }

    /* Open / overwrite the ADS */
    hStream = CreateFile(szAdsPath,
                         GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                         NULL, CREATE_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL, NULL);
    if (hStream == INVALID_HANDLE_VALUE) {
        common_ShowError(hDlg, ResStr(IDS_STREAM_ERR_DST_OPEN));
        CloseHandle(hSrc);
        return;
    }

    /* Copy in chunks */
    while (bOk
           && ReadFile(hSrc, buf, sizeof(buf), &dwRead, NULL)
           && dwRead > 0) {
        bOk = WriteFile(hStream, buf, dwRead, &dwWritten, NULL)
              && (dwWritten == dwRead);
    }

    if (!bOk)
        common_ShowError(hDlg, ResStr(IDS_STREAM_ERR_WRITE));

    CloseHandle(hStream);
    CloseHandle(hSrc);

    if (bOk) {
        MessageBox(hDlg, ResStr(IDS_STREAM_WRITE_OK),
                   ResStr(IDS_STREAM_WRITE_OK_TITLE), MB_OK | MB_ICONINFORMATION);
        /* Refresh sizes in the list */
        SendMessage(hDlg, WM_SETFILE_HANDLE, 0, (LPARAM)hFile);
    }
}

/* ------------------------------------------------------------------ */
/* Copy the contents of an NTFS stream to a regular file.
   lpstrFilePath — base file path (e.g. C:\dir\file.txt)
   lpstrStreamName — stream name as reported by FILE_STREAM_INFO
                     (e.g. ":mydata:$DATA")
   Shows a Save File dialog and writes stream bytes to the chosen file. */
static VOID CALLBACK
private_SaveStream(HWND hDlg, LPCTSTR lpstrFilePath, LPCTSTR lpstrStreamName)
{
    TCHAR        szAdsPath[MAX_PATH * 2];
    TCHAR        szSavePath[MAX_PATH];
    OPENFILENAME ofn;
    HANDLE       hStream  = INVALID_HANDLE_VALUE;
    HANDLE       hOutFile = INVALID_HANDLE_VALUE;
    BYTE         buf[64 * 1024];
    DWORD        dwRead, dwWritten;
    BOOL         bOk = TRUE;

    /* Build full ADS path: "C:\file.txt:streamname:$DATA" */
    StringCchCopy(szAdsPath, ARRAYSIZE(szAdsPath), lpstrFilePath);
    StringCchCat (szAdsPath, ARRAYSIZE(szAdsPath), lpstrStreamName);

    /* Show Save-As dialog */
    ZeroMemory(&ofn, sizeof(ofn));
    szSavePath[0]     = TEXT('\0');
    ofn.lStructSize   = sizeof(ofn);
    ofn.hwndOwner     = hDlg;
    ofn.lpstrFile     = szSavePath;
    ofn.nMaxFile      = ARRAYSIZE(szSavePath);
    ofn.lpstrTitle    = ResStr(IDS_STREAM_SAVE_TITLE);
    ofn.lpstrFilter   = TEXT("Все файлы (*.*)\0*.*\0");
    ofn.Flags         = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (!GetSaveFileName(&ofn))
        return; /* user cancelled */

    /* Open stream for reading */
    hStream = CreateFile(szAdsPath,
                         GENERIC_READ, FILE_SHARE_READ,
                         NULL, OPEN_EXISTING,
                         FILE_ATTRIBUTE_NORMAL, NULL);
    if (hStream == INVALID_HANDLE_VALUE) {
        common_ShowError(hDlg, ResStr(IDS_STREAM_ERR_OPEN));
        return;
    }

    /* Create destination file */
    hOutFile = CreateFile(szSavePath,
                          GENERIC_WRITE, 0,
                          NULL, CREATE_ALWAYS,
                          FILE_ATTRIBUTE_NORMAL, NULL);
    if (hOutFile == INVALID_HANDLE_VALUE) {
        common_ShowError(hDlg, ResStr(IDS_STREAM_ERR_FILE_OPEN));
        CloseHandle(hStream);
        return;
    }

    /* Copy in chunks */
    while (bOk
           && ReadFile(hStream, buf, sizeof(buf), &dwRead, NULL)
           && dwRead > 0) {
        bOk = WriteFile(hOutFile, buf, dwRead, &dwWritten, NULL)
              && (dwWritten == dwRead);
    }

    if (!bOk)
        common_ShowError(hDlg, ResStr(IDS_STREAM_ERR_READ));

    CloseHandle(hOutFile);
    CloseHandle(hStream);

    if (bOk)
        MessageBox(hDlg,
                   ResStr(IDS_STREAM_SAVE_OK),
                   ResStr(IDS_STREAM_SAVE_OK_TITLE),
                   MB_OK | MB_ICONINFORMATION);
}

static LPTSTR CALLBACK
private_GetStreamName(HANDLE hFile, LPCTSTR pPrefix) {
	PFILE_NAME_INFO pfni;
	DWORD           dwFileStructureLength = sizeof(FILE_NAME_INFO) + (MAX_PATH * sizeof(TCHAR));
	LPTSTR          lpstrFileName = NULL;
	DWORD           dwPrefixLength =  pPrefix == NULL ? 0 : lstrlen(pPrefix);


	pfni = (PFILE_NAME_INFO)LocalAlloc(LPTR, dwFileStructureLength);
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
    static LPTSTR    lpstrFilePath = NULL;
    static HWND      hStreamNameEdit = NULL;
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
				case IDM_SAVE_STREAM: {
					INT iSelected = ListView_GetSelectionMark(hListView);
					if (iSelected >= 0 && lpstrFilePath) {
						TCHAR szStreamName[MAX_PATH + 64];
						ListView_GetItemText(hListView, iSelected, 1,
						                    szStreamName, ARRAYSIZE(szStreamName));
						private_SaveStream(hDlg, lpstrFilePath, szStreamName);
					}
					break;
				}
                case IDC_CREATE_STREAM: {
                    /* Button: add blank row + inline name editor */
                    private_BeginStreamCreate(hDlg, hListView, hInstance,
                                             &hStreamNameEdit);
                    break;
                }
				case IDM_CREATE_STREAM: {
                    /* Context menu: write a file's contents to selected stream */
                    INT iSel = ListView_GetSelectionMark(hListView);
                    if (iSel < 0 || !lpstrFilePath) {
                        MessageBox(hDlg,
                            ResStr(IDS_STREAM_NO_SEL_MSG),
                            ResStr(IDS_STREAM_NO_SEL_TITLE), MB_OK | MB_ICONINFORMATION);
                        break;
                    }
                    {
                        TCHAR szStreamName[MAX_PATH + 64];
                        ListView_GetItemText(hListView, iSel, 1,
                                            szStreamName, ARRAYSIZE(szStreamName));
                        if (szStreamName[0] == TEXT('\0')) {
                            MessageBox(hDlg,
                                ResStr(IDS_STREAM_NO_SEL_MSG),
                                ResStr(IDS_STREAM_NO_SEL_TITLE), MB_OK | MB_ICONINFORMATION);
                            break;
                        }
                        private_LoadFileToStream(hDlg, lpstrFilePath,
                                                 szStreamName, hFile);
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
			            AppendMenu(hStreamMenu, uFlags, IDM_VIEW_STREAM,   ResStr(IDS_STREAM_MENU_VIEW));
                        AppendMenu(hStreamMenu, uFlags, IDM_SAVE_STREAM,   ResStr(IDS_STREAM_MENU_SAVE));
                        AppendMenu(hStreamMenu, MF_BYPOSITION | MF_STRING, IDM_CREATE_STREAM, ResStr(IDS_STREAM_MENU_CREATE));
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
      case WM_STREAM_NAME_DONE: {
          /* Posted by StreamNameEditProc when the user presses Enter (wParam=1)
             or Escape/KillFocus (wParam=0). */
          if (!hStreamNameEdit)   /* already handled (Enter then KillFocus) */
              break;

          if (wParam == 1) {
              /* Confirm: read the name and try to create the stream */
              TCHAR szName[MAX_PATH];
              GetWindowText(hStreamNameEdit, szName, ARRAYSIZE(szName));

              private_DestroyStreamEdit(&hStreamNameEdit);

              if (szName[0] == TEXT('\0')) {
                  MessageBox(hDlg,
                      ResStr(IDS_STREAM_EMPTY_NAME_MSG),
                      ResStr(IDS_STREAM_NO_SEL_TITLE), MB_OK | MB_ICONWARNING);
                  /* Remove the blank row we inserted */
                  if (iStreamNameEditItem >= 0) {
                      ListView_DeleteItem(hListView, iStreamNameEditItem);
                      iStreamNameEditItem = -1;
                  }
                  break;
              }

              /* Build full ADS path: file + ":" + name + ":$DATA" */
              if (lpstrFilePath) {
                  TCHAR szAdsPath[MAX_PATH * 2];
                  HANDLE hStream;
                  StringCchCopy(szAdsPath, ARRAYSIZE(szAdsPath), lpstrFilePath);
                  StringCchCat (szAdsPath, ARRAYSIZE(szAdsPath), TEXT(":"));
                  StringCchCat (szAdsPath, ARRAYSIZE(szAdsPath), szName);
                  StringCchCat (szAdsPath, ARRAYSIZE(szAdsPath), TEXT(":$DATA"));

                  hStream = CreateFile(szAdsPath,
                                       GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                       NULL, CREATE_NEW,
                                       FILE_ATTRIBUTE_NORMAL, NULL);
                  if (hStream != INVALID_HANDLE_VALUE) {
                      CloseHandle(hStream);
                      /* Refresh the stream list */
                      SendMessage(hDlg, WM_SETFILE_HANDLE, 0, (LPARAM)hFile);
                  } else {
                      common_ShowError(hDlg, ResStr(IDS_STREAM_ERR_CREATE));
                      if (iStreamNameEditItem >= 0) {
                          ListView_DeleteItem(hListView, iStreamNameEditItem);
                          iStreamNameEditItem = -1;
                      }
                  }
              } else {
                  /* lpstrFilePath is NULL - no file selected */
                  MessageBox(hDlg,
                      ResStr(IDS_STREAM_NO_FILE_MSG),
                      ResStr(IDS_STREAM_NO_SEL_TITLE), MB_OK | MB_ICONWARNING);
                  if (iStreamNameEditItem >= 0) {
                      ListView_DeleteItem(hListView, iStreamNameEditItem);
                      iStreamNameEditItem = -1;
                  }
              }
          } else {
              /* Cancel */
              private_DestroyStreamEdit(&hStreamNameEdit);
              if (iStreamNameEditItem >= 0) {
                  ListView_DeleteItem(hListView, iStreamNameEditItem);
                  iStreamNameEditItem = -1;
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
	  case WM_SETFILE_NAME: {
		  if (lpstrFilePath) {
			  LocalFree(lpstrFilePath);
			  lpstrFilePath = NULL;
		  }
		  if (lParam) {
			  int cch = lstrlen((LPCTSTR)lParam) + 1;
			  lpstrFilePath = (LPTSTR)LocalAlloc(LPTR, cch * sizeof(TCHAR));
			  if (lpstrFilePath)
				  StringCchCopy(lpstrFilePath, cch, (LPCTSTR)lParam);
		  }
		  break;
	  }
	  case WM_RESETFILE_HANDLE: {
          hFile = NULL;
		  if (lpstrFilePath) {
			  LocalFree(lpstrFilePath);
			  lpstrFilePath = NULL;
		  }
		  break;
	  }
	  case WM_CLOSE:
	  case WM_DESTROY: {
		  LocalFree(pfsi);
		  if (lpstrFilePath) {
			  LocalFree(lpstrFilePath);
			  lpstrFilePath = NULL;
		  }
		  break;
	  }
  }
  return FALSE;
}