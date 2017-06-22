#ifndef _COMMON_H_
#define _COMMON_H_
#include <windows.h>
#include <strsafe.h>


#define WM_SETFILE_HANDLE    WM_USER + 1020
#define WM_RESETFILE_HANDLE  WM_USER + 1021
#define WM_SETFILE_NAME      WM_USER + 1022

BOOL CALLBACK common_CreateSecurityAttributes(LPSECURITY_ATTRIBUTES lpSecurity);
VOID CALLBACK common_FreeSecurityAttributes(LPSECURITY_ATTRIBUTES lpSecurity);
VOID CALLBACK common_ShowError(HWND hParent, LPTSTR lpstrMessage);

#ifndef nullptr
#define nullptr NULL
#endif

#endif /** _COMMON_H_ */
