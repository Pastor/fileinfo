#ifndef _FILE_BASIC_INFO_H_
#define _FILE_BASIC_INFO_H_
#include <windows.h>
#include <commctrl.h>

VOID CALLBACK fbi_EnableControls(HWND hDlg, BOOL bStatus);
DWORD CALLBACK fbi_GetFileAttributes(HWND hDlg);
VOID CALLBACK fbi_SetFileAttributes(HWND hDlg, DWORD dwFileAttributes);

INT_PTR CALLBACK fbi_WindowHandler(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

#endif /** _FILE_BASIC_INFO_H_ */
