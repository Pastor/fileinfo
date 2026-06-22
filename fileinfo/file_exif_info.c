/*
 * file_exif_info.c — EXIF/IPTC/XMP metadata viewer via Exiv2.
 *
 * Compiled as C++ (see .vcxproj per-file override).
 * When EXIV2_AVAILABLE is not defined the tab shows a stub message so the
 * project still builds without the 3rd-party library.
 *
 * To enable full EXIF support:
 *   1. Clone/build Exiv2 (https://github.com/Exiv2/exiv2) for Win32/x64.
 *   2. Place headers in  $(SolutionDir)3rdParty\exiv2\include
 *      and import library in $(SolutionDir)3rdParty\exiv2\lib\exiv2.lib
 *   3. Add EXIV2_AVAILABLE to the project preprocessor definitions.
 */

#include <windows.h>
#include <commctrl.h>
#include <strsafe.h>
#include "common.h"
#include "file_exif_info.h"
#include "resource.h"

#ifdef EXIV2_AVAILABLE
#include <string>
#include <vector>
#include <exiv2/exiv2.hpp>

static std::vector<std::string> g_ExifKeys;
#endif

/* ------------------------------------------------------------------ */
/* ListView column layout                                               */
/* ------------------------------------------------------------------ */

static struct {
    UINT   nNameID;
    int    iWidth;
} g_ExifColumns[] = {
    { IDS_EXIF_COL_NUM,        30  },
    { IDS_EXIF_COL_TAG,      200 },
    { IDS_EXIF_COL_TYPE,      60  },
    { IDS_EXIF_COL_VALUE, 280 },
};

static void
private_InitListView(HWND hListView)
{
    int i;
    LVCOLUMN lvc;

    ZeroMemory(&lvc, sizeof(lvc));
    lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM | LVCF_FMT;
    lvc.fmt  = LVCFMT_LEFT;

    for (i = 0; i < sizeof(g_ExifColumns)/sizeof(g_ExifColumns[0]); ++i) {
        lvc.iSubItem = i;
        lvc.pszText  = (LPTSTR)ResStr(g_ExifColumns[i].nNameID);
        lvc.cx       = g_ExifColumns[i].iWidth;
        ListView_InsertColumn(hListView, i, &lvc);
    }

    ListView_SetExtendedListViewStyleEx(hListView, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);
    ListView_SetExtendedListViewStyleEx(hListView, LVS_EX_GRIDLINES,     LVS_EX_GRIDLINES);
}

/* Fill the entire client area with the ListView. */
static void
private_ResizeListView(HWND hDlg, HWND hListView)
{
    RECT rc;
    GetClientRect(hDlg, &rc);
    MoveWindow(hListView, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, TRUE);
}

/* Clear and reset the list. */
static void
private_ClearList(HWND hListView)
{
    ListView_DeleteAllItems(hListView);
#ifdef EXIV2_AVAILABLE
    g_ExifKeys.clear();
#endif
}

/* ------------------------------------------------------------------ */
/* Helper: set one row in the ListView (4 sub-items).                   */
/* ------------------------------------------------------------------ */

static void
private_AddRow(HWND hListView, int nIndex, LPCTSTR lpNum, LPCTSTR lpTag,
               LPCTSTR lpType, LPCTSTR lpValue)
{
    LVITEM lvi;
    ZeroMemory(&lvi, sizeof(lvi));
    lvi.mask       = LVIF_TEXT | LVIF_PARAM;
    lvi.iItem      = nIndex;
    lvi.iSubItem   = 0;
    lvi.pszText    = (LPTSTR)lpNum;
    lvi.lParam     = (LPARAM)nIndex;
    ListView_InsertItem(hListView, &lvi);
    ListView_SetItemText(hListView, nIndex, 1, (LPTSTR)lpTag);
    ListView_SetItemText(hListView, nIndex, 2, (LPTSTR)lpType);
    ListView_SetItemText(hListView, nIndex, 3, (LPTSTR)lpValue);
}

/* ------------------------------------------------------------------ */
/* EXIF loading                                                          */
/* ------------------------------------------------------------------ */

#ifdef EXIV2_AVAILABLE

/* Convert a Windows UTF-16 path to UTF-8 for Exiv2. */
static std::string
private_WideToUtf8(LPCWSTR wstr)
{
    if (!wstr || !*wstr)
        return std::string();
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0)
        return std::string();
    std::string result(static_cast<size_t>(len - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, &result[0], len, nullptr, nullptr);
    return result;
}

/* Convert UTF-8 string from Exiv2 to a wide TCHAR buffer. */
static void
private_Utf8ToWide(const std::string& src, LPTSTR dst, int cchDst)
{
    MultiByteToWideChar(CP_UTF8, 0, src.c_str(), -1, dst, cchDst);
    dst[cchDst - 1] = TEXT('\0');
}

/* Load all EXIF (+ IPTC + XMP) tags from lpstrFileName into hListView. */
static void
private_LoadExif(HWND hDlg, HWND hListView, LPCWSTR lpstrFileName)
{
    private_ClearList(hListView);

    std::string utf8Path = private_WideToUtf8(lpstrFileName);
    if (utf8Path.empty())
        return;

    try {
        Exiv2::Image::UniquePtr image = Exiv2::ImageFactory::open(utf8Path);
        if (!image)
            return;
        image->readMetadata();

        TCHAR szNum[16], szTag[256], szType[64], szValue[1024];
        int nIndex = 0;

        /* EXIF */
        Exiv2::ExifData& exifData = image->exifData();
        for (auto& md : exifData) {
            StringCchPrintf(szNum,   ARRAYSIZE(szNum),   TEXT("%d"), nIndex + 1);
            private_Utf8ToWide(md.tagLabel(),         szTag,   ARRAYSIZE(szTag));
            private_Utf8ToWide(md.typeName(),         szType,  ARRAYSIZE(szType));
            private_Utf8ToWide(md.value().toString(), szValue, ARRAYSIZE(szValue));
            g_ExifKeys.push_back(md.key());
            private_AddRow(hListView, nIndex, szNum, szTag, szType, szValue);
            ++nIndex;
        }

        /* IPTC */
        Exiv2::IptcData& iptcData = image->iptcData();
        for (auto& md : iptcData) {
            StringCchPrintf(szNum,   ARRAYSIZE(szNum),   TEXT("%d"), nIndex + 1);
            private_Utf8ToWide(md.tagLabel(),         szTag,   ARRAYSIZE(szTag));
            private_Utf8ToWide(md.typeName(),         szType,  ARRAYSIZE(szType));
            private_Utf8ToWide(md.value().toString(), szValue, ARRAYSIZE(szValue));
            g_ExifKeys.push_back(md.key());
            private_AddRow(hListView, nIndex, szNum, szTag, szType, szValue);
            ++nIndex;
        }

        /* XMP */
        Exiv2::XmpData& xmpData = image->xmpData();
        for (auto& md : xmpData) {
            StringCchPrintf(szNum,   ARRAYSIZE(szNum),   TEXT("%d"), nIndex + 1);
            private_Utf8ToWide(md.tagLabel(),         szTag,   ARRAYSIZE(szTag));
            private_Utf8ToWide(md.typeName(),         szType,  ARRAYSIZE(szType));
            private_Utf8ToWide(md.value().toString(), szValue, ARRAYSIZE(szValue));
            g_ExifKeys.push_back(md.key());
            private_AddRow(hListView, nIndex, szNum, szTag, szType, szValue);
            ++nIndex;
        }

        if (nIndex == 0) {
            /* File opened successfully but contains no metadata. */
            private_AddRow(hListView, 0, ResStr(IDS_EXIF_DASH), ResStr(IDS_EXIF_NO_META_MSG),
                           TEXT(""), TEXT(""));
        }
    } catch (const Exiv2::Error& e) {
        TCHAR szMsg[512];
        char  szWhat[256];
        StringCchCopyA(szWhat, ARRAYSIZE(szWhat), e.what());
        MultiByteToWideChar(CP_ACP, 0, szWhat, -1, szMsg, ARRAYSIZE(szMsg));
        private_AddRow(hListView, 0, TEXT("!"), ResStr(IDS_EXIF_ERR_READ),
                       TEXT(""), szMsg);
    }
}

#else /* EXIV2_AVAILABLE not defined */

static void
private_LoadExif(HWND hDlg, HWND hListView, LPCWSTR lpstrFileName)
{
    private_ClearList(hListView);
    (void)hDlg;
    (void)lpstrFileName;
    private_AddRow(hListView, 0,
                   ResStr(IDS_EXIF_DASH),
                   ResStr(IDS_EXIF_NO_SUPPORT),
                   TEXT(""),
                   ResStr(IDS_EXIF_NO_SUPPORT_HINT));
}

#endif /* EXIV2_AVAILABLE */

/* ------------------------------------------------------------------ */
/* Dialog procedure                                                      */
/* ------------------------------------------------------------------ */

INT_PTR CALLBACK
fxi_WindowHandler(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static HWND   hListView     = NULL;
    static LPTSTR lpstrFileName = NULL;

    switch (uMsg) {
    case WM_INITDIALOG: {
        RECT rc;
        GetClientRect(hDlg, &rc);

        hListView = CreateWindowEx(
            0, WC_LISTVIEW, TEXT(""),
            WS_CHILD | WS_VISIBLE | WS_BORDER |
            LVS_REPORT | LVS_SHOWSELALWAYS | LVS_NOSORTHEADER,
            rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
            hDlg, (HMENU)(UINT_PTR)IDC_EXIF_LIST,
            (HINSTANCE)lParam, NULL);

        if (hListView) {
            private_InitListView(hListView);
        }
        return TRUE;
    }

    case WM_SIZE: {
        if (hListView) {
            private_ResizeListView(hDlg, hListView);
        }
        break;
    }

    case WM_COMMAND: {
        if (LOWORD(wParam) == IDM_COPY_SELECTION && hListView) {
            INT nItems = ListView_GetItemCount(hListView);
            INT nCols  = 4, ci, cj;
            HGLOBAL hMem;
            DWORD cch = 0;
            for (ci = 0; ci < nItems; ++ci) {
                if (ListView_GetItemState(hListView, ci, LVIS_SELECTED) & LVIS_SELECTED) {
                    for (cj = 0; cj < nCols; ++cj) {
                        TCHAR tmp[1024]; tmp[0]=0;
                        ListView_GetItemText(hListView, ci, cj, tmp, ARRAYSIZE(tmp));
                        cch += (DWORD)lstrlen(tmp) + 2;
                    }
                    cch += 3;
                }
            }
            if (cch > 0) {
                cch += 4;
                hMem = GlobalAlloc(GMEM_MOVEABLE, cch * sizeof(TCHAR));
                if (hMem) {
                    LPTSTR p = (LPTSTR)GlobalLock(hMem);
                    if (p) {
                        p[0] = 0;
                        for (ci = 0; ci < nItems; ++ci) {
                            if (ListView_GetItemState(hListView, ci, LVIS_SELECTED) & LVIS_SELECTED) {
                                for (cj = 0; cj < nCols; ++cj) {
                                    TCHAR tmp[1024]; tmp[0]=0;
                                    ListView_GetItemText(hListView, ci, cj, tmp, ARRAYSIZE(tmp));
                                    StringCchCat(p, cch, tmp);
                                    if (cj < nCols-1) StringCchCat(p, cch, TEXT("\t"));
                                }
                                StringCchCat(p, cch, TEXT("\r\n"));
                            }
                        }
                        GlobalUnlock(hMem);
                    }
                    if (OpenClipboard(hDlg)) {
                        EmptyClipboard();
                        SetClipboardData(CF_UNICODETEXT, hMem);
                        CloseClipboard();
                    } else {
                        GlobalFree(hMem);
                    }
                }
            }
        }
        break;
    }

    case WM_KEYDOWN: {
        if (hListView && (GetAsyncKeyState(VK_CONTROL) & 0x8000)) {
            if (wParam == 0x41) {
                INT n = ListView_GetItemCount(hListView), ka;
                for (ka = 0; ka < n; ++ka)
                    ListView_SetItemState(hListView, ka, LVIS_SELECTED, LVIS_SELECTED);
            } else if (wParam == 0x43) {
                SendMessage(hDlg, WM_COMMAND, IDM_COPY_SELECTION, 0);
            }
        }
        break;
    }

    case WM_NOTIFY: {
        LPNMHDR hdr = (LPNMHDR)lParam;
        if (hdr->idFrom == IDC_EXIF_LIST) {
            if (hdr->code == NM_RCLICK) {
                HMENU hMenu;
                POINT pt;
                BOOL bSel = ListView_GetSelectedCount(hListView) != 0;
                UINT uF = MF_STRING | (bSel ? 0U : MF_GRAYED);
                GetCursorPos(&pt);
                hMenu = CreatePopupMenu();
                AppendMenu(hMenu, uF, IDM_COPY_SELECTION, ResStr(IDS_MENU_COPY));
                SetForegroundWindow(hListView);
                TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_BOTTOMALIGN, pt.x, pt.y, 0, hListView, NULL);
                DestroyMenu(hMenu);
            }
#ifdef EXIV2_AVAILABLE
            else if (hdr->code == NM_DBLCLK && lpstrFileName) {
                LVITEM lvi2;
                INT iSel = ListView_GetSelectionMark(hListView);
                if (iSel >= 0) {
                    ZeroMemory(&lvi2, sizeof(lvi2));
                    lvi2.mask  = LVIF_PARAM;
                    lvi2.iItem = iSel;
                    ListView_GetItem(hListView, &lvi2);
                    { INT keyIdx = (INT)lvi2.lParam;
                      if (keyIdx >= 0 && keyIdx < (INT)g_ExifKeys.size()) {
                        std::string key = g_ExifKeys[keyIdx];
                        TCHAR szCurVal[1024], szNewVal[1024];
                        RECT rcItem;
                        POINT ptItem = {0, 0};
                        HWND hOverEdit;
                        szCurVal[0] = 0;
                        ListView_GetItemText(hListView, iSel, 3, szCurVal, ARRAYSIZE(szCurVal));
                        ListView_GetSubItemRect(hListView, iSel, 3, LVIR_BOUNDS, &rcItem);
                        ptItem.x = rcItem.left; ptItem.y = rcItem.top;
                        ClientToScreen(hListView, &ptItem);
                        hOverEdit = CreateWindowEx(WS_EX_TOPMOST, WC_EDIT, szCurVal,
                            WS_POPUP | WS_BORDER | WS_VISIBLE | ES_AUTOHSCROLL,
                            ptItem.x, ptItem.y,
                            (rcItem.right > rcItem.left) ? rcItem.right - rcItem.left : 200,
                            (rcItem.bottom > rcItem.top) ? rcItem.bottom - rcItem.top + 2 : 22,
                            hDlg, NULL, (HINSTANCE)GetWindowLongPtr(hDlg, GWLP_HINSTANCE), NULL);
                        if (hOverEdit) {
                            MSG emsg;
                            BOOL bOK = FALSE;
                            szNewVal[0] = 0;
                            SendMessage(hOverEdit, WM_SETFONT, (WPARAM)SendMessage(hListView, WM_GETFONT, 0, 0), TRUE);
                            SendMessage(hOverEdit, EM_SETSEL, 0, -1);
                            SetFocus(hOverEdit);
                            while (IsWindow(hOverEdit)) {
                                if (!GetMessage(&emsg, NULL, 0, 0)) break;
                                if (emsg.hwnd == hOverEdit && emsg.message == WM_KEYDOWN) {
                                    if (emsg.wParam == VK_RETURN) { GetWindowText(hOverEdit, szNewVal, ARRAYSIZE(szNewVal)); bOK = TRUE; DestroyWindow(hOverEdit); break; }
                                    if (emsg.wParam == VK_ESCAPE) { DestroyWindow(hOverEdit); break; }
                                }
                                TranslateMessage(&emsg); DispatchMessage(&emsg);
                            }
                            if (bOK) {
                                try {
                                    std::string utf8Path = private_WideToUtf8(lpstrFileName);
                                    Exiv2::Image::UniquePtr img = Exiv2::ImageFactory::open(utf8Path);
                                    img->readMetadata();
                                    int cchV = WideCharToMultiByte(CP_UTF8, 0, szNewVal, -1, NULL, 0, NULL, NULL);
                                    std::string utf8Val(cchV > 0 ? cchV : 1, 0);
                                    WideCharToMultiByte(CP_UTF8, 0, szNewVal, -1, &utf8Val[0], cchV, NULL, NULL);
                                    utf8Val.resize(cchV > 0 ? cchV - 1 : 0);
                                    Exiv2::ExifData& ed = img->exifData();
                                    auto it = ed.findKey(Exiv2::ExifKey(key));
                                    if (it != ed.end()) {
                                        it->setValue(utf8Val);
                                        img->writeMetadata();
                                        private_LoadExif(hDlg, hListView, lpstrFileName);
                                        MessageBox(hDlg, ResStr(IDS_EXIF_SAVE_OK), ResStr(IDS_EXIF_SAVE_OK_TITLE), MB_OK | MB_ICONINFORMATION);
                                    } else {
                                        MessageBox(hDlg, ResStr(IDS_EXIF_READONLY), ResStr(IDS_EXIF_SAVE_OK_TITLE), MB_OK | MB_ICONWARNING);
                                    }
                                } catch (...) {
                                    MessageBox(hDlg, ResStr(IDS_EXIF_SAVE_ERR), ResStr(IDS_EXIF_SAVE_OK_TITLE), MB_OK | MB_ICONERROR);
                                }
                            }
                        }
                      }
                    }
                }
            }
#endif
        }
        break;
    }

    case WM_SETFILE_NAME: {
        LPCWSTR lpNew = (LPCWSTR)lParam;
        int     cch   = lpNew ? lstrlen(lpNew) + 1 : 0;

        if (lpstrFileName) {
            LocalFree(lpstrFileName);
            lpstrFileName = NULL;
        }
        if (cch > 0) {
            lpstrFileName = (LPTSTR)LocalAlloc(LPTR, (SIZE_T)cch * sizeof(TCHAR));
            if (lpstrFileName) {
                StringCchCopy(lpstrFileName, cch, lpNew);
            }
        }

        if (hListView) {
            private_LoadExif(hDlg, hListView, lpstrFileName);
        }
        break;
    }

    case WM_SETFILE_HANDLE:
        /* File handle not used directly — EXIF is read by path via Exiv2. */
        break;

    case WM_RESETFILE_HANDLE: {
        if (lpstrFileName) {
            LocalFree(lpstrFileName);
            lpstrFileName = NULL;
        }
        if (hListView) {
            private_ClearList(hListView);
        }
        break;
    }

    case WM_CLOSE:
    case WM_DESTROY: {
        if (lpstrFileName) {
            LocalFree(lpstrFileName);
            lpstrFileName = NULL;
        }
        break;
    }
    }

    return FALSE;
}
