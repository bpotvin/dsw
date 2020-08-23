/*++
 * x86 or x64 ...
 *   cl -W3 -O2 dsw.c [-link advapi32.lib]
 *   cl -W3 -Zi -D_DEBUG dsw.c [-link -debug advapi32.lib]
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>

/*++
 * security related functions are in the advapi32 library. use the pragma to
 * pull it in automatically, or remove this and add the library to the compile
 * command line ...
 */
#pragma comment(lib, "advapi32.lib")

/*++ do a private arraysize macro and avoid including the stdlib.h header ... */
#ifndef _countof
 #define _countof(__arr)        (sizeof((__arr)) / sizeof((__arr)[0]))
#endif  /* _countof */

/*++
 */
void*
_DswMalloc (
    size_t size
    );

/*++
 */
int
_DswFree (
    void* block
    );

/*++
 */
wchar_t*
_DswGetCwd (
    void 
    );

/*++
 */
int
_DswDeleteFile (
    wchar_t* filename,
    PFILE_ID_BOTH_DIR_INFO pinfo 
    );

/*++
 */
wchar_t*
_DswGetPrefixedName (
    wchar_t* cwd,
    PFILE_ID_BOTH_DIR_INFO pinfo 
    );

/*++
 */
BOOL 
_DswAccessFile (
    wchar_t* filename
    );

/*++
 */
BOOL 
_DswSetNullDacl (
    wchar_t* filename
    );

/*++
 */
BOOL 
_DswAssertPrivilege (
    wchar_t* privilege, 
    TOKEN_PRIVILEGES* pprevious_state
    );

/*++
 */
BOOL 
_DswRevertPrivilege (
    TOKEN_PRIVILEGES* pprevious_state
    );

/*++
 */
BOOL
_DswGetTokenHandle (
    HANDLE* posth 
    );

/*++
 */
BOOL 
_DswSetOwner (
    wchar_t* filename, 
    PSID psid_owner
    );

/*++
 */
int
wmain (
    int argc, 
    wchar_t** argv )
{
    int c;
    int cc;
    void* buf = NULL;
    DWORD bufsize = 1024;
    HANDLE osh = NULL;
    wchar_t* cwd = NULL;
    PFILE_ID_BOTH_DIR_INFO pinfo = NULL;
    
    if(argc == 2)
    {
        if( SetCurrentDirectoryW(argv[1]) == FALSE)  
        {
            fwprintf(stderr, L"dsw: set cwd failed, status(%X)\n", GetLastError());
            return 1;
        }
    }

    if((cwd = _DswGetCwd()) == NULL)
    {
        /*++ error ... */
        fwprintf(stderr, L"dsw: get cwd failed, status(%X)\n", GetLastError());
        return 2;
    }

    /*++ open the directory ... */
    osh = CreateFileW(
     cwd, 
     (GENERIC_READ | SYNCHRONIZE | FILE_READ_ATTRIBUTES),
     (FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE),
     NULL, 
     OPEN_EXISTING, 
     (FILE_FLAG_NO_BUFFERING | FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_POSIX_SEMANTICS),
     NULL
     );

    if(osh == NULL)
    {
        fwprintf(stderr, L"dsw: open cwd failed, status(%X)\n", GetLastError());
        _DswFree(cwd);
        return 3;
    }

    if((buf = _DswMalloc(bufsize)) == NULL)
    {
        fwprintf(stderr, L"dsw: allocate storage(%d) failed, status(%X)\n", bufsize, GetLastError());
        CloseHandle(osh);
        _DswFree(cwd);
        return 4;
    }

    /*++ read the directory, prompt to delete each file ... */
    while( GetFileInformationByHandleEx(osh, FileIdBothDirectoryInfo, (void*)buf, bufsize))
    {
        pinfo = (PFILE_ID_BOTH_DIR_INFO)buf;
        while(pinfo)
        {
            if((pinfo->FileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
            {
                if(pinfo->FileName)
                {
retry:
                    fwprintf(stdout, L"%.*s ? ", (int)(pinfo->FileNameLength / sizeof(wchar_t)), pinfo->FileName);
                    cc = c = getchar();
                    while(cc != L'\n') cc = getchar();
                    switch(c)
                    {
                    case L'y':
                    case L'Y':
                        if( _DswDeleteFile(cwd, pinfo) == -1)
                        {
                            /*++ delete failed ... */
                        }
                        break;

                    case L'x':
                    case L'X':
                        goto bounce;

                    case L'\n':
                        break;

                    default:
                        goto retry;
                    }
                }
            }

            if(pinfo->NextEntryOffset == 0)
            {
                break;
            }
            pinfo = (PFILE_ID_BOTH_DIR_INFO)((char*)pinfo + pinfo->NextEntryOffset);
        }
    }

bounce:
    _DswFree(buf);
    CloseHandle(osh);
    _DswFree(cwd);

    return 0;
}

/*++ >>>>>>>>>>>>>>>>>>>>>>>           |           <<<<<<<<<<<<<<<<<<<<<<< --*/
/*++ >>>>>>>>>>>>>>>>>>>>>>>        support        <<<<<<<<<<<<<<<<<<<<<<< --*/

/*++
 */
void*
_DswMalloc (
    size_t size )
{
    return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size);
}

/*++
 */
int
_DswFree (
    void* block )
{
    if(block == NULL)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return -1;
    }
    return ((HeapFree(GetProcessHeap(), 0, block) == 0) ? -1 : 0);
}

/*++
 */
wchar_t*
_DswGetCwd (
    void )
{
    wchar_t* cwd = NULL;
    DWORD cchwd = GetCurrentDirectoryW(0, cwd);
    if(cchwd == 0)
    {
        /*++ last error set by function ... */
        return NULL;
    }

    cchwd++;   /*++ returned length does not include null-term ... */
    if((cwd = _DswMalloc((cchwd * sizeof(wchar_t)))) == NULL)
    {
        /*++ last error set by function ... */
        return NULL;
    }

    if( GetCurrentDirectoryW(cchwd, cwd) == 0)
    {
        /*++ last error set by function ... */
        return NULL;
    }
    return cwd;
}

/*++
 */
int
_DswDeleteFile (
    wchar_t* cwd,
    PFILE_ID_BOTH_DIR_INFO pinfo )
{
    wchar_t* name = NULL;

    /*++
     * the assumption is that 'del' or some other utility failed to delete a
     * file. it could be that,
     * 
     *   - the file is marked read-only.
     *   - the file's name contains invalid (w32) characters.
     *   - the file's security does not allow access to the file.
     *   - the file is held open by one or more threads.
     * 
     * a filename containing bits considered by the w32 subsystem to be invalid
     * can be side-stepped by using the \\?\ prefix on the file name. the prefix
     * turns off parsing and just hands the path off to the file system. given a
     * file "C:\foo\bar\aux.c", the prefixed name is "\\?\C:\foo\bar\aux.c" and
     * this name is used throughout ...
     */
    if((name = _DswGetPrefixedName(cwd, pinfo)) == NULL)
    {
        /*++ last error set by function ... */
        return -1;
    }

    __try
    {
        /*++
         * check the read-only attribute and attempt ot clear it if it's set. a 
         * previous call to reset the attribute may have failed due to the name
         * provided and might be successful using a prefixed name ...
         */
        if(pinfo->FileAttributes & FILE_ATTRIBUTE_READONLY)
        {
            if( SetFileAttributesW(name, pinfo->FileAttributes & ~FILE_ATTRIBUTE_READONLY) == FALSE)
            {
                /*++ warn ... */
            }
        }

        /*++
         * similarly, a call to deletefile may have previously failed due to the
         * provided name, but might work using a prefixed name ...
         */
        if( DeleteFileW(name) == TRUE)
        {
            /*++ delete worked, so bounce ... */
            return 0;
        }

        /*++
         * if the delete failed for access, try to get access. succeed or fail,
         * this is the last effort ...
         */
        if(GetLastError() == ERROR_ACCESS_DENIED)
        {
            /*++ try to update file security to gain access ... */
            if( _DswAccessFile(name) == TRUE)
            {
                /*++ security was cleared and the file was deleted ... */
                return 0;
            }
        }
    }
    __finally
    {
        _DswFree(name);
    }
    return -1;
}

/*++
 */
wchar_t*
_DswGetPrefixedName (
    wchar_t* cwd,
    PFILE_ID_BOTH_DIR_INFO pinfo )
{
    int err;
    size_t cchname;
    wchar_t* name = NULL;
    wchar_t* prefix = L"\\\\?\\";

    /*++ add two, one for dir-sep in front of name and one for the null-term ... */
    cchname = (wcslen(prefix) + wcslen(cwd) + (pinfo->FileNameLength / sizeof(wchar_t)) + 2);
    if((name = _DswMalloc((cchname * sizeof(wchar_t)))) == NULL)
    {
        /*++ last error set by function ... */
        return NULL;
    }

    err = _snwprintf_s(
     name,
     cchname,
     _TRUNCATE,
     L"%s%s\\%.*s",
     prefix,
     cwd,
     (int)(pinfo->FileNameLength / sizeof(wchar_t)),
     pinfo->FileName
     );

    if(err <= 0)
    {
        /*++ 
         * two specific errors are noted, ERANGE is the buffer is too small and 
         * EINVAL if the buffer or format is null or if length is .LE. zero. in
         * this case, just set EINVAL and be done with it ...
         */
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }
    return name;
}

/*++ >>>>>>>>>>>>>>>>>>>>>>>           |           <<<<<<<<<<<<<<<<<<<<<<< --*/
/*++ >>>>>>>>>>>>>>>>>>>>>>>     security fcns     <<<<<<<<<<<<<<<<<<<<<<< --*/

SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;

/*++
 */
BOOL
_DswAccessFile (
    wchar_t* filename )
{
    BOOL result;
    PSID sid_admin = NULL;

    /*++
     * try to set a null dacl on the file. if it works, retry the delete. if it
     * fails, fall through to owner assignment ...
     */
    if( _DswSetNullDacl(filename) == TRUE)
    {
        if( DeleteFileW(filename) == TRUE)
        {
            return TRUE;
        }
        /*++ DELETE FAILED ... */
    }
    else
    {
        /*++ SET NULL DACL FAILED, FALL THROUGH TO TAKE_OWN ... */
    }

    /*++
     * make an admin sid and try to assign it as the file's owner. this has to
     * be free'd when we're done ...
     */
    result = AllocateAndInitializeSid(
     &NtAuthority,
     2,
     SECURITY_BUILTIN_DOMAIN_RID,
     DOMAIN_ALIAS_RID_ADMINS,
     0, 0, 0, 0, 0, 0,
     psid_admin
     );

    if(result == FALSE)
    {
        /*++ if an admin sid can't be made, bounce ... */
        return FALSE;
    }

    __try
    {
        /*++
         * enable the takeownership privilege and save the previous state ...
         */
        TOKEN_PRIVILEGES PreviousState = {0};
        if( _DswAssertPrivilege(L"SeTakeOwnershipPrivilege", &PreviousState) == FALSE)
        {
            /*++ if the priv can't be enabled, we're done ... */
            return FALSE;
        }

        __try
        {
            if( _DswSetOwner(filename, sid_admin) == FALSE)
            {
                /*++ if the owner can't be set, we're done ... */
                return FALSE;
            }

            /*++ ADMIN OWNER SET, REDRIVING DELETE ... */

            if( DeleteFileW(filename) == FALSE)
            {
                /*++ DELETE FAILED, WE'RE DONE ... */
                return FALSE;
            }
            /*++ DELETE SUCCEEDED ... */
        }
        __finally
        {
            if( _DswRevertPrivilege(&PreviousState) == FALSE)
            {
                /*++ no error ret here ... */
            }
        }
    }
    __finally
    {
        FreeSid(sid_admin);
    }
    return TRUE;
}

/*++
 */
BOOL
_DswSetNullDacl (
    wchar_t* filename )
{
    SECURITY_DESCRIPTOR security_descriptor = {0};
    InitializeSecurityDescriptor(&security_descriptor, SECURITY_DESCRIPTOR_REVISION);

    if( SetSecurityDescriptorDacl(&security_descriptor, TRUE, NULL, FALSE) == FALSE)
    {
        /*++ last error set by function ... */
        return FALSE;
    }

    if( SetFileSecurityW(filename, DACL_SECURITY_INFORMATION, &security_descriptor) == FALSE)
    {
        /*++ last error set by function ... */
        return FALSE;
    }
    return TRUE;
}

/*++
 */
BOOL
_DswAssertPrivilege (
    wchar_t* privilege,
    TOKEN_PRIVILEGES* pprevious_state )
{
    BOOL status;
    LUID privilege_luid;
    DWORD previous_size = 0;

    if( LookupPrivilegeValueW(NULL, privilege, &privilege_luid) == FALSE)
    {
        /*++ last error set by function ... */
        return FALSE;
    }

    TOKEN_PRIVILEGES TokenPrivileges = {0};
    TokenPrivileges.PrivilegeCount = 1;
    TokenPrivileges.Privileges[0].Luid = privilege_luid;
    TokenPrivileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    HANDLE osth = NULL;
    if( _DswGetTokenHandle(&osth) == FALSE)
    {
        /*++ last error set by function ... */
        return FALSE;
    }

    status = AdjustTokenPrivileges(
     osth, 
     FALSE, 
     &TokenPrivileges, 
     sizeof(TOKEN_PRIVILEGES), 
     pprevious_state, 
     &previous_size
     );

    if(status == FALSE)
    {
        /*++ last error set by function ... */
        return FALSE;
    }
    return TRUE;
}

/*++
 */
BOOL
_DswRevertPrivilege (
    TOKEN_PRIVILEGES* pprevious_state )
{
    BOOL status;
    HANDLE osth = NULL;
    if( _DswGetTokenHandle(&osth) == FALSE)
    {
        /*++ last error set by function ... */
        return FALSE;
    }

    status = AdjustTokenPrivileges(
     osth, 
     FALSE, 
     pprevious_state, 
     sizeof(TOKEN_PRIVILEGES),
     NULL,
     NULL
     );

    if(status == FALSE)
    {
        /*++ last error set by function ... */
        return FALSE;
    }
    return TRUE;
}

/*++
 */
BOOL
_DswGetTokenHandle (
    HANDLE* posth )
{
    HANDLE osph = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, GetCurrentProcessId());
    if(osph == NULL)
    {
        /*++ last error set by function ... */
        return FALSE;
    }

    if( OpenProcessToken(osph, (TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY), posth) == FALSE)
    {
        /*++ last error set by function ... */
        return FALSE;
    }
    return TRUE;
}

/*++
 */
BOOL
_DswSetOwner (
    wchar_t* filename,
    PSID psid_owner )
{
    SECURITY_DESCRIPTOR security_descriptor = {0};
    InitializeSecurityDescriptor(&security_descriptor, SECURITY_DESCRIPTOR_REVISION);

    if( SetSecurityDescriptorDacl(&security_descriptor, TRUE, NULL, FALSE) == FALSE)
    {
        /*++ last error set by function ... */
        return FALSE;
    }

    if( SetSecurityDescriptorOwner(&security_descriptor, psid_owner, FALSE) == FALSE)
    {
        /*++ last error set by function ... */
        return FALSE;
    }

    if( SetFileSecurityW(filename, OWNER_SECURITY_INFORMATION, &security_descriptor) == FALSE)
    {
        /*++ last error set by function ... */
        return FALSE;
    }
    return TRUE;
}
