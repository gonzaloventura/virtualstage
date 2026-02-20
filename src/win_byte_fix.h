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
#endif
