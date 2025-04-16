// +------------------------------------------------------------------------------------------------------------------+
// |  FILE:  CArcBaseDllMain.h                                                                                        |
// +------------------------------------------------------------------------------------------------------------------+
// |  PURPOSE: This file defines the system dependent import/export properties of the library. On windows this is     |
// |           the main entry point for the library.                                                                  |
// |                                                                                                                  |
// |  AUTHOR:  Scott Streit			DATE: March 25, 2020                                                              |
// |                                                                                                                  |
// |  Copyright 2013 Astronomical Research Cameras, Inc. All rights reserved.                                         |
// +------------------------------------------------------------------------------------------------------------------+
#ifndef _GEN3_CARCBASE_DLLMAIN_H_
#define _GEN3_CARCBASE_DLLMAIN_H_


// +------------------------------------------------------------------------------------------------------------------+
// | The following ifdef block is the standard way of creating macros which make exporting from a DLL simpler. All    |
// | files within this DLL are compiled with the gen3_CARCBASE_EXPORTS symbol defined on the command line.            |
// | this symbol should not be defined on any project that uses this DLL. This way any other project whose source     |
// | files include this file see CARCDEVICE_API functions as being imported from a DLL, whereas this DLL sees symbols |
// | defined with this macro as being exported.                                                                       |
// +------------------------------------------------------------------------------------------------------------------+
#ifdef _WINDOWS
	#ifdef GEN3_CARCBASE_EXPORTS
		#define GEN3_CARCBASE_API __declspec( dllexport )
	#else
		#define GEN3_CARCBASE_API __declspec( dllimport )
	#endif
#else
	#define GEN3_CARCBASE_API __attribute__( ( visibility( "default" ) ) )
#endif


#endif	// _GEN3_CARCBASE_DLLMAIN_H_
