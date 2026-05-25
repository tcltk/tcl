// This is a simple test application to be used for testing the exec command
// on Windows. Minimal (or no) error handling.
//    execTestApp ?-cwd? ?-argv0? ?-path? ?-env ENVVAR?
// -cwd: print the current working directory
// -argv0: print the argv[0] passed to this application
// -path: print the full path of this application
// -env: print the value of the specified environment variable

#include <windows.h>
#include <wchar.h>
#include <stdlib.h>
#include <stdio.h>

void print(const WCHAR *s) 
{
    int utf8len = WideCharToMultiByte(CP_UTF8, 0, s, -1, NULL, 0, NULL, NULL);
    if (utf8len > 0) {
        char *utf8 = (char *)malloc((size_t)utf8len);
        if (utf8) {
            WideCharToMultiByte(CP_UTF8, 0, s, -1, utf8, utf8len, NULL, NULL);
            printf("%s\n", utf8);
            free(utf8);
	    return;
	}
    }
    printf("%s", "Error converting to UTF-8\n");
}

int wmain(int argc, wchar_t **argv)
{
    WCHAR buf[8000];

    for (int i = 1; i < argc; i++) {
	if (_wcsicmp(argv[i], L"-cwd") == 0) {
	    GetCurrentDirectoryW(sizeof(buf)/sizeof(buf[0]), buf);
	    print(buf);
	} else if (_wcsicmp(argv[i], L"-argv0") == 0) {
	    print(argv[0]);
	} else if (_wcsicmp(argv[i], L"-env") == 0) {
	    ++i;
	    if (i == argc) {
		print(L"Missing environment variable name");
		return 1;
	    }
	    DWORD len = GetEnvironmentVariableW(argv[i], buf,
		sizeof(buf) / sizeof(buf[0]));
	    if (len == 0 || len >= (sizeof(buf) / sizeof(buf[0]))) {
		print(L"Failed to retrieve environment variable");
		return 1;
	    }
	    print(buf);
	} else if (_wcsicmp(argv[i], L"-path") == 0) {
	    GetModuleFileNameW(NULL, buf, sizeof(buf) / sizeof(buf[0]));
	    print(buf);
	} else {
	    print(L"Unknown option");
	    return 1;
	}
    }
    return 0;
}
