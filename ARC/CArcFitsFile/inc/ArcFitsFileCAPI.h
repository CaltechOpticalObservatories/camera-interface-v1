// +------------------------------------------------------------------------------------------------------------------+
// |  FILE:  ArcFitsFileCAPI.h  ( Gen3 )                                                                              |
// +------------------------------------------------------------------------------------------------------------------+
// |  PURPOSE: This file defines a C interface for all the CArcFitsFile class methods.                                |
// |                                                                                                                  |
// |  AUTHOR:  Scott Streit			DATE: March 25, 2020                                                              |
// |                                                                                                                  |
// |  Copyright 2013 Astronomical Research Cameras, Inc. All rights reserved.                                         |
// +------------------------------------------------------------------------------------------------------------------+

#ifndef _ARC_GEN3_FITSFILE_CAPI_H_
#define _ARC_GEN3_FITSFILE_CAPI_H_


#include <stdlib.h>

#include <fitsio.h>

#include <CArcFitsFileDllMain.h>


// +------------------------------------------------------------------------------------------------------------------+
// | Status definitions                                                                                               |
// +------------------------------------------------------------------------------------------------------------------+

typedef unsigned int ArcStatus_t;				/** Return status type */


#ifdef __cplusplus
extern "C" {		// Using a C++ compiler
#endif

	typedef uint32_t ArcError_t;

	extern GEN3_CARCFITSFILE_API const ArcError_t* ARC_STATUS_NONE;
	extern GEN3_CARCFITSFILE_API const ArcError_t  ARC_STATUS_OK;
	extern GEN3_CARCFITSFILE_API const ArcError_t  ARC_STATUS_ERROR;

	extern GEN3_CARCFITSFILE_API const ArcError_t  ARC_MSG_SIZE;
	extern GEN3_CARCFITSFILE_API const ArcError_t  ARC_ERROR_MSG_SIZE;

#ifdef __cplusplus
}
#endif


#ifdef __cplusplus
extern "C" {		// Using a C++ compiler
#endif

	// +------------------------------------------------------------------------------------------------------------------+
	// | Define FITS file modes                                                                                           |
	// +------------------------------------------------------------------------------------------------------------------+
	GEN3_CARCFITSFILE_API extern const unsigned int FITS_READMODE;
	GEN3_CARCFITSFILE_API extern const unsigned int FITS_READWRITEMODE;


	// +------------------------------------------------------------------------------------------------------------------+
	// | Define FITS header keywords                                                                                      |
	// +------------------------------------------------------------------------------------------------------------------+
	GEN3_CARCFITSFILE_API extern const int FITS_STRING_KEY;
	GEN3_CARCFITSFILE_API extern const int FITS_INT_KEY;
	GEN3_CARCFITSFILE_API extern const int FITS_UINT_KEY;
	GEN3_CARCFITSFILE_API extern const int FITS_SHORT_KEY;
	GEN3_CARCFITSFILE_API extern const int FITS_USHORT_KEY;
	GEN3_CARCFITSFILE_API extern const int FITS_FLOAT_KEY;
	GEN3_CARCFITSFILE_API extern const int FITS_DOUBLE_KEY;
	GEN3_CARCFITSFILE_API extern const int FITS_BYTE_KEY;
	GEN3_CARCFITSFILE_API extern const int FITS_LONG_KEY;
	GEN3_CARCFITSFILE_API extern const int FITS_ULONG_KEY;
	GEN3_CARCFITSFILE_API extern const int FITS_LONGLONG_KEY;
	GEN3_CARCFITSFILE_API extern const int FITS_LOGICAL_KEY;
	GEN3_CARCFITSFILE_API extern const int FITS_COMMENT_KEY;
	GEN3_CARCFITSFILE_API extern const int FITS_HISTORY_KEY;
	GEN3_CARCFITSFILE_API extern const int FITS_DATE_KEY;

#ifdef __cplusplus
}
#endif


#ifdef __cplusplus
extern "C" {		// Using a C++ compiler
#endif

	// +------------------------------------------------------------------------------------------------------------------+
	// | Define FITS file bits-per-pixel constants                                                                        |
	// +------------------------------------------------------------------------------------------------------------------+
	GEN3_CARCFITSFILE_API extern const unsigned int FITS_BPP16;				/** 16-bit bits-per-pixel image data */
	GEN3_CARCFITSFILE_API extern const unsigned int FITS_BPP32;				/** 32-bit bits-per-pixel image data */


	/** Returns a handle to the FITS file object appropriate for the specified bits-per-pixel.
	 *  @param uiBpp	- The number of bits-per-pixel in the image.
	 *  @param pStatus	- Success state; equals ARC_STATUS_OK or ARC_STATUS_ERROR.
	 *  @return A reference to a FITS file object.
	 */
	GEN3_CARCFITSFILE_API unsigned long long ArcFitsFile_getInstance( unsigned int uiBpp, ArcStatus_t* pStatus );

	/** Returns the library build and version info.
	 *  @param pStatus	- Success state; equals ARC_STATUS_OK or ARC_STATUS_ERROR.
	 *  @return A strng representation of the library version.
	 */
	GEN3_CARCFITSFILE_API const char* ArcFitsFile_version( ArcStatus_t* pStatus );

	/** Creates a new single image file on disk with the specified image dimensions.
	 *  @param ulHandle	 - A reference to a FITS file object.
	 *  @param sFileName - The new FITS file name.
	 *  @param uiCols	 - The image column size ( in pixels ).
	 *  @param uiRows	 - The image row size ( in pixels ).
	 *  @param pStatus	 - Success state; equals ARC_STATUS_OK or ARC_STATUS_ERROR.
	 */
	GEN3_CARCFITSFILE_API void ArcFitsFile_create( unsigned long long ulHandle, const char* pszFileName, unsigned int uiCols, unsigned int uiRows, ArcStatus_t* pStatus );

	/** Creates a new data cube file on disk with the specified image dimensions.
	 *  @param ulHandle		- A reference to a FITS file object.
	 *  @param pszFileName - The new FITS file name.
	 *  @param uiCols		- The image column size ( in pixels ).
	 *  @param uiRows		- The image row size ( in pixels ).
	 *  @param pStatus		- Success state; equals ARC_STATUS_OK or ARC_STATUS_ERROR.
	 */
	GEN3_CARCFITSFILE_API void ArcFitsFile_create3D( unsigned long long ulHandle, const char* pszFileName, unsigned int uiCols, unsigned int uiRows, ArcStatus_t* pStatus );

	/** Opens an existing file. Can be used to open a file containing a single image or data cube
	 *  ( a file with multiple image planes ).
	 *  @param ulHandle		- A reference to a FITS file object.
	 *  @param pszsFileName - The file name.
	 *  @param uiReadMode	- The mode ( FITS_READMODE or FITS_READWRITEMODE ) with which to open the file.
	 *  @param pStatus		- Success state; equals ARC_STATUS_OK or ARC_STATUS_ERROR.
	 */
	GEN3_CARCFITSFILE_API void ArcFitsFile_open( unsigned long long ulHandle, const char* pszFileName, unsigned int uiReadMode, ArcStatus_t* pStatus );

	/** Closes the file. All subsequent method calls, except for create and open will result
	 *  in an error and an exception will be thrown.
	 *  @param ulHandle - A reference to a FITS file object.
	 *  @param pStatus	- Success state; equals ARC_STATUS_OK or ARC_STATUS_ERROR.
	 */
	GEN3_CARCFITSFILE_API void ArcFitsFile_close( unsigned long long ulHandle, ArcStatus_t* pStatus );

	/** Returns the FITS header as a list of strings.
	 *  @param ulHandle - A reference to a FITS file object.
	 *  @param uiCount	- Returns the number of strings in the returned list.
	 *  @param pStatus	- Success state; equals ARC_STATUS_OK or ARC_STATUS_ERROR.
	 *  @return A pointer to a list of strings. The number of strings is set in uiCount.
	 */
	GEN3_CARCFITSFILE_API const char** ArcFitsFile_getHeader( unsigned long long ulHandle, unsigned int* uiCount, ArcStatus_t* pStatus );

	/** Frees the FITS header as returned by ArcFitsFile_getHeader().
	 *  @param ulHandle - A reference to a FITS file object.
	 *  @param pStatus	- Success state; equals ARC_STATUS_OK or ARC_STATUS_ERROR.
	 */
	GEN3_CARCFITSFILE_API void ArcFitsFile_freeHeader( unsigned long long ulHandle, ArcStatus_t* pStatus );

	/** Returns the file name.
	 *  @param ulHandle - A reference to a FITS file object.
	 *  @param pStatus	- Success state; equals ARC_STATUS_OK or ARC_STATUS_ERROR.
	 *  @return The FITS filename associated with this object.
	 */
	GEN3_CARCFITSFILE_API const char* ArcFitsFile_getFileName( unsigned long long ulHandle, ArcStatus_t* pStatus );

	/** Writes a new keyword to the header.
	 *
	 * 'HIERARCH' keyword NOTE: This text will be prefixed to any keyword by the cfitsio library if the keyword
	 *                          is greater than 8 characters, which is the standard FITS keyword length. See the
	 *                          link below for details:
	 *
	 * http://heasarc.gsfc.nasa.gov/docs/software/fitsio/c/f_user/node28.html
	 *
	 *  @param ulHandle		- A reference to a FITS file object.
	 *  @param pszKey		- A valid FITS keyword.
	 *  @param pKeyVal		- A pointer to the keyword value. The value type is determined by the value type parameter. Must not equal NULL.
	 *  @param uiValType	- The keyword type.
	 *  @param pszComment	- The optional header comment ( can be NULL ).
	 *  @param pStatus		- Success state; equals ARC_STATUS_OK or ARC_STATUS_ERROR.
	 */
	GEN3_CARCFITSFILE_API void ArcFitsFile_writeKeyword( unsigned long long ulHandle, const char* pszKey, void* pKeyVal, unsigned int uiValType, const char* pszComment, ArcStatus_t* pStatus );

	/** Updates an existng header keyword.
	 *
	 * 'HIERARCH' keyword NOTE: This text will be prefixed to any keyword by the cfitsio library if the keyword
	 *                          is greater than 8 characters, which is the standard FITS keyword length. See the
	 *                          link below for details:
	 *
	 * http://heasarc.gsfc.nasa.gov/docs/software/fitsio/c/f_user/node28.html
	 *
	 *  @param ulHandle		- A reference to a FITS file object.
	 *  @param pszKey		- A valid FITS keyword.
	 *  @param pKeyVal		- A pointer to the keyword value. The value type is determined by the value type parameter. Must not equal NULL.
	 *  @param uiValType	- The keyword type.
	 *  @param pszComment	- The optional header comment ( can be NULL ).
	 *  @param pStatus		- Success state; equals ARC_STATUS_OK or ARC_STATUS_ERROR.
	 */
	GEN3_CARCFITSFILE_API void ArcFitsFile_updateKeyword( unsigned long long ulHandle, const char* pszKey, void* pKeyVal, unsigned int uiValType, const char* pszComment, ArcStatus_t* pStatus );

	/** Returns the basic image parameters from the FITS header ( number of cols, rows, frames, dimensions and bits-per-pixel ).
	 *  @param ulHandle	- A reference to a FITS file object.
	 *  @param plNaxes	- Returns the dimension of each axis. MUST have a size of 3. ( 0 = cols, 1 = rows, 2 = frames ).
	 *  @param piNaxis	- Returns the number of axes in the file ( 2 = normal image, 3 = data cube ).
	 *  @param piBpp	- The bits-per-pixel.
	 *  @param pStatus	- Success state; equals ARC_STATUS_OK or ARC_STATUS_ERROR.
	 */
	GEN3_CARCFITSFILE_API void ArcFitsFile_getParameters( unsigned long long ulHandle, long* plNaxes, int* piNaxis, int* piBpp, ArcStatus_t* pStatus );

	/** Returns the number of frames.  A single image file will return a value of 0.
	 *  @param ulHandle	- A reference to a FITS file object.
	 *  @param pStatus	- Success state; equals ARC_STATUS_OK or ARC_STATUS_ERROR.
	 *  @return	The number of frames.
	 */
	GEN3_CARCFITSFILE_API unsigned int ArcFitsFile_getNumberOfFrames( unsigned long long ulHandle, ArcStatus_t* pStatus );

	/** Returns the number of rows in the image.
	 *  @param ulHandle	- A reference to a FITS file object.
	 *  @param pStatus	- Success state; equals ARC_STATUS_OK or ARC_STATUS_ERROR.
	 *  @return	The number of rows in the image.
	 */
	GEN3_CARCFITSFILE_API unsigned int ArcFitsFile_getRows( unsigned long long ulHandle, ArcStatus_t* pStatus );

	/** Returns the number of columns in the image.
	 *  @param ulHandle	- A reference to a FITS file object.
	 *  @param pStatus	- Success state; equals ARC_STATUS_OK or ARC_STATUS_ERROR.
	 *  @return	The number of columns in the image.
	 */
	GEN3_CARCFITSFILE_API unsigned int ArcFitsFile_getCols( unsigned long long ulHandle, ArcStatus_t* pStatus );

	/** Returns the number of dimensions in the image.
	 *  @param ulHandle	- A reference to a FITS file object.
	 *  @param pStatus	- Success state; equals ARC_STATUS_OK or ARC_STATUS_ERROR.
	 *  @return	The number of dimensions in the image.
	 */
	GEN3_CARCFITSFILE_API unsigned int ArcFitsFile_getNAxis( unsigned long long ulHandle, ArcStatus_t* pStatus );

	/** Returns the image bits-per-pixel value.
	 *  @param ulHandle	- A reference to a FITS file object.
	 *  @param pStatus	- Success state; equals ARC_STATUS_OK or ARC_STATUS_ERROR.
	 *  @return	The image bits-per-pixel value.
	 */
	GEN3_CARCFITSFILE_API unsigned int ArcFitsFile_getBitsPerPixel( unsigned long long ulHandle, ArcStatus_t* pStatus );

	/** Generates a ramp test pattern image within the file. The size of the image is determined by the image
	 *  dimensions supplied during the create() method call. This method is only valid for single image files.
	 *  @param ulHandle	- A reference to a FITS file object.
	 *  @param pStatus	- Success state; equals ARC_STATUS_OK or ARC_STATUS_ERROR.
	 */
	GEN3_CARCFITSFILE_API void ArcFitsFile_generateTestData( unsigned long long ulHandle, ArcStatus_t* pStatus );

	/** Effectively closes and re-opens the underlying disk file.
	 *  @param ulHandle	- A reference to a FITS file object.
	 *  @param pStatus	- Success state; equals ARC_STATUS_OK or ARC_STATUS_ERROR.
	 */
	GEN3_CARCFITSFILE_API void ArcFitsFile_reOpen( unsigned long long ulHandle, ArcStatus_t* pStatus );

	/** Causes all internal data buffers to write data to the disk file.
	 *  @param ulHandle	- A reference to a FITS file object.
	 *  @param pStatus	- Success state; equals ARC_STATUS_OK or ARC_STATUS_ERROR.
	 */
	GEN3_CARCFITSFILE_API void ArcFitsFile_flush( unsigned long long ulHandle, ArcStatus_t* pStatus );

	/** Resizes a single image file by modifying the NAXES keyword and increasing the image data portion of the file.
	 *  @param ulHandle	- A reference to a FITS file object.
	 *  @param uiCols	- The number of cols the new FITS file will have.
	 *  @param uiRows	- The number of rows the new FITS file will have.
	 *  @param pStatus	- Success state; equals ARC_STATUS_OK or ARC_STATUS_ERROR.
	 */
	GEN3_CARCFITSFILE_API void ArcFitsFile_reSize( unsigned long long ulHandle, unsigned int uiCols, unsigned int uiRows, ArcStatus_t* pStatus );

	/** Writes image data to a single image file.
	 *  @param ulHandle	- A reference to a FITS file object.
	 *  @param pBuf		- The image buffer to write. Buffer access violation results in undefined behavior.
	 *  @param pStatus	- Success state; equals ARC_STATUS_OK or ARC_STATUS_ERROR.
	 */
	GEN3_CARCFITSFILE_API void ArcFitsFile_write( unsigned long long ulHandle, void* pBuf, ArcStatus_t* pStatus );

	/** Writes the specified number of bytes to a single image file. The start position of the data within
	 *  the file image can be specified.
	 *  @param ulHandle	- A reference to a FITS file object.
	 *  @param pBuf		- The image buffer to write. Buffer access violation results in undefined behavior.
	 *  @param i64Bytes	- The number of bytes to write.
	 *  @param i64Pixel	- The start pixel within the file image. ( for default set = 1 ).
	 *  @param pStatus	- Success state; equals ARC_STATUS_OK or ARC_STATUS_ERROR.
	 */
	GEN3_CARCFITSFILE_API void ArcFitsFile_writeN( unsigned long long ulHandle, void* pBuf, unsigned long long i64Bytes, unsigned long long i64Pixel, ArcStatus_t* pStatus );

	/** Writes a sub-image of the specified buffer to a single image file.
	 *  @param ulHandle	- A reference to a FITS file object.
	 *  @param pBuf		- The image buffer to write. Buffer access violation results in undefined behavior.
	 *  @param llX		- The lower left column of the sub-image.
	 *  @param llY		- The lower left row of the sub-image.
	 *  @param urX		- The upper right column of the sub-image.
	 *  @param urY		- The upper right row of the sub-image.
	 *  @param pStatus	- Success state; equals ARC_STATUS_OK or ARC_STATUS_ERROR.
	 */
	GEN3_CARCFITSFILE_API void ArcFitsFile_writeSubImage( unsigned long long ulHandle, void* pBuf, long llX, long llY, long urX, long urY, ArcStatus_t* pStatus );

	/** Reads a sub-image from a single image file.
	 *  @param ulHandle	- A reference to a FITS file object.
	 *  @param llX		- The lower left column of the sub-image.
	 *  @param llY		- The lower left row of the sub-image.
	 *  @param urX		- The upper right column of the sub-image.
	 *  @param urY		- The upper right row of the sub-image.
	 *  @param pStatus	- Success state; equals ARC_STATUS_OK or ARC_STATUS_ERROR.
	 *  @return A pointer to the sub-image data.
	 */
	GEN3_CARCFITSFILE_API void* ArcFitsFile_readSubImage( unsigned long long ulHandle, long llX, long llY, long urX, long urY, ArcStatus_t* pStatus );

	/** Read the image from a single image file.
	 *  @param ulHandle	- A reference to a FITS file object.
	 *  @param pStatus	- Success state; equals ARC_STATUS_OK or ARC_STATUS_ERROR.
	 *  @return A pointer to the image data.
	 */
	GEN3_CARCFITSFILE_API void* ArcFitsFile_read( unsigned long long ulHandle, ArcStatus_t* pStatus );

	/** Writes an image to the end of a data cube file.
	 *  @param ulHandle	- A reference to a FITS file object.
	 *  @param pBuf		- The image buffer to write. Buffer access violation results in undefined behavior.
	 *  @param pStatus	- Success state; equals ARC_STATUS_OK or ARC_STATUS_ERROR.
	 */
	GEN3_CARCFITSFILE_API void ArcFitsFile_write3D( unsigned long long ulHandle, void* pBuf, ArcStatus_t* pStatus );

	/** Re-writes an existing image in a data cube file. The image data MUST match in size to the exising images within the data cube.
	 *  @param ulHandle			- A reference to a FITS file object.
	 *  @param pBuf				- The image buffer. Buffer access violation results in undefined behavior.
	 *  @param uiImageNumber	- The number of the data cube image to replace.
	 *  @param pStatus			- Success state; equals ARC_STATUS_OK or ARC_STATUS_ERROR.
	 */
	GEN3_CARCFITSFILE_API void ArcFitsFile_reWrite3D( unsigned long long ulHandle, void* pBuf, unsigned int uiImgNumber, ArcStatus_t* pStatus );

	/** Reads an image from a data cube file.
	 *  @param ulHandle			- A reference to a FITS file object.
	 *  @param uiImageNumber	- The image number.
	 *  @param pStatus			- Success state; equals ARC_STATUS_OK or ARC_STATUS_ERROR.
	 *  @return A pointer to the image data.
	 */
	GEN3_CARCFITSFILE_API void* ArcFitsFile_read3D( unsigned long long ulHandle, unsigned int uiImgNumber, ArcStatus_t* pStatus );

	/** Returns the underlying cfitsio file pointer.
	 *  @param ulHandle	- A reference to a FITS file object.
	 *  @param pStatus	- Success state; equals ARC_STATUS_OK or ARC_STATUS_ERROR.
	 *  @return A pointer to the internal cfitsio file pointer ( may be nullptr ).
	 */
	GEN3_CARCFITSFILE_API fitsfile* ArcFitsFile_getBaseFile( unsigned long long ulHandle, ArcStatus_t* pStatus );

	/** Determines the maximum value for a specific data type. Example, for std::uint16_t: 2^16 = 65536.
	 *  @param ulHandle	- A reference to a FITS file object.
	 *  @param pStatus	- Success state; equals ARC_STATUS_OK or ARC_STATUS_ERROR.
	 *  @return The maximum value for the data type currently in use.
	 */
	GEN3_CARCFITSFILE_API unsigned int ArcFitsFile_maxTVal( unsigned long long ulHandle, ArcStatus_t* pStatus );

	/** Returns the last reported error message.
	 *  @return The last error message.
	 */
	GEN3_CARCFITSFILE_API const char* ArcFitsFile_getLastError( void );


#ifdef __cplusplus
}
#endif


#endif		// _ARC_GEN3_FITSFILE_CAPI_H_








//#ifndef _ARC_FITSFILE_CAPI_H_
//#define _ARC_FITSFILE_CAPI_H_
//
//#include <fitsio.h>
//
//#include <CArcFitsFileDllMain.h>
//
//
//// +------------------------------------------------------------------------------------+
//// | Status/Error constants
//// +------------------------------------------------------------------------------------+
//#ifndef ARC_STATUS
//#define ARC_STATUS
//
//	#define ARC_STATUS_OK			0
//	#define ARC_STATUS_ERROR		1
//	#define ARC_ERROR_MSG_SIZE		128
//
//#endif
//
//
//#ifdef __cplusplus
//   extern "C" {		// Using a C++ compiler
//#endif
//
//// +----------------------------------------------------------------------------------+
//// |  C Interface - CArcFitsFile Constants                                            |
//// +----------------------------------------------------------------------------------+
//DLLFITSFILE_API extern const int FITS_READMODE;
//DLLFITSFILE_API extern const int FITS_READWRITEMODE;
//
//DLLFITSFILE_API extern const int FITS_BPP16;
//DLLFITSFILE_API extern const int FITS_BPP32;
//
//DLLFITSFILE_API extern const int FITS_NAXES_COL;
//DLLFITSFILE_API extern const int FITS_NAXES_ROW;
//DLLFITSFILE_API extern const int FITS_NAXES_NOF;
//DLLFITSFILE_API extern const int FITS_NAXES_SIZE;
//
//DLLFITSFILE_API extern const int FITS_STRING_KEY;
//DLLFITSFILE_API extern const int FITS_INTEGER_KEY;
//DLLFITSFILE_API extern const int FITS_DOUBLE_KEY;
//DLLFITSFILE_API extern const int FITS_LOGICAL_KEY;
//DLLFITSFILE_API extern const int FITS_COMMENT_KEY;
//DLLFITSFILE_API extern const int FITS_HISTORY_KEY;
//DLLFITSFILE_API extern const int FITS_DATE_KEY;
//
//
//// +----------------------------------------------------------------------------------+
//// |  C Interface - Deinterlace Functions                                             |
//// +----------------------------------------------------------------------------------+
//DLLFITSFILE_API void ArcFitsFile_Create( const char* pszFilename, int dRows, int dCols, int dBpp, int dIs3D, int* pStatus );
//DLLFITSFILE_API void ArcFitsFile_Open( const char* pszFilename, int dMode, int* pStatus );
//DLLFITSFILE_API	void ArcFitsFile_Close();
//
//DLLFITSFILE_API const char*  ArcFitsFile_GetFilename( int* pStatus );
//DLLFITSFILE_API const char** ArcFitsFile_GetHeader( int* pKeyCount, int* pStatus );
//DLLFITSFILE_API void         ArcFitsFile_FreeHeader();
//
//DLLFITSFILE_API void ArcFitsFile_WriteKeyword( char* szKey, void* pKeyVal, int dValType, char* szComment, int* pStatus );
//DLLFITSFILE_API void ArcFitsFile_UpdateKeyword( char* szKey, void* pKeyVal, int dValType, char* szComment, int* pStatus );
//DLLFITSFILE_API void ArcFitsFile_GetParameters( long* pNaxes, int* pNaxis, int* pBitsPerPixel, int* pStatus );
//DLLFITSFILE_API long ArcFitsFile_GetNumberOfFrames( int* pStatus );
//DLLFITSFILE_API long ArcFitsFile_GetRows( int* pStatus );
//DLLFITSFILE_API long ArcFitsFile_GetCols( int* pStatus );
//DLLFITSFILE_API int	 ArcFitsFile_GetNAxis( int* pStatus );
//DLLFITSFILE_API int	 ArcFitsFile_GetBpp( int* pStatus );
//DLLFITSFILE_API void ArcFitsFile_GenerateTestData( int* pStatus );
//DLLFITSFILE_API void ArcFitsFile_ReOpen( int* pStatus );
//
//DLLFITSFILE_API void  ArcFitsFile_Resize( int dRows, int dCols, int* pStatus );
//DLLFITSFILE_API void  ArcFitsFile_Write( void* pData, int* pStatus );
//DLLFITSFILE_API void  ArcFitsFile_WriteToOffset( void* pData, unsigned int bytesToWrite, int fPixl, int* pStatus );
//DLLFITSFILE_API void  ArcFitsFile_WriteSubImage( void* pData, int llrow, int llcol, int urrow, int urcol, int* pStatus );
//DLLFITSFILE_API void* ArcFitsFile_ReadSubImage( int llrow, int llcol, int urrow, int urcol, int* pStatus );
//DLLFITSFILE_API void* ArcFitsFile_Read( int* pStatus );
//
//DLLFITSFILE_API void  ArcFitsFile_Write3D( void* pData, int* pStatus );
//DLLFITSFILE_API void  ArcFitsFile_ReWrite3D( void* pData, int dImageNumber, int* pStatus );
//DLLFITSFILE_API void* ArcFitsFile_Read3D( int dImageNumber, int* pStatus );
//
//DLLFITSFILE_API fitsfile* ArcFitsFile_GetBaseFile( int* pStatus );
//
//DLLFITSFILE_API const char* ArcFitsFile_GetLastError();
//
//#ifdef __cplusplus
//   }
//#endif
//
//#endif		// _ARC_FITSFILE_CAPI_H_
