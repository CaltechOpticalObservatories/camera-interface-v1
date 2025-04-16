// +------------------------------------------------------------------------------------------------------------------+
// |  FILE:  CArcBaseDllMain.cpp                                                                                      |
// +------------------------------------------------------------------------------------------------------------------+
// |  PURPOSE: This file implements the system dependent import/export properties of the library. On windows this is  |
// |           the main entry point for the library.                                                                  |
// |                                                                                                                  |
// |  AUTHOR:  Scott Streit			DATE: Sept 24, 2014                                                               |
// |                                                                                                                  |
// |  Copyright 2013 Astronomical Research Cameras, Inc. All rights reserved.                                         |
// +------------------------------------------------------------------------------------------------------------------+
#ifdef _WINDOWS

#include <windows.h>

BOOL APIENTRY DllMain( HMODULE hModule,
					   DWORD  ul_reason_for_call,
					   LPVOID lpReserved
					 )
{
	switch ( ul_reason_for_call )
	{
		case DLL_PROCESS_ATTACH:
		case DLL_THREAD_ATTACH:
		case DLL_THREAD_DETACH:
		case DLL_PROCESS_DETACH:
		break;
	}

	return TRUE;
}

#endif
