#include <aclapi.h>
#include <strsafe.h>
#include "common.h"

static PSECURITY_DESCRIPTOR pSecurityDescriptor = NULL;
static PSID pSid = NULL;
static PACL pAcl = NULL;

LPCTSTR ResStr(UINT nID)
{
    enum { SLOTS = 8, MAXLEN = 1024 };
    static TCHAR s_bufs[SLOTS][MAXLEN];
    static int   s_slot = 0;
    TCHAR *buf = s_bufs[s_slot++ & (SLOTS - 1)];
    if (!LoadString(GetModuleHandle(NULL), nID, buf, MAXLEN))
        buf[0] = 0;
    return buf;
}

VOID CALLBACK 
common_FreeSecurityAttributes(LPSECURITY_ATTRIBUTES lpSecurity) {
	if ( pSid )
		FreeSid(pSid);
	if ( pAcl )
		LocalFree(pAcl);
	if ( pSecurityDescriptor )
		LocalFree(pSecurityDescriptor);
	pSid = NULL;
	pAcl = NULL;
	pSecurityDescriptor = NULL;
}

/** Not thread safe */
BOOL CALLBACK
common_CreateSecurityAttributes(LPSECURITY_ATTRIBUTES lpSecurity) {
	EXPLICIT_ACCESS ea;
	SID_IDENTIFIER_AUTHORITY sid = SECURITY_NT_AUTHORITY;	
	DWORD dwResult;	

	common_FreeSecurityAttributes(lpSecurity);
	RtlZeroMemory(&ea, sizeof(ea));
	RtlZeroMemory(lpSecurity, sizeof(SECURITY_ATTRIBUTES));
	if ( !AllocateAndInitializeSid(&sid, 1, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
		0, 0, 0, 0, 0, 0, &pSid) )
		return FALSE;
	ea.grfAccessPermissions = KEY_ALL_ACCESS;
	ea.grfAccessMode = SET_ACCESS;
	ea.grfInheritance = NO_INHERITANCE;
	ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
	ea.Trustee.TrusteeType = TRUSTEE_IS_GROUP;
	ea.Trustee.ptstrName = (LPTSTR)pSid;
	dwResult = SetEntriesInAcl(1, &ea, NULL, &pAcl);
	if ( dwResult != ERROR_SUCCESS )
		return FALSE;
	pSecurityDescriptor = (PSECURITY_DESCRIPTOR)LocalAlloc(LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH);
	if ( pSecurityDescriptor == NULL )
		return FALSE;
	if ( !InitializeSecurityDescriptor(pSecurityDescriptor, SECURITY_DESCRIPTOR_REVISION) )
		return FALSE;
	if ( !SetSecurityDescriptorDacl(pSecurityDescriptor, TRUE, pAcl, FALSE) )
		return FALSE;
	lpSecurity->nLength = sizeof(SECURITY_ATTRIBUTES);
	lpSecurity->lpSecurityDescriptor = pSecurityDescriptor;
	lpSecurity->bInheritHandle = FALSE;
    return TRUE;
}

static LPTSTR CALLBACK 
private_GetLastError(LPCTSTR pMessage) {
  LPVOID lpMsgBuf;
  LPVOID lpDisplayBuf;
  DWORD dwError;

  dwError = GetLastError();
  FormatMessage( 
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		dwError,
		GetThreadUILanguage(), // Default language
		(LPTSTR) &lpMsgBuf,
		0,
		NULL 
  );
  lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT, (
	  lstrlen((LPCTSTR)lpMsgBuf)+ 
	  lstrlen((LPCTSTR)pMessage)+ 40 )
	  * sizeof(TCHAR) );
  StringCchPrintf((LPTSTR)lpDisplayBuf, 
        LocalSize(lpDisplayBuf) / sizeof(TCHAR),
        ResStr(IDS_ERROR_FORMAT), 
        pMessage, dwError, lpMsgBuf); 
  LocalFree(lpMsgBuf);
  return (LPTSTR)lpDisplayBuf;
}

VOID CALLBACK
common_ShowError(HWND hParent, LPCTSTR lpstrMessage) {
	LPTSTR lpstrErrorMsg;

	lpstrErrorMsg = private_GetLastError(lpstrMessage);
	MessageBox(hParent, lpstrErrorMsg, ResStr(IDS_ERROR_TITLE), MB_OK | MB_ICONERROR);
	LocalFree(lpstrErrorMsg);
}