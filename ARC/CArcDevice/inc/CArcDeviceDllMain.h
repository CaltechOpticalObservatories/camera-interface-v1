#ifndef _CARCDEVICE_DLLMAIN_H_
#define _CARCDEVICE_DLLMAIN_H_


// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the CARCDEVICE_EXPORTS
// symbol defined on the command line. this symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// GEN3_CARCDEVICE_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
#ifdef _WINDOWS
	#ifdef GEN3_CARCDEVICE_EXPORTS
		#define GEN3_CARCDEVICE_API __declspec( dllexport )
		#define GEN3_CARCDEVICE_STL_EXPORT
	#else
		#define GEN3_CARCDEVICE_API __declspec( dllimport )
		#define GEN3_CARCDEVICE_STL_EXPORT extern
	#endif
#else
	#define GEN3_CARCDEVICE_API __attribute__((visibility("default")))
#endif


#endif		// _CARCDEVICE_DLLMAIN_H_
