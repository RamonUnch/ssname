/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * SSNAME: Set short name automatically under windows NT (8.3 DOS name)  *
 * Tested under Windows XP and Windows 7.                                *
 * Copyright 2021 Raymond GILLIBERT                                      *
 * I wrote this program to re-generate shortnames automatically after    *
 * copying some files under Linux using NTFS-3G.                         *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - *
 * Usage: ssname.exe "long file name.extension"                          *
 * If a valid short-name already exists it does nothing, otherwise it    *
 * will find an available shortname and set it.                          *
 * It relyes on the SetFileShortNameW(), Windows XP/2003+ and later      *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - *
 * Note that it will start at ~0 and go up to ~ZZ (Base 36)              *
 * This is bad code I wrote in one night with some good drink.....       *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - *
 * This file is under the DWTFPL.                                        *
 * DO WHAT THE FUCK YOU WANT WITH THIS SOFTWARE                          *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <windows.h>
#include <stdio.h>
/*
build command line I used (yes I kno I am a dummy trying to look cool):

gcc -m32 -march=i386 ssname.c -o ssname.exe -nostdlib -e_unfuckMain@0 -s -Wl,-s,-dynamicbase,-nxcompat,--no-seh,--relax,--disable-runtime-pseudo-reloc,--enable-auto-import,--disable-stdcall-fixup -fno-stack-check -fno-stack-protector -fno-ident -fomit-frame-pointer -mno-stack-arg-probe -m32 -march=i386 -mtune=i686 -mpreferred-stack-boundary=2 -momit-leaf-frame-pointer -fno-exceptions -fno-dwarf2-cfi-asm -fno-asynchronous-unwind-tables -lkernel32 -lmsvcrt -luser32 -ladvapi32 -D__USE_MINGW_ANSI_STDIO=0 -Wall

*/

///////////////////////////////////////////////////////////////////////////
// Copy pasted from msdn...
// We need this function to get the privilege to modify the short name
HRESULT ModifyPrivilege(IN LPCTSTR szPrivilege, IN BOOL fEnable)
{
    HRESULT hr = S_OK;
    TOKEN_PRIVILEGES NewState;
    LUID luid;
    HANDLE hToken = NULL;

    // Open the process token for this process.
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES|TOKEN_QUERY, &hToken )) {
        wprintf(L"Failed OpenProcessToken\n");
        return ERROR_FUNCTION_FAILED;
    }

    // Get the local unique ID for the privilege.
    if ( !LookupPrivilegeValue( NULL, szPrivilege, &luid )) {
        CloseHandle( hToken );
        wprintf(L"Failed LookupPrivilegeValue\n");
        return ERROR_FUNCTION_FAILED;
    }

    // Assign values to the TOKEN_PRIVILEGE structure.
    NewState.PrivilegeCount = 1;
    NewState.Privileges[0].Luid = luid;
    NewState.Privileges[0].Attributes = fEnable ? SE_PRIVILEGE_ENABLED : 0;

    // Adjust the token privilege.
    if (!AdjustTokenPrivileges(hToken, FALSE, &NewState, 0, NULL, NULL)) {
        wprintf(L"Failed AdjustTokenPrivileges\n");
        hr = ERROR_FUNCTION_FAILED;
    }

    // Close the handle.
    CloseHandle(hToken);

    return hr;
}
///////////////////////////////////////////////////////////////////////////
// Conversion function UTF-16 -> CP_ACP
// We transform all non ansi chars into '_'
#define FLAGS WC_COMPOSITECHECK|WC_DISCARDNS|WC_SEPCHARS|WC_DEFAULTCHAR
char *utf16_to_ACP(const wchar_t *input)
{
    char *acp=NULL;
    size_t BuffSize = 0, Result = 0;

    if (!input) return NULL;
    BOOL yep = TRUE;

    BuffSize = WideCharToMultiByte(CP_ACP, FLAGS, input, -1, NULL, 0, "_", &yep);
    acp = (char*) calloc(BuffSize+16, sizeof(char));
    if (acp) {
        Result = WideCharToMultiByte(CP_ACP, FLAGS, input, -1, acp, BuffSize, "_", &yep);

        if ((Result > 0) && (Result <= BuffSize)){
            acp[BuffSize-1]=(char) 0;
            return acp;
        }
     }

    return NULL;
}
///////////////////////////////////////////////////////////////////////////
// Removes the trailing file name from a path
static BOOL PathRemoveFileSpecL(wchar_t *p)
{
    int i=0;
    if (!p) return FALSE;

    while(p[++i] != '\0');
    while(i > 0 && p[i] != '\\' && p[i] != '/') i--;
    i++;
    i +=  ((p[i+1] == '\\') | (p[i+1] == '/'));
    p[i]='\0';

    return TRUE;
}

///////////////////////////////////////////////////////////////////////////
// Removes the path and keeps only the file name
static void PathStripPathL(wchar_t *p)
{
    int i=0, j;
    if (!p) return;

    while(p[++i] != '\0');
    while(i >= 0 && p[i] != '\\' && p[i] != '/') i--;
    i++;
    for(j=0; p[i+j] != '\0'; j++) p[j]=p[i+j];
    p[j]= '\0';
}
static int IsBadChar(wchar_t c)
{
    return c < 33 || c > 127 // Control and non ASCHII
       || (c > 57 && c < 64) // : ; < = > ?
       || (c > 90 && c < 94) // [ / ]
       || (c > 41 && c < 45); // * + ,
}
///////////////////////////////////////////////////////////////////////////
// Tell is there are any non-shortname compatible char in fn
static int IsCoolStr(wchar_t *fn)
{
    int i, npoints=0;
    for (i=0; fn[i]; i++) {
        npoints += fn[i] == '.';
        if (IsBadChar(fn[i]))
            return 0; // Not Cool
    }
    return npoints <= 1; // Cool
}
///////////////////////////////////////////////////////////////////////////
void StripUnCool(char* s)
{
    char *d = s;
    do {
        while (*d && IsBadChar(*d)) {
            ++d;
        }
    } while ((*s++ = *d++));
}
///////////////////////////////////////////////////////////////////////////
static void StripPoints(char *s)
{
    char *d = s;
    do {
        while (*d
           && (*d == '.' )) {
            ++d;
        }
    } while ((*s++ = *d++));
}
///////////////////////////////////////////////////////////////////////////
static int IsShortName(wchar_t *fn_)
{
    int ret=0;
    wchar_t *fn = calloc(wcslen(fn_)+16, sizeof(wchar_t));
    wcscpy(fn, fn_);
    PathStripPathL(fn); // must already be stripped...
    int len;
    if ((len = wcslen(fn)) < 13  && IsCoolStr(fn)) {
        wchar_t *point;
        if ((point = wcschr(fn, L'.'))) {
            if((fn + len-1)-point <= 3) {
                ret = 1;
            }
        } else {
            // No extension in file.
            // then max legth is 8!
            ret = (len < 9);
        }
    }
    free(fn);
    return ret;
}

///////////////////////////////////////////////////////////////////////////
static int FileExistsW(const wchar_t *fn, const wchar_t *path)
{
    if(!fn || !fn[0]) return -1;
    if(!path || !path[0]) return INVALID_FILE_ATTRIBUTES != GetFileAttributesW(fn);

    wchar_t *tmpstr = calloc(wcslen(fn)+wcslen(path)+16, sizeof(wchar_t));
    if (!tmpstr) return -1;
    wcscpy(tmpstr, path);
    wcscat(tmpstr, fn);
    //_putws(tmpstr);
    int ret = INVALID_FILE_ATTRIBUTES != GetFileAttributesW(tmpstr);
    free(tmpstr);
    return ret;
}


// Note that we could also use '!', '&', '#', '@' '$' ''' '^' '{' '}' '`'
static const char g_mapping[] =  {
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9' , 'A', 'B'
  , 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J' , 'K', 'L', 'M', 'N'
  , 'O', 'P', 'Q', 'R', 'S', 'T' , 'U', 'V', 'W', 'X', 'Y', 'Z'
};
// 0 - 35 => BASE = 36
#define BASE (sizeof(g_mapping)/sizeof(g_mapping[0]))

///////////////////////////////////////////////////////////////////////////
static void strtowide(wchar_t *shorty, char *sfn)
{
    int len = strlen(sfn)+1;
    int i;
    for (i=0; i < len; i++) {
        shorty[i] = sfn[i];
    }
}
///////////////////////////////////////////////////////////////////////////
// Create a new valid short filename for fn, and copy it in shorty.
// We first need to cleanup the path, amke it ASCII only,
// then separate the name from the extension, Then truncate the extension
// Finallly truncate the name appending a ~X sufix, and check that file
// does not already exists.
// This is super slow, stupid and bad coding but whatever...
static int NewShortname(wchar_t *shorty, wchar_t *fnW, wchar_t *path)
{
    PathStripPathL(fnW);
    _wcsupr(fnW);
    char *fn = utf16_to_ACP(fnW);
    StripUnCool(fn);

    int i=0;
    while(fn[++i] != '\0');
    while(i >= 0 && fn[i] != '.') i--;
    fn[i] = '\0'; // separate ext and fn.
    int iext = ++i; //File extension
    char sext[5] = {'.', 0, 0, 0, 0};
    if (iext) {
        strncpy(&sext[1], &fn[iext], 3);
        //puts(sext);
    }
    // if there is a file extension then we stop to the point
    // We always cap to 6 for the ~X after...
    char sfn[13];
    StripPoints(fn);
    ZeroMemory(sfn, sizeof(sfn));
    strncpy(sfn, fn, iext? min(6, iext-1): 6);
    free(fn);
    int slen = strlen(sfn); // max is 6.
    sfn[slen] = '~'; // TILDE
    // We only tru 0-9 and A-Z ie 36 tests max!

    for (i=0; i < BASE; i++) {
        sfn[slen+1] = g_mapping [i];
        sfn[slen+2] = '\0';
        if (iext) strcat(sfn, sext);
        strtowide(shorty, sfn);
        if (!FileExistsW(shorty, path)) {
            // We are done!
            return i+1;
        }
    }
    // Move the TILDE in case we have no room
    if (slen == 6) {
        slen--; // ~XX mode
        sfn[slen] = '~'; // TILDE
    }
    // two digits mode
    for (i=0; i < BASE*BASE; i++) {
        sfn[slen+1] = g_mapping[i/BASE];
        sfn[slen+2] = g_mapping[i%BASE];
        sfn[slen+3] = '\0';

        if (iext) strcat(sfn, sext);
        strtowide(shorty, sfn);
        if (!FileExistsW(shorty, path)) {
            // We are done!
            return i+37;
        }
    }
    // Max possible collisions is 36*36+36 = 1332;
    return 0;
}
///////////////////////////////////////////////////////////////////////////
// Main with UNICODE support!
int wmain(int argc, wchar_t **argv)
{
    int ret = 0;
    if (argc !=2) {
        wprintf(L"usage: ssname.exe filename\n");
        return 1;
    }
    wchar_t *fn = argv[1];

    if (!FileExistsW(fn, NULL)) {
        wprintf(L"File %s does not exist!\n", fn);
        return 1;
    }
    DWORD slen = GetShortPathNameW(fn, NULL, 0);
    wchar_t *sfn = calloc(slen+16, sizeof(wchar_t));
    if (slen) slen = GetShortPathNameW(fn, sfn, slen);
    if (slen && IsShortName(sfn)) {
        PathStripPathL(sfn);
        wprintf(L"Already short-named: %s\n", sfn);
    } else {
        // No existing short name!
        ModifyPrivilege(SE_RESTORE_NAME, TRUE);

        HANDLE hfile = CreateFileW(fn, GENERIC_ALL
                                 , FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE, NULL
                                 , OPEN_EXISTING
                                 , FILE_FLAG_BACKUP_SEMANTICS, NULL);
        if(!hfile) {
            wprintf(L"error: file %s cannot be opened, error=%lu\n", fn, GetLastError());
            return 1;
        }
        // Separate path from file...
        wchar_t *path = calloc(wcslen(fn)+16, sizeof(wchar_t));
        wcscpy(path, fn);
        PathRemoveFileSpecL(path);
        PathStripPathL(fn);

        // We need to create a new shortname names shorty!
        // This one should not include the path...
        wchar_t shorty[14];
        ZeroMemory(shorty, sizeof(shorty));
        if (!NewShortname(shorty, fn, path)) {
            wprintf(L"Unable to find a new valid shortname for %s\n", fn);
            ret = 1;
        }
        if (!ret && SetFileShortNameW(hfile, shorty)) {
            wprintf(L"SHORT-NAME SET TO: %s\n", shorty);
        } else {
            wprintf(L"Unable to set short-name: %s, error=%lu\n", shorty, GetLastError());
        }
        CloseHandle(hfile);
        ModifyPrivilege(SE_RESTORE_NAME, FALSE);
        free(path);
    }
    return ret;
}

///////////////////////////////////////////////////////////////////////////
int __wgetmainargs(long*,wchar_t ***,wchar_t ***, long, long*);
int WINAPI unfuckMain(void)
{
    long argc, l;
    wchar_t **argv, **Env_;
    __wgetmainargs(&argc, &argv, &Env_,1 , &l);
    ExitProcess(wmain(argc, argv));
}
