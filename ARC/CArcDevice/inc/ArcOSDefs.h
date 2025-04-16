// +----------------------------------------------------------------------------
// |  FILE:  ArcOSDefs.h
// +----------------------------------------------------------------------------
// |  PURPOSE: This file redefines system dependent functions and data types.
// |
// |  AUTHOR:  Scott Streit			DATE: Dec 8, 2010
// +----------------------------------------------------------------------------
#ifndef _ARC_OS_DEFS_H_
#define _ARC_OS_DEFS_H_


#ifdef _WINDOWS

	#include <windows.h>

#elif defined( __APPLE__ )

	#include <IOKit/IOKitLib.h>
	#include <ApplicationServices/ApplicationServices.h>

#else

	#include <unistd.h>

#endif

#include <string>



#ifdef __cplusplus
namespace arc
{
#endif

	// +==================================================================+
	// |  WINDOWS DEFINITIONS
	// +==================================================================+
	#ifdef _WINDOWS

		//  Data Types
		// +---------------------------------+
		typedef unsigned short	ushort;
		typedef unsigned long	ulong;
		typedef unsigned char	ubyte;
		typedef unsigned int	uint;
		typedef HANDLE			Arc_DevHandle;

		//  Constants
		// +---------------------------------+
		#define ARC_CTRL_ID			33000  // Used as part of DeviceIoCtrl
		#define	MAP_FAILED			0
		#define ARC_MAX_PATH		_MAX_PATH

		//  Functions
		// +---------------------------------+
		#define Arc_GetPageSize()						1
		#define Arc_ZeroMemory( pDest, dSize )			ZeroMemory( pDest, dSize )
		#define Arc_CopyMemory( pDest, pSrc, dSize )	CopyMemory( pDest, pSrc, dSize )
		#define Arc_StrCopy( pDest, dSize, pSrc )		strcpy_s( pDest, dSize, pSrc )
		#define Arc_ErrorCode()							::GetLastError()
		#define Arc_Sleep( dMilliSec )					Sleep( dMilliSec )
		#define Arc_CloseHandle( hDevice )				CloseHandle( hDevice )

		#define Arc_OpenHandle( hDev, szDevice )																	\
								std::wstring wsDevice = arc::gen3::CArcBase::convertAnsiToWide( szDevice ).c_str();	\
								hDev = CreateFile( ( LPCTSTR )( wsDevice.c_str() ),									\
													GENERIC_READ,													\
													FILE_SHARE_READ | FILE_SHARE_WRITE,								\
													NULL,															\
													OPEN_EXISTING,													\
													NULL,															\
													NULL )
		//
		// NOTE: The passed in parameter is also the output buffer. This is for
		//       simplicity and to best match the linux ioctl. A global bytes
		//       returned variable is declared as it's required.
		//
		static DWORD dBytesReturned = 0;
		#define Arc_IOCtl( hDev, dCmd, pIn, dInSize )						\
								( int )DeviceIoControl( hDev,					\
														CTL_CODE( ARC_CTRL_ID,	\
															( 0x800 | dCmd ),	\
															METHOD_BUFFERED,	\
															FILE_ANY_ACCESS ),	\
														pIn,					\
														dInSize,				\
														pIn,					\
														dInSize,				\
														&dBytesReturned,		\
														NULL )

		void* Arc_MMap( Arc_DevHandle hDev, int dMapCmd, int dSize );
		void  Arc_MUnMap( Arc_DevHandle hDev, int dMapCmd, void* pAddr, int dSize );


	// +==================================================================+
	// |  MAC DEFINITIONS
	// +==================================================================+
	#elif defined( __APPLE__ )

		//  Data Types
		// +---------------------------------+
		typedef unsigned short	ushort;
		typedef unsigned long	ulong;
		typedef unsigned char	ubyte;
		typedef unsigned int	uint;
		typedef io_connect_t    Arc_DevHandle;


		//  Constants
		// +---------------------------------+
		#define INVALID_HANDLE_VALUE		IO_OBJECT_NULL
		#define MAX_IOCTL_IN_COUNT			7
		#define MAX_IOCTL_OUT_COUNT			4
		#define ARC_MAX_PATH				512

		// User client method dispatch selectors.
		enum
		{
			kArcOpenUserClient,
			kArcCloseUserClient,
			kArcIOCtlUserClient,
			kArcNumberOfMethods // Must be last 
		};

		//  Macros
		// +---------------------------------+
		#define MKCMD( cmd )		( 0x41524300 | cmd )

		//  Functions
		// +---------------------------------+
		#define Arc_GetPageSize()						getpagesize()
		#define Arc_ZeroMemory( pDest, dSize )			memset( pDest, 0, dSize )
		#define Arc_CopyMemory( pDest, pSrc, dSize )	memcpy( pDest, pSrc, dSize )
		#define Arc_StrCopy( pDest, dSize, pSrc )		strcpy( pDest, pSrc )
		#define Arc_ErrorCode()							errno
		#define Arc_Sleep( dMilliSec )					usleep( ( 1000 * dMilliSec ) )

		int Arc_OpenHandle( Arc_DevHandle& hDev, void* pService );
		int Arc_CloseHandle( Arc_DevHandle hDev );

		void* Arc_MMap( Arc_DevHandle hDev, int dMapCmd, int dSize );
		void  Arc_MUnMap( Arc_DevHandle hDev, int dMapCmd, void* pAddr, int dSize );

		int Arc_IOCtl( Arc_DevHandle hDev, int dCmd, ushort* pArg, int dArgSize );
		int Arc_IOCtl( Arc_DevHandle hDev, int dCmd, ulong* pArg, int dArgSize );
		int Arc_IOCtl( Arc_DevHandle hDev, int dCmd, ubyte* pArg, int dArgSize );
		int Arc_IOCtl( Arc_DevHandle hDev, int dCmd, uint* pArg, int dArgSize );
		int Arc_IOCtl( Arc_DevHandle hDev, int dCmd, int* pArg, int dArgSize );


	// +==================================================================+
	// |  LINUX DEFINITIONS
	// +==================================================================+
	#else

		//  Data Types
		// +---------------------------------+
		typedef int				Arc_DevHandle;
		typedef unsigned char	ubyte;
		typedef unsigned long	ulong;

		//  Constants
		// +---------------------------------+
		#define INVALID_HANDLE_VALUE	int( -1 )
		#define ARC_MAX_PATH			512

		//  Macros
		// +---------------------------------+
		#define MKCMD( cmd )		( 0x41524300 | cmd )

		//  Functions
		// +---------------------------------+
		#define Arc_GetPageSize()						getpagesize()
		#define Arc_ZeroMemory( pDest, dSize )			memset( pDest, 0, dSize )
		#define Arc_CopyMemory( pDest, pSrc, dSize )	memcpy( pDest, pSrc, dSize )
		#define Arc_StrCopy( pDest, dSize, pSrc )		strcpy( pDest, pSrc )
		#define Arc_ErrorCode()							errno
		#define Arc_Sleep( dMilliSec )					usleep( ( 1000 * dMilliSec ) )
		#define Arc_CloseHandle( hDevice )				::close( hDevice )
		#define Arc_OpenHandle( hDev, szDevice )		hDev = ::open( szDevice, O_RDWR )

		#define Arc_IOCtl( hDev, dCmd, pArg, dArgSize )		\
									( ioctl( hDev, MKCMD( dCmd ), pArg ) < 0 ? 0 : 1 )

		#define Arc_MMap( hDev, dMapCmd, dSize )						\
									mmap( 0,							\
										  dSize,						\
										  ( PROT_READ | PROT_WRITE ),	\
										  MAP_SHARED,					\
										  hDev,							\
										  0 )

		#define Arc_MUnMap( hDev, dMapCmd, pAddr, dSize )	munmap( pAddr, dSize )

	#endif

#ifdef __cplusplus
}	// end namespace
#endif



#endif		// _ARC_OS_DEFS_H_
