//////////////////////////////////////////////////////////////////////////
// Niflib Includes
//////////////////////////////////////////////////////////////////////////
#include <stdafx.h>
#include <niflib.h>
#include <nif_io.h>

#ifdef _WIN64
#ifdef NIFLIB_STATIC_LINK
# ifdef _DEBUG
#  pragma comment(lib, "niflib_static_debug_x64.lib")
# else
#  pragma comment(lib, "niflib_static_x64.lib")
# endif
#else
# ifdef _DEBUG
#  pragma comment(lib, "niflib_dll_debug_x64.lib")
# else
#  pragma comment(lib, "niflib_dll_x64.lib")
# endif
#endif
#else
#ifdef NIFLIB_STATIC_LINK
# ifdef _DEBUG
#  pragma comment(lib, "niflib_static_debug.lib")
# else
#  pragma comment(lib, "niflib_static.lib")
# endif
#else
# ifdef _DEBUG
#  pragma comment(lib, "niflib_dll_debug.lib")
# else
#  pragma comment(lib, "niflib_dll.lib")
# endif
#endif
#endif