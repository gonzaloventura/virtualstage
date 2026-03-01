#pragma once
// Workaround: GCC 15+ on MSYS2/MinGW defines std::byte (C++17) which
// conflicts with Windows SDK "typedef unsigned char byte" in rpcndr.h.
// openFrameworks has "using namespace std;" in its headers, bringing
// std::byte into the global namespace and making "byte" ambiguous in
// Windows SDK headers included later by ofxSpout (via dxgiformat.h).
//
// Fix: pre-include the Windows SDK headers BEFORE any C++ standard
// headers are processed, so all "byte" references in the SDK are
// resolved before std::byte even exists.
#if defined(_WIN32) && defined(__cplusplus)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN
#include <rpc.h>
#include <ole2.h>

// Run a shell command without showing a console window.
// Equivalent to system() but with CREATE_NO_WINDOW flag.
#include <string>
static inline int silentSystem(const std::string& cmd) {
    std::string full = "cmd.exe /c " + cmd;
    // CreateProcessA needs a mutable buffer
    char* buf = new char[full.size() + 1];
    memcpy(buf, full.c_str(), full.size() + 1);

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = {};

    BOOL ok = CreateProcessA(NULL, buf, NULL, NULL, FALSE,
                             CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    delete[] buf;
    if (!ok) return -1;

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (int)exitCode;
}
#endif
