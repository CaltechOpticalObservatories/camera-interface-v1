#ifndef _ARC_DEVICE_CAPI_H_
#define _ARC_DEVICE_CAPI_H_

#include <CArcDeviceDllMain.h>


// +------------------------------------------------------------------------------------+
// | Status/Error constants                                                             |
// +------------------------------------------------------------------------------------+
#ifndef ARC_STATUS
#define ARC_STATUS

	#define ARC_STATUS_OK			0
	#define ARC_STATUS_ERROR		1
	#define ARC_MSG_SIZE			256
	#define ARC_ERROR_MSG_SIZE		256

#endif


#ifdef __cplusplus
   extern "C" {		// Using a C++ compiler
#endif


// +----------------------------------------------------------------------------------------------------------------------------+
// | Device constants                                                                                                           |
// +----------------------------------------------------------------------------------------------------------------------------+
GEN3_CARCDEVICE_API const int extern DEVICE_NOPARAM;


// +----------------------------------------------------------------------------------------------------------------------------+
// | Device access                                                                                                              |
// +----------------------------------------------------------------------------------------------------------------------------+
GEN3_CARCDEVICE_API const char* ArcDevice_ToString( int* pStatus );

GEN3_CARCDEVICE_API void ArcDevice_FindDevices( int* pStatus );
GEN3_CARCDEVICE_API unsigned int ArcDevice_DeviceCount();
GEN3_CARCDEVICE_API const char** ArcDevice_GetDeviceStringList( int* pStatus );
GEN3_CARCDEVICE_API void ArcDevice_FreeDeviceStringList();

GEN3_CARCDEVICE_API unsigned int ArcDevice_IsOpen( int* pStatus );
GEN3_CARCDEVICE_API void ArcDevice_Open( unsigned int uiDeviceNumber, int* pStatus );
GEN3_CARCDEVICE_API void ArcDevice_Open_I( unsigned int uiDeviceNumber, unsigned int uiBytes, int* pStatus );
GEN3_CARCDEVICE_API void ArcDevice_Open_II( unsigned int uiDeviceNumber, unsigned int uiRows, unsigned int uiCols, int* pStatus );
GEN3_CARCDEVICE_API void ArcDevice_Close( void );
GEN3_CARCDEVICE_API void ArcDevice_Reset( int* pStatus );

GEN3_CARCDEVICE_API void ArcDevice_MapCommonBuffer( unsigned int uiBytes, int* pStatus );
GEN3_CARCDEVICE_API void ArcDevice_UnMapCommonBuffer( int* pStatus );
GEN3_CARCDEVICE_API void ArcDevice_ReMapCommonBuffer( unsigned int uiBytes, int* pStatus );
GEN3_CARCDEVICE_API void ArcDevice_FillCommonBuffer( unsigned short u16Value, int* pStatus );
GEN3_CARCDEVICE_API void* ArcDevice_CommonBufferVA( int* pStatus );
GEN3_CARCDEVICE_API unsigned long long ArcDevice_CommonBufferPA( int* pStatus );
GEN3_CARCDEVICE_API unsigned long long ArcDevice_CommonBufferSize( int* pStatus );

GEN3_CARCDEVICE_API unsigned int ArcDevice_GetId( int* pStatus );
GEN3_CARCDEVICE_API unsigned int ArcDevice_GetStatus( int* pStatus );
GEN3_CARCDEVICE_API void ArcDevice_ClearStatus( int* pStatus );

GEN3_CARCDEVICE_API void ArcDevice_Set2xFOTransmitter( int bOnOff, int* pStatus );
GEN3_CARCDEVICE_API void ArcDevice_LoadDeviceFile( const char* pszFile, int* pStatus );

// +----------------------------------------------------------------------------------------------------------------------------+
// | Setup & general commands                                                                                                    |
// +----------------------------------------------------------------------------------------------------------------------------+
GEN3_CARCDEVICE_API unsigned int ArcDevice_Command( unsigned int uiBoardId, unsigned int uiCommand, int* pStatus );
GEN3_CARCDEVICE_API unsigned int ArcDevice_Command_I( unsigned int uiBoardId, unsigned int uiCommand, unsigned int uiArg1, int* pStatus );
GEN3_CARCDEVICE_API unsigned int ArcDevice_Command_II( unsigned int uiBoardId, unsigned int uiCommand, unsigned int uiArg1, unsigned int uiArg2, int* pStatus );
GEN3_CARCDEVICE_API unsigned int ArcDevice_Command_III( unsigned int uiBoardId, unsigned int uiCommand, unsigned int uiArg1, unsigned int uiArg2, unsigned int uiArg3, int* pStatus );
GEN3_CARCDEVICE_API unsigned int ArcDevice_Command_IIII( unsigned int uiBoardId, unsigned int uiCommand, unsigned int uiArg1, unsigned int uiArg2, unsigned int uiArg3, unsigned int uiArg4, int* pStatus );

GEN3_CARCDEVICE_API unsigned int ArcDevice_GetControllerId( int* pStatus );
GEN3_CARCDEVICE_API void ArcDevice_ResetController( int* pStatus );
GEN3_CARCDEVICE_API unsigned int ArcDevice_IsControllerConnected( int* pStatus );

GEN3_CARCDEVICE_API void ArcDevice_SetupController( unsigned int uiReset, unsigned int uiTdl, unsigned int uiPower, unsigned int uiRows, unsigned int uiCols, 
													const char* pszTimFile, const char* pszUtilFile, const char* pszPciFile, int* pStatus );

GEN3_CARCDEVICE_API void ArcDevice_LoadControllerFile( const char* pszFilename, unsigned int uiValidate, int* pStatus );
GEN3_CARCDEVICE_API void ArcDevice_SetImageSize( unsigned int uiRows, unsigned int uiCols, int* pStatus );

GEN3_CARCDEVICE_API unsigned int ArcDevice_GetImageRows( int* pStatus );
GEN3_CARCDEVICE_API unsigned int ArcDevice_GetImageCols( int* pStatus );
GEN3_CARCDEVICE_API unsigned int ArcDevice_GetCCParams( int* pStatus );
GEN3_CARCDEVICE_API unsigned int ArcDevice_IsCCParamSupported( unsigned int uiParameter, int* pStatus );
GEN3_CARCDEVICE_API unsigned int ArcDevice_IsCCD( int* pStatus );

GEN3_CARCDEVICE_API unsigned int ArcDevice_IsBinningSet( int* pStatus );
GEN3_CARCDEVICE_API void ArcDevice_SetBinning( unsigned int uiRows, unsigned int uiCols, unsigned int uiRowFactor, unsigned int uiColFactor, unsigned int* pBinRows, unsigned int* pBinCols, int* pStatus );
GEN3_CARCDEVICE_API void ArcDevice_UnSetBinning( unsigned int uiRows, unsigned int uiCols, int* pStatus );

GEN3_CARCDEVICE_API void ArcDevice_SetSubArray( unsigned int* pOldRows, unsigned int* pOldCols, unsigned int uiRow, unsigned int uiCol, unsigned int uiSubRows,
										   unsigned int uiSubCols, unsigned int uiBiasOffset, unsigned int uiBiasWidth, int* pStatus );

GEN3_CARCDEVICE_API void ArcDevice_UnSetSubArray( unsigned int uiRows, unsigned int uiCols, int* pStatus );

GEN3_CARCDEVICE_API unsigned int ArcDevice_IsSyntheticImageMode( int* pStatus );
GEN3_CARCDEVICE_API void ArcDevice_SetSyntheticImageMode( unsigned int uiMode, int* pStatus );

// +----------------------------------------------------------------------------------------------------------------------------+
// | expose commands                                                                                                            |
// +----------------------------------------------------------------------------------------------------------------------------+
GEN3_CARCDEVICE_API void ArcDevice_SetOpenShutter( int bShouldOpen, int* pStatus );

GEN3_CARCDEVICE_API void ArcDevice_Expose( float fExpTime, unsigned int uiRows, unsigned int uiCols, void ( *pExposeCall )( float ),
										   void ( *pReadCall )( int ), int bOpenShutter, int* pStatus );

GEN3_CARCDEVICE_API void ArcDevice_StopExposure( int* pStatus );

GEN3_CARCDEVICE_API void ArcDevice_Continuous( unsigned int uiRows, unsigned int uiCols, unsigned int uiNumOfFrames, float fExpTime,
											   void ( *pFrameCall )( int, int, int, int, void * ), unsigned int uiOpenShutter, int* pStatus );

GEN3_CARCDEVICE_API void ArcDevice_StopContinuous( int* pStatus );

GEN3_CARCDEVICE_API unsigned int ArcDevice_IsReadout( int* pStatus );
GEN3_CARCDEVICE_API unsigned int ArcDevice_GetPixelCount( int* pStatus );
GEN3_CARCDEVICE_API unsigned int ArcDevice_GetCRPixelCount( int* pStatus );
GEN3_CARCDEVICE_API unsigned int ArcDevice_GetFrameCount( int* pStatus );

// +----------------------------------------------------------------------------------------------------------------------------+
// | Error & Degug message access                                                                                               |
// +----------------------------------------------------------------------------------------------------------------------------+
GEN3_CARCDEVICE_API unsigned int ArcDevice_ContainsError( unsigned int uiWord, int* pStatus );
GEN3_CARCDEVICE_API unsigned int ArcDevice_ContainsError_I( unsigned int uiWord, unsigned int uiWordMin, unsigned int uiWordMax, int* pStatus );

GEN3_CARCDEVICE_API const char*	ArcDevice_GetNextLoggedCmd( int* pStatus );
GEN3_CARCDEVICE_API unsigned int ArcDevice_GetLoggedCmdCount( int* pStatus );
GEN3_CARCDEVICE_API void ArcDevice_SetLogCmds( int bOnOff, int* pStatus );

// +----------------------------------------------------------------------------------------------------------------------------+
// | Temperature control                                                                                                        |
// +----------------------------------------------------------------------------------------------------------------------------+
GEN3_CARCDEVICE_API  double ArcDevice_GetArrayTemperature( int* pStatus );
GEN3_CARCDEVICE_API  double ArcDevice_GetArrayTemperatureDN( int* pStatus );
GEN3_CARCDEVICE_API  void ArcDevice_SetArrayTemperature( double gTempVal, int* pStatus );
GEN3_CARCDEVICE_API  void ArcDevice_LoadTemperatureCtrlData( const char* pszFilename, int* pStatus );
GEN3_CARCDEVICE_API  void ArcDevice_SaveTemperatureCtrlData( const char* pszFilename, int* pStatus );

GEN3_CARCDEVICE_API const char* ArcDevice_GetLastError( void );

#ifdef __cplusplus
   }
#endif

#endif		// _ARC_IMAGE_CAPI_H_
