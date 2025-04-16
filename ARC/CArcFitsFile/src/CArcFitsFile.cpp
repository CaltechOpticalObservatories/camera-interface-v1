// +------------------------------------------------------------------------------------------------------------------+
// |  FILE:  CArcFitsFile.cpp  ( Gen3 )                                                                               |
// +------------------------------------------------------------------------------------------------------------------+
// |  PURPOSE:  Defines the exported functions for the CArcFitsFile DLL.  Wraps the cfitsio library for convenience.  |
// |                                                                                                                  |
// |  AUTHOR:  Scott Streit			DATE: March 25, 2020                                                              |
// |                                                                                                                  |
// |  Copyright 2013 Astronomical Research Cameras, Inc. All rights reserved.                                         |
// +------------------------------------------------------------------------------------------------------------------+

#include <type_traits>
#include <typeinfo>
#include <sstream>
#include <memory>
#include <cmath>

#include <CArcFitsFile.h>


#if defined( _WINDOWS ) || defined( __linux )
#include <filesystem>
#define ArcRemove( sFileName )		std::filesystem::remove( std::filesystem::path( sFileName ) )
#else
#include <cstdio>
#define ArcRemove( sFileName )		remove( sFileName.c_str() )
#endif




namespace arc
{
	namespace gen3
	{


		/** Ensures that the internal fits handle is valid.
		 *  @throws Throws an exception if the fits handle is not valid.
		 */
		#define VERIFY_FILE_HANDLE()		if ( m_pFits == nullptr )									\
											{															\
												THROW( "Invalid FITS handle, no file open" );			\
											}


		 // +----------------------------------------------------------------------------------------------------------+
		 // | Library build and version info                                                                           |
		 // +----------------------------------------------------------------------------------------------------------+
		template <typename T> const std::string CArcFitsFile<T>::m_sVersion = std::string( "ARC Gen III FITS API Library v3.6.     " ) +

#ifdef _WINDOWS
			CArcBase::formatString( "[ Compiler Version: %d, Built: %s ]", _MSC_VER, __TIMESTAMP__ );
#else
			arc::gen3::CArcBase::formatString( "[ Compiler Version: %d.%d.%d, Built: %s %s ]", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__, __DATE__, __TIME__ );
#endif


		// +----------------------------------------------------------------------------------------------------------+
		// |  CParam Class constructor                                                                                |
		// +----------------------------------------------------------------------------------------------------------+
		fits::CParam::CParam( void ) : m_iNAxis( 0 ), m_iBpp( 0 )
		{
			m_plCols = &m_lNAxes[ 0 ];  *m_plCols = 0;
			m_plRows = &m_lNAxes[ 1 ];  *m_plRows = 0;
			m_plFrames = &m_lNAxes[ 2 ];  *m_plFrames = 0;
		}


		// +----------------------------------------------------------------------------------------------------------+
		// |  CParam Class destructor                                                                                 |
		// +----------------------------------------------------------------------------------------------------------+
		fits::CParam::~CParam( void )
		{
		}

		// +----------------------------------------------------------------------------------------------------------+
		// |  CParam Class getCols                                                                                    |
		// +----------------------------------------------------------------------------------------------------------+
		// |  Returns the number of image columns.                                                                    |
		// +----------------------------------------------------------------------------------------------------------+
		std::uint32_t fits::CParam::getCols( void )
		{
			return static_cast< std::uint32_t >( m_lNAxes[ 0 ] );
		}


		// +----------------------------------------------------------------------------------------------------------+
		// |  CParam Class getRows                                                                                    |
		// +----------------------------------------------------------------------------------------------------------+
		// |  Returns the number of image rows.                                                                       |
		// +----------------------------------------------------------------------------------------------------------+
		std::uint32_t fits::CParam::getRows( void )
		{
			return static_cast< std::uint32_t >( m_lNAxes[ 1 ] );
		}


		// +----------------------------------------------------------------------------------------------------------+
		// |  CParam Class getFrames                                                                                  |
		// +----------------------------------------------------------------------------------------------------------+
		// |  Returns the number of frames in the file.                                                               |
		// +----------------------------------------------------------------------------------------------------------+
		std::uint32_t fits::CParam::getFrames( void )
		{
			return static_cast< std::uint32_t >( m_lNAxes[ 2 ] );
		}


		// +----------------------------------------------------------------------------------------------------------+
		// |  CParam Class getNAxis                                                                                   |
		// +----------------------------------------------------------------------------------------------------------+
		// |  Returns the number of axes in the file.                                                                 |
		// +----------------------------------------------------------------------------------------------------------+
		std::uint32_t fits::CParam::getNAxis( void )
		{
			return static_cast< std::uint32_t >( m_iNAxis );
		}


		// +----------------------------------------------------------------------------------------------------------+
		// |  CParam Class getBpp                                                                                     |
		// +----------------------------------------------------------------------------------------------------------+
		// |  Returns the number of bits-per-pixel in the file.                                                       |
		// +----------------------------------------------------------------------------------------------------------+
		std::uint32_t fits::CParam::getBpp( void )
		{
			return static_cast< std::uint32_t >( m_iBpp );
		}


		// +----------------------------------------------------------------------------------------------------------+
		// |  Class constructor                                                                                       |
		// +----------------------------------------------------------------------------------------------------------+
		template <typename T> CArcFitsFile<T>::CArcFitsFile( void ) : CArcBase(), m_i64Pixel( 0 ), m_iFrame( 0 ), m_pFits( nullptr )
		{
		}


		// +----------------------------------------------------------------------------------------------------------+
		// |  Class destructor                                                                                        |
		// +----------------------------------------------------------------------------------------------------------+
		template <typename T> CArcFitsFile<T>::~CArcFitsFile( void )
		{
			try
			{
				close();
			}
			catch ( ... ) {}
		}


		template <typename T> std::string CArcFitsFile<T>::getType( void )
		{
			return typeid( T ).name();
		}
		//template <typename T> typeid CArcFitsFile<T>::getType( void )
		//{
		//	return typeid( T );
		//}


		// +----------------------------------------------------------------------------------------------------------+
		// |  version                                                                                                 |
		// +----------------------------------------------------------------------------------------------------------+
		// |  Returns a textual representation of the library version.                                                |
		// +----------------------------------------------------------------------------------------------------------+
		template <typename T> const std::string CArcFitsFile<T>::version( void )
		{
			return m_sVersion;
		}


		// +----------------------------------------------------------------------------------------------------------+
		// |  version                                                                                                 |
		// +----------------------------------------------------------------------------------------------------------+
		// |  Returns a textual representation of the library version.                                                |
		// +----------------------------------------------------------------------------------------------------------+
		template <typename T> const std::string CArcFitsFile<T>::cfitsioVersion( void )
		{
			float fVersion = 0.f;

			fits_get_version( &fVersion );

			const std::string sVersion = CArcBase::formatString( "CFITSIO Library.                 [ Version: %f ]", fVersion );

			return sVersion;
		}


		// +----------------------------------------------------------------------------------------------------------+
		// |  create                                                                                                  |
		// +----------------------------------------------------------------------------------------------------------+
		// |  Creates a new single image file on disk with the specified image dimensions.                            |
		// |                                                                                                          |
		// |  <IN> -> sFileName - The new file name.                                                                  |
		// |  <IN> -> uiCols	- The image column size ( in pixels ).                                                |
		// |  <IN> -> uiRows	- The image row size ( in pixels ).                                                   |
		// |                                                                                                          |
		// |  Throws std::runtime_error, std::invalid_argument                                                        |
		// +----------------------------------------------------------------------------------------------------------+
		template <typename T>
		void CArcFitsFile<T>::create( const std::string& sFileName, std::uint32_t uiCols, std::uint32_t uiRows )
		{
			std::int32_t iImageType = ( ( sizeof( T ) == sizeof( std::uint16_t ) ) ? USHORT_IMG : ULONG_IMG );

			std::int32_t iStatus = 0;

			long a_lNAxes[] = { static_cast< long >( uiCols ), static_cast< long >( uiRows ) };

			if ( m_pFits != nullptr )
			{
				close();
			}

			//
			// Verify image dimensions
			//
			if ( uiRows == 0 )
			{
				THROW_INVALID_ARGUMENT( "Row dimension must be greater than zero!" );
			}

			if ( uiCols == 0 )
			{
				THROW_INVALID_ARGUMENT( "Column dimension must be greater than zero!" );
			}

			//
			// Verify filename
			//
			if ( sFileName.empty() )
			{
				THROW_INVALID_ARGUMENT( "Invalid file name : %s", sFileName.c_str() );
			}

			//
			// Delete the file if it exists. This is to prevent creation errors.
			//
			ArcRemove( sFileName );

			//
			// Create the file ( force overwrite )
			//
			fits_create_file( &m_pFits, const_cast< std::string& >( sFileName ).insert( 0, 1, '!' ).c_str(), &iStatus );

			if ( iStatus )
			{
				close();

				//
				// Delete the file if it exists. This is to prevent creation errors.
				//
				ArcRemove( sFileName );

				throwFitsError( iStatus );
			}

			fits_create_img( m_pFits, iImageType, 2, a_lNAxes, &iStatus );

			if ( iStatus )
			{
				close();

				//
				// Delete the file if it exists. This is to prevent creation errors.
				//
				ArcRemove( sFileName );

				throwFitsError( iStatus );
			}

			m_i64Pixel = 0;
			m_iFrame = 0;
		}


		// +----------------------------------------------------------------------------------------------------------+
		// |  create3D                                                                                                |
		// +----------------------------------------------------------------------------------------------------------+
		// |  Creates a new data cube file on disk with the specified image dimensions.                               |
		// |                                                                                                          |
		// |  <IN> -> sFileName - The new file name.                                                                  |
		// |  <IN> -> uiCols	- The image column size ( in pixels ).                                                |
		// |  <IN> -> uiRows	- The image row size ( in pixels ).                                                   |
		// |                                                                                                          |
		// |  Throws std::runtime_error, std::invalid_argument                                                        |
		// +----------------------------------------------------------------------------------------------------------+
		template <typename T>
		void CArcFitsFile<T>::create3D( const std::string& sFileName, std::uint32_t uiCols, std::uint32_t uiRows )
		{
			std::int32_t iImageType = ( ( sizeof( T ) == sizeof( std::uint16_t ) ) ? USHORT_IMG : ULONG_IMG );

			std::int32_t iStatus = 0;

			long a_lNAxes[] = { static_cast< long >( uiCols ), static_cast< long >( uiRows ), 1 };		// cols, rows, nof

			if ( m_pFits != nullptr )
			{
				close();
			}

			//
			// Verify image dimensions
			//
			if ( uiRows == 0 )
			{
				THROW_INVALID_ARGUMENT( "Row dimension must be greater than zero!" );
			}

			if ( uiCols == 0 )
			{
				THROW_INVALID_ARGUMENT( "Column dimension must be greater than zero!" );
			}

			//
			// Verify filename
			//
			if ( sFileName.empty() )
			{
				THROW_INVALID_ARGUMENT( "Invalid file name : %s", sFileName.c_str() );
			}

			//
			// Delete the file if it exists. This is to prevent creation errors.
			//
			ArcRemove( sFileName );

			//
			// Create the file ( force overwrite )
			//
			fits_create_file( &m_pFits, const_cast< std::string& >( sFileName ).insert( 0, 1, '!' ).c_str(), &iStatus );

			if ( iStatus )
			{
				close();

				//
				// Delete the file if it exists. This is to prevent creation errors.
				//
				ArcRemove( sFileName );

				throwFitsError( iStatus );
			}

			fits_create_img( m_pFits, iImageType, 3, a_lNAxes, &iStatus );

			if ( iStatus )
			{
				close();

				//
				// Delete the file if it exists. This is to prevent creation errors.
				//
				ArcRemove( sFileName );

				throwFitsError( iStatus );
			}

			m_i64Pixel = 0;
			m_iFrame = 0;
		}


		// +----------------------------------------------------------------------------------------------------------+
		// |  open                                                                                                    |
		// +----------------------------------------------------------------------------------------------------------+
		// |  Opens an existing file. Can be used to open a file containing a single image or data cube ( a file with |
		// |  multiple image planes ).                                                                                |
		// |                                                                                                          |
		// |  <IN> -> sFileName - The file name.                                                                      |
		// |  <IN> -> eMode - The mode ( read/write ) with which to open the file ( default = e_ReadMode::READMODE ). |
		// |                                                                                                          |
		// |  Throws std::runtime_error, std::invalid_argument                                                        |
		// +----------------------------------------------------------------------------------------------------------+
		template <typename T> void CArcFitsFile<T>::open( const std::string& sFileName, arc::gen3::fits::e_ReadMode eMode )
		{
			std::int32_t iStatus = 0;
			std::int32_t iExists = 0;

			if ( m_pFits != nullptr )
			{
				close();
			}

			//
			// Verify filename
			//
			if ( sFileName.empty() )
			{
				THROW_INVALID_ARGUMENT( "Invalid file name : %s", sFileName.c_str() );
			}

			//
			// Make sure the specified file exists
			//
			fits_file_exists( sFileName.c_str(), &iExists, &iStatus );

			if ( iStatus )
			{
				m_pFits = nullptr;

				throwFitsError( iStatus );
			}

			//
			// Open the FITS file
			//
			fits_open_file( &m_pFits, sFileName.c_str(), static_cast< int >( eMode ), &iStatus );

			if ( iStatus )
			{
				m_pFits = nullptr;

				throwFitsError( iStatus );
			}

			m_i64Pixel = 0;
			m_iFrame = 0;
		}


		// +----------------------------------------------------------------------------------------------------------+
		// |  close                                                                                                   |
		// +----------------------------------------------------------------------------------------------------------+
		// |  Closes the file. All subsequent methods, except for create and open will result in an error and an      |
		// |  exception will be thrown.                                                                               |
		// |                                                                                                          |
		// |  Throws std::runtime_error on error.                                                                     |
		// +----------------------------------------------------------------------------------------------------------+
		template <typename T>
		void CArcFitsFile<T>::close( void )
		{
			//
			//  DeleteBuffer requires access to the file, so don't close the file until
			// after deleteBuffer has been called!!!
			//
			if ( m_pFits != nullptr )
			{
				std::int32_t iStatus = 0;

				fits_close_file( m_pFits, &iStatus );
			}

			m_pFits = nullptr;

			m_i64Pixel = 0;
			m_iFrame = 0;
		}


		// +----------------------------------------------------------------------------------------------------------+
		// |  getHeader                                                                                               |
		// +----------------------------------------------------------------------------------------------------------+
		// |  Returns the FITS header as a list of strings.                                                           |
		// |                                                                                                          |
		// |  Throws std::runtime_error on error.                                                                     |
		// +----------------------------------------------------------------------------------------------------------+
		template <typename T> std::unique_ptr<arc::gen3::CArcStringList> CArcFitsFile<T>::getHeader( void )
		{
			std::int32_t iNumOfKeys = 0;
			std::int32_t iStatus = 0;

			VERIFY_FILE_HANDLE()

				fits_get_hdrspace( m_pFits, &iNumOfKeys, nullptr, &iStatus );

			if ( iStatus )
			{
				throwFitsError( iStatus );
			}

			std::unique_ptr<arc::gen3::CArcStringList> pList( new arc::gen3::CArcStringList() );

			std::string sCard( 100, ' ' );

			for ( int i = 0; i < iNumOfKeys; i++ )
			{
				fits_read_record( m_pFits,
					( i + 1 ),
					const_cast< char* >( sCard.data() ),
					&iStatus );

				if ( iStatus )
				{
					throwFitsError( iStatus );
				}

				*( pList.get() ) << sCard.c_str();
			}

			return pList;
		}


		// +----------------------------------------------------------------------------------------------------------+
		// |  getFileName                                                                                             |
		// +----------------------------------------------------------------------------------------------------------+
		// |  Returns the filename associated with this CArcFitsFile object.                                          |
		// |                                                                                                          |
		// |  Throws std::runtime_error on error.                                                                     |
		// +----------------------------------------------------------------------------------------------------------+
		template <typename T> const std::string CArcFitsFile<T>::getFileName( void )
		{
			std::int32_t iStatus = 0;

			std::string sFileName( 150, ' ' );

			VERIFY_FILE_HANDLE()

				fits_file_name( m_pFits, const_cast< char* >( sFileName.data() ), &iStatus );

			if ( iStatus )
			{
				throwFitsError( iStatus );
			}

			return sFileName.c_str();
		}


		// +----------------------------------------------------------------------------------------------------------+
		// |  readKeyword                                                                                             |
		// +----------------------------------------------------------------------------------------------------------+
		// |  Reads a FITS keyword value from the header.  The keyword must be valid or an exception will be thrown.  |
		// |                                                                                                          |
		// |  <IN> -> sKey  - The header keyword. Can be "".  Ex: SIMPLE                                              |
		// |  <IN> -> eType - The keyword type, as defined in CArcFitsFile.h                                          |
		// |                                                                                                          |
		// |  Throws std::runtime_error, std::invalid_argument                                                        |
		// +----------------------------------------------------------------------------------------------------------+
		template <typename T>
		arc::gen3::fits::keywordValue_t CArcFitsFile<T>::readKeyword( const std::string& sKey, arc::gen3::fits::e_Type eType )
		{
			std::int32_t iType = static_cast< std::int32_t >( arc::gen3::fits::e_Type::FITS_INVALID_KEY );
			std::int32_t iStatus = 0;

			VERIFY_FILE_HANDLE()

				arc::gen3::fits::keywordValue_t keyValue; // ( 0, 0, 0, 0, 0.0, std::string() );

			switch ( eType )
			{
			case arc::gen3::fits::e_Type::FITS_STRING_KEY:
			{
				iType = TSTRING;

				char szValue[ 80 ];

				fits_read_key( m_pFits, iType, sKey.c_str(), szValue, NULL, &iStatus );

#ifndef __APPLE__
				keyValue = std::string( szValue );
#else
				keyValue = std::make_tuple( 0, 0, 0, 0, 0.0, std::string( szValue ) );
#endif
			}
			break;

			case arc::gen3::fits::e_Type::FITS_INT_KEY:
			{
				iType = TINT;

				std::int32_t iValue = 0;

				fits_read_key( m_pFits, iType, sKey.c_str(), &iValue, NULL, &iStatus );

#ifndef __APPLE__
				keyValue = iValue;
#else
				keyValue = std::make_tuple( 0, iValue, 0, 0, 0.0, std::string() );
#endif
			}
			break;

			case arc::gen3::fits::e_Type::FITS_UINT_KEY:
			{
				iType = TUINT;

				std::uint32_t uiValue = 0;

				fits_read_key( m_pFits, iType, sKey.c_str(), &uiValue, NULL, &iStatus );

#ifndef __APPLE__
				keyValue = uiValue;
#else
				keyValue = std::make_tuple( uiValue, 0, 0, 0, 0.0, std::string() );
#endif
			}
			break;

			case arc::gen3::fits::e_Type::FITS_SHORT_KEY:
			{
				iType = TSHORT;

				std::int16_t wValue = 0;

				fits_read_key( m_pFits, iType, sKey.c_str(), &wValue, NULL, &iStatus );

#ifndef __APPLE__
				keyValue = static_cast< std::int32_t >( wValue );
#else
				keyValue = std::make_tuple( 0, static_cast< std::int32_t >( wValue ), 0, 0, 0.0, std::string() );
#endif
			}
			break;

			case arc::gen3::fits::e_Type::FITS_USHORT_KEY:
			{
				iType = TUSHORT;

				std::uint16_t uwValue = 0;

				fits_read_key( m_pFits, iType, sKey.c_str(), &uwValue, NULL, &iStatus );

#ifndef __APPLE__
				keyValue = static_cast< std::uint32_t >( uwValue );
#else
				keyValue = std::make_tuple( static_cast< std::uint32_t >( uwValue ), 0, 0, 0, 0.0, std::string() );
#endif
			}
			break;

			case arc::gen3::fits::e_Type::FITS_FLOAT_KEY:
			{
				iType = TFLOAT;

				float fValue = 0;

				fits_read_key( m_pFits, iType, sKey.c_str(), &fValue, NULL, &iStatus );

#ifndef __APPLE__
				keyValue = static_cast< double >( fValue );
#else
				keyValue = std::make_tuple( 0, 0, 0, 0, static_cast< double >( fValue ), std::string() );
#endif
			}
			break;

			case arc::gen3::fits::e_Type::FITS_DOUBLE_KEY:
			{
				iType = TDOUBLE;

				double gValue = 0;

				fits_read_key( m_pFits, iType, sKey.c_str(), &gValue, NULL, &iStatus );

#ifndef __APPLE__
				keyValue = gValue;
#else
				keyValue = std::make_tuple( 0, 0, 0, 0, gValue, std::string() );
#endif
			}
			break;

			case arc::gen3::fits::e_Type::FITS_BYTE_KEY:
			{
				iType = TBYTE;

				std::int8_t iValue = 0;

				fits_read_key( m_pFits, iType, sKey.c_str(), &iValue, NULL, &iStatus );

#ifndef __APPLE__
				keyValue = static_cast< std::uint32_t >( iValue );
#else
				keyValue = std::make_tuple( 0, static_cast< std::uint32_t >( iValue ), 0, 0, 0.0, std::string() );
#endif
			}
			break;

			case arc::gen3::fits::e_Type::FITS_LONG_KEY:
			{
				iType = TLONG;

				std::int32_t iValue = 0;

				fits_read_key( m_pFits, iType, sKey.c_str(), &iValue, NULL, &iStatus );

#ifndef __APPLE__
				keyValue = iValue;
#else
				keyValue = std::make_tuple( 0, static_cast< std::int32_t >( iValue ), 0, 0, 0.0, std::string() );
#endif
			}
			break;

			case arc::gen3::fits::e_Type::FITS_ULONG_KEY:
			{
				iType = TULONG;

				std::uint32_t uiValue = 0;

				fits_read_key( m_pFits, iType, sKey.c_str(), &uiValue, NULL, &iStatus );

#ifndef __APPLE__
				keyValue = uiValue;
#else
				keyValue = std::make_tuple( uiValue, 0, 0, 0, 0.0, std::string() );
#endif
			}
			break;

			case arc::gen3::fits::e_Type::FITS_LONGLONG_KEY:
			{
				iType = TLONGLONG;

				std::int64_t iValue = 0;

				fits_read_key( m_pFits, iType, sKey.c_str(), &iValue, NULL, &iStatus );

#ifndef __APPLE__
				keyValue = iValue;
#else
				keyValue = std::make_tuple( 0, 0, 0, iValue, 0.0, std::string() );
#endif
			}
			break;

			case arc::gen3::fits::e_Type::FITS_LOGICAL_KEY:
			{
				iType = TLOGICAL;

				std::int32_t iValue = 0;

				fits_read_key( m_pFits, iType, sKey.c_str(), &iValue, NULL, &iStatus );

#ifndef __APPLE__
				keyValue = iValue;
#else
				keyValue = std::make_tuple( 0, iValue, 0, 0, 0.0, std::string() );
#endif
			}
			break;

			case arc::gen3::fits::e_Type::FITS_COMMENT_KEY:
			case arc::gen3::fits::e_Type::FITS_HISTORY_KEY:
			case arc::gen3::fits::e_Type::FITS_DATE_KEY:
			{
			}
			break;

			default:
			{
				iType = static_cast< std::int32_t >( arc::gen3::fits::e_Type::FITS_INVALID_KEY );

				THROW_INVALID_ARGUMENT( "Invalid FITS keyword type. See CArcFitsFile.h for valid type list" );
			}
			}

			return keyValue;
		}


		// +----------------------------------------------------------------------------------------------------------+
		// |  writeKeyword                                                                                            |
		// +----------------------------------------------------------------------------------------------------------+
		// |  Writes a FITS keyword to the header.  The keyword must be valid or an exception will be thrown.         |
		// |                                                                                                          |
		// |  'HIERARCH' keyword NOTE: This text will be prefixed to any keyword by the cfitsio library if the        |
		// |                           keyword is greater than 8 characters, which is the standard FITS keyword       |
		// |                           length. See the link below for details:                                        |
		// |                                                                                                          |
		// |   http://heasarc.gsfc.nasa.gov/docs/software/fitsio/c/f_user/node28.html                                 |
		// |                                                                                                          |
		// |   HIERARCH examples:                                                                                     |
		// |   -----------------                                                                                      |
		// |   HIERARCH LongKeyword = 47.5 / Keyword has > 8 characters & mixed case                                  |
		// |   HIERARCH XTE$TEMP = 98.6 / Keyword contains the '$' character                                          |
		// |   HIERARCH Earth is a star = F / Keyword contains embedded spaces                                        |
		// |                                                                                                          |
		// |  <IN> -> sKey     - The header keyword. Can be "".  Ex: SIMPLE                                           |
		// |  <IN> -> pvalue   - The value associated with the key. Ex: T                                             |
		// |  <IN> -> eType    - The keyword type, as defined in CArcFitsFile.h                                       |
		// |  <IN> -> sComment - The comment to attach to the keyword.  Can be "".                                    |
		// |                                                                                                          |
		// |  Throws std::runtime_error, std::invalid_argument                                                        |
		// +----------------------------------------------------------------------------------------------------------+
		template <typename T> void CArcFitsFile<T>::writeKeyword( const std::string& sKey, void* pValue,
			arc::gen3::fits::e_Type eType,
			const std::string& sComment )
		{
			std::int32_t iType = static_cast< std::int32_t >( arc::gen3::fits::e_Type::FITS_INVALID_KEY );
			std::int32_t iStatus = 0;

			VERIFY_FILE_HANDLE()

				//
				// Verify value pointer
				//
				if ( eType != arc::gen3::fits::e_Type::FITS_DATE_KEY && pValue == nullptr )
				{
					THROW_INVALID_ARGUMENT( "Invalid FITS key value, cannot be nullptr" );
				}

			//
			// Update the fits header with the specified keys
			//
			switch ( eType )
			{
			case arc::gen3::fits::e_Type::FITS_STRING_KEY:
			{
				iType = TSTRING;
			}
			break;

			case arc::gen3::fits::e_Type::FITS_INT_KEY:
			{
				iType = TINT;
			}
			break;

			case arc::gen3::fits::e_Type::FITS_UINT_KEY:
			{
				iType = TUINT;
			}
			break;

			case arc::gen3::fits::e_Type::FITS_SHORT_KEY:
			{
				iType = TSHORT;
			}
			break;

			case arc::gen3::fits::e_Type::FITS_USHORT_KEY:
			{
				iType = TUSHORT;
			}
			break;

			case arc::gen3::fits::e_Type::FITS_FLOAT_KEY:
			{
				iType = TFLOAT;
			}
			break;

			case arc::gen3::fits::e_Type::FITS_DOUBLE_KEY:
			{
				iType = TDOUBLE;
			}
			break;

			case arc::gen3::fits::e_Type::FITS_BYTE_KEY:
			{
				iType = TBYTE;
			}
			break;

			case arc::gen3::fits::e_Type::FITS_LONG_KEY:
			{
				iType = TLONG;
			}
			break;

			case arc::gen3::fits::e_Type::FITS_ULONG_KEY:
			{
				iType = TULONG;
			}
			break;

			case arc::gen3::fits::e_Type::FITS_LONGLONG_KEY:
			{
				iType = TLONGLONG;
			}
			break;

			case arc::gen3::fits::e_Type::FITS_LOGICAL_KEY:
			{
				iType = TLOGICAL;
			}
			break;

			case arc::gen3::fits::e_Type::FITS_COMMENT_KEY:
			case arc::gen3::fits::e_Type::FITS_HISTORY_KEY:
			case arc::gen3::fits::e_Type::FITS_DATE_KEY:
			{
			}
			break;

			default:
			{
				iType = static_cast< std::int32_t >( arc::gen3::fits::e_Type::FITS_INVALID_KEY );

				THROW_INVALID_ARGUMENT( "Invalid FITS keyword type. See CArcFitsFile.h for valid type list" );
			}
			}

			//
			// write ( append ) a COMMENT keyword to the header. The comment
			// string will be continued over multiple keywords if it is
			// longer than 70 characters. 
			//
			if ( eType == arc::gen3::fits::e_Type::FITS_COMMENT_KEY )
			{
				fits_write_comment( m_pFits, static_cast< char* >( pValue ), &iStatus );
			}

			//
			// write ( append ) a HISTORY keyword to the header. The history
			// string will be continued over multiple keywords if it is
			// longer than 70 characters. 
			//
			else if ( eType == arc::gen3::fits::e_Type::FITS_HISTORY_KEY )
			{
				fits_write_history( m_pFits, static_cast< char* >( pValue ), &iStatus );
			}

			//
			// write the DATE keyword to the header. The keyword value will contain
			// the current system date as a character string in 'yyyy-mm-ddThh:mm:ss'
			// format. If a DATE keyword already exists in the header, then this
			// routine will simply update the keyword value with the current date.
			//
			else if ( eType == arc::gen3::fits::e_Type::FITS_DATE_KEY )
			{
				fits_write_date( m_pFits, &iStatus );
			}

			//
			// write a keyword of the appropriate data type into the header
			//
			else
			{
				fits_update_key( m_pFits,
					iType,
					sKey.c_str(),
					pValue,
					( sComment == "" ? nullptr : sComment.c_str() ),
					&iStatus );
			}

			if ( iStatus )
			{
				throwFitsError( iStatus );
			}
		}


		// +----------------------------------------------------------------------------------------------------------+
		// |  updateKeyword                                                                                           |
		// +----------------------------------------------------------------------------------------------------------+
		// |  Updates an existing FITS header keyword.  The keyword must be valid or an exception will be thrown.     |
		// |                                                                                                          |
		// |  'HIERARCH' keyword NOTE: This text will be prefixed to any keyword by the cfitsio library if the        |
		// |                           keyword is greater than 8 characters, which is the standard FITS keyword       |
		// |                           length. See the link below for details:                                        |
		// |   http://heasarc.gsfc.nasa.gov/docs/software/fitsio/c/f_user/node28.html                                 |
		// |                                                                                                          |
		// |   HIERARCH examples:                                                                                     |
		// |   -----------------                                                                                      |
		// |   HIERARCH LongKeyword = 47.5 / Keyword has > 8 characters & mixed case                                  |
		// |   HIERARCH XTE$TEMP = 98.6 / Keyword contains the '$' character                                          |
		// |   HIERARCH Earth is a star = F / Keyword contains embedded spaces                                        |
		// |                                                                                                          |
		// |  <IN> -> sKey     - The header keyword. Can be nullptr. Ex: SIMPLE                                       |
		// |  <IN> -> pKey     - The value associated with the key. Ex: T                                             |
		// |  <IN> -> iType    - The keyword type, as defined in CArcFitsFile.h                                       |
		// |  <IN> -> sComment - The comment to attach to the keyword.  Can be nullptr.                               |
		// |                                                                                                          |
		// |  Throws std::runtime_error, std::invalid_argument                                                        |
		// +----------------------------------------------------------------------------------------------------------+
		template <typename T> void CArcFitsFile<T>::updateKeyword( const std::string& sKey, void* pKey,
			arc::gen3::fits::e_Type eType,
			const std::string& sComment )
		{
			writeKeyword( sKey, pKey, eType, sComment );
		}


		// +----------------------------------------------------------------------------------------------------------+
		// |  getParameters                                                                                           |
		// +----------------------------------------------------------------------------------------------------------+
		// |  Returns a pointer to an arc::gen3::fits::CParam class that contains all the image parameters, such as   |
		// |  number of cols, rows, frames, dimensions and bits-per-pixel. May return nullptr.                        |
		// |                                                                                                          |
		// |  Throws std::runtime_error on error.                                                                     |
		// +----------------------------------------------------------------------------------------------------------+
		template <typename T> std::unique_ptr<arc::gen3::fits::CParam> CArcFitsFile<T>::getParameters( void )
		{
			std::int32_t iStatus = 0;

			VERIFY_FILE_HANDLE()

				std::unique_ptr<arc::gen3::fits::CParam> pParam( new arc::gen3::fits::CParam() );

			if ( pParam == nullptr )
			{
				THROW( "Failed to create parameters structure!" );
			}

			//
			// Get the image parameters
			//
			fits_get_img_param( m_pFits,
				3,
				&pParam->m_iBpp,
				&pParam->m_iNAxis,
				pParam->m_lNAxes,
				&iStatus );

			if ( iStatus )
			{
				throwFitsError( iStatus );
			}

			return pParam;
		}


		// +----------------------------------------------------------------------------------------------------------+
		// |  getNumberOfFrames                                                                                       |
		// +----------------------------------------------------------------------------------------------------------+
		// |  Returns the number of frames.  A single image file will return a value of 0.                            |
		// |                                                                                                          |
		// |  Throws std::runtime_error on error.                                                                     |
		// +----------------------------------------------------------------------------------------------------------+
		template <typename T> std::uint32_t CArcFitsFile<T>::getNumberOfFrames( void )
		{
			return static_cast< std::uint32_t >( getParameters()->getFrames() );
		}


		// +----------------------------------------------------------------------------------------------------------+
		// |  getRows                                                                                                 |
		// +----------------------------------------------------------------------------------------------------------+
		// |  Returns the number of rows in the image.                                                                |
		// |                                                                                                          |
		// |  Throws std::runtime_error on error.                                                                     |
		// +----------------------------------------------------------------------------------------------------------+
		template <typename T> std::uint32_t CArcFitsFile<T>::getRows( void )
		{
			return static_cast< std::uint32_t >( getParameters()->getRows() );
		}


		// +----------------------------------------------------------------------------------------------------------+
		// |  getCols																							      |
		// +----------------------------------------------------------------------------------------------------------+
		// |  Returns the number of columns in the image.                                                             |
		// |                                                                                                          |
		// |  Throws std::runtime_error on error.                                                                     |
		// +----------------------------------------------------------------------------------------------------------+
		template <typename T> std::uint32_t CArcFitsFile<T>::getCols( void )
		{
			return static_cast< std::uint32_t >( getParameters()->getCols() );
		}


		// +----------------------------------------------------------------------------------------------------------+
		// |  getNAxis                                                                                                |
		// +----------------------------------------------------------------------------------------------------------+
		// |  Returns the number of dimensions in the image.                                                          |
		// |                                                                                                          |
		// |  Throws std::runtime_error on error.                                                                     |
		// +----------------------------------------------------------------------------------------------------------+
		template <typename T> std::uint32_t CArcFitsFile<T>::getNAxis( void )
		{
			return static_cast< std::uint32_t >( getParameters()->getNAxis() );
		}


		// +----------------------------------------------------------------------------------------------------------+
		// |  getBitsPerPixel                                                                                         |
		// +----------------------------------------------------------------------------------------------------------+
		// |  Returns the image bits-per-pixel value.                                                                 |
		// |                                                                                                          |
		// |  Throws std::runtime_error on error.                                                                     |
		// +----------------------------------------------------------------------------------------------------------+
		template <typename T> std::uint32_t CArcFitsFile<T>::getBitsPerPixel( void )
		{
			return static_cast< std::uint32_t >( getParameters()->getBpp() );
		}


		// +----------------------------------------------------------------------------------------------------------+
		// |  generateTestData                                                                                        |
		// +----------------------------------------------------------------------------------------------------------+
		// |  Generates a ramp test pattern image within the file. The size of the image is determined by the image   |
		// |  dimensions supplied during the create() method call.This method is only valid for single image files.   |
		// |                                                                                                          |
		// |  Throws std::runtime_error on error.                                                                     |
		// +----------------------------------------------------------------------------------------------------------+
		template <typename T> void CArcFitsFile<T>::generateTestData( void )
		{
			std::size_t  iNElements = 0;
			std::int64_t i64FPixel = 1;
			std::int32_t iStatus = 0;

			VERIFY_FILE_HANDLE()

				auto pParam = getParameters();

			if ( pParam->getNAxis() > 2 )
			{
				THROW( "This method only supports single 2-D image files." );
			}

			//
			// Set number of pixels to write
			//
			iNElements = static_cast< std::int64_t >( pParam->getCols() * pParam->getRows() );

			std::unique_ptr<T[]> pBuf( new T[ iNElements ] );

			T uiValue = 0;

			for ( std::size_t i = 0; i < iNElements; i++ )
			{
				pBuf.get()[ i ] = uiValue;

				uiValue++;

				if ( uiValue >= maxTVal() )
				{
					uiValue = 0;
				}
			}

			fits_write_img( m_pFits,
				( sizeof( T ) == sizeof( std::uint16_t ) ? TUSHORT : TUINT ),
				i64FPixel,
				iNElements,
				pBuf.get(),
				&iStatus );

			if ( iStatus )
			{
				throwFitsError( iStatus );
			}
		}


		// +----------------------------------------------------------------------------------------------------------+
		// |  reOpen                                                                                                  |
		// +----------------------------------------------------------------------------------------------------------+
		// |  Effectively closes and re-opens the underlying disk file.                                               |
		// |                                                                                                          |
		// |  Throws std::runtime_error on error.                                                                     |
		// +----------------------------------------------------------------------------------------------------------+
		template <typename T> void CArcFitsFile<T>::reOpen( void )
		{
			std::int32_t	iStatus = 0;
			std::int32_t	iIOMode = 0;

			std::string sFilename = getFileName();

			fits_file_mode( m_pFits, &iIOMode, &iStatus );

			if ( iStatus )
			{
				throwFitsError( iStatus );
			}

			close();

			fits_open_file( &m_pFits, sFilename.c_str(), iIOMode, &iStatus );

			if ( iStatus )
			{
				close();

				throwFitsError( iStatus );
			}
		}


		// +----------------------------------------------------------------------------------------------------------+
		// |  flush                                                                                                   |
		// +----------------------------------------------------------------------------------------------------------+
		// |  Causes all internal data buffers to write data to the disk file.                                        |
		// |                                                                                                          |
		// |  Throws std::runtime_error on error.                                                                     |
		// +----------------------------------------------------------------------------------------------------------+
		template <typename T> void CArcFitsFile<T>::flush( void )
		{
			std::int32_t iStatus = 0;

			VERIFY_FILE_HANDLE()

				fits_flush_file( m_pFits, &iStatus );

			if ( iStatus )
			{
				throwFitsError( iStatus );
			}
		}


		// +----------------------------------------------------------------------------------------------------------+
		// |  compare ( Single Images )                                                                               |
		// +----------------------------------------------------------------------------------------------------------+
		// |  Compares this file image data to another. This method does not check headers, except for size values,   |
		// | and does not throw any exceptions.See the error message parameter if a <i>false< / i> value is returned. |
		// |                                                                                                          |
		// |  <IN> - > cFitsFile - Reference to FITS file to compare.	                                              |
		// |  <IN> - > psErrMsg - The reason for failure, should it occur. ( default = nullptr ).                     |
		// +----------------------------------------------------------------------------------------------------------+
		template <typename T> bool CArcFitsFile<T>::compare( CArcFitsFile<T>& cFitsFile, std::string* psErrMsg )
		{
			bool bMatch = true;

			try
			{
				auto pParam = cFitsFile.getParameters();

				auto pThisParams = getParameters();

				//
				//  Verify file dimensions
				//
				if ( pThisParams->getNAxis() != pParam->getNAxis() )
				{
					THROW( "Comparison file dimensions DO NOT match! This: %u Passed: %u.",
						pThisParams->getNAxis(),
						pParam->getNAxis() );
				}

				//
				//  Verify image dimensions
				//
				if ( ( pThisParams->getCols() != pParam->getCols() ) || ( pThisParams->getRows() != pParam->getRows() ) )
				{
					THROW( "Image dimensions of comparison files DO NOT match! This: %ux%u Passed: %ux%u.",
						pThisParams->getCols(),
						pThisParams->getRows(),
						pParam->getCols(),
						pParam->getRows() );
				}

				if ( pThisParams->getBpp() != pParam->getBpp() )
				{
					THROW( "Image bits-per-pixel of comparison files DO NOT match! This: %u Passed: %u.",
						pThisParams->getBpp(),
						pParam->getBpp() );
				}

				//
				//  Read input file image buffer
				//
				auto pBuf = cFitsFile.read();

				if ( pBuf.get() == nullptr )
				{
					THROW( "Failed to read passed FITS file image data!" );
				}

				//
				//  Read this file image buffer
				//
				auto pThisBuf = read();

				if ( pThisBuf.get() == nullptr )
				{
					THROW( "Failed to read this FITS file image data!" );
				}

				//
				//  compare image buffers
				//
				for ( std::uint32_t r = 0; r < pThisParams->getRows(); r++ )
				{
					for ( std::uint32_t c = 0; c < pThisParams->getCols(); c++ )
					{
						auto uIndex = c + r * pThisParams->getCols();

						if ( pThisBuf.get()[ uIndex ] != pBuf.get()[ uIndex ] )
						{
							THROW( "Images do not match at col: %u, row: %u, this: %u, passed: %u",
								c,
								r,
								pThisBuf.get()[ uIndex ],
								pBuf.get()[ uIndex ] );
						}
					}
				}
			}
			catch ( ... )
			{
				bMatch = false;
			}

			return bMatch;
		}


		// +----------------------------------------------------------------------------------------------------------+
		// |  reSize ( Single Image )                                                                                 |
		// +----------------------------------------------------------------------------------------------------------+
		// |  Resizes a single image file by modifying the NAXES keyword and increasing the image data portion of     |
		// |  the file.                                                                                               |
		// |                                                                                                          |
		// |  <IN> -> uiCols - The number of image columns the new file will have.                                    |
		// |  <IN> -> uiRows - The number of image rows the new file will have.                                       |
		// |                                                                                                          |
		// |  Throws std::runtime_error, std::invalid_argument                                                        |
		// +----------------------------------------------------------------------------------------------------------+
		template <typename T> void CArcFitsFile<T>::reSize( std::uint32_t uiCols, std::uint32_t uiRows )
		{
			std::int32_t  iStatus = 0;

			VERIFY_FILE_HANDLE()

				auto pParam = getParameters();

			//
			// Verify NAXIS parameter
			//
			if ( pParam->getNAxis() != 2 )
			{
				THROW_INVALID_ARGUMENT( "Invalid NAXIS value. This method is only valid for a file containing a single image." );
			}

			*pParam->m_plCols = uiCols;
			*pParam->m_plRows = uiRows;

			fits_resize_img( m_pFits,
				pParam->m_iBpp,
				pParam->m_iNAxis,
				pParam->m_lNAxes,
				&iStatus );

			if ( iStatus )
			{
				throwFitsError( iStatus );
			}
		}


		// +----------------------------------------------------------------------------------------------------------+
		// |  write ( Single Image )                                                                                  |
		// +----------------------------------------------------------------------------------------------------------+
		// |  Writes image data to a single image file.                                                               |
		// |                                                                                                          |
		// |  <IN> -> pBuf - Pointer to the image data.                                                               |
		// |                                                                                                          |
		// |  Throws std::runtime_error, std::invalid_argument                                                        |
		// +----------------------------------------------------------------------------------------------------------+
		template <typename T> void CArcFitsFile<T>::write( T* pBuf )
		{
			std::int64_t i64NElements = 0;
			std::int64_t i64FPixel = 1;
			std::int32_t iStatus = 0;

			VERIFY_FILE_HANDLE()

				//
				// Verify buffer pointer
				//
				if ( pBuf == nullptr )
				{
					THROW_INVALID_ARGUMENT( "Invalid data buffer." );
				}

			//
			// Verify parameters
			//
			auto pParam = getParameters();

			if ( pParam->getNAxis() != 2 )
			{
				THROW_INVALID_ARGUMENT( "Invalid NAXIS value. This method is only valid for a file containing a single image." );
			}

			//
			// Set number of pixels to write
			//
			i64NElements = ( pParam->getCols() * pParam->getRows() );

			//
			// Write image data
			//
			fits_write_img( m_pFits,
				( sizeof( T ) == sizeof( std::uint16_t ) ? TUSHORT : TUINT ),
				i64FPixel,
				i64NElements,
				pBuf,
				&iStatus );

			if ( iStatus )
			{
				throwFitsError( iStatus );
			}

			//
			// Force data flush for more real-time performance
			//
			fits_flush_file( m_pFits, &iStatus );

			if ( iStatus )
			{
				throwFitsError( iStatus );
			}
		}


		// +----------------------------------------------------------------------------------------------------------+
		// |  write ( Single Image )                                                                                  |
		// +----------------------------------------------------------------------------------------------------------+
		// |  Writes the specified number of bytes to a single image file. The start position of the data within the  |
		// |  file image can be specified.                                                                            |
		// |                                                                                                          |
		// |  <IN> -> pBuf		- Pointer to the data to write.                                                       |
		// |  <IN> -> i64Bytes  - The number of bytes to write.                                                       |
		// |  <IN> -> i64Pixel  - The start pixel within the file image. This parameter is optional. If -1, then the  |
		// |                      next write position will be at zero.                                                |
		// |                                                                                                          |
		// |  Throws std::runtime_error, std::invalid_argument                                                        |
		// +----------------------------------------------------------------------------------------------------------+
		template <typename T> void CArcFitsFile<T>::write( T* pBuf, std::int64_t i64Bytes, std::int64_t i64Pixel )
		{
			std::int64_t i64NElements = 0;
			std::int32_t iStatus = 0;

			bool bMultiWrite = false;

			VERIFY_FILE_HANDLE()

				auto pParam = getParameters();

			//
			// Verify parameters
			//
			if ( pParam->getNAxis() != 2 )
			{
				THROW_INVALID_ARGUMENT( "Invalid NAXIS value. This method is only valid for a file containing a single image." );
			}

			if ( pBuf == nullptr )
			{
				THROW_INVALID_ARGUMENT( "Invalid data buffer." );
			}

			if ( i64Pixel >= static_cast< std::int64_t >( pParam->getCols() ) * static_cast< std::int64_t >( pParam->getRows() ) )
			{
				THROW_INVALID_ARGUMENT( "Invalid start position, pixel position outside image size." );
			}

			//
			// Set the start pixel ( position ) within the file.
			//
			if ( i64Pixel < 0 && m_i64Pixel == 0 )
			{
				m_i64Pixel = 1;

				bMultiWrite = true;
			}

			else if ( i64Pixel == 0 && m_i64Pixel != 0 )
			{
				m_i64Pixel = 1;

				bMultiWrite = true;
			}

			else if ( i64Pixel < 0 && m_i64Pixel != 0 )
			{
				bMultiWrite = true;
			}

			else
			{
				m_i64Pixel = i64Pixel + 1;

				bMultiWrite = false;
			}

			//
			// Verify the start position
			//
			if ( m_i64Pixel >= static_cast< std::int64_t >( pParam->getCols() ) * static_cast< std::int64_t >( pParam->getRows() ) )
			{
				THROW( "Invalid start position, pixel position outside image size." );
			}

			//
			// Number of pixels to write
			//
			i64NElements = i64Bytes / sizeof( T );

			fits_write_img( m_pFits,
				( sizeof( T ) == sizeof( std::uint16_t ) ? TUSHORT : TUINT ),
				static_cast< LONGLONG >( m_i64Pixel ),
				static_cast< LONGLONG >( i64NElements ),
				pBuf,
				&iStatus );

			if ( iStatus )
			{
				throwFitsError( iStatus );
			}

			if ( bMultiWrite )
			{
				m_i64Pixel += i64NElements;
			}

			//
			// Force data flush for more real-time performance
			//
			fits_flush_file( m_pFits, &iStatus );

			if ( iStatus )
			{
				throwFitsError( iStatus );
			}

		}


		// +----------------------------------------------------------------------------------------------------------+
		// |  writeSubImage ( Single Image )                                                                          |
		// +----------------------------------------------------------------------------------------------------------+
		// |  Writes a sub-image of the specified buffer to a single image file.                                      |
		// |                                                                                                          |
		// |  <IN> -> pBuf	          - The pixel data.                                                               |
		// |  <IN> -> lowerLeftPoint  - The lower left point { col, row } of the sub-image.                           |
		// |  <IN> -> upperRightPoint - The upper right point { col, row } of the sub-image.                          |
		// |                                                                                                          |
		// |  Throws std::runtime_error, std::invalid_argument                                                        |
		// +----------------------------------------------------------------------------------------------------------+
		template <typename T>
		void CArcFitsFile<T>::writeSubImage( T* pBuf, arc::gen3::fits::Point lowerLeftPoint, arc::gen3::fits::Point upperRightPoint )
		{
			std::int32_t iStatus = 0;

			VERIFY_FILE_HANDLE()

				auto pParam = getParameters();

			//
			// Verify parameters
			//
			if ( pParam->getNAxis() != 2 )
			{
				THROW_INVALID_ARGUMENT( "Invalid NAXIS value. This method is only valid for a file containing a single image." );
			}

			if ( lowerLeftPoint.second > upperRightPoint.second )
			{
				THROW_INVALID_ARGUMENT( "Invalid LOWER LEFT ROW parameter!" );
			}

			if ( lowerLeftPoint.first > upperRightPoint.first )
			{
				THROW_INVALID_ARGUMENT( "Invalid LOWER LEFT COLUMN parameter!" );
			}

			if ( lowerLeftPoint.second < 0 || lowerLeftPoint.second >= static_cast< long >( pParam->getRows() ) )
			{
				THROW_INVALID_ARGUMENT( "Invalid LOWER LEFT ROW parameter!" );
			}

			if ( upperRightPoint.second < 0 || upperRightPoint.second >= static_cast< long >( pParam->getRows() ) )
			{
				THROW_INVALID_ARGUMENT( "Invalid UPPER RIGHT ROW parameter!" );
			}

			if ( lowerLeftPoint.first < 0 || lowerLeftPoint.first >= static_cast< long >( pParam->getCols() ) )
			{
				THROW_INVALID_ARGUMENT( "Invalid LOWER LEFT COLUMN parameter!" );
			}

			if ( upperRightPoint.first < 0 || upperRightPoint.first >= static_cast< long >( pParam->getCols() ) )
			{
				THROW_INVALID_ARGUMENT( "Invalid UPPER RIGHT COLUMN parameter!" );
			}

			if ( pBuf == nullptr )
			{
				THROW_INVALID_ARGUMENT( "Invalid data buffer." );
			}

			//
			// Set the subset start pixels
			//
			long lFirstPixel[] = { lowerLeftPoint.first + 1, lowerLeftPoint.second + 1 };
			long lLastPixel[] = { upperRightPoint.first + 1, upperRightPoint.second + 1 };

			fits_write_subset( m_pFits,
				( sizeof( T ) == sizeof( std::uint16_t ) ? TUSHORT : TUINT ),
				lFirstPixel,
				lLastPixel,
				pBuf,
				&iStatus );

			if ( iStatus )
			{
				throwFitsError( iStatus );
			}

			//
			// Force data flush for more real-time performance
			//
			fits_flush_file( m_pFits, &iStatus );

			if ( iStatus )
			{
				throwFitsError( iStatus );
			}
		}


		// +----------------------------------------------------------------------------------------------------------+
		// |  readSubImage ( Single Image )                                                                           |
		// +----------------------------------------------------------------------------------------------------------+
		// |  Reads a sub-image from a single image file.                                                             |
		// |                                                                                                          |
		// |  <IN> -> lowerLeftPoint  - The lower left point { col, row } of the sub-image.                           |
		// |  <IN> -> upperRightPoint - The upper right point { col, row } of the sub-image.                          |
		// |                                                                                                          |
		// |  Throws std::runtime_error, std::invalid_argument                                                        |
		// +----------------------------------------------------------------------------------------------------------+
		template <typename T> std::unique_ptr<T[], arc::gen3::fits::ArrayDeleter<T>>
			CArcFitsFile<T>::readSubImage( arc::gen3::fits::Point lowerLeftPoint, arc::gen3::fits::Point upperRightPoint )
			{
				std::int32_t iStatus = 0;
				std::int32_t iAnyNul = 0;

				VERIFY_FILE_HANDLE()

					auto pParam = getParameters();

				//
				// Verify parameters
				//
				if ( pParam->getNAxis() != 2 )
				{
					THROW_INVALID_ARGUMENT( "Invalid NAXIS value. This method is only valid for a file containing a single image." );
				}

				if ( lowerLeftPoint.second > upperRightPoint.second )
				{
					THROW_INVALID_ARGUMENT( "Invalid LOWER LEFT ROW parameter!" );
				}

				if ( lowerLeftPoint.first > upperRightPoint.first )
				{
					THROW_INVALID_ARGUMENT( "Invalid LOWER LEFT COLUMN parameter!" );
				}

				if ( lowerLeftPoint.second < 0 || lowerLeftPoint.second >= static_cast< long >( pParam->getRows() ) )
				{
					THROW_INVALID_ARGUMENT( "Invalid LOWER LEFT ROW parameter!" );
				}

				if ( upperRightPoint.second < 0 || upperRightPoint.second >= static_cast< long >( pParam->getRows() ) )
				{
					THROW_INVALID_ARGUMENT( "Invalid UPPER RIGHT ROW parameter!" );
				}

				if ( lowerLeftPoint.first < 0 || lowerLeftPoint.first >= static_cast< long >( pParam->getCols() ) )
				{
					THROW_INVALID_ARGUMENT( "Invalid LOWER LEFT COLUMN parameter!" );
				}

				if ( upperRightPoint.first < 0 || upperRightPoint.first >= static_cast< long >( pParam->getCols() ) )
				{
					THROW_INVALID_ARGUMENT( "Invalid UPPER RIGHT COLUMN parameter!" );
				}

				if ( upperRightPoint.first == static_cast< long >( pParam->getCols() ) )
				{
					upperRightPoint.first = ( static_cast< long >( pParam->getCols() ) - 1 );
				}

				if ( upperRightPoint.second == static_cast< long >( pParam->getRows() ) )
				{
					upperRightPoint.second = ( static_cast< long >( pParam->getRows() ) - 1 );
				}

				//
				// Set the subset start pixels
				//
				long lFirstPixel[] = { lowerLeftPoint.first + 1, lowerLeftPoint.second + 1 };
				long lLastPixel[] = { upperRightPoint.first + 1, upperRightPoint.second + 1 };

				//
				// The read routine also has an inc parameter which can be used to
				// read only every inc-th pixel along each dimension of the image.
				// Normally inc[0] = inc[1] = 1 to read every pixel in a 2D image.
				// To read every other pixel in the entire 2D image, set 
				//
				long lInc[] = { 1, 1 };

				//
				// Set the data length ( in pixels )
				//
				std::uint32_t uiDataLength = ( pParam->getCols() * pParam->getRows() );

				std::unique_ptr<T[], arc::gen3::fits::ArrayDeleter<T>> pSubBuf( new T[ uiDataLength ], arc::gen3::fits::ArrayDeleter<T>() );

				if ( pSubBuf.get() == nullptr )
				{
					THROW( "Failed to allocate buffer for image pixel data." );
				}

				fits_read_subset( m_pFits,
					( sizeof( T ) == sizeof( std::uint16_t ) ? TUSHORT : TUINT ),
					lFirstPixel,
					lLastPixel,
					lInc,
					0,
					pSubBuf.get(),
					&iAnyNul,
					&iStatus );

				if ( iStatus )
				{
					throwFitsError( iStatus );
				}

				return pSubBuf;
			}


			// +----------------------------------------------------------------------------------------------------------+
			// |  Read ( Single Image )                                                                                   |
			// +----------------------------------------------------------------------------------------------------------+
			// |  Read the image from a single image file. Returns a pointer to the image data.                           |
			// |                                                                                                          |
			// |  Throws std::runtime_error, std::invalid_argument                                                        |
			// +----------------------------------------------------------------------------------------------------------+
			template <typename T> std::unique_ptr<T[], arc::gen3::fits::ArrayDeleter<T>> CArcFitsFile<T>::read( void )
			{
				std::int32_t iStatus = 0;

				VERIFY_FILE_HANDLE()

					auto pParam = getParameters();

				//
				// Verify NAXIS parameter
				//
				if ( pParam->getNAxis() != 2 )
				{
					THROW_INVALID_ARGUMENT( "Invalid NAXIS value. This method is only valid for a file containing a single image." );
				}

				//
				// Set the data length ( in pixels )
				//
				std::uint32_t uiDataLength = ( pParam->getCols() * pParam->getRows() );

				std::unique_ptr<T[], arc::gen3::fits::ArrayDeleter<T>> pImgBuf( new T[ uiDataLength ], arc::gen3::fits::ArrayDeleter<T>() );

				if ( pImgBuf.get() == nullptr )
				{
					THROW( "Failed to allocate buffer for image pixel data." );
				}

				//
				// Read the image data
				//
				fits_read_img( m_pFits,
					( sizeof( T ) == sizeof( std::uint16_t ) ? TUSHORT : TUINT ),
					1,
					static_cast< long >( uiDataLength ),
					nullptr,
					pImgBuf.get(),
					nullptr,
					&iStatus );

				if ( iStatus )
				{
					throwFitsError( iStatus );
				}

				return pImgBuf;
			}


			// +----------------------------------------------------------------------------------------------------------+
			// |  Read ( Single Image )                                                                                   |
			// +----------------------------------------------------------------------------------------------------------+
			// |  Read the image from a single image file into the user supplied buffer.                                  |
			// |                                                                                                          |
			// |  <IN> -> pBuf   - A pointer to the user supplied image buffer.                                           |
			// |  <IN> -> uiCols - The column length of the user image buffer ( in pixels ).                              |
			// |  <IN> -> uiRows - The row length of the user image buffer ( in pixels ).                                 |
			// |                                                                                                          |
			// |  Throws std::runtime_error, std::invalid_argument, std::length_error                                     |
			// +----------------------------------------------------------------------------------------------------------+
			template <typename T> void CArcFitsFile<T>::read( T* pBuf, std::uint32_t uiCols, std::uint32_t uiRows )
			{
				std::int32_t iStatus = 0;

				VERIFY_FILE_HANDLE()

					auto pParam = getParameters();

				//
				// Verify NAXIS parameter
				//
				if ( pParam->getNAxis() != 2 )
				{
					THROW_INVALID_ARGUMENT( "Invalid NAXIS value. This method is only valid for a file containing a single image." );
				}

				//
				// Set the data length ( in pixels )
				//
				std::uint32_t uiDataLength = ( pParam->getCols() * pParam->getRows() );

				//
				// Verify the buffer dimensions
				//
				if ( uiDataLength > ( uiCols * uiRows ) )
				{
					THROW_LENGTH_ERROR( "Error, user supplied buffer is too small. Expected: %d bytes, Supplied: %d bytes.", uiDataLength, ( uiCols * uiRows ) );
				}

				if ( pBuf == nullptr )
				{
					THROW_INVALID_ARGUMENT( "Invalid image buffer parameter ( nullptr )." );
				}

				//
				// Read the image data
				//
				fits_read_img( m_pFits,
					( sizeof( T ) == sizeof( std::uint16_t ) ? TUSHORT : TUINT ),
					1,
					static_cast< long >( uiDataLength ),
					nullptr,
					pBuf,
					nullptr,
					&iStatus );

				if ( iStatus )
				{
					throwFitsError( iStatus );
				}
			}


			// +----------------------------------------------------------------------------------------------------------+
			// |  write3D ( Data Cube )                                                                                   |
			// +----------------------------------------------------------------------------------------------------------+
			// |  Writes an image to the end of a data cube file.                                                         |
			// |                                                                                                          |
			// |  <IN> -> pBuf - A pointer to the image data.                                                             |
			// |                                                                                                          |
			// |  Throws std::runtime_error, std::invalid_argument                                                        |
			// +----------------------------------------------------------------------------------------------------------+
			template <typename T> void CArcFitsFile<T>::write3D( T* pBuf )
			{
				std::int64_t i64NElements = 0;
				std::int32_t iStatus = 0;

				VERIFY_FILE_HANDLE()

					auto pParam = getParameters();

				*pParam->m_plFrames = ++m_iFrame;

				//
				// Verify parameters
				//
				if ( pParam->getNAxis() != 3 )
				{
					THROW_INVALID_ARGUMENT( "Invalid NAXIS value. This method is only valid for a FITS data cube." );
				}

				if ( pBuf == nullptr )
				{
					THROW_INVALID_ARGUMENT( "Invalid data buffer ( write3D )." );
				}

				//
				// Set number of pixels to write
				//
				i64NElements = static_cast< std::int64_t >( pParam->getCols() * pParam->getRows() );

				if ( m_i64Pixel == 0 )
				{
					m_i64Pixel = 1;
				}

				fits_write_img( m_pFits,
					( sizeof( T ) == sizeof( std::uint16_t ) ? TUSHORT : TUINT ),
					m_i64Pixel,
					i64NElements,
					pBuf,
					&iStatus );

				if ( iStatus )
				{
					throwFitsError( iStatus );
				}

				//
				// Update the start pixel
				//
				m_i64Pixel += i64NElements;

				//
				// Increment the image number and update the key
				//
				fits_update_key( m_pFits,
					TINT,
					"NAXIS3",
					&m_iFrame,
					nullptr,
					&iStatus );

				if ( iStatus )
				{
					throwFitsError( iStatus );
				}

				//
				// Force data flush for more real-time performance
				//
				fits_flush_file( m_pFits, &iStatus );

				if ( iStatus )
				{
					throwFitsError( iStatus );
				}
			}


			// +----------------------------------------------------------------------------------------------------------+
			// |  reWrite3D ( Data Cube )                                                                                 |
			// +----------------------------------------------------------------------------------------------------------+
			// |  Re-writes an existing image in a FITS data cube. The image data MUST match in size to the exising       |
			// |  images within the data cube. The image size is NOT checked for by this method.                          |
			// |                                                                                                          |
			// |  <IN> -> pBuf			- A pointer to the image data.                                                    |
			// |  <IN> -> uiImageNumber	- The number of the data cube image to replace.                                   |
			// |                                                                                                          |
			// |  Throws std::runtime_error, std::invalid_argument                                                        |
			// +----------------------------------------------------------------------------------------------------------+
			template <typename T> void CArcFitsFile<T>::reWrite3D( T* pBuf, std::uint32_t uiImageNumber )
			{
				std::int64_t i64NElements = 0;
				std::int64_t i64Pixel = 0;
				std::int32_t iStatus = 0;

				VERIFY_FILE_HANDLE()

					auto pParam = getParameters();

				//
				// Verify parameters
				//
				if ( pParam->getNAxis() != 3 )
				{
					THROW_INVALID_ARGUMENT( "Invalid NAXIS value. This method is only valid for a FITS data cube." );
				}

				if ( pBuf == nullptr )
				{
					THROW_INVALID_ARGUMENT( "Invalid data buffer." );
				}

				//
				// Set number of pixels to write; also set the start position
				//
				i64NElements = static_cast< std::int64_t >( pParam->getCols() ) * static_cast< std::int64_t >( pParam->getRows() );

				i64Pixel = i64NElements * uiImageNumber + 1;

				fits_write_img( m_pFits,
					( sizeof( T ) == sizeof( std::uint16_t ) ? TUSHORT : TUINT ),
					i64Pixel,
					i64NElements,
					pBuf,
					&iStatus );

				if ( iStatus )
				{
					throwFitsError( iStatus );
				}

				//
				// Force data flush for more real-time performance
				//
				fits_flush_file( m_pFits, &iStatus );

				if ( iStatus )
				{
					throwFitsError( iStatus );
				}
			}


			// +----------------------------------------------------------------------------------------------------------+
			// |  read3D ( Data Cube )                                                                                    |
			// +----------------------------------------------------------------------------------------------------------+
			// |  Reads an image from a data cube file.                                                                   |
			// |                                                                                                          |
			// |  <IN>  -> uiImageNumber - The image number to read.                                                      |
			// |                                                                                                          |
			// |  Throws std::runtime_error, std::invalid_argument                                                        |
			// +----------------------------------------------------------------------------------------------------------+
			template <typename T>
			std::unique_ptr<T[], arc::gen3::fits::ArrayDeleter<T>> CArcFitsFile<T>::read3D( std::uint32_t uiImageNumber )
			{
				std::size_t  iNElements = 0;
				std::int64_t i64Pixel = 0;
				std::int32_t iStatus = 0;

				VERIFY_FILE_HANDLE()

					auto pParam = getParameters();

				if ( pParam->getNAxis() != 3 )
				{
					THROW_INVALID_ARGUMENT( "Invalid NAXIS value. This method is only valid for a FITS data cube." );
				}

				//
				// Verify parameters
				//
				if ( ( uiImageNumber + 1 ) > pParam->getFrames() )
				{
					THROW_INVALID_ARGUMENT( "Invalid image number. File contains %u images.", pParam->getFrames() );
				}

				//
				// Set number of pixels to read
				//
				iNElements = static_cast< std::int64_t >( pParam->getCols() * pParam->getRows() );

				i64Pixel = iNElements * uiImageNumber + 1;

				std::unique_ptr<T[], arc::gen3::fits::ArrayDeleter<T>> pImgBuf( new T[ iNElements ], arc::gen3::fits::ArrayDeleter<T>() );

				if ( pImgBuf.get() == nullptr )
				{
					THROW( "Failed to allocate buffer for image pixel data." );
				}

				//
				// Read the image data
				//
				fits_read_img( m_pFits,
					( sizeof( T ) == sizeof( std::uint16_t ) ? TUSHORT : TUINT ),
					i64Pixel,
					iNElements,
					nullptr,
					pImgBuf.get(),
					nullptr,
					&iStatus );

				if ( iStatus )
				{
					throwFitsError( iStatus );
				}

				return pImgBuf;
			}


			// +----------------------------------------------------------------------------------------------------------+
			// |  getBaseFile                                                                                             |
			// +----------------------------------------------------------------------------------------------------------+
			// |  Returns the underlying cfitsio file pointer. May return nullptr.                                        |
			// +----------------------------------------------------------------------------------------------------------+
			template <typename T> fitsfile* CArcFitsFile<T>::getBaseFile( void )
			{
				return m_pFits;
			}


			// +----------------------------------------------------------------------------------------------------------+
			// |  maxTVal                                                                                                 |
			// +----------------------------------------------------------------------------------------------------------+
			// |  Determines the maximum value for a specific data type. Example, for std::uint16_t: 2^16 = 65536.        |
			// +----------------------------------------------------------------------------------------------------------+
			template <typename T> std::uint32_t CArcFitsFile<T>::maxTVal( void )
			{
				auto gExponent = ( ( sizeof( T ) == sizeof( arc::gen3::fits::BPP_32 ) ) ? 20 : ( sizeof( T ) * 8 ) );

				return static_cast< std::uint32_t >( std::pow( 2.0, gExponent ) );
			}


			// +----------------------------------------------------------------------------------------------------------+
			// |  ThrowException                                                                                          |
			// +----------------------------------------------------------------------------------------------------------+
			// |  Throws a std::runtime_error based on the supplied cfitsio status value.                                 |
			// |                                                                                                          |
			// |  <IN> -> iStatus : cfitsio error value.                                                                  |
			// +----------------------------------------------------------------------------------------------------------+
			template <typename T> void CArcFitsFile<T>::throwFitsError( std::int32_t iStatus )
			{
				char szFitsMsg[ 100 ];

				fits_get_errstatus( iStatus, szFitsMsg );

				THROW( "%s", szFitsMsg );
			}


	}	// end gen3 namespace
}		// end arc namespace



/** Explicit instantiations - These are the only allowed instantiations of this class */
template class arc::gen3::CArcFitsFile<arc::gen3::fits::BPP_16>;
template class arc::gen3::CArcFitsFile<arc::gen3::fits::BPP_32>;



// +------------------------------------------------------------------------------------------------+
// |  default_delete definition                                                                     |
// +------------------------------------------------------------------------------------------------+
// |  Creates a modified version of the std::default_delete class for use by all std::unique_ptr's  |
// |  that wrap a CStats/CDifStats object.                                                          |
// +------------------------------------------------------------------------------------------------+
namespace std
{

	void default_delete<arc::gen3::fits::CParam>::operator()( arc::gen3::fits::CParam* pObj )
	{
		if ( pObj != nullptr )
		{
			delete pObj;
		}
	}

}
















//// +---------------------------------------------------------------------------+
//// |  CArcFitsFile.cpp                                                         |
//// +---------------------------------------------------------------------------+
//// |  Defines the exported functions for the CArcFitsFile DLL.  Wraps the      |
//// |  cfitsio library for convenience and for use with Owl.                    |
//// |                                                                           |
//// |  Scott Streit                                                             |
//// |  Astronomical Research Cameras, Inc.                                      |
//// +---------------------------------------------------------------------------+
//#include <iostream>
//
//#include <typeinfo>
//#include <sstream>
//#include <cmath>
//#include <CArcFitsFile.h>
//
//using namespace std;
//using namespace arc::fits;
//
//
//// +---------------------------------------------------------------------------+
//// | ArrayDeleter                                                              |
//// +---------------------------------------------------------------------------+
//// | Called by std::shared_ptr to delete the temporary image buffer.           |
//// | This method should NEVER be called directly by the user.                  |
//// +---------------------------------------------------------------------------+
//template<typename T> void CArcFitsFile::ArrayDeleter( T* p )
//{
//	if ( p != NULL )
//	{
//		delete [] p;
//	}
//}
//
//// +---------------------------------------------------------------------------+
//// |  Copy constructor                                                         |
//// +---------------------------------------------------------------------------+
//// |  Copies an existing CArcFitsFile, headers only.  The image data is not    |
//// |  copied.                                                                  |
//// |                                                                           |
//// |  Throws std::runtime_error on error.                                      |
//// |                                                                           |
//// |  <IN> -> rAnotherFitsFile - The file to copy.                             |
//// +---------------------------------------------------------------------------+
//CArcFitsFile::CArcFitsFile( CArcFitsFile& rAnotherFitsFile, const char* pszNewFileName )
//{
//    int			dFitsStatus = 0;	// Initialize status before calling fitsio routines
//	fitsfile*	fptr		= NULL;
//
//	//  Read the other file parameters
//	// +------------------------------------------------------------+
//	long lNAxes[ CArcFitsFile::NAXES_SIZE ]	= { 0, 0, 0 };
//	int  dNAxis								= 0;
//	int  dBitsPerPixel						= 0;
//
//	rAnotherFitsFile.GetParameters( &lNAxes[ 0 ], &dNAxis, &dBitsPerPixel );
//
//	//  Create the new file
//	// +------------------------------------------------------------+
//	string sFilename( pszNewFileName );
//
//	if ( sFilename.empty() )
//	{
//		ThrowException( "CArcFitsFile",
//						 string( "Invalid file name : " ) + sFilename );
//	}
//
//	sFilename = "!" + sFilename;	// ! = Force the file to be overwritten
//	fits_create_file( &fptr, sFilename.c_str(), &dFitsStatus );
//
//	if ( dFitsStatus )
//	{
//		m_fptr.reset();
//
//		ThrowException( "CArcFitsFile", dFitsStatus );
//	}
//
//	m_fptr.reset( fptr );
//	m_pDataHeader.reset();
//	m_pDataBuffer.reset();
//
//	m_lFPixel = 0;
//	m_lFrame  = 0;
//
//	//  Copy the FITS header
//	// +------------------------------------------------------------+
//	fits_copy_header( rAnotherFitsFile.GetBaseFile(),
//					  m_fptr.get(),
//					  &dFitsStatus );
//
//	if ( dFitsStatus )
//	{
//		m_fptr.reset();
//
//		ThrowException( "CArcFitsFile", dFitsStatus );
//	}
//}
//
//// +---------------------------------------------------------------------------+
//// |  Constructor for OPENING an existing FITS file for reading and/or writing |
//// +---------------------------------------------------------------------------+
//// |  Opens an existing FITS file for reading. May contain a single image or   |
//// |  a FITS data cube.                                                        |
//// |                                                                           |
//// |  Throws std::runtime_error on error.                                      |
//// |                                                                           |
//// |  <IN> -> pszFilename - The file to open, including path.                  |
//// |  <IN> -> dMode       - Read/Write mode. Can be READMODE or READWRITEMODE. |
//// +---------------------------------------------------------------------------+
//CArcFitsFile::CArcFitsFile( const char* pszFilename, int dMode )
//{
//    int			dFitsStatus = 0;	// Initialize status before calling fitsio routines
//	fitsfile*	fptr		= NULL;
//	string		sFilename( pszFilename );
//
//	// Verify filename and make sure the kernel image
//	// buffer has been initialized.
//	// --------------------------------------------------
//	if ( sFilename.empty() )
//	{
//		ThrowException( "CArcFitsFile",
//						 string( "Invalid file name : " ) + sFilename );
//	}
//
//	// Open the FITS file
//	// --------------------------------------------------
//	fits_open_file( &fptr, sFilename.c_str(), dMode, &dFitsStatus );
//
//	if ( dFitsStatus )
//	{
//		m_fptr.reset();
//
//		ThrowException( "CArcFitsFile", dFitsStatus );
//	}
//
//	m_fptr.reset( fptr );
//	m_pDataHeader.reset();
//	m_pDataBuffer.reset();
//
//	m_lFPixel = 0;
//	m_lFrame  = 0;
//}
//
//// +---------------------------------------------------------------------------+
//// |  Constructor for CREATING a new FITS file for writing                     |
//// +---------------------------------------------------------------------------+
//// |  Creates a FITS file the will contain a single image.                     |
//// |                                                                           |
//// |  Throws std::runtime_error on error.                                      |
//// |                                                                           |
//// |  <IN> -> pszFilename   - The to create, including path.                   |
//// |  <IN> -> rows          - The number of rows in the image.                 |
//// |  <IN> -> cols          - The number of columns in the image.              |
//// |  <IN> -> dBitsPerPixel - Can be 16 ( BPP16 ) or 32 ( BPP32 ).             |
//// |  <IN> -> is3D          - True to create a fits data cube.                 |
//// +---------------------------------------------------------------------------+
//CArcFitsFile::CArcFitsFile( const char* pszFilename, int dRows, int dCols,
//						    int dBitsPerPixel, bool bIs3D )
//{
//	int			dFitsStatus = 0;		// Initialize status before calling fitsio routines
//	long*		plNaxes     = ( bIs3D ? new long[ 3 ] : new long[ 2 ] );	// { COLS, ROWS, PLANE# }
//	int			dNAxis      = ( bIs3D ? 3 : 2 );
//	int			dImageType  = 0;
//	fitsfile*	fptr		= NULL;
//
//	string sFilename( pszFilename );
//
//	if ( bIs3D )
//	{
//		plNaxes[ CArcFitsFile::NAXES_COL ] = dCols;
//		plNaxes[ CArcFitsFile::NAXES_ROW ] = dRows;
//		plNaxes[ CArcFitsFile::NAXES_NOF ] = 1;
//	}
//	else
//	{
//		plNaxes[ CArcFitsFile::NAXES_COL ] = dCols;
//		plNaxes[ CArcFitsFile::NAXES_ROW ] = dRows;
//	}
//
//	// Verify image dimensions
//	// --------------------------------------------------
//	if ( dRows <= 0 )
//	{
//		ThrowException( "CArcFitsFile",
//						"Row dimension should be greater than zero!" );
//	}
//
//	if ( dCols <= 0 )
//	{
//		ThrowException( "CArcFitsFile",
//						"Column dimension should be greater than zero!" );
//	}
//
//	// Verify filename
//	// --------------------------------------------------
//	if ( sFilename.empty() )
//	{
//		ThrowException( "CArcFitsFile",
//						 string( "Invalid file name : " ) + sFilename );
//	}
//
//	// Verify bits-per-pixel
//	// --------------------------------------------------
//	if ( dBitsPerPixel != CArcFitsFile::BPP16 && dBitsPerPixel != CArcFitsFile::BPP32 )
//	{
//		ThrowException( "CArcFitsFile",
//			"Invalid dBitsPerPixel, should be 16 ( BPP16 ) or 32 ( BPP32 )." );
//	}
//
//	// Create a new FITS file
//	// --------------------------------------------------
//	sFilename = "!" + sFilename;	// ! = Force the file to be overwritten
//	fits_create_file( &fptr, sFilename.c_str(), &dFitsStatus );
//
//	if ( dFitsStatus )
//	{
//		ThrowException( "CArcFitsFile", dFitsStatus );
//	}
//
//	m_fptr.reset( fptr );
//
//   	// Create the primary array image - 16-bit short integer pixels
//	// -------------------------------------------------------------
//	if ( dBitsPerPixel == CArcFitsFile::BPP16 ) { dImageType = USHORT_IMG; }
//	else { dImageType = ULONG_IMG; }
//
//	fits_create_img( m_fptr.get(), dImageType, dNAxis, plNaxes, &dFitsStatus );
//
//	if ( dFitsStatus )
//	{
//		ThrowException( "CArcFitsFile", dFitsStatus );
//	}
//
//	m_pDataHeader.reset();
//	m_pDataBuffer.reset();
//
//	m_lFPixel = 0;
//	m_lFrame  = 0;
//
//	delete [] plNaxes;
//}
//
//// +---------------------------------------------------------------------------+
//// |  Class destructor                                                         |
//// +---------------------------------------------------------------------------+
//// |  Destroys the class. Deallocates any header and data buffers. Closes any  |
//// |  open FITS pointers.                                                      |
//// |                                                                           |
//// |  Throws std::runtime_error on error.                                      |
//// +---------------------------------------------------------------------------+
//CArcFitsFile::~CArcFitsFile()
//{
//	m_pDataHeader.reset();
//	m_pDataBuffer.reset();
//
//	//  Using release because reset causes errors with glib.
//	//  Results in a "double free on pointer" error.
//	// -----------------------------------------------------
//	fitsfile* fptr = m_fptr.release();
//
//	//  DeleteBuffer requires access to the file,
//	//  so don't close the file until after DeleteBuffer
//	//  has been called!!!
//	// -----------------------------------------------------
//	if ( fptr != NULL )
//	{
//		int dFitsStatus = 0;
//
//		fits_close_file( fptr, &dFitsStatus );
//	}
//
//	m_lFPixel = 0;
//	m_lFrame  = 0;
//}
//
//// +---------------------------------------------------------------------------+
//// |  GetFilename                                                              |
//// +---------------------------------------------------------------------------+
//// |  Returns the filename associated with this CArcFitsFile object.           |
//// |                                                                           |
//// |  Throws std::runtime_error on error.                                      |
//// +---------------------------------------------------------------------------+
//const std::string CArcFitsFile::GetFilename()
//{
//	char szFilename[ 150 ];
//	int  dFitsStatus = 0;
//
//	// Verify FITS file handle
//	// --------------------------------------------------
//	if ( m_fptr.get() == NULL )
//	{
//		ThrowException( "GetFilename", "Invalid FITS handle, no file open" );
//	}
//
//	fits_file_name( m_fptr.get(), szFilename, &dFitsStatus );
//
//	if ( dFitsStatus )
//	{
//		ThrowException( "GetFilename", dFitsStatus );
//	}
//
//	return std::string( szFilename );
//}
//
//// +---------------------------------------------------------------------------+
//// | GetHeader                                                                 |
//// +---------------------------------------------------------------------------+
//// |  Returns the FITS header as an array of strings.                          |
//// |                                                                           |
//// |  Throws std::runtime_error on error.                                      |
//// |                                                                           |
//// |  <OUT> -> pKeyCount - The number of keys in the returned header array.    |
//// +---------------------------------------------------------------------------+
//string* CArcFitsFile::GetHeader( int* pKeyCount )
//{
//	int dFitsStatus = 0;
//	int dNumOfKeys  = 0;
//
//	// Verify FITS file handle
//	// --------------------------------------------------
//	if ( m_fptr.get() == NULL )
//	{
//		ThrowException( "GetHeader", "Invalid FITS handle, no file open" );
//	}
//
//	fits_get_hdrspace( m_fptr.get(), &dNumOfKeys, NULL, &dFitsStatus );
//
//	if ( dFitsStatus )
//	{
//		ThrowException( "GetHeader", dFitsStatus );
//	}
//
//	m_pDataHeader.reset( new string[ dNumOfKeys ], &CArcFitsFile::ArrayDeleter<string> ); //ArrayDeleter<string>() ); //&CArcFitsFile::ArrayDeleter );
//
//	char pszCard[ 100 ];
//
//	for ( int i=0; i<dNumOfKeys; i++ )
//	{
//		fits_read_record( m_fptr.get(),
//						  ( i + 1 ),
//						  &pszCard[ 0 ],
//						  &dFitsStatus );
//
//		if ( dFitsStatus )
//		{
//			ThrowException( "GetHeader", dFitsStatus );
//		}
//
//		m_pDataHeader.get()[ i ] = string( pszCard );
//	}
//
//	*pKeyCount = dNumOfKeys;
//
//	return m_pDataHeader.get();
//}
//
//// +---------------------------------------------------------------------------+
//// |  WriteKeyword                                                             |
//// +---------------------------------------------------------------------------+
//// |  Writes a FITS keyword to an existing FITS file.  The keyword must be     |
//// |  valid or an exception will be thrown. For list of valid FITS keywords,   |
//// |  see:                                                                     |
//// |                                                                           |
//// |  http://heasarc.gsfc.nasa.gov/docs/fcg/standard_dict.html                 |
//// |  http://archive.stsci.edu/fits/fits_standard/node38.html# \               |
//// |  SECTION00940000000000000000                                              |
//// |                                                                           |
//// |  'HIERARCH' keyword NOTE: This text will be prefixed to any keyword by    |
//// |                           the cfitsio library if the keyword is greater   |
//// |                           than 8 characters, which is the standard FITS   |
//// |                           keyword length. See the link below for details: |
//// |   http://heasarc.gsfc.nasa.gov/docs/software/fitsio/c/f_user/node28.html  |
//// |                                                                           |
//// |   HIERARCH examples:                                                      |
//// |   -----------------                                                       |
//// |   HIERARCH LongKeyword = 47.5 / Keyword has > 8 characters & mixed case   |
//// |   HIERARCH XTE$TEMP = 98.6 / Keyword contains the '$' character           |
//// |   HIERARCH Earth is a star = F / Keyword contains embedded spaces         |
//// |                                                                           |
//// |  Throws std::runtime_error on error.                                      |
//// |                                                                           |
//// |  <IN> -> pszKey    - The header keyword. Can be NULL. Ex: SIMPLE          |
//// |  <IN> -> pKeyVal   - The value associated with the key. Ex: T             |
//// |  <IN> -> dValType  - The keyword value type, as defined in CArcFitsFile.h |
//// |  <IN> -> pszComment - The comment to attach to the keyword.  Can be NULL. |
//// +---------------------------------------------------------------------------+
//void CArcFitsFile::WriteKeyword( char* pszKey, void* pKeyVal, int dValType, char* pszComment )
//{
//    int dFitsStatus = 0;
//
//	if ( pszKey != nullptr )
//	{
//		cout << "FITS KEY -> \"" << pszKey << "\" TYPE -> " << dValType << std::endl;
//	}
//	else
//	{
//		cout << "TYPE -> " << dValType << std::endl;
//	}
//
//	// Verify FITS file handle
//	// --------------------------------------------------
//	if ( m_fptr.get() == NULL )
//	{
//		ThrowException( "WriteKeyword",
//						"Invalid FITS handle, no file open" );
//	}
//
//	// Verify value pointer
//	// --------------------------------------------------
//	if ( dValType != FITS_DATE_KEY && pKeyVal == NULL )
//	{
//		ThrowException( "WriteKeyword",
//						"Invalid FITS value, cannot be NULL" );
//	}
//
//	// Update the fits header with the specified keys
//	// -------------------------------------------------------------
//	switch ( dValType )
//	{
//		case FITS_STRING_KEY:
//		{
//			cout << "FITS_STRING_KEY VAL -> " << *( reinterpret_cast< char* >( pKeyVal ) ) << std::endl;
//
//			dValType = TSTRING;
//		}
//		break;
//
//		case FITS_INTEGER_KEY:
//		{
//			dValType = TINT;
//		}
//		break;
//
//		case FITS_DOUBLE_KEY:
//		{
//			cout << "FITS_DOUBLE_KEY VAL -> " << *( reinterpret_cast<double*>( pKeyVal ) ) << std::endl;
//
//			dValType = TDOUBLE;
//		}
//		break;
//
//		case FITS_LOGICAL_KEY:
//		{
//			dValType = TLOGICAL;
//		}
//		break;
//
//		case FITS_COMMENT_KEY:
//		case FITS_HISTORY_KEY:
//		case FITS_DATE_KEY:
//		{
//		}
//		break;
//
//		default:
//		{
//			dValType = -1;
//
//			string msg =
//					string( "Invalid FITS key type. Must be one of: " ) +
//					string( "FITS_STRING_KEY, FITS_INTEGER_KEY, " )     +
//					string( "FITS_DOUBLE_KEY, FITS_LOGICAL_KEY." )      +
//					string( "FITS_COMMENT_KEY, FITS_HISTORY_KEY, " )    +
//					string( "FITS_DATA_KEY" );
//
//			ThrowException( "WriteKeyword", msg );
//		}
//	}
//
//	//
//	// Write ( append ) a COMMENT keyword to the header. The comment
//	// string will be continued over multiple keywords if it is
//	// longer than 70 characters. 
//	//
//	if ( dValType == FITS_COMMENT_KEY )
//	{
//		fits_write_comment( m_fptr.get(), ( char * )pKeyVal, &dFitsStatus );
//	}
//
//	//
//	// Write ( append ) a HISTORY keyword to the header. The history
//	// string will be continued over multiple keywords if it is
//	// longer than 70 characters. 
//	//
//	else if ( dValType == FITS_HISTORY_KEY )
//	{
//		fits_write_history( m_fptr.get(), ( char * )pKeyVal, &dFitsStatus );
//	}
//
//	//
//	// Write the DATE keyword to the header. The keyword value will contain
//	// the current system date as a character string in 'yyyy-mm-ddThh:mm:ss'
//	// format. If a DATE keyword already exists in the header, then this
//	// routine will simply update the keyword value with the current date.
//	//
//	else if ( dValType == FITS_DATE_KEY )
//	{
//		cout << "FITS DATE" << std::endl;
//
//		fits_write_date( m_fptr.get(), &dFitsStatus );
//	}
//
//	//
//	// Write a keyword of the appropriate data type into the header
//	//
//	else
//	{
//		fits_update_key( m_fptr.get(),
//						 dValType,
//						 pszKey,
//						 pKeyVal,
//						 pszComment,
//						 &dFitsStatus );
//	}
//
//	if ( dFitsStatus )
//	{
//		ThrowException( "WriteKeyword", dFitsStatus );
//	}
//}
//
//// +---------------------------------------------------------------------------+
//// |  UpdateKeyword                                                            |
//// +---------------------------------------------------------------------------+
//// |  Updates an existing FITS keyword to an existing FITS file.  The keyword  |
//// |  must be valid or an exception will be thrown. For list of valid FITS     |
//// |  keywords, see:                                                           |
//// |                                                                           |
//// |  http://heasarc.gsfc.nasa.gov/docs/fcg/standard_dict.html                 |
//// |  http://archive.stsci.edu/fits/fits_standard/node38.html# \               |
//// |  SECTION00940000000000000000                                              |
//// |                                                                           |
//// |  'HIERARCH' keyword NOTE: This text will be prefixed to any keyword by    |
//// |                           the cfitsio library if the keyword is greater   |
//// |                           than 8 characters, which is the standard FITS   |
//// |                           keyword length. See the link below for details: |
//// |   http://heasarc.gsfc.nasa.gov/docs/software/fitsio/c/f_user/node28.html  |
//// |                                                                           |
//// |   HIERARCH examples:                                                      |
//// |   -----------------                                                       |
//// |   HIERARCH LongKeyword = 47.5 / Keyword has > 8 characters & mixed case   |
//// |   HIERARCH XTE$TEMP = 98.6 / Keyword contains the '$' character           |
//// |   HIERARCH Earth is a star = F / Keyword contains embedded spaces         |
//// |                                                                           |
//// |  Throws std::runtime_error on error.                                      |
//// |                                                                           |
//// |  <IN> -> pszKey     - The header keyword. Ex: SIMPLE                      |
//// |  <IN> -> pKeyVal    - The value associated with the key. Ex: T            |
//// |  <IN> -> dValType   - The keyword type, as defined in CArcFitsFile.h      |
//// |  <IN> -> pszComment - The comment to attach to the keyword.               |
//// +---------------------------------------------------------------------------+
//void CArcFitsFile::UpdateKeyword( char* pszKey, void* pKeyVal, int dValType, char* pszComment )
//{
//	WriteKeyword( pszKey, pKeyVal, dValType, pszComment );
//}
//
//// +---------------------------------------------------------------------------+
//// |  GetParameters                                                            |
//// +---------------------------------------------------------------------------+
//// |  Reads the lNAxes, dNAxis and bits-per-pixel header values from a FITS    |
//// |  file.                                                                    |
//// |                                                                           |
//// |  Throws std::runtime_error on error.                                      |
//// |                                                                           |
//// |  <OUT> -> lNAxes        - MUST be a pointer to:  int lNAxes[ 3 ]. Index 0 |
//// |                          will have column size, 1 will have row size, and |
//// |                          2 will have number of frames if file is a data   |
//// |                          cube. Or use NAXES_COL, NAXES_ROW, NAXES_NOF.    |
//// |  <OUT> -> dNAxis        - Optional pointer to int for NAXIS keyword value |
//// |  <OUT> -> dBitsPerPixel - Optional pointer to int for BITPIX keyword value|
//// +---------------------------------------------------------------------------+
//void CArcFitsFile::GetParameters( long* pNaxes, int* pNaxis, int* pBitsPerPixel )
//{
// 	int  dFitsStatus     = 0;
//	int  dTempBPP        = 0;
//	int  dTempNAXIS      = 0;
//	long lTempNAXES[ CArcFitsFile::NAXES_SIZE ] = { 0, 0, 0 };
//
//	// Verify FITS file handle
//	// ----------------------------------------------------------
//	if ( m_fptr.get() == NULL )
//	{
//		ThrowException( "GetParameters",
//						"Invalid FITS handle, no file open ( GetParameters )" );
//	}
//
//	// Verify that at least one parameter is being requested
//	// ----------------------------------------------------------
//	if ( pNaxes == NULL && pNaxis == NULL && pBitsPerPixel == NULL )
//	{
//		ThrowException(
//				"GetParameters",
//				"All parameters are NULL. Why did you bother calling this method?" );
//	}
//
//	// Get the image parameters
//	// ----------------------------------------------------------
//	fits_get_img_param( m_fptr.get(),
//						CArcFitsFile::NAXES_SIZE,
//						&dTempBPP,
//						&dTempNAXIS,
//						lTempNAXES,
//						&dFitsStatus );
//
//	if ( dFitsStatus )
//	{
//		ThrowException( "GetParameters", dFitsStatus );
//	}
//
//	if ( pNaxes != NULL )
//	{
//		pNaxes[ CArcFitsFile::NAXES_COL ] = lTempNAXES[ CArcFitsFile::NAXES_COL ];
//		pNaxes[ CArcFitsFile::NAXES_ROW ] = lTempNAXES[ CArcFitsFile::NAXES_ROW ];
//		pNaxes[ CArcFitsFile::NAXES_NOF ] = lTempNAXES[ CArcFitsFile::NAXES_NOF ];
//	}
//
//	if ( pNaxis != NULL )
//	{
//		*pNaxis = dTempNAXIS;
//	}
//
//	if ( pBitsPerPixel != NULL )
//	{
//		*pBitsPerPixel = dTempBPP;
//	}
//}
//
//// +---------------------------------------------------------------------------+
//// |  GetNumberOfFrames                                                        |
//// +---------------------------------------------------------------------------+
//// |  Convenience method to get the column count ( NAXIS3 keyword ).  This     |
//// |  returns the CArcFitsFile::NAXES_NOF element in the pNaxes array to the   |
//// |  method GetParameters().  To get all parameters ( rows, cols,             |
//// |  number-of-frames, naxis value and bits-per-pixel ) at one time ... call  |
//// |  GetParameters() instead.                                                 |
//// |                                                                           |
//// |  Throws std::runtime_error on error.                                      |
//// +---------------------------------------------------------------------------+
//long CArcFitsFile::GetNumberOfFrames()
//{
//	long lNAxes[ CArcFitsFile::NAXES_SIZE ] = { 0, 0, 0 };
//
//	GetParameters( &lNAxes[ 0 ] );
//
//	return lNAxes[ CArcFitsFile::NAXES_NOF ];
//}
//
//// +---------------------------------------------------------------------------+
//// |  GetRows                                                                  |
//// +---------------------------------------------------------------------------+
//// |  Convenience method to get the column count ( NAXIS2 keyword ).  This     |
//// |  returns the CArcFitsFile::NAXES_ROW element in the pNaxes array to the   |
//// |  method GetParameters().  To get all parameters ( rows, cols,             |
//// |  number-of-frames, naxis value and bits-per-pixel ) at one time ... call  |
//// |  GetParameters() instead.                                                 |
//// |                                                                           |
//// |  Throws std::runtime_error on error.                                      |
//// +---------------------------------------------------------------------------+
//long CArcFitsFile::GetRows()
//{
//	long lNAxes[ CArcFitsFile::NAXES_SIZE ] = { 0, 0, 0 };
//
//	GetParameters( &lNAxes[ 0 ] );
//
//	return lNAxes[ CArcFitsFile::NAXES_ROW ];
//}
//
//// +---------------------------------------------------------------------------+
//// |  GetCols                                                                  |
//// +---------------------------------------------------------------------------+
//// |  Convenience method to get the column count ( NAXIS1 keyword ).  This     |
//// |  returns the CArcFitsFile::NAXES_COL element in the pNaxes array to the   |
//// |  method GetParameters().  To get all parameters ( rows, cols,             |
//// |  number-of-frames, naxis value and bits-per-pixel ) at one time ... call  |
//// |  GetParameters() instead.                                                 |
//// |                                                                           |
//// |  Throws std::runtime_error on error.                                      |
//// +---------------------------------------------------------------------------+
//long CArcFitsFile::GetCols()
//{
//	long lNAxes[ CArcFitsFile::NAXES_SIZE ] = { 0, 0, 0 };
//
//	GetParameters( &lNAxes[ 0 ] );
//
//	return lNAxes[ CArcFitsFile::NAXES_COL ];
//}
//
//// +---------------------------------------------------------------------------+
//// |  GetNAxis                                                                 |
//// +---------------------------------------------------------------------------+
//// |  Convenience method to get the NAXIS keyword.  This returns the pNaxis    |
//// |  paramter to the method GetParameters().  To get all parameters ( rows,   |
//// |  cols, number-of-frames, naxis value and bits-per-pixel ) at one time ... |
//// |  call GetParameters() instead.                                            |
//// |                                                                           |
//// |  Throws std::runtime_error on error.                                      |
//// +---------------------------------------------------------------------------+
//int CArcFitsFile::GetNAxis()
//{
//	int dNAxis = 0;
//
//	GetParameters( NULL, &dNAxis, NULL );
//
//	return dNAxis;
//}
//
//// +---------------------------------------------------------------------------+
//// |  GetBpp                                                                   |
//// +---------------------------------------------------------------------------+
//// |  Convenience method to get the bits-per-pixel ( BITPIX keyword ).  This   |
//// |  returns the pBitsPerPixel paramter to the method GetParameters().  To    |
//// |  get all parameters ( rows, cols, number-of-frames, naxis value and       |
//// |  bits-per-pixel ) at one time ... call GetParameters() instead.           |
//// |                                                                           |
//// |  Throws std::runtime_error on error.                                      |
//// +---------------------------------------------------------------------------+
//int CArcFitsFile::GetBpp()
//{
//	int dBpp = 0;
//
//	GetParameters( NULL, NULL, &dBpp );
//
//	return dBpp;
//}
//
//// +---------------------------------------------------------------------------+
//// |  GenerateTestData                                                         |
//// +---------------------------------------------------------------------------+
//// |  Writes test data to the file. The data's in the form 0, 1, 2 ... etc.    |
//// |  The purpose of the method is purely for testing when a FITS image is     |
//// |  otherwise unavailable.                                                   |
//// |                                                                           |
//// |  Throws std::runtime_error on error.                                      |
//// +---------------------------------------------------------------------------+
//void CArcFitsFile::GenerateTestData()
//{
//	int  dFitsStatus   = 0;	// Initialize status before calling fitsio routines
//	int  dBitsPerPixel = 0;
//	int  dNAxis        = 0;
//	long lNAxes[ 2 ]   = { 0, 0 };
//	long lNElements    = 0;
//	int  dFPixel       = 1;
//
//	// Verify FITS file handle
//	// ----------------------------------------------------------
//	if ( m_fptr.get() == NULL )
//	{
//		ThrowException( "GenerateTestData",
//						"Invalid FITS handle, no file open" );
//	}
//
//	// Get the image parameters
//	// ----------------------------------------------------------
//	fits_get_img_param( m_fptr.get(),
//						2,
//						&dBitsPerPixel,
//						&dNAxis,
//						lNAxes,
//						&dFitsStatus );
//
//	if ( dFitsStatus )
//	{
//		ThrowException( "GenerateTestData", dFitsStatus );
//	}
//
//	// Set number of pixels to write
//	// ----------------------------------------------------------
//	lNElements = lNAxes[ NAXES_ROW ] * lNAxes[ NAXES_COL ];
//
//	if ( dBitsPerPixel == CArcFitsFile::BPP16 )
//	{
//		unsigned short *pU16Buf = NULL;
//
//		try
//		{
//			pU16Buf = new unsigned short[ lNElements ];
//		}
//		catch ( bad_alloc &maEx )
//		{
//			ThrowException( "GenerateTestData",
//							 maEx.what() );
//		}
//
//		for ( long i=0, j=0; i<lNElements; i++, j++ )
//		{
//			pU16Buf[ i ] = static_cast<unsigned short>( j );
//
//			if ( j >= ( T_SIZE( unsigned short ) - 1 ) ) { j = 0; }
//		}
//
//		fits_write_img( m_fptr.get(),
//						TUSHORT,
//						dFPixel,
//						lNElements,
//						pU16Buf,
//						&dFitsStatus );
//
//		delete [] pU16Buf;
//	}
//
//	else if ( dBitsPerPixel == CArcFitsFile::BPP32 )
//	{
//		unsigned int *pUIntBuf = NULL;
//		unsigned int  val       = 0;
//
//		try
//		{
//			pUIntBuf = new unsigned int[ lNElements ];
//		}
//		catch ( bad_alloc &maEx )
//		{
//			ThrowException( "GenerateTestData",
//							 maEx.what() );
//		}
//
//		for ( long i=0; i<lNElements; i++ )
//		{
//			pUIntBuf[ i ] = val;
//			if ( val >= ( T_SIZE( unsigned int ) - 1 ) ) val = 0;
//			val++;
//		}
//
//		fits_write_img( m_fptr.get(),
//						TUINT,
//						dFPixel,
//						lNElements,
//						pUIntBuf,
//						&dFitsStatus );
//
//		delete [] pUIntBuf;
//	}
//
//	// Invalid bits-per-pixel value
//	// ---------------------------------------------------------------
//	else
//	{
//		ThrowException( "GenerateTestData",
//				string( "Invalid dBitsPerPixel, " ) +
//				string( "should be 16 ( BPP16 ) or 32 ( BPP32 )." ) );
//	}
//
//	if ( dFitsStatus )
//	{
//		ThrowException( "GenerateTestData", dFitsStatus );
//	}
//}
//
//// +---------------------------------------------------------------------------+
//// |  ReOpen                                                                   |
//// +---------------------------------------------------------------------------+
//// |  Closes and re-opens the current FITS file.                               |
//// |                                                                           |
//// |  Throws std::runtime_error on error.                                      |
//// +---------------------------------------------------------------------------+
//void CArcFitsFile::ReOpen()
//{
//	fitsfile*	fptr		= NULL;
//	int			dFitsStatus	= 0;
//	int			dIOMode		= 0;
//
//	std::string sFilename = GetFilename();
//
//	fits_file_mode( m_fptr.get(), &dIOMode, &dFitsStatus );
//
//	if ( dFitsStatus )
//	{
//		ThrowException( "ReOpen", dFitsStatus );
//	}
//
//	fits_close_file( m_fptr.get(), &dFitsStatus );
//
//	if ( dFitsStatus )
//	{
//		ThrowException( "ReOpen", dFitsStatus );
//	}
//
//	fits_open_file( &fptr,
//					sFilename.c_str(),
//					dIOMode,
//					&dFitsStatus );
//
//	if ( dFitsStatus )
//	{
//		m_fptr.reset();
//
//		ThrowException( "ReOpen", dFitsStatus );
//	}
//
//	if ( fptr == NULL )
//	{
//		ThrowException( "ReOpen", "Failed to ReOpen file!" );
//	}
//
//	m_pDataBuffer.reset();
//	m_pDataHeader.reset();
//	m_fptr.reset( fptr );
//}
//
//
//// +---------------------------------------------------------------------------+
//// |  Compare ( Single Images )                                                |
//// +---------------------------------------------------------------------------+
//// |  Compares the data image in this FITS file to another one. Returns true   |
//// |  if they are a match; false otherwise. The headers ARE NOT compared;      |
//// |  except to verify image dimensions. NOTE: Only supports 16-bit image,     |
//// |  but doesn't check for it.                                                |
//// |                                                                           |
//// |  Throws std::runtime_error on error.                                      |
//// |                                                                           |
//// |  <IN> -> anotherCFitsFile - Reference to FITS file to compare.            |
//// |                                                                           |
//// |  Returns: true if the image data matches.                                 |
//// +---------------------------------------------------------------------------+
//bool CArcFitsFile::Compare( CArcFitsFile& anotherCFitsFile )
//{
//	bool bAreTheSame = true;
//
//	//  Read the image dimensions from input file
//	// +----------------------------------------------+
//	long lAnotherNaxes[ 3 ] = { 0, 0, 0 };
//	int  dAnotherBpp        = 0;
//
//	anotherCFitsFile.GetParameters( lAnotherNaxes, NULL, &dAnotherBpp );
//
//	//  Read the image dimensions from this file
//	// +----------------------------------------------+
//	long lNaxes[ 3 ] = { 0, 0, 0 };
//	int  dBpp        = 0;
//
//	this->GetParameters( lNaxes, NULL, &dBpp );
//
//	//  Verify image dimensions match between files
//	// +----------------------------------------------+
//	if ( lAnotherNaxes[ CArcFitsFile::NAXES_ROW ] != lNaxes[ CArcFitsFile::NAXES_ROW ] ||
//		 lAnotherNaxes[ CArcFitsFile::NAXES_COL ] != lNaxes[ CArcFitsFile::NAXES_COL ] )
//	{
//		ostringstream oss;
//
//		oss << "Image dimensions of comparison files DO NOT match! FITS 1: "
//			<< lNaxes[ CArcFitsFile::NAXES_ROW ] << "x" << lNaxes[ CArcFitsFile::NAXES_COL ]
//			<< " FITS 2: " << lAnotherNaxes[ CArcFitsFile::NAXES_ROW ] << "x"
//			<< lAnotherNaxes[ CArcFitsFile::NAXES_COL ] << ends;
//
//		ThrowException( "Compare", oss.str() );
//	}
//
//	if ( dAnotherBpp != dBpp )
//	{
//		ostringstream oss;
//
//		oss << "Image bits-per-pixel of comparison files DO NOT match! FITS 1: "
//			<< dBpp	<< " FITS 2: " << dAnotherBpp << ends;
//
//		ThrowException( "Compare", oss.str() );
//	}
//
//	//  Read input file image buffer
//	// +----------------------------------------------+
//	unsigned short *pAnotherUShortBuf =
//				( unsigned short * )anotherCFitsFile.Read();
//
//	if ( pAnotherUShortBuf == NULL )
//	{
//		ThrowException( "Compare",
//						"Failed to read input FITS file image data!" );
//	}
//
//	//  Read this file image buffer
//	// +----------------------------------------------+
//	unsigned short *pU16Buf = ( unsigned short * )this->Read();
//
//	if ( pAnotherUShortBuf == NULL )
//	{
//		ThrowException( "Compare",
//						"Failed to read this FITS file image data!" );
//	}
//
//	//  Compare image buffers
//	// +----------------------------------------------+
//	for ( int i=0; i<( lNaxes[ CArcFitsFile::NAXES_ROW ] * lNaxes[ CArcFitsFile::NAXES_COL ] ); i++ )
//	{
//		if ( pU16Buf[ i ] != pAnotherUShortBuf[ i ] )
//		{
//			bAreTheSame = false;
//
//			break;
//		}
//	}
//
//	return bAreTheSame;
//}
//
//// +---------------------------------------------------------------------------+
//// |  Write ( Single Image )                                                   |
//// +---------------------------------------------------------------------------+
//// |  Writes to a FITS file the will contain a single image.                   |
//// |                                                                           |
//// |  Throws std::runtime_error on error.                                      |
//// |                                                                           |
//// |  <IN> -> data   - Pointer to image data to write.                         |
//// +---------------------------------------------------------------------------+
//void CArcFitsFile::Write( void* pData )
//{
//	int  dFitsStatus   = 0;	// Initialize status before calling fitsio routines
//	int  dBitsPerPixel = 0;
//	int  dNAxis        = 0;
//	long lNAxes[ 2 ]   = { 0, 0 };
//	long lNElements    = 0;
//	int  dFPixel       = 1;
//
//	// Verify FITS file handle
//	// ----------------------------------------------------------
//	if ( m_fptr.get() == NULL )
//	{
//		ThrowException( "Write",
//						"Invalid FITS handle, no file open" );
//	}
//
//	// Get the image parameters
//	// ----------------------------------------------------------
//	fits_get_img_param( m_fptr.get(),
//						2,
//						&dBitsPerPixel,
//						&dNAxis,
//						lNAxes,
//						&dFitsStatus );
//
//	if ( dFitsStatus )
//	{
//		ThrowException( "Write", dFitsStatus );
//	}
//
//	// Verify parameters
//	// ----------------------------------------------------------
//	if ( dNAxis != 2 )
//	{
//		ThrowException( "Write",
//				string( "Invalid NAXIS value. " ) +
//				string( "This method is only valid for a " ) +
//				string( "file containing a single image." ) );
//	}
//
//	if ( pData == NULL )
//	{
//		ThrowException( "Write", "Invalid data buffer pointer" );
//	}
//
//	// Set number of pixels to write
//	// ----------------------------------------------------------
//	lNElements = lNAxes[ 0 ] * lNAxes[ 1 ];
//
//	// Write 16-bit data
//	// ---------------------------------------------------------------
//	if ( dBitsPerPixel == CArcFitsFile::BPP16 )
//	{
//		fits_write_img( m_fptr.get(),
//						TUSHORT,
//						dFPixel,
//						lNElements,
//						( unsigned short * )pData,
//						&dFitsStatus );
//	}
//
//	// Write 32-bit data
//	// ---------------------------------------------------------------
//	else if ( dBitsPerPixel == CArcFitsFile::BPP32 )
//	{
//		fits_write_img( m_fptr.get(),
//						TUINT,
//						dFPixel,
//						lNElements,
//						( unsigned int * )pData,
//						&dFitsStatus );
//	}
//
//	// Invalid bits-per-pixel value
//	// ---------------------------------------------------------------
//	else
//	{
//		ThrowException(
//			"Write",
//			"Invalid dBitsPerPixel, should be 16 ( BPP16 ) or 32 ( BPP32 )." );
//	}
//
//	if ( dFitsStatus )
//	{
//		ThrowException( "Write", dFitsStatus );
//	}
//}
//
//// +---------------------------------------------------------------------------+
//// |  Write ( Single Image )                                                   |
//// +---------------------------------------------------------------------------+
//// |  Writes a specified number of bytes from the provided buffer to a FITS    |
//// |  that contains a single image. The start position of the data within the  |
//// |  FITS file image can be specified.                                        |
//// |                                                                           |
//// |  Throws std::runtime_error on error.                                      |
//// |                                                                           |
//// |  <IN> -> pData         - Pointer to the data to write.                    |
//// |  <IN> -> uBytesToWrite - The number of bytes to write.                    |
//// |  <IN> -> dFPixl        - The start pixel within the FITS file image. This |
//// |                          parameter is optional. If -1, then the next      |
//// |                          write position will be at zero. If fPixel >= 0,  |
//// |                          then data will be written there.                 |
//// +---------------------------------------------------------------------------+
//void CArcFitsFile::Write( void* pData, unsigned int uBytesToWrite, int dFPixel )
//{
//	int  dFitsStatus   = 0;	// Initialize status before calling fitsio routines
//	int  dBitsPerPixel = 0;
//	int  dNAxis        = 0;
//	long lNAxes[ 2 ]   = { 0, 0 };
//	long lNElements    = 0;
//	bool bMultiWrite  = false;
//
//	//
//	// Verify FITS file handle
//	// ----------------------------------------------------------
//	if ( m_fptr.get() == NULL )
//	{
//		ThrowException( "Write",
//						"Invalid FITS handle, no file open" );
//	}
//
//	//
//	// Get the image parameters
//	// ----------------------------------------------------------
//	fits_get_img_param( m_fptr.get(),
//						2,
//						&dBitsPerPixel,
//						&dNAxis,
//						lNAxes,
//						&dFitsStatus );
//
//	if ( dFitsStatus )
//	{
//		ThrowException( "Write", dFitsStatus );
//	}
//
//	//
//	// Verify parameters
//	// ----------------------------------------------------------
//	if ( dNAxis != 2 )
//	{
//		ThrowException( "Write",
//				string( "Invalid NAXIS value. " ) +
//				string( "This method is only valid for a " ) +
//				string( "file containing a single image." ) );
//	}
//
//	if ( pData == NULL )
//	{
//		ThrowException( "Write", "Invalid data buffer pointer" );
//	}
//
//	if ( dFPixel >= ( lNAxes[ 0 ] * lNAxes[ 1 ] ) )
//	{
//		ThrowException( "Write",
//				string( "Invalid start position, " ) +
//				string( " pixel position outside image size." ) );
//	}
//
//	//
//	// Set the start pixel ( position ) within the file
//	// ---------------------------------------------------------
//	if ( dFPixel < 0 && this->m_lFPixel == 0 )
//	{
//		this->m_lFPixel = 1;
//		bMultiWrite  = true;
//	}
//
//	else if ( dFPixel == 0 && this->m_lFPixel != 0 )
//	{
//		this->m_lFPixel = 1;
//		bMultiWrite  = true;
//	}
//
//	else if ( dFPixel < 0 && this->m_lFPixel != 0 )
//	{
//		bMultiWrite  = true;
//	}
//
//	else
//	{
//		this->m_lFPixel = dFPixel + 1;
//		bMultiWrite  = false;
//	}
//
//	//
//	// Verify the start position
//	// ----------------------------------------------------------
//	if ( this->m_lFPixel >= ( lNAxes[ 0 ] * lNAxes[ 1 ] ) )
//	{
//		ThrowException( "Write",
//				string( "Invalid start position, " ) +
//				string( " pixel position outside image size." ) );
//	}
//
//	//
//	// Write 16-bit data
//	// ----------------------------------------------------------
//	if ( dBitsPerPixel == CArcFitsFile::BPP16 )
//	{
//		// Number of pixels to write
//		lNElements = uBytesToWrite / sizeof( unsigned short );
//
//		fits_write_img( m_fptr.get(),
//						TUSHORT,
//						this->m_lFPixel,
//						lNElements,
//						( unsigned short * )pData,
//						&dFitsStatus );
//	}
//
//	//
//	// Write 32-bit data
//	// ----------------------------------------------------------
//	else if ( dBitsPerPixel == CArcFitsFile::BPP32 )
//	{
//		// Number of pixels to write
//		lNElements = uBytesToWrite / sizeof( unsigned int );
//
//		fits_write_img( m_fptr.get(),
//						TUINT,
//						this->m_lFPixel,
//						lNElements,
//						( unsigned int * )pData,
//						&dFitsStatus );
//	}
//
//	//
//	// Invalid bits-per-pixel value
//	// ---------------------------------------------------------------
//	else
//	{
//		ThrowException( "Write",
//				string( "Invalid dBitsPerPixel, " ) +
//				string( " should be 16 ( BPP16 ) or 32 ( BPP32 )." ) );
//	}
//
//	if ( dFitsStatus )
//	{
//		ThrowException( "Write", dFitsStatus );
//	}
//
//	if ( bMultiWrite )
//	{
//		this->m_lFPixel += lNElements;
//	}
//}
//
//// +---------------------------------------------------------------------------+
//// |  WriteSubImage ( Single Image )                                           |
//// +---------------------------------------------------------------------------+
//// |  Writes a sub-image to a FITS file.                                       |
//// |                                                                           |
//// |  Throws std::runtime_error on error.                                      |
//// |                                                                           |
//// |  <IN> -> data         - The pixel data.                                   |
//// |  <IN> -> llrow        - The lower left row of the sub-image.              |
//// |  <IN> -> llcol        - The lower left column of the sub-image.           |
//// |  <IN> -> urrow        - The upper right row of the sub-image.             |
//// |  <IN> -> urcol        - The upper right column of the sub-image.          |
//// +---------------------------------------------------------------------------+
//void CArcFitsFile::WriteSubImage( void* pData, int llrow, int llcol, int urrow, int urcol )
//{
//	int  dFitsStatus   = 0;		// Initialize status before calling fitsio routines
//	int  dBitsPerPixel = 0;
//	int  dNAxis        = 0;
//	long lNAxes[ 2 ]   = { 0, 0 };
//
//	// Verify FITS file handle
//	// ----------------------------------------------------------
//	if ( m_fptr.get() == NULL )
//	{
//		ThrowException( "WriteSubImage",
//						"Invalid FITS handle, no file open" );
//	}
//
//	// Get the image parameters
//	// ----------------------------------------------------------
//	fits_get_img_param( m_fptr.get(),
//						2,
//						&dBitsPerPixel,
//						&dNAxis,
//						lNAxes,
//						&dFitsStatus );
//
//	if ( dFitsStatus )
//	{
//		ThrowException( "WriteSubImage", dFitsStatus );
//	}
//
//	// Verify parameters
//	// ----------------------------------------------------------
//	if ( dNAxis != 2 )
//	{
//		ThrowException( "WriteSubImage",
//				string( "Invalid NAXIS value. " ) +
//				string( "This method is only valid for a file " ) +
//				string( "containing a single image." ) );
//	}
//
//	if ( llrow > urrow )
//	{
//		ThrowException( "WriteSubImage", "Invalid llrow/urrow parameter!" );
//	}
//
//	if ( llcol > urcol )
//	{
//		ThrowException( "WriteSubImage", "Invalid llcol/urcol parameter!" );
//	}
//
//	if ( llrow < 0 || llrow >= lNAxes[ 1 ] )
//	{
//		ThrowException( "WriteSubImage", "Invalid llrow parameter!" );
//	}
//
//	if ( urrow < 0 || urrow >= lNAxes[ 1 ] )
//	{
//		ThrowException( "WriteSubImage", "Invalid urrow parameter!" );
//	}
//
//	if ( llcol < 0 || llcol >= lNAxes[ 0 ] )
//	{
//		ThrowException( "WriteSubImage", "Invalid llcol parameter!" );
//	}
//
//	if ( urcol < 0 || urcol >= lNAxes[ 0 ] )
//	{
//		ThrowException( "WriteSubImage", "Invalid urcol parameter!" );
//	}
//
//	if ( pData == NULL )
//	{
//		ThrowException( "WriteSubImage", "Invalid data buffer pointer!" );
//	}
//
//	// Set the subset start pixels
//	// ---------------------------------------------------------------
//	long dFPixel[ 2 ] = { llcol + 1, llrow + 1 };
//	long lpixel[ 2 ] = { urcol + 1, urrow + 1 };
//
//	// Write 16-bit data
//	// ---------------------------------------------------------------
//	if ( dBitsPerPixel == CArcFitsFile::BPP16 )
//	{
//		fits_write_subset( m_fptr.get(),
//						   TUSHORT,
//						   dFPixel,
//						   lpixel,
//						   pData,
//						   &dFitsStatus );
//	}
//
//	// Write 32-bit data
//	// ---------------------------------------------------------------
//	else if ( dBitsPerPixel == CArcFitsFile::BPP32 )
//	{
//		fits_write_subset( m_fptr.get(),
//						   TUINT,
//						   dFPixel,
//						   lpixel,
//						   pData,
//						   &dFitsStatus );
//	}
//
//	// Invalid bits-per-pixel value
//	// ---------------------------------------------------------------
//	else
//	{
//		ThrowException( "WriteSubImage",
//				string( "Invalid dBitsPerPixel " ) +
//				string( ", should be 16 ( BPP16 ) or 32 ( BPP32 )." ) );
//	}
//
//	if ( dFitsStatus )
//	{
//		ThrowException( "WriteSubImage", dFitsStatus );
//	}
//}
//
//// +---------------------------------------------------------------------------+
//// |  ReadSubImage ( Single Image )                                            |
//// +---------------------------------------------------------------------------+
//// |  Reads a sub-image from a FITS file.                                      |
//// |                                                                           |
//// |  Throws std::runtime_error on error.                                      |
//// |                                                                           |
//// |  <IN> -> llrow        - The lower left row of the sub-image.              |
//// |  <IN> -> llcol        - The lower left column of the sub-image.           |
//// |  <IN> -> urrow        - The upper right row of the sub-image.             |
//// |  <IN> -> urcol        - The upper right column of the sub-image.          |
//// +---------------------------------------------------------------------------+
//void *CArcFitsFile::ReadSubImage( int llrow, int llcol, int urrow, int urcol )
//{
//	int  dFitsStatus   = 0;	// Initialize status before calling fitsio routines
//	int  dBitsPerPixel = 0;
//	int  dNAxis        = 0;
//	int  dAnyNul       = 0;
//	long lNAxes[ 2 ]   = { 0, 0 };
//
//	// Verify FITS file handle
//	// ----------------------------------------------------------
//	if ( m_fptr.get() == NULL )
//	{
//		ThrowException( "ReadSubImage",
//						"Invalid FITS handle, no file open" );
//	}
//
//	// Get the image parameters
//	// ----------------------------------------------------------
//	fits_get_img_param( m_fptr.get(),
//						2,
//						&dBitsPerPixel,
//						&dNAxis,
//						lNAxes,
//						&dFitsStatus );
//
//	if ( dFitsStatus )
//	{
//		ThrowException( "ReadSubImage", dFitsStatus );
//	}
//
//	// Verify parameters
//	// ----------------------------------------------------------
//	if ( dNAxis != 2 )
//	{
//		ThrowException( "ReadSubImage",
//				string( "Invalid NAXIS value. " ) +
//				string( "This method is only valid for a file " ) +
//				string( "containing a single image." ) );
//	}
//
//	if ( llrow > urrow )
//	{
//		ThrowException( "ReadSubImage", "Invalid llrow/urrow parameter!" );
//	}
//
//	if ( llcol > urcol )
//	{
//		ThrowException( "ReadSubImage", "Invalid llcol/urcol parameter!" );
//	}
//
//	if ( llrow < 0 || llrow > lNAxes[ 1 ] )
//	{
//		ThrowException( "ReadSubImage", "Invalid llrow parameter!" );
//	}
//
//	if ( urrow < 0 || urrow > lNAxes[ 1 ] )
//	{
//		ThrowException( "ReadSubImage", "Invalid urrow parameter!" );
//	}
//
//	if ( llcol < 0 || llcol > lNAxes[ 0 ] )
//	{
//		ThrowException( "ReadSubImage", "Invalid llcol parameter!" );
//	}
//
//	if ( urcol < 0 || urcol > lNAxes[ 0 ] )
//	{
//		ThrowException( "ReadSubImage", "Invalid urcol parameter!" );
//	}
//
//	if ( long( urrow ) == lNAxes[ 1 ] ) { urrow = int( lNAxes[ 1 ] - 1 ); }
//	if ( long( urcol ) == lNAxes[ 0 ] ) { urcol = int( lNAxes[ 0 ] - 1 ); }
//
//	// Set the subset start pixels
//	// ---------------------------------------------------------------
//	long lFPixel[ 2 ] = { llcol + 1, llrow + 1 };
//	long lpixel[ 2 ]  = { urcol + 1, urrow + 1 };
//
//	// The read routine also has an inc parameter which can be used to
//	// read only every inc-th pixel along each dimension of the image.
//	// Normally inc[0] = inc[1] = 1 to read every pixel in a 2D image.
//	// To read every other pixel in the entire 2D image, set 
//	// ---------------------------------------------------------------
//	long lInc[ 2 ] = { 1, 1 };
//
//	// Set the data length ( in pixels )
//	// ----------------------------------------------------------
//	long lDataLength = lNAxes[ 0 ] * lNAxes[ 1 ];
//
//	// Read 16-bit data
//	// ---------------------------------------------------------------
//	if ( dBitsPerPixel == CArcFitsFile::BPP16 )
//	{
//		m_pDataBuffer.reset( new unsigned short[ lDataLength ],
//							 &CArcFitsFile::ArrayDeleter<unsigned short> );	//ArrayDeleter<unsigned short>() ); //&CArcFitsFile::ArrayDeleter );
//
//		if ( m_pDataBuffer.get() == NULL )
//		{
//			ThrowException(
//					"ReadSubImage",
//					"Failed to allocate buffer for image pixel data" );
//		}
//
//		fits_read_subset( m_fptr.get(),
//						  TUSHORT,
//						  lFPixel,
//						  lpixel,
//						  lInc,
//						  0,
//						  m_pDataBuffer.get(),
//						  &dAnyNul,
//						  &dFitsStatus );
//	}
//
//	// Read 32-bit data
//	// ---------------------------------------------------------------
//	else if ( dBitsPerPixel == CArcFitsFile::BPP32 )
//	{
//		m_pDataBuffer.reset( new unsigned int[ lDataLength ],
//							 &CArcFitsFile::ArrayDeleter<unsigned int> ); //&CArcFitsFile::ArrayDeleter );
//
//		if ( m_pDataBuffer.get() == NULL )
//		{
//			ThrowException(
//					"ReadSubImage",
//					"Failed to allocate buffer for image pixel data" );
//		}
//
//		fits_read_subset( m_fptr.get(),
//						  TUINT,
//						  lFPixel,
//						  lpixel,
//						  lInc,
//						  0,
//						  m_pDataBuffer.get(),
//						  &dAnyNul,
//						  &dFitsStatus );
//	}
//
//	// Invalid bits-per-pixel value
//	// ---------------------------------------------------------------
//	else
//	{
//		ThrowException(
//			"ReadSubImage",
//			"Invalid dBitsPerPixel, should be 16 ( BPP16 ) or 32 ( BPP32 )." );
//	}
//
//	if ( dFitsStatus )
//	{
//		ThrowException( "ReadSubImage", dFitsStatus );
//	}
//
//	return m_pDataBuffer.get();
//}
//
//// +---------------------------------------------------------------------------+
//// |  Read ( Single Image )                                                    |
//// +---------------------------------------------------------------------------+
//// |  Reads the pixels from a FITS file containing a single image.             |
//// |                                                                           |
//// |  Throws std::runtime_error on error.                                      |
//// |                                                                           |
//// |  Returns a pointer to the image data. The caller of this method is NOT    |
//// |  responsible for freeing the memory allocated to the image data.          |
//// +---------------------------------------------------------------------------+
//void *CArcFitsFile::Read()
//{
//	int  dBitsPerPixel = 0;
//	int  dFitsStatus   = 0;
//	int  dNAxis        = 0;
//	long lNAxes[ 2 ]   = { 0, 0 };
//
//	// Verify FITS file handle
//	// --------------------------------------------------
//	if ( m_fptr.get() == NULL )
//	{
//		ThrowException( "Read",
//						"Invalid FITS handle, no file open" );
//	}
//
//	// Get the image parameters
//	// ----------------------------------------------------------
//	fits_get_img_param( m_fptr.get(),
//						2,
//						&dBitsPerPixel,
//						&dNAxis,
//						lNAxes,
//						&dFitsStatus );
//
//	if ( dFitsStatus )
//	{
//		ThrowException( "Read", dFitsStatus );
//	}
//
//	// Verify NAXIS parameter
//	// ----------------------------------------------------------
//	if ( dNAxis != 2 )
//	{
//		ThrowException( "Read",
//				string( "Invalid NAXIS value. " ) +
//				string( "This method is only valid for a file " ) +
//				string( "containing a single image." ) );
//	}
//
//	// Set the data length ( in pixels )
//	// ----------------------------------------------------------
//	long lDataLength = lNAxes[ 0 ] * lNAxes[ 1 ];
//
//	// Get the image data
//	// ----------------------------------------------------------
//	if ( dBitsPerPixel == CArcFitsFile::BPP16 )
//	{
//		m_pDataBuffer.reset( new unsigned short[ lDataLength ],
//							 &CArcFitsFile::ArrayDeleter<unsigned short> ); //&CArcFitsFile::ArrayDeleter );
//
//		if ( m_pDataBuffer.get() == NULL )
//		{
//			ThrowException(
//				"Read",
//				"Failed to allocate buffer for image pixel data" );
//		}
//
//		// Read the image data
//		// ----------------------------------------------------------
//		fits_read_img( m_fptr.get(),
//					   TUSHORT,
//					   1,
//					   lDataLength,
//					   NULL,
//					   m_pDataBuffer.get(),
//					   NULL,
//					   &dFitsStatus );
//
//		if ( dFitsStatus )
//		{
//			ThrowException( "Read", dFitsStatus );
//		}
//	}
//	else
//	{
//		m_pDataBuffer.reset( new unsigned int[ lDataLength ],
//							 &CArcFitsFile::ArrayDeleter<unsigned int> ); //&CArcFitsFile::ArrayDeleter );
//
//		if ( m_pDataBuffer.get() == NULL )
//		{
//			ThrowException(
//				"Read",
//				"Failed to allocate buffer for image pixel data" );
//		}
//
//		// Read the image data
//		// ----------------------------------------------------------
//		fits_read_img( m_fptr.get(),
//					   TUINT,
//					   1,
//					   lDataLength,
//					   NULL,
//					   m_pDataBuffer.get(),
//					   NULL,
//					   &dFitsStatus );
//
//		if ( dFitsStatus )
//		{
//			ThrowException( "Read", dFitsStatus );
//		}
//	}
//
//	return m_pDataBuffer.get();
//}
//
//// +---------------------------------------------------------------------------+
//// |  Resize ( Single Image )                                                  |
//// +---------------------------------------------------------------------------+
//// |  Resizes a single image FITS file by modifying the NAXES keyword and      |
//// |  increasing the image data portion of the file.                           |
//// |                                                                           |
//// |  Throws std::runtime_error on error.                                      |
//// |                                                                           |
//// |  <IN> -> dRows - The number of rows the new FITS file will have.          |
//// |  <IN> -> dCols - The number of cols the new FITS file will have.          |
//// +---------------------------------------------------------------------------+
//void CArcFitsFile::Resize( int dRows, int dCols )
//{
//	int  dBitsPerPixel = 0;
//	int  dFitsStatus   = 0;
//	int  dNAxis        = 0;
//	long lNAxes[ 2 ]   = { 0, 0 };
//
//	// Verify FITS file handle
//	// --------------------------------------------------
//	if ( m_fptr.get() == NULL )
//	{
//		ThrowException( "Resize",
//						"Invalid FITS handle, no file open" );
//	}
//
//	// Get the image parameters
//	// ----------------------------------------------------------
//	fits_get_img_param( m_fptr.get(),
//						2,
//						&dBitsPerPixel,
//						&dNAxis,
//						lNAxes,
//						&dFitsStatus );
//
//	if ( dFitsStatus )
//	{
//		ThrowException( "Resize", dFitsStatus );
//	}
//
//	// Verify NAXIS parameter
//	// ----------------------------------------------------------
//	if ( dNAxis != 2 )
//	{
//		ThrowException( "Resize",
//				string( "Invalid NAXIS value. " ) +
//				string( "This method is only valid for a " ) +
//				string( "file containing a single image." ) );
//	}
//
//	// Resize the FITS file
//	// ------------------------------------------------------------
//	lNAxes[ CArcFitsFile::NAXES_ROW ] = dRows;
//	lNAxes[ CArcFitsFile::NAXES_COL ] = dCols;
//
//	fits_resize_img( m_fptr.get(),
//					 dBitsPerPixel,
//					 dNAxis,
//					 lNAxes,
//					 &dFitsStatus );
//
//	if ( dFitsStatus )
//	{
//		ThrowException( "Resize", dFitsStatus );
//	}
//}
//
//// +---------------------------------------------------------------------------+
//// |  Write3D ( Data Cube )                                                    |
//// +---------------------------------------------------------------------------+
//// |  Add an image to the end of a FITS data cube.                             |
//// |                                                                           |
//// |  Throws std::runtime_error on error.                                      |
//// |                                                                           |
//// |  <IN> -> pData - A pointer to the image data.                             |
//// +---------------------------------------------------------------------------+
//void CArcFitsFile::Write3D( void* pData )
//{
//	int  dFitsStatus   = 0;	// Initialize status before calling fitsio routines
//	int  dBitsPerPixel = 0;
//	int  dNAxis        = 0;
//	long lNElements    = 0;
//	long lNAxes[ 3 ]   = { 1, 1, ++m_lFrame };
//
//	// Verify FITS file handle
//	// ----------------------------------------------------------
//	if ( m_fptr.get() == NULL )
//	{
//		ThrowException( "Write3D",
//				"Invalid FITS handle, no file open!" );
//	}
//
//	// Get the image parameters
//	// ----------------------------------------------------------
//	fits_get_img_param( m_fptr.get(),
//						3,
//						&dBitsPerPixel,
//						&dNAxis,
//						lNAxes,
//						&dFitsStatus );
//
//	if ( dFitsStatus )
//	{
//		ThrowException( "Write3D", dFitsStatus );
//	}
//
//	// Verify parameters
//	// ----------------------------------------------------------
//	if ( dNAxis != 3 )
//	{
//		ThrowException( "Write3D",
//				string( "Invalid NAXIS value. " ) +
//				string( "This method is only valid for a FITS data cube." ) );
//	}
//
//	if ( pData == NULL )
//	{
//		ThrowException( "Write3D", "Invalid data buffer pointer ( Write3D )" );
//	}
//
//	// Set number of pixels to write
//	// ----------------------------------------------------------
//	lNElements = lNAxes[ CArcFitsFile::NAXES_ROW ] * lNAxes[ CArcFitsFile::NAXES_COL ];
//
//	if ( m_lFPixel == 0 )
//	{
//		m_lFPixel = 1;
//	}
//
//	// Write 16-bit data
//	// ---------------------------------------------------------------
//	if ( dBitsPerPixel == CArcFitsFile::BPP16 )
//	{
//		fits_write_img( m_fptr.get(),
//						TUSHORT,
//						m_lFPixel,
//						lNElements,
//						( unsigned short * )pData,
//						&dFitsStatus );
//	}
//
//	// Write 32-bit data
//	// ---------------------------------------------------------------
//	else if ( dBitsPerPixel == CArcFitsFile::BPP32 )
//	{
//		fits_write_img( m_fptr.get(),
//						TUINT,
//						m_lFPixel,
//						lNElements,
//						( unsigned int * )pData,
//						&dFitsStatus );
//	}
//
//	// Invalid bits-per-pixel value
//	// ---------------------------------------------------------------
//	else
//	{
//		ThrowException( "Write3D",
//				string( "Invalid dBitsPerPixel, " ) +
//				string( "should be 16 ( BPP16 ) or 32 ( BPP32 )." ) );
//	}
//
//	if ( dFitsStatus )
//	{
//		ThrowException( "Write3D", dFitsStatus );
//	}
//
//	// Update the start pixel
//	// ----------------------------------------------------------
//	m_lFPixel += lNElements;
//
//	// Increment the image number and update the key
//	// ----------------------------------------------------------
//	fits_update_key( m_fptr.get(),
//					 TLONG,
//					 "NAXIS3",
//					 &m_lFrame,
//					 NULL,
//					 &dFitsStatus );
//
//	if ( dFitsStatus )
//	{
//		ThrowException( "Write3D", dFitsStatus );
//	}
//}
//
//// +---------------------------------------------------------------------------+
//// |  ReWrite3D ( Data Cube )                                                  |
//// +---------------------------------------------------------------------------+
//// |  Re-writes an existing image in a FITS data cube. The image data MUST     |
//// |  match in size to the exising images within the data cube. The image      |
//// |  size is NOT checked for by this method.                                  |
//// |                                                                           |
//// |  Throws std::runtime_error on error.                                      |
//// |                                                                           |
//// |  <IN> -> data - A pointer to the image data.                              |
//// |  <IN> -> dImageNumber - The number of the data cube image to replace.     |
//// +---------------------------------------------------------------------------+
//void CArcFitsFile::ReWrite3D( void* pData, int dImageNumber )
//{
//	int  dFitsStatus   = 0;	// Initialize status before calling fitsio routines
//	int  dBitsPerPixel = 0;
//	int  dNAxis        = 0;
//	long lNAxes[ 3 ]   = { 0, 0, 0 };
//	long lNElements    = 0;
//	long lFPixel       = 0;
//
//	// Verify FITS file handle
//	// ----------------------------------------------------------
//	if ( m_fptr.get() == NULL )
//	{
//		ThrowException( "ReWrite3D",
//						"Invalid FITS handle, no file open!" );
//	}
//
//	// Get the image parameters
//	// ----------------------------------------------------------
//	fits_get_img_param( m_fptr.get(),
//						3,
//						&dBitsPerPixel,
//						&dNAxis,
//						lNAxes,
//						&dFitsStatus );
//
//	if ( dFitsStatus )
//	{
//		ThrowException( "ReWrite3D", dFitsStatus );
//	}
//
//	// Verify parameters
//	// ----------------------------------------------------------
//	if ( dNAxis != 3 )
//	{
//		ThrowException( "ReWrite3D",
//				string( "Invalid NAXIS value. " ) +
//				string( "This method is only valid for a " ) +
//				string( "FITS data cube." ) );
//	}
//
//	if ( pData == NULL )
//	{
//		ThrowException( "ReWrite3D", "Invalid data buffer pointer" );
//	}
//
//	// Set number of pixels to write; also set the start position
//	// ----------------------------------------------------------
//	lNElements = lNAxes[ 0 ] * lNAxes[ 1 ];
//	lFPixel = lNElements * dImageNumber + 1;
//
//	// Write 16-bit data
//	// ---------------------------------------------------------------
//	if ( dBitsPerPixel == CArcFitsFile::BPP16 )
//	{
//		fits_write_img( m_fptr.get(),
//						TUSHORT,
//						lFPixel,
//						lNElements,
//						( unsigned short * )pData,
//						&dFitsStatus );
//	}
//
//	// Write 32-bit data
//	// ---------------------------------------------------------------
//	else if ( dBitsPerPixel == CArcFitsFile::BPP32 )
//	{
//		fits_write_img( m_fptr.get(),
//						TUINT,
//						lFPixel,
//						lNElements,
//						( unsigned int * )pData,
//						&dFitsStatus );
//	}
//
//	// Invalid bits-per-pixel value
//	// ---------------------------------------------------------------
//	else
//	{
//		ThrowException( "ReWrite3D",
//				string( "Invalid dBitsPerPixel, " ) +
//				string( "should be 16 ( BPP16 ) or 32 ( BPP32 )." ) );
//	}
//
//	if ( dFitsStatus )
//	{
//		ThrowException( "ReWrite3D", dFitsStatus );
//	}
//}
//
//// +---------------------------------------------------------------------------+
//// |  Read3D ( Data Cube )                                                     |
//// +---------------------------------------------------------------------------+
//// |  Reads an image from a FITS data cube.                                    |
//// |                                                                           |
//// |  Throws std::runtime_error on error.                                      |
//// |                                                                           |
//// |  <IN>  -> dImageNumber - The image number to read.                        |
//// |  <OUT> -> void *       - A pointer to the image data.                     |
//// +---------------------------------------------------------------------------+
//void *CArcFitsFile::Read3D( int dImageNumber )
//{
//	int  dFitsStatus   = 0;	// Initialize status before calling fitsio routines
//	int  dBitsPerPixel = 0;
//	int  dNAxis        = 0;
//	long lNAxes[ 3 ]   = { 0, 0, 0 };
//	long lNElements    = 0;
//	int  dFPixel       = 0;
//
//	// Verify FITS file handle
//	// ----------------------------------------------------------
//	if ( m_fptr.get() == NULL )
//	{
//		ThrowException( "Read3D",
//						"Invalid FITS handle, no file open!" );
//	}
//
//	// Get the image parameters
//	// ----------------------------------------------------------
//	fits_get_img_param( m_fptr.get(),
//						CArcFitsFile::NAXES_SIZE,
//						&dBitsPerPixel,
//						&dNAxis,
//						lNAxes,
//						&dFitsStatus );
//
//	if ( dFitsStatus )
//	{
//		ThrowException( "Read3D", dFitsStatus );
//	}
//
//	if ( dNAxis != 3 )
//	{
//		ThrowException( "Read3D",
//				string( "Invalid NAXIS value. This " ) +
//				string( "method is only valid for a FITS data cube." ) );
//	}
//
//	// Verify parameters
//	// ----------------------------------------------------------
//	if ( ( dImageNumber + 1 ) > lNAxes[ CArcFitsFile::NAXES_NOF ] )
//	{
//		ostringstream oss;
//
//		oss << "Invalid image number. File contains "
//			<< lNAxes[ CArcFitsFile::NAXES_NOF ]
//			<< " images." << ends;
//
//		ThrowException( "Read3D", oss.str() );
//	}
//
//	// Set number of pixels to read
//	// ----------------------------------------------------------
//	lNElements = lNAxes[ CArcFitsFile::NAXES_COL ] * lNAxes[ CArcFitsFile::NAXES_ROW ];
//	dFPixel = lNElements * dImageNumber + 1;
//
//	// Read 16-bit data
//	// ---------------------------------------------------------------
//	if ( dBitsPerPixel == CArcFitsFile::BPP16 )
//	{
//		m_pDataBuffer.reset( new unsigned short[ lNElements ],
//							 &CArcFitsFile::ArrayDeleter<unsigned short> ); //&CArcFitsFile::ArrayDeleter );
//
//		if ( m_pDataBuffer.get() == NULL )
//		{
//			ThrowException(
//				"Read3D",
//				"Failed to allocate buffer for image pixel data" );
//		}
//
//		// Read the image data
//		// ----------------------------------------------------------
//		fits_read_img( m_fptr.get(),
//					   TUSHORT,
//					   dFPixel,
//					   lNElements,
//					   NULL,
//					   m_pDataBuffer.get(),
//					   NULL,
//					   &dFitsStatus );
//
//		if ( dFitsStatus )
//		{
//			ThrowException( "Read3D", dFitsStatus );
//		}
//	}
//
//	// Read 32-bit data
//	// ---------------------------------------------------------------
//	else if ( dBitsPerPixel == CArcFitsFile::BPP32 )
//	{
//		m_pDataBuffer.reset( new unsigned int[ lNElements ],
//							 &CArcFitsFile::ArrayDeleter<unsigned int> ); //&CArcFitsFile::ArrayDeleter );
//
//		if ( m_pDataBuffer.get() == NULL )
//		{
//			ThrowException(
//				"Read3D",
//				"Failed to allocate buffer for image pixel data." );
//		}
//
//		// Read the image data
//		// ----------------------------------------------------------
//		fits_read_img( m_fptr.get(),
//					   TUINT,
//					   dFPixel,
//					   lNElements,
//					   NULL,
//					   m_pDataBuffer.get(),
//					   NULL,
//					   &dFitsStatus );
//
//		if ( dFitsStatus )
//		{
//			ThrowException( "Read3D", dFitsStatus );
//		}
//	}
//
//	// Invalid bits-per-pixel value
//	// ---------------------------------------------------------------
//	else
//	{
//		ThrowException(
//			"Read3D",
//			"Invalid dBitsPerPixel, should be 16 ( BPP16 ) or 32 ( BPP32 )." );
//	}
//
//	return m_pDataBuffer.get();
//}
//
//// +---------------------------------------------------------------------------+
//// |  GetBaseFile                                                              |
//// +---------------------------------------------------------------------------+
//// |  Returns the underlying cfitsio file pointer.  INTERNAL USE ONLY.         |
//// +---------------------------------------------------------------------------+
//fitsfile* CArcFitsFile::GetBaseFile()
//{
//	return m_fptr.get();
//}
//
//// +---------------------------------------------------------------------------+
//// |  ThrowException                                                           |
//// +---------------------------------------------------------------------------+
//// |  Throws a std::runtime_error based on the supplied cfitsio status value.  |
//// |                                                                           |
//// |  <IN> -> sMethodName : Name of the method where the exception occurred.   |
//// |  <IN> -> dFitsStatus : cfitsio error value.                               |
//// +---------------------------------------------------------------------------+
//void CArcFitsFile::ThrowException( std::string sMethodName, int dFitsStatus )
//{
//	char szFitsMsg[ 100 ];
//	ostringstream oss;
//
//	fits_get_errstatus( dFitsStatus, szFitsMsg );
//
//	oss << "( CArcFitsFile::"
//		<< ( sMethodName.empty() ? "???" : sMethodName )
//		<< "() ): "
//		<< szFitsMsg
//		<< ends;
//
//	throw std::runtime_error( ( const std::string )oss.str() );
//}
//
//// +---------------------------------------------------------------------------+
//// |  ThrowException                                                           |
//// +---------------------------------------------------------------------------+
//// |  Throws a std::runtime_error based on the supplied cfitsion status value. |
//// |                                                                           |
//// |  <IN> -> sMethodName : Name of the method where the exception occurred.   |
//// |  <IN> -> sMsg        : The exception message.                             |
//// +---------------------------------------------------------------------------+
//void CArcFitsFile::ThrowException( std::string sMethodName, std::string sMsg )
//{
//	ostringstream oss;
//
//	oss << "( CArcFitsFile::"
//		<< ( sMethodName.empty() ? "???" : sMethodName )
//		<< "() ): "
//		<< sMsg
//		<< ends;
//
//	throw std::runtime_error( ( const std::string )oss.str() );
//}
