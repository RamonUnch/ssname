/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * SSNAME: Set short name automatically under windows NT (8.3 DOS name)  *
 * VERSION 1.2 Tested under Windows XP and Windows 7.                    *
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
 * Since 1.1 you no longer need to prefix \\?\ to a path in order to     *
 * use long-path names.                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - *
 * This file is under the DWTFPL.                                        *
 * DO WHAT THE FUCK YOU WANT WITH THIS SOFTWARE                          *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define UNICODE
#define _UNICODE

#include <windows.h>
#include <stdio.h>
//#include "nanolibc.h"
/*
build command line I used (yes I know I am a dummy trying to look cool):

gcc -m32 ssname.c -o ssname.exe -march=i386 -mtune=i686 -nostdlib -e_unfuckMain@0 -s -Wl,-s,-dynamicbase,-nxcompat,--no-seh,--relax,--disable-runtime-pseudo-reloc,--enable-auto-import,--disable-stdcall-fixup -fno-stack-check -fno-stack-protector -fno-ident -fomit-frame-pointer -mno-stack-arg-probe -mtune=i686 -mpreferred-stack-boundary=2 -momit-leaf-frame-pointer -fno-exceptions -fno-dwarf2-cfi-asm -fno-asynchronous-unwind-tables -lkernel32 -lmsvcrt -luser32 -ladvapi32 -D__USE_MINGW_ANSI_STDIO=0 -Wall

*/

///////////////////////////////////////////////////////////////////////////
// Beter print stuff to display unicode in the prompt...
HANDLE hCONSOLE;
static BOOL printW(const wchar_t *s)
{
    return WriteConsoleW(hCONSOLE, s, wcslen(s), NULL, NULL);
}
static void LastErrorEndl(DWORD err)
{
    wchar_t txt[16];
    wchar_t str[32];
    wcscpy(str, L", Error=");
    wcscat(str, _ultow(err, txt, 10));
    wcscat(str, L"\n");
    printW(str);
}
static void printWErr(const wchar_t *buf1, const wchar_t *buf2)
{
    DWORD err = GetLastError();
    printW(buf1);
    if(buf2) printW(buf2);
    LastErrorEndl(err);
}
///////////////////////////////////////////////////////////////////////////
// Copy pasted from msdn...
// We need this function to get the privilege to modify the short name
static HRESULT ModifyPrivilege(IN LPCTSTR szPrivilege, IN BOOL fEnable)
{
    HRESULT hr = S_OK;
    TOKEN_PRIVILEGES NewState;
    LUID luid;
    HANDLE hToken = NULL;

    // Open the process token for this process.
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES|TOKEN_QUERY, &hToken )) {
        printWErr(L"Failed OpenProcessToken", NULL);
        return ERROR_FUNCTION_FAILED;
    }

    // Get the local unique ID for the privilege.
    if ( !LookupPrivilegeValue( NULL, szPrivilege, &luid )) {
        printWErr(L"Failed LookupPrivilegeValue", NULL);
        CloseHandle( hToken );
        return ERROR_FUNCTION_FAILED;
    }

    // Assign values to the TOKEN_PRIVILEGE structure.
    NewState.PrivilegeCount = 1;
    NewState.Privileges[0].Luid = luid;
    NewState.Privileges[0].Attributes = fEnable ? SE_PRIVILEGE_ENABLED : 0;

    // Adjust the token privilege.
    if (!AdjustTokenPrivileges(hToken, FALSE, &NewState, 0, NULL, NULL)) {
        printWErr(L"Failed AdjustTokenPrivileges", NULL);
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
static char *utf16_to_ACP(const wchar_t *input)
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
static wchar_t *GetFNinPath(wchar_t *p)
{
    int i=0;

    while(p[++i] != '\0');
    while(i >= 0 && p[i] != '\\' && p[i] != '/') i--;
    i++;
    i += (p[i] == '\\' || p[i] == '/');
    return &p[i]; // first char of the filename
}
///////////////////////////////////////////////////////////////////////////
// Removes the trailing file name from a path
// and returns the pointer to filename only
static wchar_t *SeparatePathAndFN(wchar_t *p)
{
    wchar_t *fn = GetFNinPath(p);

    if(fn > p) fn[-1] = '\0'; // Only if we are within the p range...

    return fn;
}
///////////////////////////////////////////////////////////////////////////
// Removes the path and keeps only the file name
static BOOL IsBadChar(const wchar_t c)
{
    return c < 33 || c > 127 // Control and non ASCHII
       || (c > 57 && c < 64) // : ; < = > ?
       || (c > 90 && c < 94) // [ / ]
       || (c > 41 && c < 45); // * + ,
}
///////////////////////////////////////////////////////////////////////////
// Tell is there are any non-shortname compatible char in fn
static BOOL IsCoolStr(const wchar_t *fn)
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
static void StripUnCool(char* s)
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
// Determines if a filename really has a vaid short-name.
static wchar_t *IsShortName(wchar_t *fn_)
{
    wchar_t *fn = GetFNinPath(fn_);
    int len;
    wchar_t *point;
    if (((len = wcslen(fn)) < 13  )
    && (IsCoolStr(fn))
    // The POINT has to be maximum 3 char from the end if there is one.
    && (((point = wcschr(fn, L'.')) && (fn + len-1)-point <= 3)
      ||(len <= 8)) ) { // No extension in file, then max legth is 8!
        return fn;
    }
    return NULL;
}

#define FileExistsW(fn) (INVALID_FILE_ATTRIBUTES != GetFileAttributesW(fn))
///////////////////////////////////////////////////////////////////////////
// Version that takes separately the path and the filename
// note that path has to be long enough so that path+fn+1 can fit inside...
static int FileExistsInPath(const wchar_t *fn, wchar_t *path)
{
    size_t pathlen = wcslen(path);
    path[pathlen] ='\\'; // Add the backslash.
    path[pathlen+1] ='\0'; // and Null terminate
    wcscat(path, fn);
    int ret = INVALID_FILE_ATTRIBUTES != GetFileAttributesW(path);
    path[pathlen] = '\0'; // put back the NULL terminator.
    return ret;
}

// Note that we could also use  '~' '!', '&', '#', '@' '$' ''' '^' '{' '}' '`'
static const char g_mapping[] =  {
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9' , 'A', 'B'
  , 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J' , 'K', 'L', 'M', 'N'
  , 'O', 'P', 'Q', 'R', 'S', 'T' , 'U', 'V', 'W', 'X', 'Y', 'Z'
};
// 0 - 35 => BASE = 36
#define BASE (sizeof(g_mapping)/sizeof(g_mapping[0]))

///////////////////////////////////////////////////////////////////////////
static void strntowide(wchar_t *shorty, const char *sfn, size_t len)
{
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
static int NewShortname(wchar_t *sfn, wchar_t *fnW, wchar_t *path)
{
    char *fn = utf16_to_ACP(fnW);
    StripUnCool(fn);
    _strupr(fn); // Uppercase

    int i=0;
    while(fn[++i] != '\0');
    while(i >= 0 && fn[i] != '.') i--;
    fn[i] = '\0'; // separate ext and fn.
    int iext = ++i; //File extension
    wchar_t sext[5] = {'.', 0, 0, 0, 0};
    if (iext) {
        if (fn[iext] == '\0')  {
            iext = 0; // if extension is empty
        } else {
            strntowide(&sext[1], &fn[iext], 3);
        }
        //printWErr(L"File extension ", sext);
    }
    // if there is a file extension then we stop to the point
    // We always cap to 6 for the ~X after...
    StripPoints(fn);

    strntowide(sfn, fn, iext? min(6, iext-1): 6);
    free(fn);
    int slen = wcslen(sfn); // max is 6.

    sfn[slen] = '~'; // TILDE
    // We only tru 0-9 and A-Z ie 36 tests max!
    for (i=0; i < BASE; i++) {
        sfn[slen+1] = g_mapping [i];
        sfn[slen+2] = '\0';
        if (iext) wcscat(sfn, sext);
        if (!FileExistsInPath(sfn, path)) {
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

        if (iext) wcscat(sfn, sext);
        if (!FileExistsInPath(sfn, path)) {
            // We are done!
            return i+37;
        }
    }
    // Max possible collisions is 36*36+36 = 1332;
    return 0;
}
static wchar_t *GetUNCPath(const wchar_t *ifn)
{
    // Actually for GetFullifn it is not needed to
    // add the funny \\?\ prefix to get the full path name.
    // However we add it in to the buffer for future uses...
    // also we should not add it if it is alrady an UNC
    // and we should prefix with \\?\UNC\ in case of Network path
    ULONG len = GetFullPathNameW(ifn, 0, 0, 0);
    if (len) {
        wchar_t *buf = calloc(len + 16, sizeof(wchar_t));
        if (!buf) return NULL;
        int buffstart;
        if (ifn[0] == '\\' && ifn[1] == '\\') {
            if (ifn[2] == '?') {
                // Already an UNC...
                buffstart = 0;
            } else {
                // Network path "\\server\share" style...
                buf[0] = '\\'; buf[1] = '\\';  buf[2] = '?'; buf[3] = '\\';
                buf[4] = 'U'; buf[5] = 'N';
                buffstart = 6;
            }
        } else {
            // Relative or non UNC path.
            buf[0] = '\\'; buf[1] = '\\';  buf[2] = '?'; buf[3] = '\\';
            buffstart = 4;
        }
        // Get the real pathname this time...
        if (GetFullPathNameW(ifn, len, &buf[buffstart], NULL)) {
            //printWErr(L"FullPathName=", &buf[buffstart]);
            if (buffstart == 6) {
                buf[6] = 'C'; // Network path
            } else if (buffstart == 4 && buf[4] == '\\' && buf[5] == '\\') {
                // it was a relative network path,
                // so now it is in the \\?\\\server\share format (BAD).
                // shift the full path two char to the right.
                int i = wcslen(buf);
                for (; i > 4; i--) { buf[i+2] = buf[i]; }
                // Add UNC so that we have \\?\UNC\server\share
                buf[4] = 'U'; buf[5] = 'N'; buf[6] = 'C';
            }
            //printWErr(L"UNCPathName=", buf);
            return buf;
        } else {
            printWErr(L"Unable to get full path name for ", ifn);
            free (buf);
        }
    }
    return NULL;
}
///////////////////////////////////////////////////////////////////////////
// Main with UNICODE support!
int wmain(int argc, wchar_t **argv)
{
    int ret = 0;
    if (argc !=2) {
        printW(L"Usage: ssname.exe filename\n");
        return 1;
    }

    // fn will be the full path with the \\?\ prefix
    // So LONG-PATH will be handled.
    wchar_t *fn = GetUNCPath(argv[1]);
    if(!fn) {
        return 1;
    } else if (!FileExistsW(fn)) {
        printWErr(fn, L" Not found!");
        free(fn);
        return 1;
    }
    DWORD slen = GetShortPathNameW(fn, NULL, 0);
    wchar_t *sfn = calloc(slen+16, sizeof(wchar_t));
    wchar_t *shortname;
    if (slen) slen = GetShortPathNameW(fn, sfn, slen);
    if (slen && (shortname = IsShortName(sfn))) {
        printWErr(L"Already short-named: ", shortname);
    } else {
        // No existing short name!
        ModifyPrivilege(SE_RESTORE_NAME, TRUE);

        HANDLE hfile = CreateFileW(fn, GENERIC_ALL
                                 , FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE, NULL
                                 , OPEN_EXISTING
                                 , FILE_FLAG_BACKUP_SEMANTICS, NULL);
        if(!hfile) {
            printWErr(fn, L" cannot be opened");
            free(fn);
            return 1;
        }
        // Separate path from file...
        wchar_t *path = fn;
        wchar_t *filename = SeparatePathAndFN(path);

        // We need to create a new shortname names shorty!
        // This one should not include the path...
        wchar_t shorty[14];
        ZeroMemory(shorty, sizeof(shorty));
        if (!NewShortname(shorty, filename, path)) {
            printWErr(filename, L" Unable to find a new valid shortname!");
            ret = 1;
        }
        if (!ret && SetFileShortNameW(hfile, shorty)) {
            printWErr(L"SHORT-NAME SET TO: ", shorty);
        } else {
            printWErr(L"Unable to set short-name: ", shorty);
        }
        CloseHandle(hfile);
        ModifyPrivilege(SE_RESTORE_NAME, FALSE);
    }
    free(fn);
    return ret;
}

///////////////////////////////////////////////////////////////////////////
int __wgetmainargs(long*,wchar_t ***,wchar_t ***, long, long*);
int WINAPI unfuckMain(void)
{
    long argc, l;
    wchar_t **argv, **Env_;
    __wgetmainargs(&argc, &argv, &Env_,1 , &l);
    hCONSOLE = GetStdHandle(STD_OUTPUT_HANDLE);
    return wmain(argc, argv);
}
