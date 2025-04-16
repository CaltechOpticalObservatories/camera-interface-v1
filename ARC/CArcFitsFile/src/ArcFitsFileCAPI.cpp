// +------------------------------------------------------------------------------------------------------------------+
// |  FILE:  ArcFitsFileCAPI.cpp  ( Gen3 )                                                                            |
// +------------------------------------------------------------------------------------------------------------------+
// |  PURPOSE: This file implements a C interface for all the CArcFitsFile class methods.                             |
// |                                                                                                                  |
// |  AUTHOR:  Scott Streit			DATE: March 25, 2020                                                              |
// |                                                                                                                  |
// |  Copyright 2013 Astronomical Research Cameras, Inc. All rights reserved.                                         |
// +------------------------------------------------------------------------------------------------------------------+

#include <cstdint>
#include <cstring>
#include <cstdio>
#include <memory>

#include <ArcFitsFileCAPI.h>
#include <CArcFitsFile.h>



// +------------------------------------------------------------------------------------------------------------------+
// | Define status constants                                                                                          |
// +------------------------------------------------------------------------------------------------------------------+
const ArcError_t* ARC_STATUS_NONE = ( const ArcError_t* )NULL;
const ArcError_t  ARC_STATUS_OK = 1;
const ArcError_t  ARC_STATUS_ERROR = 2;

const std::uint32_t ARC_MSG_SIZE = 64;
const std::uint32_t ARC_ERROR_MSG_SIZE = 256;


// +------------------------------------------------------------------------------------------------------------------+
// | Initialize status pointer macro                                                                                  |
// +------------------------------------------------------------------------------------------------------------------+
#define INIT_STATUS( status, value )			if ( status != nullptr )						\
												{												\
													*status = value;							\
												}


// +------------------------------------------------------------------------------------------------------------------+
// | Set the status pointer and associated message macro                                                              |
// +------------------------------------------------------------------------------------------------------------------+
#define SET_ERROR_STATUS( status, except )	if ( status != nullptr && g_pErrMsg != nullptr )		\
											{														\
												*status = ARC_STATUS_ERROR;							\
												g_pErrMsg->assign( except.what() );					\
											}


#define VERIFY_INSTANCE_HANDLE( handle )	if ( handle == 0 ||															\
											   ( handle != reinterpret_cast< std::uint64_t >( g_pFits16.get() ) &&		\
												 handle != reinterpret_cast< std::uint64_t >( g_pFits32.get() ) ) )		\
											{																			\
												THROW( "Invalid FITS file handle: 0x%X", ulHandle );					\
											}


// +------------------------------------------------------------------------------------------------------------------+
// |  Define explicit FITS file objects                                                                               |
// +------------------------------------------------------------------------------------------------------------------+
//std::unique_ptr<arc::gen3::CArcFitsFile<arc::gen3::fits::BPP_16>> g_pFits16( new arc::gen3::CArcFitsFile<arc::gen3::fits::BPP_16>() );
//std::unique_ptr<arc::gen3::CArcFitsFile<arc::gen3::fits::BPP_32>> g_pFits32( new arc::gen3::CArcFitsFile<arc::gen3::fits::BPP_32>() );
std::unique_ptr<arc::gen3::CArcFitsFile<arc::gen3::fits::BPP_16>> g_pFits16( nullptr );
std::unique_ptr<arc::gen3::CArcFitsFile<arc::gen3::fits::BPP_32>> g_pFits32( nullptr );


// +------------------------------------------------------------------------------------------------------------------+
// | Define message buffers                                                                                           |
// +------------------------------------------------------------------------------------------------------------------+
std::unique_ptr<std::string>	g_pErrMsg( new std::string(), std::default_delete<std::string>() );
std::unique_ptr<char[]>			g_pVerBuf( new char[ ARC_MSG_SIZE ], std::default_delete<char[]>() );


// +------------------------------------------------------------------------------------------------------------------+
// | Define constants                                                                                                 |
// +------------------------------------------------------------------------------------------------------------------+
const unsigned int FITS_BPP16 = sizeof( arc::gen3::fits::BPP_16 ); // 16;
const unsigned int FITS_BPP32 = sizeof( arc::gen3::fits::BPP_32 ); // 32;

const int FITS_STRING_KEY = static_cast< int >( arc::gen3::fits::e_Type::FITS_STRING_KEY );
const int FITS_INT_KEY = static_cast< int >( arc::gen3::fits::e_Type::FITS_INT_KEY );
const int FITS_UINT_KEY = static_cast< int >( arc::gen3::fits::e_Type::FITS_UINT_KEY );
const int FITS_SHORT_KEY = static_cast< int >( arc::gen3::fits::e_Type::FITS_SHORT_KEY );
const int FITS_USHORT_KEY = static_cast< int >( arc::gen3::fits::e_Type::FITS_USHORT_KEY );
const int FITS_FLOAT_KEY = static_cast< int >( arc::gen3::fits::e_Type::FITS_FLOAT_KEY );
const int FITS_DOUBLE_KEY = static_cast< int >( arc::gen3::fits::e_Type::FITS_DOUBLE_KEY );
const int FITS_BYTE_KEY = static_cast< int >( arc::gen3::fits::e_Type::FITS_BYTE_KEY );
const int FITS_LONG_KEY = static_cast< int >( arc::gen3::fits::e_Type::FITS_LONG_KEY );
const int FITS_ULONG_KEY = static_cast< int >( arc::gen3::fits::e_Type::FITS_ULONG_KEY );
const int FITS_LONGLONG_KEY = static_cast< int >( arc::gen3::fits::e_Type::FITS_LONGLONG_KEY );
const int FITS_LOGICAL_KEY = static_cast< int >( arc::gen3::fits::e_Type::FITS_LOGICAL_KEY );
const int FITS_COMMENT_KEY = static_cast< int >( arc::gen3::fits::e_Type::FITS_COMMENT_KEY );
const int FITS_HISTORY_KEY = static_cast< int >( arc::gen3::fits::e_Type::FITS_HISTORY_KEY );
const int FITS_DATE_KEY = static_cast< int >( arc::gen3::fits::e_Type::FITS_DATE_KEY );


// +------------------------------------------------------------------------------------------------------------------+
// | Define FITS file modes                                                                                           |
// +------------------------------------------------------------------------------------------------------------------+
const unsigned int FITS_READMODE = static_cast< unsigned int >( arc::gen3::fits::e_ReadMode::READMODE );
const unsigned int FITS_READWRITEMODE = static_cast< unsigned int >( arc::gen3::fits::e_ReadMode::READWRITEMODE );


// +------------------------------------------------------------------------------------+
// | Globals                                                                            |
// +------------------------------------------------------------------------------------+
static std::string	g_sFileName;

static char** g_pHeader = nullptr;
static int			g_iHeaderCount = 0;



// +------------------------------------------------------------------------------------------------------------------+
// |  ArcFitsFile_getInstance                                                                                         |
// +------------------------------------------------------------------------------------------------------------------+
// |  Returns a handle to the FITS file object appropriate for the specified bits-per-pixel.                          |
// |                                                                                                                  |
// |  <IN>  -> uiBpp	- The number of bits-per-pixel in the image.                                                  |
// |  <OUT> -> pStatus	- Success state; equals ARC_STATUS_OK or ARC_STATUS_ERROR.                                    |
// +------------------------------------------------------------------------------------------------------------------+
GEN3_CARCFITSFILE_API unsigned long long ArcFitsFile_getInstance( unsigned int uiBpp, ArcStatus_t* pStatus )
{
	unsigned long long ulHandle = 0;

	INIT_STATUS( pStatus, ARC_STATUS_OK )

		try
	{
		if ( uiBpp != FITS_BPP16 && uiBpp != FITS_BPP32 )
		{
			THROW( "Invalid bits-per-pixel setting [ %d ]. Must be FITS_BPP16 or FITS_BPP32.", uiBpp );
		}

		if ( uiBpp == FITS_BPP16 )
		{
			g_pFits16.reset( new arc::gen3::CArcFitsFile<arc::gen3::fits::BPP_16>() );

			if ( g_pFits16.get() != nullptr )
			{
				ulHandle = reinterpret_cast< std::uint64_t >( g_pFits16.get() );
			}
		}

		else if ( uiBpp == FITS_BPP32 )
		{
			g_pFits32.reset( new arc::gen3::CArcFitsFile<arc::gen3::fits::BPP_32>() );

			if ( g_pFits32.get() != nullptr )
			{
				ulHandle = reinterpret_cast< std::uint64_t >( g_pFits32.get() );
			}
		}
	}
	catch ( const std::exception& e )
	{
		SET_ERROR_STATUS( pStatus, e );
	}

	return ulHandle;
}


// +------------------------------------------------------------------------------------------------------------------+
// |  ArcFitsFile_version                                                                                             |
// +------------------------------------------------------------------------------------------------------------------+
// |  Returns a textual representation of the class version. See CArcFitsFile::version() for details.                 |
// |                                                                                                                  |
// |  <OUT> -> pStatus : Success state; equals ARC_STATUS_OK or ARC_STATUS_ERROR.                                     |
// +------------------------------------------------------------------------------------------------------------------+
GEN3_CARCFITSFILE_API const char* ArcFitsFile_version( ArcStatus_t* pStatus )
{
	INIT_STATUS( pStatus, ARC_STATUS_OK )

		memset( g_pVerBuf.get(), 0, ARC_MSG_SIZE );

	try
	{
#ifdef _WINDOWS
		sprintf_s( g_pVerBuf.get(), ARC_MSG_SIZE, "%s", arc::gen3::CArcFitsFile<>::version().c_str() );
#else
		snprintf( g_pVerBuf.get(), ARC_MSG_SIZE, "%s", arc::gen3::CArcFitsFile<>::version().c_str() );
#endif
	}
	catch ( std::exception& e )
	{
		SET_ERROR_STATUS( pStatus, e );
	}

	return const_cast< const char* >( g_pVerBuf.get() );
}


// +----------------------------------------------------------------------------------------------------------+
// |  ArcFitsFile_create                                                                                      |
// +----------------------------------------------------------------------------------------------------------+
// |  Creates a new single image file on disk with the specified image dimensions.                            |
// |                                                                                                          |
// |  <IN>  -> ulHandle	   - A reference to a FITS file object.                                               |
// |  <IN>  -> pszFileName - The new file name.                                                               |
// |  <IN>  -> uiCols	   - The image column size ( in pixels ).                                             |
// |  <IN>  -> uiRows	   - The image row size ( in pixels ).                                                |
// |  <OUT> -> pStatus	   - Success state; equals ARC_STATUS_OK or ARC_STATUS_ERROR.                         |
// +----------------------------------------------------------------------------------------------------------+
GEN3_CARCFITSFILE_API void
ArcFitsFile_create( unsigned long long ulHandle, const char* pszFileName, unsigned int uiCols, unsigned int uiRows, ArcStatus_t* pStatus )
{
	INIT_STATUS( pStatus, ARC_STATUS_OK )

		try
	{
		VERIFY_INSTANCE_HANDLE( ulHandle )

			if ( ulHandle == reinterpret_cast< std::uint64_t >( g_pFits16.get() ) )
			{
				g_pFits16->create( std::string( pszFileName ), uiCols, uiRows );
			}

			else
			{
				g_pFits32->create( std::string( pszFileName ), uiCols, uiRows );
			}
	}
	catch ( std::exception& e )
	{
		SET_ERROR_STATUS( pStatus, e );
	}
}


// +----------------------------------------------------------------------------------------------------------+
// |  ArcFitsFile_create3D                                                                                    |
// +----------------------------------------------------------------------------------------------------------+
// |  Creates a new data cube file on disk with the specified image dimensions.                               |
// |                                                                                                          |
// |  <IN>  -> ulHandle	   - A reference to a FITS file object.                                               |
// |  <IN>  -> pszFileName - The new file name.                                                               |
// |  <IN>  -> uiCols	   - The image column size ( in pixels ).                                             |
// |  <IN>  -> uiRows	   - The image row size ( in pixels ).                                                |
// |  <OUT> -> pStatus	   - Success state; equals ARC_STATUS_OK or ARC_STATUS_ERROR.                         |
// +----------------------------------------------------------------------------------------------------------+
GEN3_CARCFITSFILE_API
void ArcFitsFile_create3D( unsigned long long ulHandle, const char* pszFileName, unsigned int uiCols, unsigned int uiRows, ArcStatus_t* pStatus )
{
	INIT_STATUS( pStatus, ARC_STATUS_OK )

		try
	{
		VERIFY_INSTANCE_HANDLE( ulHandle )

			if ( ulHandle == reinterpret_cast< std::uint64_t >( g_pFits16.get() ) )
			{
				g_pFits16->create3D( std::string( pszFileName ), uiCols, uiRows );
			}

			else
			{
				g_pFits32->create3D( std::string( pszFileName ), uiCols, uiRows );
			}
	}
	catch ( std::exception& e )
	{
		SET_ERROR_STATUS( pStatus, e );
	}
}


// +----------------------------------------------------------------------------------------------------------+
// |  ArcFitsFile_open                                                                                        |
// +----------------------------------------------------------------------------------------------------------+
// |  Opens an existing file. Can be used to open a file containing a single image or data cube ( a file with |
// |  multiple image planes ).                                                                                |
// |                                                                                                          |
// |  <IN>  -> ulHandle	   - A reference to a FITS file object.                                               |
// |  <IN>  -> pszFileName - The file name.                                                                   |
// |  <IN>  -> uiReadMode  - The mode ( FITS_READMODE or FITS_READWRITEMODE ) with which to open the file.    |
// |  <OUT> -> pStatus	   - Success state; equals ARC_STATUS_OK or ARC_STATUS_ERROR.                         |
// +----------------------------------------------------------------------------------------------------------+
GEN3_CARCFITSFILE_API void ArcFitsFile_open( unsigned long long ulHandle, const char* pszFileName, unsigned int uiReadMode, ArcStatus_t* pStatus )
{
	INIT_STATUS( pStatus, ARC_STATUS_OK )

		try
	{
		VERIFY_INSTANCE_HANDLE( ulHandle )

			if ( ulHandle == reinterpret_cast< std::uint64_t >( g_pFits16.get() ) )
			{
				g_pFits16->open( std::string( pszFileName ), static_cast< arc::gen3::fits::e_ReadMode >( uiReadMode ) );
			}

			else
			{
				g_pFits32->open( std::string( pszFileName ), static_cast< arc::gen3::fits::e_ReadMode >( uiReadMode ) );
			}
	}
	catch ( std::exception& e )
	{
		SET_ERROR_STATUS( pStatus, e );
	}
}


// +----------------------------------------------------------------------------------------------------------+
// |  ArcFitsFile_close                                                                                       |
// +----------------------------------------------------------------------------------------------------------+
// |  Closes the file. All subsequent methods, except for create and open will result in an error and an      |
// |  exception will be thrown.                                                                               |
// |                                                                                                          |
// |  <IN>  -> ulHandle - A reference to a FITS file object.                                                  |
// |  <OUT> -> pStatus	- Success state; equals ARC_STATUS_OK or ARC_STATUS_ERROR.                            |
// +----------------------------------------------------------------------------------------------------------+
GEN3_CARCFITSFILE_API void ArcFitsFile_close( unsigned long long ulHandle, ArcStatus_t* pStatus )
{
	INIT_STATUS( pStatus, ARC_STATUS_OK )

		try
	{
		VERIFY_INSTANCE_HANDLE( ulHandle )

			if ( ulHandle == reinterpret_cast< std::uint64_t >( g_pFits16.get() ) )
			{
				g_pFits16->close();
			}

			else
			{
				g_pFits32->close();
			}
	}
	catch ( std::exception& e )
	{
		SET_ERROR_STATUS( pStatus, e );
	}
}


// +----------------------------------------------------------------------------------------------------------+
// |  ArcFitsFile_getHeader                                                                                   |
// +----------------------------------------------------------------------------------------------------------+
// |  Returns the FITS header as a list of strings.                                                           |
// |                                                                                                          |
// |  <IN>  -> ulHandle - A reference to a FITS file object.                                                  |
// |  <OUT> -> uiCount	- Returns the number of strings in the returned list.                                 |
// |  <OUT> -> pStatus	- Success state; equals ARC_STATUS_OK or ARC_STATUS_ERROR.                            |
// +----------------------------------------------------------------------------------------------------------+
GEN3_CARCFITSFILE_API const char** ArcFitsFile_getHeader( unsigned long long ulHandle, unsigned int* uiCount, ArcStatus_t* pStatus )
{
	INIT_STATUS( pStatus, ARC_STATUS_OK )

		std::unique_ptr<arc::gen3::CArcStringList> pHeader( nullptr );

	try
	{
		VERIFY_INSTANCE_HANDLE( ulHandle )

			if ( ulHandle == reinterpret_cast< std::uint64_t >( g_pFits16.get() ) )
			{
				pHeader = g_pFits16->getHeader();
			}

			else
			{
				pHeader = g_pFits32->getHeader();
			}

		*uiCount = pHeader->length();

		g_pHeader = new char* [ *uiCount ];

		if ( g_pHeader != nullptr )
		{
			for ( decltype( pHeader->length() ) i = 0; i < *uiCount; i++ )
			{
				g_pHeader[ i ] = new char[ 100 ];

				if ( g_pHeader[ i ] != nullptr )
				{
					std::snprintf( &g_pHeader[ i ][ 0 ], 100, "%s", pHeader->at( i ).c_str() );
				}
			}
		}
	}
	catch ( std::exception& e )
	{
		SET_ERROR_STATUS( pStatus, e );
	}

	return const_cast< const char** >( g_pHeader );
}


// +----------------------------------------------------------------------------------------------------------+
// |  ArcFitsFile_freeHeader                                                                                  |
// +----------------------------------------------------------------------------------------------------------+
// |  Frees the FITS header as returned by ArcFitsFile_getHeader().                                           |
// |                                                                                                          |
// |  <IN>  -> ulHandle - A reference to a FITS file object.                                                  |
// |  <OUT> -> pStatus	- Success state; equals ARC_STATUS_OK or ARC_STATUS_ERROR.                            |
// +----------------------------------------------------------------------------------------------------------+
GEN3_CARCFITSFILE_API void ArcFitsFile_freeHeader( unsigned long long ulHandle, ArcStatus_t* pStatus )
{
	INIT_STATUS( pStatus, ARC_STATUS_OK )

		try
	{
		if ( g_pHeader != nullptr )
		{
			for ( auto i = 0; i < g_iHeaderCount; i++ )
			{
				delete[] g_pHeader[ i ];
			}

			delete[] g_pHeader;
		}

		g_iHeaderCount = 0;
	}
	catch ( std::exception& e )
	{
		SET_ERROR_STATUS( pStatus, e );
	}
}


// +----------------------------------------------------------------------------------------------------------+
// |  ArcFitsFile_getHeader                                                                                   |
// +----------------------------------------------------------------------------------------------------------+
// |  Returns the file name.                                                                                  |
// |                                                                                                          |
// |  <IN>  -> ulHandle - A reference to a FITS file object.                                                  |
// |  <OUT> -> pStatus	- Success state; equals ARC_STATUS_OK or ARC_STATUS_ERROR.                            |
// +----------------------------------------------------------------------------------------------------------+
GEN3_CARCFITSFILE_API const char* ArcFitsFile_getFileName( unsigned long long ulHandle, ArcStatus_t* pStatus )
{
	INIT_STATUS( pStatus, ARC_STATUS_OK )

		g_sFileName = "";

	try
	{
		VERIFY_INSTANCE_HANDLE( ulHandle )

			if ( ulHandle == reinterpret_cast< std::uint64_t >( g_pFits16.get() ) )
			{
				g_sFileName = g_pFits16->getFileName();
			}

			else
			{
				g_sFileName = g_pFits32->getFileName();
			}
	}
	catch ( std::exception& e )
	{
		SET_ERROR_STATUS( pStatus, e );
	}

	return g_sFileName.c_str();
}


// +----------------------------------------------------------------------------------------------------------+
// |  ArcFitsFile_writeKeyword                                                                                |
// +----------------------------------------------------------------------------------------------------------+
// |  Writes a FITS keyword to an existing FITS file.  The keyword must be valid or an error will occur. For  |
// |  a list of valid FITS keywords, see:                                                                     |
// |                                                                                                          |
// |  http://heasarc.gsfc.nasa.gov/docs/fcg/standard_dict.html                                                |
// |  http://archive.stsci.edu/fits/fits_standard/node38.html# \                                              |
// |  SECTION00940000000000000000                                                                             |
// |                                                                                                          |
// |  'HIERARCH' keyword NOTE: This text will be prefixed to any keyword by                                   |
// |                           the cfitsio library if the keyword is greater                                  |
// |                           than 8 characters, which is the standard FITS                                  |
// |                           keyword length. See the link below for details:                                |
// |   http://heasarc.gsfc.nasa.gov/docs/software/fitsio/c/f_user/node28.html                                 |
// |                                                                                                          |
// |   HIERARCH examples:                                                                                     |
// |   -----------------                                                                                      |
// |   HIERARCH LongKeyword = 47.5 / Keyword has > 8 characters & mixed case                                  |
// |   HIERARCH XTE$TEMP = 98.6 / Keyword contains the '$' character                                          |
// |   HIERARCH Earth is a star = F / Keyword contains embedded spaces                                        |                               
// |                                                                                                          |
// |  <IN>  -> ulHandle   - A reference to a FITS file object.                                                |
// |  <IN>  -> pszKey     - The header keyword. Can be NULL. Ex: SIMPLE                                       |
// |  <IN>  -> pKeyVal    - The value associated with the key. Ex: T                                          |
// |  <IN>  -> uiValType  - The keyword type, as defined in ArcFitsFileCAPI.h                                 |
// |  <IN>  -> pszComment - The comment to attach to the keyword.  Can be NULL                                |
// |  <OUT> -> pStatus	  - Success state; equals ARC_STATUS_OK or ARC_STATUS_ERROR.                          |
// +----------------------------------------------------------------------------------------------------------+
GEN3_CARCFITSFILE_API void ArcFitsFile_writeKeyword( unsigned long long ulHandle, const char* pszKey, void* pKeyVal,
	unsigned int uiValType, const char* pszComment, ArcStatus_t* pStatus )
{
	INIT_STATUS( pStatus, ARC_STATUS_OK )

		try
	{
		VERIFY_INSTANCE_HANDLE( ulHandle )

			if ( ulHandle == reinterpret_cast< std::uint64_t >( g_pFits16.get() ) )
			{
				g_pFits16->writeKeyword( ( pszKey != nullptr ? std::string( pszKey ) : std::string( "" ) ),
					pKeyVal,
					static_cast< arc::gen3::fits::e_Type >( uiValType ),
					( pszComment != NULL ? std::string( pszComment ) : std::string( " " ) ) );
			}

			else
			{
				g_pFits32->writeKeyword( ( pszKey != nullptr ? std::string( pszKey ) : std::string( "" ) ),
					pKeyVal,
					static_cast< arc::gen3::fits::e_Type >( uiValType ),
					( pszComment != NULL ? std::string( pszComment ) : std::string( " " ) ) );
			}
	}
	catch ( std::exception& e )
	{
		SET_ERROR_STATUS( pStatus, e );
	}
}


// +----------------------------------------------------------------------------------------------------------+
// |  ArcFitsFile_updateKeyword                                                                               |
// +----------------------------------------------------------------------------------------------------------+
// |  Updates an existing FITS keyword to an existing FITS file.  The keyword must be valid or an error will  |
// |  occur. For list of valid FITS keywords, see:                                                            |
// |                                                                                                          |
// |  http://heasarc.gsfc.nasa.gov/docs/fcg/standard_dict.html                                                |
// |  http://archive.stsci.edu/fits/fits_standard/node38.html# \                                              |
// |  SECTION00940000000000000000                                                                             |
// |                                                                                                          |
// |  'HIERARCH' keyword NOTE: This text will be prefixed to any keyword by                                   |
// |                           the cfitsio library if the keyword is greater                                  |
// |                           than 8 characters, which is the standard FITS                                  |
// |                           keyword length. See the link below for details:                                |
// |   http://heasarc.gsfc.nasa.gov/docs/software/fitsio/c/f_user/node28.html                                 |
// |                                                                                                          |
// |   HIERARCH examples:                                                                                     |
// |   -----------------                                                                                      |
// |   HIERARCH LongKeyword = 47.5 / Keyword has > 8 characters & mixed case                                  |
// |   HIERARCH XTE$TEMP = 98.6 / Keyword contains the '$' character                                          |
// |   HIERARCH Earth is a star = F / Keyword contains embedded spaces                                        |
// |                                                                                                          |
// |  <IN>  -> ulHandle   - A reference to a FITS file object.                                                |
// |  <IN>  -> pszKey     - The header keyword. Can be NULL. Ex: SIMPLE                                       |
// |  <IN>  -> pKeyVal    - The value associated with the key. Ex: T                                          |
// |  <IN>  -> uiValType  - The keyword type, as defined in ArcFitsFileCAPI.h                                 |
// |  <IN>  -> pszComment - The comment to attach to the keyword.  Can be NULL.                               |
// |  <OUT> -> pStatus	  - Success state; equals ARC_STATUS_OK or ARC_STATUS_ERROR.                          |
// +----------------------------------------------------------------------------------------------------------+
GEN3_CARCFITSFILE_API void ArcFitsFile_updateKeyword( unsigned long long ulHandle, const char* pszKey, void* pKeyVal,
	unsigned int uiValType, const char* pszComment, ArcStatus_t* pStatus )
{
	INIT_STATUS( pStatus, ARC_STATUS_OK )

		try
	{
		VERIFY_INSTANCE_HANDLE( ulHandle )

			if ( ulHandle == reinterpret_cast< std::uint64_t >( g_pFits16.get() ) )
			{
				g_pFits16->updateKeyword( ( pszKey != nullptr ? std::string( pszKey ) : std::string( "" ) ),
					pKeyVal,
					static_cast< arc::gen3::fits::e_Type >( uiValType ),
					( pszComment != NULL ? std::string( pszComment ) : std::string( " " ) ) );
			}

			else
			{
				g_pFits32->updateKeyword( ( pszKey != nullptr ? std::string( pszKey ) : std::string( "" ) ),
					pKeyVal,
					static_cast< arc::gen3::fits::e_Type >( uiValType ),
					( pszComment != NULL ? std::string( pszComment ) : std::string( " " ) ) );
			}
	}
	catch ( std::exception& e )
	{
		SET_ERROR_STATUS( pStatus, e );
	}
}


// +----------------------------------------------------------------------------------------------------------+
// |  ArcFitsFile_getParameters                                                                               |
// +----------------------------------------------------------------------------------------------------------+
// |  Returns the basic image parameters from the FITS header ( number of cols, rows, frames, dimensions      |
// |  and bits-per-pixel ).                                                                                   |
// |                                                                                                          |
// |  <IN>  -> ulHandle - A reference to a FITS file object.                                                  |
// |  <IN>  -> plNaxes  - Returns the dimension of each axis. Must be 3. ( 0 = cols, 1 = rows, 2 = frames ).  |
// |  <IN>  -> piNaxis  - Returns the number of axes in the file( 2 = normal image, 3 = data cube ).          |
// |  <IN>  -> piBpp    - The bits - per - pixel.                                                             |
// |  <OUT> -> pStatus  - Success state; equals ARC_STATUS_OK or ARC_STATUS_ERROR.                            |
// +----------------------------------------------------------------------------------------------------------+
GEN3_CARCFITSFILE_API void ArcFitsFile_getParameters( unsigned long long ulHandle, long* pNaxes, int* pNaxis,
	int* pBpp, ArcStatus_t* pStatus )
{
	INIT_STATUS( pStatus, ARC_STATUS_OK )

		std::unique_ptr<arc::gen3::fits::CParam> pParam( nullptr );

	try
	{
		VERIFY_INSTANCE_HANDLE( ulHandle )

			if ( ulHandle == reinterpret_cast< std::uint64_t >( g_pFits16.get() ) )
			{
				pParam = g_pFits16->getParameters();
			}

			else
			{
				pParam = g_pFits32->getParameters();
			}

		if ( pParam != nullptr )
		{
			if ( pNaxes != nullptr )
			{
				pNaxes[ 0 ] = pParam->getCols();
				pNaxes[ 1 ] = pParam->getRows();
				pNaxes[ 2 ] = pParam->getFrames();
			}

			if ( pNaxis != nullptr )
			{
				*pNaxis = pParam->getNAxis();
			}

			if ( pBpp != nullptr )
			{
				*pBpp = pParam->getBpp();
			}
		}
	}
	catch ( std::exception& e )
	{
		SET_ERROR_STATUS( pStatus, e );
	}
}


// +----------------------------------------------------------------------------------------------------------+
// |  ArcFitsFile_getNumberOfFrames                                                                           |
// +----------------------------------------------------------------------------------------------------------+
// |  Returns the number of frames.  A single image file will return a value of 0.                            |
// |                                                                                                          |
// |  <IN>  -> ulHandle - A reference to a FITS file object.                                                  |
// |  <OUT> -> pStatus  - Success state; equals ARC_STATUS_OK or ARC_STATUS_ERROR.                            |
// +----------------------------------------------------------------------------------------------------------+
GEN3_CARCFITSFILE_API unsigned int ArcFitsFile_getNumberOfFrames( unsigned long long ulHandle, ArcStatus_t* pStatus )
{
	INIT_STATUS( pStatus, ARC_STATUS_OK )

		unsigned int uiFrames = 0;

	try
	{
		VERIFY_INSTANCE_HANDLE( ulHandle )

			if ( ulHandle == reinterpret_cast< std::uint64_t >( g_pFits16.get() ) )
			{
				uiFrames = g_pFits16->getNumberOfFrames();
			}

			else
			{
				uiFrames = g_pFits32->getNumberOfFrames();
			}
	}
	catch ( std::exception& e )
	{
		SET_ERROR_STATUS( pStatus, e );
	}

	return uiFrames;
}


// +----------------------------------------------------------------------------------------------------------+
// |  ArcFitsFile_getRows                                                                                     |
// +----------------------------------------------------------------------------------------------------------+
// |  Returns the number of rows in the image.                                                                |
// |                                                                                                          |
// |  <IN>  -> ulHandle - A reference to a FITS file object.                                                  |
// |  <OUT> -> pStatus  - Success state; equals ARC_STATUS_OK or ARC_STATUS_ERROR.                            |
// +----------------------------------------------------------------------------------------------------------+
GEN3_CARCFITSFILE_API unsigned int ArcFitsFile_getRows( unsigned long long ulHandle, ArcStatus_t* pStatus )
{
	INIT_STATUS( pStatus, ARC_STATUS_OK )

		unsigned int uiRows = 0;

	try
	{
		VERIFY_INSTANCE_HANDLE( ulHandle )

			if ( ulHandle == reinterpret_cast< std::uint64_t >( g_pFits16.get() ) )
			{
				uiRows = g_pFits16->getRows();
			}

			else
			{
				uiRows = g_pFits32->getRows();
			}
	}
	catch ( std::exception& e )
	{
		SET_ERROR_STATUS( pStatus, e );
	}

	return uiRows;
}


// +----------------------------------------------------------------------------------------------------------+
// |  ArcFitsFile_getCols                                                                                     |
// +----------------------------------------------------------------------------------------------------------+
// |  Returns the number of columns in the image.                                                             |
// |                                                                                                          |
// |  <IN>  -> ulHandle - A reference to a FITS file object.                                                  |
// |  <OUT> -> pStatus  - Success state; equals ARC_STATUS_OK or ARC_STATUS_ERROR.                            |
// +----------------------------------------------------------------------------------------------------------+
GEN3_CARCFITSFILE_API unsigned int ArcFitsFile_getCols( unsigned long long ulHandle, ArcStatus_t* pStatus )
{
	INIT_STATUS( pStatus, ARC_STATUS_OK )

		unsigned int uiCols = 0;

	try
	{
		VERIFY_INSTANCE_HANDLE( ulHandle )

			if ( ulHandle == reinterpret_cast< std::uint64_t >( g_pFits16.get() ) )
			{
				uiCols = g_pFits16->getCols();
			}

			else
			{
				uiCols = g_pFits32->getCols();
			}
	}
	catch ( std::exception& e )
	{
		SET_ERROR_STATUS( pStatus, e );
	}

	return uiCols;
}


// +----------------------------------------------------------------------------------------------------------+
// |  ArcFitsFile_getNAxis                                                                                    |
// +----------------------------------------------------------------------------------------------------------+
// |  Returns the number of dimensions in the image.                                                          |
// |                                                                                                          |
// |  <IN>  -> ulHandle - A reference to a FITS file object.                                                  |
// |  <OUT> -> pStatus  - Success state; equals ARC_STATUS_OK or ARC_STATUS_ERROR.                            |
// +----------------------------------------------------------------------------------------------------------+
GEN3_CARCFITSFILE_API unsigned int ArcFitsFile_getNAxis( unsigned long long ulHandle, ArcStatus_t* pStatus )
{
	INIT_STATUS( pStatus, ARC_STATUS_OK )

		unsigned int uiNAxis = 0;

	try
	{
		VERIFY_INSTANCE_HANDLE( ulHandle )

			if ( ulHandle == reinterpret_cast< std::uint64_t >( g_pFits16.get() ) )
			{
				uiNAxis = g_pFits16->getNAxis();
			}

			else
			{
				uiNAxis = g_pFits32->getNAxis();
			}
	}
	catch ( std::exception& e )
	{
		SET_ERROR_STATUS( pStatus, e );
	}

	return uiNAxis;
}


// +----------------------------------------------------------------------------------------------------------+
// |  ArcFitsFile_getBitsPerPixel                                                                             |
// +----------------------------------------------------------------------------------------------------------+
// |  Returns the image bits-per-pixel value.                                                                 |
// |                                                                                                          |
// |  <IN>  -> ulHandle - A reference to a FITS file object.                                                  |
// |  <OUT> -> pStatus  - Success state; equals ARC_STATUS_OK or ARC_STATUS_ERROR.                            |
// +----------------------------------------------------------------------------------------------------------+
GEN3_CARCFITSFILE_API unsigned int ArcFitsFile_getBitsPerPixel( unsigned long long ulHandle, ArcStatus_t* pStatus )
{
	INIT_STATUS( pStatus, ARC_STATUS_OK )

		unsigned int uiBpp = 0;

	try
	{
		VERIFY_INSTANCE_HANDLE( ulHandle )

			if ( ulHandle == reinterpret_cast< std::uint64_t >( g_pFits16.get() ) )
			{
				uiBpp = g_pFits16->getBitsPerPixel();
			}

			else
			{
				uiBpp = g_pFits32->getBitsPerPixel();
			}
	}
	catch ( std::exception& e )
	{
		SET_ERROR_STATUS( pStatus, e );
	}

	return uiBpp;
}


// +----------------------------------------------------------------------------------------------------------+
// |  ArcFitsFile_getBitsPerPixel                                                                             |
// +----------------------------------------------------------------------------------------------------------+
// |  Generates a ramp test pattern image within the file. The size of the image is determined by the image   |
// |  dimensions supplied during the create() method call. This method is only valid for single image files.  |
// |                                                                                                          |
// |  <IN>  -> ulHandle - A reference to a FITS file object.                                                  |
// |  <OUT> -> pStatus  - Success state; equals ARC_STATUS_OK or ARC_STATUS_ERROR.                            |
// +----------------------------------------------------------------------------------------------------------+
GEN3_CARCFITSFILE_API void ArcFitsFile_generateTestData( unsigned long long ulHandle, ArcStatus_t* pStatus )
{
	INIT_STATUS( pStatus, ARC_STATUS_OK )

		try
	{
		VERIFY_INSTANCE_HANDLE( ulHandle )

			if ( ulHandle == reinterpret_cast< std::uint64_t >( g_pFits16.get() ) )
			{
				g_pFits16->generateTestData();
			}

			else
			{
				g_pFits32->generateTestData();
			}
	}
	catch ( std::exception& e )
	{
		SET_ERROR_STATUS( pStatus, e );
	}
}


// +----------------------------------------------------------------------------------------------------------+
// |  ArcFitsFile_reOpen                                                                                      |
// +----------------------------------------------------------------------------------------------------------+
// |  Effectively closes and re-opens the underlying disk file.                                               |
// |                                                                                                          |
// |  <IN>  -> ulHandle - A reference to a FITS file object.                                                  |
// |  <OUT> -> pStatus  - Success state; equals ARC_STATUS_OK or ARC_STATUS_ERROR.                            |
// +----------------------------------------------------------------------------------------------------------+
GEN3_CARCFITSFILE_API void ArcFitsFile_reOpen( unsigned long long ulHandle, ArcStatus_t* pStatus )
{
	INIT_STATUS( pStatus, ARC_STATUS_OK )

		try
	{
		VERIFY_INSTANCE_HANDLE( ulHandle )

			if ( ulHandle == reinterpret_cast< std::uint64_t >( g_pFits16.get() ) )
			{
				g_pFits16->reOpen();
			}

			else
			{
				g_pFits32->reOpen();
			}
	}
	catch ( std::exception& e )
	{
		SET_ERROR_STATUS( pStatus, e );
	}
}


// +----------------------------------------------------------------------------------------------------------+
// |  ArcFitsFile_flush                                                                                       |
// +----------------------------------------------------------------------------------------------------------+
// |  Causes all internal data buffers to write data to the disk file.                                        |
// |                                                                                                          |
// |  <IN>  -> ulHandle - A reference to a FITS file object.                                                  |
// |  <OUT> -> pStatus  - Success state; equals ARC_STATUS_OK or ARC_STATUS_ERROR.                            |
// +----------------------------------------------------------------------------------------------------------+
GEN3_CARCFITSFILE_API void ArcFitsFile_flush( unsigned long long ulHandle, ArcStatus_t* pStatus )
{
	INIT_STATUS( pStatus, ARC_STATUS_OK )

		try
	{
		VERIFY_INSTANCE_HANDLE( ulHandle )

			if ( ulHandle == reinterpret_cast< std::uint64_t >( g_pFits16.get() ) )
			{
				g_pFits16->flush();
			}

			else
			{
				g_pFits32->flush();
			}
	}
	catch ( std::exception& e )
	{
		SET_ERROR_STATUS( pStatus, e );
	}
}


// +----------------------------------------------------------------------------------------------------------+
// |  ArcFitsFile_reSize                                                                                      |
// +----------------------------------------------------------------------------------------------------------+
// |  Resizes a single image file by modifying the NAXES keyword and increasing the image data portion of     |
// |  the file.                                                                                               |
// |                                                                                                          |
// |  <IN>  -> ulHandle - A reference to a FITS file object.                                                  |
// |  <IN>  -> uiCols	- The number of cols the new FITS file will have.                                     |
// |  <IN>  -> uiRows	- The number of rows the new FITS file will have.                                     |
// |  <OUT> -> pStatus  - Success state; equals ARC_STATUS_OK or ARC_STATUS_ERROR.                            |
// +----------------------------------------------------------------------------------------------------------+
GEN3_CARCFITSFILE_API void ArcFitsFile_reSize( unsigned long long ulHandle, unsigned int uiCols, unsigned int uiRows, ArcStatus_t* pStatus )
{
	INIT_STATUS( pStatus, ARC_STATUS_OK )

		try
	{
		VERIFY_INSTANCE_HANDLE( ulHandle )

			if ( ulHandle == reinterpret_cast< std::uint64_t >( g_pFits16.get() ) )
			{
				g_pFits16->reSize( uiCols, uiRows );
			}

			else
			{
				g_pFits32->reSize( uiCols, uiRows );
			}
	}
	catch ( std::exception& e )
	{
		SET_ERROR_STATUS( pStatus, e );
	}
}


// +----------------------------------------------------------------------------------------------------------+
// |  ArcFitsFile_write                                                                                       |
// +----------------------------------------------------------------------------------------------------------+
// |  Writes image data to a single image file.                                                               |
// |                                                                                                          |
// |  <IN>  -> ulHandle - A reference to a FITS file object.                                                  |
// |  <IN>  -> pBuf		- The image buffer to write. Buffer access violation results in undefined behavior.   |
// |  <OUT> -> pStatus  - Success state; equals ARC_STATUS_OK or ARC_STATUS_ERROR.                            |
// +----------------------------------------------------------------------------------------------------------+
GEN3_CARCFITSFILE_API void ArcFitsFile_write( unsigned long long ulHandle, void* pBuf, ArcStatus_t* pStatus )
{
	INIT_STATUS( pStatus, ARC_STATUS_OK )

		try
	{
		VERIFY_INSTANCE_HANDLE( ulHandle )

			if ( ulHandle == reinterpret_cast< std::uint64_t >( g_pFits16.get() ) )
			{
				g_pFits16->write( reinterpret_cast< arc::gen3::fits::BPP_16* >( pBuf ) );
			}

			else
			{
				g_pFits32->write( reinterpret_cast< arc::gen3::fits::BPP_32* >( pBuf ) );
			}
	}
	catch ( std::exception& e )
	{
		SET_ERROR_STATUS( pStatus, e );
	}
}


// +----------------------------------------------------------------------------------------------------------+
// |  ArcFitsFile_writeN                                                                                      |
// +----------------------------------------------------------------------------------------------------------+
// |  Writes image data to a single image file.                                                               |
// |                                                                                                          |
// |  <IN>  -> ulHandle - A reference to a FITS file object.                                                  |
// |  <IN>  -> pBuf		- The image buffer to write. Buffer access violation results in undefined behavior.   |
// |  <IN>  -> i64Bytes	- The number of bytes to write.                                                       |
// |  <IN>  -> i64Pixel	- The start pixel within the file image. ( for default set = 1 ).                     |
// |  <OUT> -> pStatus  - Success state; equals ARC_STATUS_OK or ARC_STATUS_ERROR.                            |
// +----------------------------------------------------------------------------------------------------------+
GEN3_CARCFITSFILE_API void ArcFitsFile_writeN( unsigned long long ulHandle, void* pBuf, unsigned long long i64Bytes,
	unsigned long long i64Pixel, ArcStatus_t* pStatus )
{
	INIT_STATUS( pStatus, ARC_STATUS_OK )

		try
	{
		VERIFY_INSTANCE_HANDLE( ulHandle )

			if ( ulHandle == reinterpret_cast< std::uint64_t >( g_pFits16.get() ) )
			{
				g_pFits16->write( reinterpret_cast< arc::gen3::fits::BPP_16* >( pBuf ), i64Bytes, i64Pixel );
			}

			else
			{
				g_pFits32->write( reinterpret_cast< arc::gen3::fits::BPP_32* >( pBuf ), i64Bytes, i64Pixel );
			}
	}
	catch ( std::exception& e )
	{
		SET_ERROR_STATUS( pStatus, e );
	}
}


// +----------------------------------------------------------------------------------------------------------+
// |  ArcFitsFile_writeSubImage                                                                               |
// +----------------------------------------------------------------------------------------------------------+
// |  Writes a sub-image of the specified buffer to a single image file.                                      |
// |                                                                                                          |
// |  <IN>  -> ulHandle - A reference to a FITS file object.                                                  |
// |  <IN>  -> pBuf		- The image buffer to write. Buffer access violation results in undefined behavior.   |
// |  <IN>  -> llX		- The lower left column of the sub-image.					                          |
// |  <IN>  -> llY		- The lower left row of the sub-image.                                                |
// |  <IN>  -> urX		- The upper right column of the sub-image.                                            |
// |  <IN>  -> urY		- The upper right row of the sub-image.                                               |
// |  <OUT> -> pStatus  - Success state; equals ARC_STATUS_OK or ARC_STATUS_ERROR.                            |
// +----------------------------------------------------------------------------------------------------------+
GEN3_CARCFITSFILE_API void ArcFitsFile_writeSubImage( unsigned long long ulHandle, void* pBuf, long llX, long llY, long urX, long urY, ArcStatus_t* pStatus )
{
	INIT_STATUS( pStatus, ARC_STATUS_OK )

		try
	{
		VERIFY_INSTANCE_HANDLE( ulHandle )

			if ( ulHandle == reinterpret_cast< std::uint64_t >( g_pFits16.get() ) )
			{
				g_pFits16->writeSubImage( reinterpret_cast< arc::gen3::fits::BPP_16* >( pBuf ), MAKE_POINT( llX, llY ), MAKE_POINT( urX, urY ) );
			}

			else
			{
				g_pFits32->writeSubImage( reinterpret_cast< arc::gen3::fits::BPP_32* >( pBuf ), MAKE_POINT( llX, llY ), MAKE_POINT( urX, urY ) );
			}
	}
	catch ( std::exception& e )
	{
		SET_ERROR_STATUS( pStatus, e );
	}
}


// +----------------------------------------------------------------------------------------------------------+
// |  ArcFitsFile_readSubImage                                                                                |
// +----------------------------------------------------------------------------------------------------------+
// |  Returns a pointer to the sub-image data.                                                                |
// |                                                                                                          |
// |  <IN>  -> ulHandle - A reference to a FITS file object.                                                  |
// |  <IN>  -> llX		- The lower left column of the sub-image.					                          |
// |  <IN>  -> llY		- The lower left row of the sub-image.                                                |
// |  <IN>  -> urX		- The upper right column of the sub-image.                                            |
// |  <IN>  -> urY		- The upper right row of the sub-image.                                               |
// |  <OUT> -> pStatus  - Success state; equals ARC_STATUS_OK or ARC_STATUS_ERROR.                            |
// +----------------------------------------------------------------------------------------------------------+
GEN3_CARCFITSFILE_API void* ArcFitsFile_readSubImage( unsigned long long ulHandle, long llX, long llY, long urX, long urY, ArcStatus_t* pStatus )
{
	INIT_STATUS( pStatus, ARC_STATUS_OK )

		std::uint8_t* pBuf = nullptr;

	try
	{
		VERIFY_INSTANCE_HANDLE( ulHandle )

			if ( ulHandle == reinterpret_cast< std::uint64_t >( g_pFits16.get() ) )
			{
				auto pSub16 = g_pFits16->readSubImage( MAKE_POINT( llX, llY ), MAKE_POINT( urX, urY ) );

				if ( pSub16 != nullptr )
				{
					auto iSize = ( std::abs( urX - llX ) * std::abs( urY - llY ) * sizeof( arc::gen3::fits::BPP_16 ) );

					pBuf = new std::uint8_t[ iSize ];

					if ( pBuf != nullptr )
					{
						std::memcpy( pBuf, pSub16.get(), iSize );
					}
				}
			}

			else
			{
				auto pSub32 = g_pFits32->readSubImage( MAKE_POINT( llX, llY ), MAKE_POINT( urX, urY ) );

				if ( pSub32 != nullptr )
				{
					auto iSize = ( std::abs( urX - llX ) * std::abs( urY - llY ) * sizeof( arc::gen3::fits::BPP_32 ) );

					pBuf = new std::uint8_t[ iSize ];

					if ( pBuf != nullptr )
					{
						std::memcpy( pBuf, pSub32.get(), iSize );
					}
				}
			}
	}
	catch ( std::exception& e )
	{
		SET_ERROR_STATUS( pStatus, e );
	}

	return pBuf;
}


// +----------------------------------------------------------------------------------------------------------+
// |  ArcFitsFile_read                                                                                        |
// +----------------------------------------------------------------------------------------------------------+
// |  Returns a pointer to the image data ( single image file ).                                              |
// |                                                                                                          |
// |  <IN>  -> ulHandle - A reference to a FITS file object.                                                  |
// |  <OUT> -> pStatus  - Success state; equals ARC_STATUS_OK or ARC_STATUS_ERROR.                            |
// +----------------------------------------------------------------------------------------------------------+
GEN3_CARCFITSFILE_API void* ArcFitsFile_read( unsigned long long ulHandle, ArcStatus_t* pStatus )
{
	INIT_STATUS( pStatus, ARC_STATUS_OK )

		std::uint8_t* pBuf = nullptr;

	try
	{
		VERIFY_INSTANCE_HANDLE( ulHandle )

			if ( ulHandle == reinterpret_cast< std::uint64_t >( g_pFits16.get() ) )
			{
				auto pSub16 = g_pFits16->read();

				if ( pSub16 != nullptr )
				{
					auto iSize = ( g_pFits16->getCols() * g_pFits16->getRows() * sizeof( arc::gen3::fits::BPP_16 ) );

					pBuf = new std::uint8_t[ iSize ];

					if ( pBuf != nullptr )
					{
						std::memcpy( pBuf, pSub16.get(), iSize );
					}
				}
			}

			else
			{
				auto pSub32 = g_pFits32->read();

				if ( pSub32 != nullptr )
				{
					auto iSize = ( g_pFits32->getCols() * g_pFits32->getRows() * sizeof( arc::gen3::fits::BPP_32 ) );

					pBuf = new std::uint8_t[ iSize ];

					if ( pBuf != nullptr )
					{
						std::memcpy( pBuf, pSub32.get(), iSize );
					}
				}
			}
	}
	catch ( std::exception& e )
	{
		SET_ERROR_STATUS( pStatus, e );
	}

	return pBuf;
}


// +----------------------------------------------------------------------------------------------------------+
// |  ArcFitsFile_write3D                                                                                     |
// +----------------------------------------------------------------------------------------------------------+
// |  Writes an image to the end of a data cube file.                                                         |
// |                                                                                                          |
// |  <IN>  -> ulHandle - A reference to a FITS file object.                                                  |
// |  <IN>  -> pBuf		- The image buffer to write. Buffer access violation results in undefined behavior.   |
// |  <OUT> -> pStatus  - Success state; equals ARC_STATUS_OK or ARC_STATUS_ERROR.                            |
// +----------------------------------------------------------------------------------------------------------+
GEN3_CARCFITSFILE_API void ArcFitsFile_write3D( unsigned long long ulHandle, void* pBuf, ArcStatus_t* pStatus )
{
	INIT_STATUS( pStatus, ARC_STATUS_OK )

		try
	{
		VERIFY_INSTANCE_HANDLE( ulHandle )

			if ( ulHandle == reinterpret_cast< std::uint64_t >( g_pFits16.get() ) )
			{
				g_pFits16->write3D( static_cast< arc::gen3::fits::BPP_16* >( pBuf ) );
			}

			else
			{
				g_pFits32->write3D( static_cast< arc::gen3::fits::BPP_32* >( pBuf ) );
			}
	}
	catch ( std::exception& e )
	{
		SET_ERROR_STATUS( pStatus, e );
	}
}


// +----------------------------------------------------------------------------------------------------------+
// |  ArcFitsFile_reWrite3D                                                                                   |
// +----------------------------------------------------------------------------------------------------------+
// |  Re-writes an existing image in a data cube file. The image data MUST match in size to the exising       |
// |  images within the data cube.                                                                            |
// |                                                                                                          |
// |  <IN>  -> ulHandle			- A reference to a FITS file object.                                          |
// |  <IN>  -> pBuf				- The image buffer to write. Buffer access violation results in undefined     |
// |						      behavior.                                                                   |
// |  <IN>  -> uiImageNumber	- The number of the data cube image to replace.                               |
// |  <OUT> -> pStatus			- Success state; equals ARC_STATUS_OK or ARC_STATUS_ERROR.                    |
// +----------------------------------------------------------------------------------------------------------+
GEN3_CARCFITSFILE_API void ArcFitsFile_reWrite3D( unsigned long long ulHandle, void* pBuf, unsigned int uiImageNumber, ArcStatus_t* pStatus )
{
	INIT_STATUS( pStatus, ARC_STATUS_OK )

		try
	{
		VERIFY_INSTANCE_HANDLE( ulHandle )

			if ( ulHandle == reinterpret_cast< std::uint64_t >( g_pFits16.get() ) )
			{
				g_pFits16->reWrite3D( static_cast< arc::gen3::fits::BPP_16* >( pBuf ), uiImageNumber );
			}

			else
			{
				g_pFits32->reWrite3D( static_cast< arc::gen3::fits::BPP_32* >( pBuf ), uiImageNumber );
			}
	}
	catch ( std::exception& e )
	{
		SET_ERROR_STATUS( pStatus, e );
	}
}


// +----------------------------------------------------------------------------------------------------------+
// |  ArcFitsFile_read3D                                                                                      |
// +----------------------------------------------------------------------------------------------------------+
// |  Reads an image from a data cube file.                                                                   |
// |                                                                                                          |
// |  <IN>  -> ulHandle			- A reference to a FITS file object.                                          |
// |  <IN>  -> uiImageNumber	- The number of the data cube image to replace.                               |
// |  <OUT> -> pStatus			- Success state; equals ARC_STATUS_OK or ARC_STATUS_ERROR.                    |
// +----------------------------------------------------------------------------------------------------------+
GEN3_CARCFITSFILE_API void* ArcFitsFile_read3D( unsigned long long ulHandle, unsigned int uiImgNumber, ArcStatus_t* pStatus )
{
	INIT_STATUS( pStatus, ARC_STATUS_OK )

		std::uint8_t* pBuf = nullptr;

	try
	{
		VERIFY_INSTANCE_HANDLE( ulHandle )

			if ( ulHandle == reinterpret_cast< std::uint64_t >( g_pFits16.get() ) )
			{
				auto pSub16 = g_pFits16->read3D( uiImgNumber );

				if ( pSub16 != nullptr )
				{
					auto iSize = ( g_pFits16->getCols() * g_pFits16->getRows() * sizeof( arc::gen3::fits::BPP_16 ) );

					pBuf = new std::uint8_t[ iSize ];

					if ( pBuf != nullptr )
					{
						std::memcpy( pBuf, pSub16.get(), iSize );
					}
				}
			}

			else
			{
				auto pSub32 = g_pFits32->read3D( uiImgNumber );

				if ( pSub32 != nullptr )
				{
					auto iSize = ( g_pFits16->getCols() * g_pFits16->getRows() * sizeof( arc::gen3::fits::BPP_32 ) );

					pBuf = new std::uint8_t[ iSize ];

					if ( pBuf != nullptr )
					{
						std::memcpy( pBuf, pSub32.get(), iSize );
					}
				}
			}
	}
	catch ( std::exception& e )
	{
		SET_ERROR_STATUS( pStatus, e );
	}

	return pBuf;
}


// +----------------------------------------------------------------------------------------------------------+
// |  ArcFitsFile_getBaseFile                                                                                 |
// +----------------------------------------------------------------------------------------------------------+
// |  Returns the underlying cfitsio file pointer.                                                            |
// |                                                                                                          |
// |  <IN>  -> ulHandle			- A reference to a FITS file object.                                          |
// |  <OUT> -> pStatus			- Success state; equals ARC_STATUS_OK or ARC_STATUS_ERROR.                    |
// +----------------------------------------------------------------------------------------------------------+
GEN3_CARCFITSFILE_API fitsfile* ArcFitsFile_getBaseFile( unsigned long long ulHandle, ArcStatus_t* pStatus )
{
	try
	{
		VERIFY_INSTANCE_HANDLE( ulHandle )

			if ( ulHandle == reinterpret_cast< std::uint64_t >( g_pFits16.get() ) )
			{
				return g_pFits16->getBaseFile();
			}

			else
			{
				return g_pFits32->getBaseFile();
			}
	}
	catch ( std::exception& e )
	{
		SET_ERROR_STATUS( pStatus, e );
	}

	return nullptr;
}


// +----------------------------------------------------------------------------------------------------------+
// |  ArcFitsFile_maxTVal                                                                                     |
// +----------------------------------------------------------------------------------------------------------+
// |  Determines the maximum value for a specific data type. Example, for std::uint16_t: 2^16 = 65536.        |
// |                                                                                                          |
// |  <IN>  -> ulHandle			- A reference to a FITS file object.                                          |
// |  <OUT> -> pStatus			- Success state; equals ARC_STATUS_OK or ARC_STATUS_ERROR.                    |
// +----------------------------------------------------------------------------------------------------------+
GEN3_CARCFITSFILE_API unsigned int ArcFitsFile_maxTVal( unsigned long long ulHandle, ArcStatus_t* pStatus )
{
	try
	{
		VERIFY_INSTANCE_HANDLE( ulHandle )

			if ( ulHandle == reinterpret_cast< std::uint64_t >( g_pFits16.get() ) )
			{
				return g_pFits16->maxTVal();
			}

			else
			{
				return g_pFits32->maxTVal();
			}
	}
	catch ( std::exception& e )
	{
		SET_ERROR_STATUS( pStatus, e );
	}

	return 0;
}


// +------------------------------------------------------------------------------------------------------------------+
// |  ArcFitsFile_getLastError                                                                                        |
// +------------------------------------------------------------------------------------------------------------------+
// |  Returns the last reported error message.                                                                        |
// +------------------------------------------------------------------------------------------------------------------+
GEN3_CARCFITSFILE_API const char* ArcFitsFile_getLastError( void )
{
	return const_cast< const char* >( g_pErrMsg->data() );
}
