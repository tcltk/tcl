


#include <windows.h>

__declspec(dllexport) void thrower() {
    RaiseException(
        1,                    // exception code
        0,                    // continuable exception
        0, NULL);             // no arguments
}
