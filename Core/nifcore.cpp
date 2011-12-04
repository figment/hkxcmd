//////////////////////////////////////////////////////////////////////////
// Niflib Includes
//////////////////////////////////////////////////////////////////////////
#include <stdafx.h>
#include <niflib.h>
#include <nif_io.h>

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