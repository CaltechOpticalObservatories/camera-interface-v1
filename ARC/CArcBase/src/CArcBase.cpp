// +------------------------------------------------------------------------------------------------------------------+
// |  FILE:  CArcBase.cpp  ( Gen3 )                                                                                   |
// +------------------------------------------------------------------------------------------------------------------+
// |  PURPOSE: This file implements the ARC device base class.                                                        |
// |                                                                                                                  |
// |  AUTHOR:  Scott Streit			DATE: March 25, 2020                                                              |
// |                                                                                                                  |
// |  Copyright 2013 Astronomical Research Cameras, Inc. All rights reserved.                                         |
// +------------------------------------------------------------------------------------------------------------------+
#ifdef _WINDOWS

	#include <windows.h>

#else

	#include <cstring>

#endif

#include <system_error>
#include <exception>
#include <iomanip>
#include <sstream>
#include <string>
#include <cstdint>
#include <iterator>
#include <regex>

#include <CArcBase.h>



namespace arc
{
	namespace gen3
	{

		// +----------------------------------------------------------------------------------------------------------+
		// | Library build and version info                                                                           |
		// +----------------------------------------------------------------------------------------------------------+
		const std::string CArcBase::m_sVersion = std::string( "ARC Gen III Base API Library v3.6.     " ) +

		#ifdef _WINDOWS
			CArcBase::formatString( "[ Compiler Version: %d, Built: %s ]", _MSC_VER, __TIMESTAMP__ );
		#else
			arc::gen3::CArcBase::formatString( "[ Compiler Version: %d.%d.%d, Built: %s %s ]", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__, __DATE__, __TIME__ );
		#endif


		// +----------------------------------------------------------------------------------------------------------+
		// |  version                                                                                                 |
		// +----------------------------------------------------------------------------------------------------------+
		// |  Returns a textual representation of the library version.                                                |
		// +----------------------------------------------------------------------------------------------------------+
		const std::string CArcBase::version( void )
		{
			return m_sVersion;
		}


		// +----------------------------------------------------------------------------------------------------------+
		// |  zeroMemory                                                                                              |
		// +----------------------------------------------------------------------------------------------------------+
		// |  Zero the specified buffer.                                                                              |
		// |                                                                                                          |
		// |  <IN> pDest  - Pointer to the buffer to zero.                                                            |
		// |  <IN> uiSize  - The size of the buffer ( in bytes ).                                                     |
		// |                                                                                                          |
		// |  Throws a std::runtime_error exception on error.                                                         |
		// +----------------------------------------------------------------------------------------------------------+
		void CArcBase::zeroMemory( void* pDest, std::uint32_t uiSize )
		{
			if ( pDest == nullptr )
			{
				THROW( "Invalid buffer pointer ( null )." );
			}

			try
			{
				#ifdef _WINDOWS
					SecureZeroMemory( pDest, uiSize );
				#else
					memset( pDest, 0, uiSize );
				#endif
			}
			catch ( const std::system_error& e )
			{
				THROW( "System error - %s", e.what() );
			}
		}


		// +----------------------------------------------------------------------------------------------------------+
		// |  copyMemory                                                                                              |
		// +----------------------------------------------------------------------------------------------------------+
		// |  Copies the source buffer into the destination buffer.                                                   |
		// |                                                                                                          |
		// |  <IN> pDest  - Pointer to the destination buffer to copy into.                                           |
		// |  <IN> pSrc   - Pointer to the source buffer to copy out of.                                              |
		// |  <IN> uiSize - The size of the buffers ( in bytes ).                                                     |
		// |                                                                                                          |
		// |  Throws a std::runtime_error exception on error.                                                         |
		// +----------------------------------------------------------------------------------------------------------+
		void CArcBase::copyMemory( void* pDest, void* pSrc, std::uint32_t uiSize )
		{
			if ( pDest == nullptr )
			{
				THROW( "Invalid destination buffer pointer ( null )." );
			}

			if ( pSrc == nullptr )
			{
				THROW( "Invalid source buffer pointer ( null )." );
			}

			try
			{
				#ifdef _WINDOWS
					memcpy_s( pDest, uiSize, pSrc, uiSize );
				#else
					memcpy( pDest, pSrc, uiSize );
				#endif
			}
			catch ( const std::system_error& e )
			{
				THROW( "System error - %s", e.what() );
			}
		}


		// +----------------------------------------------------------------------------------------------------------+
		// |  throwOutOfRange                                                                                         |
		// +----------------------------------------------------------------------------------------------------------+
		// |  Throws a std::out_of_range exception.                                                                   |
		// |                                                                                                          |
		// |  <IN> sMethodName - Name of the method where the exception occurred.                                     |
		// |  <IN> iLine	   - The line number where the exception occurred.                                        |
		// |  <IN> uiElement   - The element that's out of range.                                                     |
		// |  <IN> range       - A pair that defines the valid range.                                                 |
		// +----------------------------------------------------------------------------------------------------------+
		void CArcBase::throwOutOfRange( const std::string& sMethodName, std::int32_t iLine, std::uint32_t uiElement,
										std::pair<std::uint32_t, std::uint32_t> range )
		{
			std::ostringstream oss;

			oss << "Element [ " << uiElement << " ] out of range [ "
				<< std::get<0>( range ) << " - " << std::get<1>( range ) << " ]"
				<< std::endl
				<< "Trace: ( " << sMethodName << "() line: " << iLine << " )"
				<< std::ends;

			throw std::out_of_range( oss.str() );
		}


		// +----------------------------------------------------------------------------------------------------------+
		// |  throwNoDeviceError                                                                                      |
		// +----------------------------------------------------------------------------------------------------------+
		// |  Throws a std::runtime_error exception.                                                                  |
		// |                                                                                                          |
		// |  <IN> sMethodName - Name of the method where the exception occurred.                                     |
		// |  <IN> iLine	   - The line number where the exception occurred.                                        |
		// |  <IN> uiElement   - The element that's out of range.                                                     |
		// |  <IN> range       - A pair that defines the valid range.                                                 |
		// +----------------------------------------------------------------------------------------------------------+
		void CArcBase::throwNoDeviceError( const std::string& sMethodName, std::int32_t iLine, const std::string& sMsg )
		{
			std::ostringstream oss;

			oss << "Not connected to any device. ";

			if ( !sMsg.empty() )
			{
				oss << sMsg;
			}

			oss << std::ends;

			throw std::runtime_error( oss.str() );
		}


		// +----------------------------------------------------------------------------------------------------------+
		// |  getSystemMessage                                                                                        |
		// +----------------------------------------------------------------------------------------------------------+
		// |  Returns a string that represents the specified system error code.                                       |
		// |                                                                                                          |
		// |  <IN> uiCode/iCode - A system error code ( GetLastError() on windows, errno on linux/mac ).              |
		// +----------------------------------------------------------------------------------------------------------+
		#ifdef _WINDOWS
		const std::string CArcBase::getSystemMessage( std::uint32_t uiCode )
		#else
		const std::string CArcBase::getSystemMessage( std::int32_t iCode )
		#endif
		{
			#ifdef _WINDOWS
				std::ostringstream	oss;
				std::string			sMsg;

				LPTSTR lpBuffer;

				FormatMessage(  FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
								NULL,
								( DWORD )uiCode,
								MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ),
								( LPTSTR )&lpBuffer,
								0,
								NULL );

				char szBuffer[ 1024 ] = { '\0' };

				std::int32_t iBytes = WideCharToMultiByte( CP_ACP,
														   0,
														   reinterpret_cast<LPWSTR>( lpBuffer ),
														   -1,
														   szBuffer,
														   sizeof( szBuffer ),
														   NULL,
														   NULL );

				if ( iBytes > 0 )
				{
					oss << szBuffer;
				}

				LocalFree( lpBuffer );

			#else

				std::ostringstream	oss;
				std::string			sMsg;

				if ( iCode != -1 )
				{
					oss << "( errno: " << iCode << " ) - " << strerror( iCode );
				}

				//
				// Remove and return characters '\r' or '\n' at the end of the string.
				//
				sMsg = oss.str();

				if ( sMsg.back() == '\n' || sMsg.back() == '\r' )
				{
					sMsg.erase( sMsg.end() - 1, sMsg.end() );
				}

				//return sMsg;

			#endif

			//
			// Remove and return characters '\r' or '\n' at the end of the string.
			//
			sMsg = oss.str();

			if ( sMsg.back() == '\n' || sMsg.back() == '\r' )
			{
				sMsg.erase( sMsg.end() - 1, sMsg.end() );
			}

			return std::move( sMsg );
		}


		// +----------------------------------------------------------------------------------------------------------+
		// |  getSystemError                                                                                          |
		// +----------------------------------------------------------------------------------------------------------+
		// |  Returns a string that represents the specified system error code.                                       |
		// |                                                                                                          |
		// |  <IN> uiCode/iCode - A system error code ( GetLastError() on windows, errno on linux/mac ).              |
		// +----------------------------------------------------------------------------------------------------------+
		#ifdef _WINDOWS
		std::uint32_t CArcBase::getSystemError( void )
		#else
		std::int32_t CArcBase::getSystemError( void )
		#endif
		{
			#ifdef _WINDOWS

				return static_cast<std::uint32_t>( ::GetLastError() );

			#else

				return static_cast< std::int32_t >( errno );

			#endif
		}


		// +----------------------------------------------------------------------------------------------------------+
		// |  splitString                                                                                             |
		// +----------------------------------------------------------------------------------------------------------+
		// |  Splits a string based on the specified deliminator.  Returns A pointer to a CArcStringList object       |
		// |  that contains the string tokens.                                                                        |
		// |                                                                                                          |
		// |  <IN> sString		- The string to split.                                                                |
		// |  <IN> zDelim		- The character used to split the string ( default = ' ' ).                           |
		// +----------------------------------------------------------------------------------------------------------+
		std::unique_ptr<CArcStringList> CArcBase::splitString( const std::string& sString, const char& zDelim )
		{
			std::unique_ptr<CArcStringList> pList( new CArcStringList() );

			const std::regex ws_re( "\\s+" );

			for ( auto it = std::sregex_token_iterator( sString.begin(), sString.end(), ws_re, -1 ); it != std::sregex_token_iterator(); ++it )
			{
				( *pList ) << *it;
			}

			//auto itFirst	= sString.begin();
			//auto it			= sString.begin();

			//for ( it = sString.begin(); it != sString.end(); ++it )
			//{
			//	if ( zDelim == *it )
			//	{
			//		if ( itFirst != it )
			//		{
			//			pList->add( std::string( itFirst, it ) );

			//			itFirst = it + 1;	// Skip the delimiter
			//		}
			//		else
			//		{
			//			++itFirst;
			//		}
			//	}
			//}

			//if ( itFirst != it )
			//{
			//	pList->add( std::string( itFirst, it ) );
			//}

			return pList;
		}


		// +----------------------------------------------------------------------------------------------------------+
		// |  formatString                                                                                            |
		// +----------------------------------------------------------------------------------------------------------+
		// |  Formats a std::string using printf-style formatting.                                                    |
		// |                                                                                                          |
		// |  <IN> pszFmt - A printf-style format string, followed by the variables.                                  |
		// |                                                                                                          |
		// |				   Acceptable format parameters:                                                          |
		// |				   %d   -> integer                                                                        |
		// |				   %u   -> unsigned integer                                                               |
		// |				   %f   -> double                                                                         |
		// |				   %l   -> long                                                                           |
		// |				   %j   -> unsigned long                                                                  |
		// |				   %J   -> unsigned long long                                                             |
		// |				   %s   -> char * string                                                                  |
		// |				   %e   -> system message                                                                 |
		// |                   %b   -> bool                                                                           |
		// |				   %x/X -> lower/upper case hexadecimal integer                                           |
		// +----------------------------------------------------------------------------------------------------------+
		std::string CArcBase::formatString( const char *pszFmt, ... )
		{
			std::ostringstream oss;
			char* pzChar;
			char* pszVal;

			va_list ap;

			va_start( ap, pszFmt );

			for ( pzChar = ( char * )pszFmt; *pzChar; pzChar++ )
			{
				if ( *pzChar != '%' )
				{
					oss << *pzChar;
					continue;
				}

				switch ( *++pzChar )
				{
					case 'd':
					{
						oss << va_arg( ap, int );
					}
					break;

					case 'u':
					{
						oss << va_arg( ap, unsigned int );
					}
					break;

					case 'l':
					{
						oss << va_arg( ap, long );
					}
					break;

					case 'j':
					{
						oss << va_arg( ap, unsigned long );
					}
					break;

					case 'J':
					{
						oss << va_arg( ap, unsigned long long );
					}
					break;

					case 'f':
					{
						oss << va_arg( ap, double );
					}
					break;

					case 's':
					{
						for ( pszVal = va_arg( ap, char * ); *pszVal; pszVal++ )
						{
							oss << *pszVal;
						}
					}
					break;

					case 'e':
					{
						#ifdef _WINDOWS
							oss << CArcBase::getSystemMessage( va_arg( ap, std::uint32_t ) );
						#else
							oss << CArcBase::getSystemMessage( va_arg( ap, std::int32_t ) );
						#endif
					}
					break;

					case 'X':
					case 'x':
					{
						oss << std::uppercase << std::hex << va_arg( ap, unsigned int ) << std::dec;
					}
					break;

					case 'b':
					{
						oss << std::boolalpha << ( va_arg( ap, int ) > 0 ? true : false );
					}
					break;

					default:
					{
						oss << *pzChar;
					}
					break;
				}
			}

			va_end( ap );

			oss << std::ends;
			
			return oss.str();
		}


		// +----------------------------------------------------------------------------------------------------------+
		// |  convertWideToAnsi                                                                                       |
		// +----------------------------------------------------------------------------------------------------------+
		// |  Converts the specified wide char string (unicode) to an ansi std::string.                               |
		// |                                                                                                          |
		// |  <IN> -> wzString - Wide character string to be converted to std::string.                                |
		// +----------------------------------------------------------------------------------------------------------+
		std::string CArcBase::convertWideToAnsi( wchar_t wzString[ ] )
		{
			std::string newString = "";

			#ifdef _WINDOWS

				char szBuffer[ 1024 ] = { '\0' };

				std::int32_t iBytes = WideCharToMultiByte( CP_ACP,
														   0,
														   ( LPCWSTR )wzString,
														   -1,
														   szBuffer,
														   sizeof( szBuffer ),
														   NULL,
														   NULL );

				if ( iBytes > 0 )
				{
					newString = std::string( szBuffer );
				}

			#endif

			return newString;
		}


		// +----------------------------------------------------------------------------------------------------------+
		// |  convertWideToAnsi                                                                                       |
		// +----------------------------------------------------------------------------------------------------------+
		// |  Converts the specified wide string ( unicode ) to an ansi std::string.                                  |
		// |                                                                                                          |
		// |  <IN> -> wsString - Wide string to be converted to std::string.                                          |
		// +----------------------------------------------------------------------------------------------------------+
		std::string CArcBase::convertWideToAnsi( const std::wstring& wsString )
		{
			std::ostringstream oss;

			const std::ctype<wchar_t>& ctfacet = std::use_facet< std::ctype<wchar_t> >( oss.getloc() );

			for ( size_t i = 0; i < wsString.size(); ++i )
			{
				oss << ctfacet.narrow( wsString[ i ], 0 );
			}

			return oss.str();
		}


		// +----------------------------------------------------------------------------------------------------------+
		// |  convertAnsiToWide                                                                                       |
		// +----------------------------------------------------------------------------------------------------------+
		// |  Converts the specified ANSI char string to a unicode std::wstring.                                      |
		// |                                                                                                          |
		// |  <IN> -> cString - ANSI C character string to be converted to std::wstring.                              |
		// +----------------------------------------------------------------------------------------------------------+
		std::wstring CArcBase::convertAnsiToWide( const char *pszString )
		{
			std::wostringstream wostr;

			wostr << pszString << std::ends;

			return wostr.str();
		}


		// +----------------------------------------------------------------------------------------------------------+
		// |  cmdToString                                                                                             |
		// +----------------------------------------------------------------------------------------------------------+
		// |  Returns the character std::string representation of the specified command. If the command fails to be   |
		// |  converted, then the hexadecimal value of the command is returned as a character string.                 |
		// |                                                                                                          |
		// |  Example:  value = 0x52444343 -> returns 'RDCC'                                                          |
		// |                                                                                                          |
		// |  <IN> -> uiCmd - The command to be converted to a character std::string.                                 |
		// +----------------------------------------------------------------------------------------------------------+
		std::string CArcBase::cmdToString( std::uint32_t uiCmd )
		{
			std::ostringstream oss;

			oss.setf( std::ios::hex, std::ios::basefield );
			oss.setf( std::ios::uppercase );

			//
			// GenIV 4-letter ascii commands
			//
			if ( ( uiCmd & 0xff000000 ) != 0 )
			{
				//if ( isgraph( ( ( uiCmd & 0xff000000 ) >> 24 ) ) &&
				//	 isgraph( ( ( uiCmd & 0x00ff0000 ) >> 16 ) ) &&
				//	 isgraph( ( ( uiCmd & 0x0000ff00 ) >> 8 ) ) &&
				//	 isgraph( ( uiCmd & 0x000000ff ) ) )
				//{
				//	oss << ( char )( ( uiCmd & 0xff000000 ) >> 24 )
				//		<< ( char )( ( uiCmd & 0x00ff0000 ) >> 16 )
				//		<< ( char )( ( uiCmd & 0x0000ff00 ) >> 8 )
				//		<< ( char )( uiCmd & 0x000000ff );
				//}

				if ( isgraph( ( ( uiCmd >> 24 ) & 0xFF ) ) &&
					 isgraph( ( ( uiCmd >> 16 ) & 0xFF ) ) &&
					 isgraph( ( ( uiCmd >> 8 )  & 0xFF ) ) &&
					 isgraph( ( uiCmd & 0xFF ) ) )
				{
					oss << static_cast< char >( ( uiCmd >> 24 ) & 0xFF )
						<< static_cast< char >( ( uiCmd >> 16 ) & 0xFF )
						<< static_cast< char >( ( uiCmd >> 8 ) & 0xFF )
						<< static_cast< char >( uiCmd & 0xFF );
				}

				else
				{
					oss << "0x" << uiCmd;
				}
			}

			//
			// 3-letter ascii commands
			//
			else
			{
				if ( isgraph( ( ( uiCmd >> 16 ) & 0xFF ) ) &&
					 isgraph( ( ( uiCmd >> 8 ) & 0xFF ) ) &&
					 isgraph( ( uiCmd & 0xFF ) ) )
				{
					oss << static_cast< char >( ( uiCmd >> 16 ) & 0xFF )
						<< static_cast< char >( ( uiCmd >> 8 ) & 0xFF )
						<< static_cast< char >( uiCmd & 0xFF );
				}

				else
				{
					oss << "0x" << std::setfill( '0' ) << std::setw( 8 ) << uiCmd;
				}
			}

			return oss.str();
		}


		// +----------------------------------------------------------------------------------------------------------+
		// |  cmdToString                                                                                             |
		// +----------------------------------------------------------------------------------------------------------+
		// |  Returns the character std::string representation of the specified command. If the command fails to be   |
		// |  converted, then the hexadecimal value of the command is returned as a character string.                 |
		// |                                                                                                          |
		// |  Example:  value = 0x52444343 -> returns 'RDCC'                                                          |
		// |                                                                                                          |
		// |  <IN> -> uiCmd    - The command reply to be converted to a character std::string.                        |
		// |  <IN> -> tCmdList - The command list to be converted to a character std::string.                         |
		// +----------------------------------------------------------------------------------------------------------+
		std::string CArcBase::cmdToString( std::uint32_t uiReply, const std::initializer_list<std::uint32_t>& tCmdList )
		{
			std::ostringstream oss;

			oss << "[ "
				<< iterToString( tCmdList.begin(), tCmdList.end() )
				<< " -> " 
				<< cmdToString( uiReply )
				<< " ]"
				<< std::ends;

			return oss.str();
		}


		// +----------------------------------------------------------------------------------------------------------+
		// |  Default denstructor                                                                                     |
		// +----------------------------------------------------------------------------------------------------------+
		CArcBase::~CArcBase( void )
		{
		}


	}		// end gen3 namespace
}			// end arc namespace
