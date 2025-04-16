// ArcDeviceCAPI.cpp : Defines the exported functions for the DLL application.
//
#ifndef _WINDOWS
	#include <cstring>
#endif

#include <stdexcept>
#include <cstdint>
#include <memory>
#include <string>

#include <ArcDeviceCAPI.h>
#include <CArcDevice.h>
#include <CArcPCIe.h>
#include <CArcPCI.h>



// +------------------------------------------------------------------------------------+
// | nullptr wasn't added to gcc until version 4.6, CentOS still uses 4.4
// +------------------------------------------------------------------------------------+
#if ( defined( __linux__ ) && ( __GNUC__ < 4 || __GNUC_MINOR__ < 6 ) )
	#define nullptr		NULL
#endif


// +------------------------------------------------------------------------------------+
// | Create exposure interface
// +------------------------------------------------------------------------------------+
class IFExpose :  public arc::gen3::CExpIFace
{
	public:

		IFExpose( void ( *pExposeCall )( float ) = nullptr, void ( *pReadCall )( unsigned int ) = nullptr )
		{
			pECFunc = pExposeCall;
			pRCFunc = pReadCall;
		}

		void exposeCallback( float fElapsedTime )
		{
			( *pECFunc )( fElapsedTime );
		}

		void readCallback( unsigned int uiPixelCount )
		{
			( *pRCFunc )( uiPixelCount );
		}

		void ( *pECFunc )( float );
		void ( *pRCFunc )( unsigned int );
};


// +------------------------------------------------------------------------------------+
// | Create continuous exposure interface
// +------------------------------------------------------------------------------------+
class IFConExp :  public arc::gen3::CConIFace
{
	public:

		IFConExp( void ( *pFrameCall )( unsigned int, unsigned int, unsigned int, unsigned int, void* ) = nullptr )
		{
			pFCFunc = pFrameCall;
		}

		void frameCallback( unsigned int uiFPBCount,
							unsigned int uiPCIFrameCount,
							unsigned int uiRows,
							unsigned int uiCols,
							void* pBuffer )
		{
			( *pFCFunc )( uiFPBCount, uiPCIFrameCount, uiRows, uiCols, pBuffer );
		}

		void ( *pFCFunc )( unsigned int, unsigned int, unsigned int, unsigned int, void* );
};


// +------------------------------------------------------------------------------------+
// | Globals
// +------------------------------------------------------------------------------------+
static std::unique_ptr<arc::gen3::CArcDevice> g_pCDevice( static_cast<arc::gen3::CArcDevice*>( nullptr ) );

static char g_szErrMsg[ ARC_ERROR_MSG_SIZE ];
static char g_szTmpBuf[ ARC_MSG_SIZE ];

static char**	g_pDeviceList	= nullptr;
static int		g_dDeviceCount	= 0;

static bool		g_bAbort		= false;


// +------------------------------------------------------------------------------------+
// | Define system dependent macros
// +------------------------------------------------------------------------------------+
#ifndef ARC_SPRINTF
#define ARC_SPRINTF

	#ifdef _WINDOWS
		#define ArcSprintf( dst, size, fmt, msg )	sprintf_s( dst, size, fmt, msg )
	#else
		#define ArcSprintf( dst, size, fmt, msg )	sprintf( dst, fmt, msg )
	#endif

#endif

#ifndef ARC_ZEROMEM
#define ARC_ZEROMEM

	#ifdef _WINDOWS
		#define ArcZeroMemory( mem, size )	ZeroMemory( mem, size )
	#else
		#define ArcZeroMemory( mem, size )	memset( mem, 0, size )
	#endif

#endif


// +------------------------------------------------------------------------------------+
// | Verify class pointer macro
// +------------------------------------------------------------------------------------+
#define VERIFY_CLASS_PTR( ptr )													\
						if ( ptr.get() == nullptr )								\
						{														\
							THROW( "Invalid class object pointer!" );			\
						}


// +------------------------------------------------------------------------------------+
// | Device class constants
// +------------------------------------------------------------------------------------+
const int DEVICE_NOPARAM = arc::gen3::CArcDevice::NOPARAM;


// +----------------------------------------------------------------------------
// |  toString
// +----------------------------------------------------------------------------
// |  Returns a std::string that represents the gen3 controlled by this library.
// |
// |  <OUT> -> pStatus : Status equals ARC_STATUS_OK or ARC_STATUS_ERROR
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API const char* ArcDevice_ToString( int* pStatus )
{
	*pStatus = ARC_STATUS_OK;

	arc::gen3::CArcBase::zeroMemory( g_szTmpBuf, ARC_MSG_SIZE );

	try
	{
		VERIFY_CLASS_PTR( g_pCDevice )

		ArcSprintf( g_szTmpBuf,
					ARC_MSG_SIZE,
					"%s",
					g_pCDevice.get()->toString().c_str() );
	}
	catch ( const std::exception& e )
	{
		*pStatus = ARC_STATUS_ERROR;

		ArcSprintf( g_szErrMsg, ARC_ERROR_MSG_SIZE, "%s", e.what() );
	}

	return g_szTmpBuf;
}


// +----------------------------------------------------------------------------
// |  findDevices
// +----------------------------------------------------------------------------
// |  Searches for available ARC, Inc PCIe devices and stores the list, which
// |  can be accessed via gen3 number ( 0,1,2... ).
// |
// |  <OUT> -> pStatus : Status equals ARC_STATUS_OK or ARC_STATUS_ERROR
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API void ArcDevice_FindDevices( int* pStatus )
{
	*pStatus = ARC_STATUS_OK;

	//
	// Check for ARC-64 PCI devices
	//
	try
	{
		arc::gen3::CArcPCI::findDevices();
	}
	catch ( ... ) {}

	//
	// Check for ARC-66 PCIe devices
	//
	try
	{
		arc::gen3::CArcPCIe::findDevices();
	}
	catch ( ... ) {}

	//
	// Return error if no devices found
	//
	if ( ArcDevice_DeviceCount() <= 0 )
	{
		*pStatus = ARC_STATUS_ERROR;

		ArcSprintf( g_szErrMsg,
					ARC_ERROR_MSG_SIZE,
					"%s",
					"No ARC, Inc. PCI or PCIe devices found!\n" );
	}
}


// +----------------------------------------------------------------------------
// |  deviceCount
// +----------------------------------------------------------------------------
// |  Returns the number of items in the gen3 list. Must be called after
// |  findDevices().
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API unsigned int ArcDevice_DeviceCount()
{
	return ( arc::gen3::CArcPCI::deviceCount() + arc::gen3::CArcPCIe::deviceCount() );
}


// +----------------------------------------------------------------------------
// |  getDeviceStringList
// +----------------------------------------------------------------------------
// |  Returns a std::string list representation of the gen3 list.
// |
// |  <OUT> -> pStatus : Status equals ARC_STATUS_OK or ARC_STATUS_ERROR
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API const char** ArcDevice_GetDeviceStringList( int* pStatus )
{
	*pStatus = ARC_STATUS_OK;

	try
	{
		if ( ArcDevice_DeviceCount() > 0 )
		{
			const std::string* sPCIList  = arc::gen3::CArcPCI::getDeviceStringList();
			const std::string* sPCIeList = arc::gen3::CArcPCIe::getDeviceStringList();

			if ( g_pDeviceList != nullptr )
			{
				ArcDevice_FreeDeviceStringList();
			}

			g_pDeviceList = new char*[ ArcDevice_DeviceCount() ];

			if ( g_pDeviceList == nullptr )
			{
				THROW( "Failed to allocate list memory!" );
			}

			//  Get PCI list
			// +-----------------------------------------+
			for ( std::uint32_t i = 0; i < arc::gen3::CArcPCI::deviceCount(); i++ )
			{
				g_pDeviceList[ i ] = new char[ 100 ];

				ArcSprintf( &g_pDeviceList[ i ][ 0 ], 100, "%s", sPCIList[ i ].c_str() );
			}

			//  Get PCIe list
			// +-----------------------------------------+
			for ( std::uint32_t i = arc::gen3::CArcPCI::deviceCount(); i < ArcDevice_DeviceCount(); i++ )
			{
				g_pDeviceList[ i ] = new char[ 100 ];

				ArcSprintf( &g_pDeviceList[ i ][ 0 ],
							100,
							"%s",
							sPCIeList[ i - arc::gen3::CArcPCI::deviceCount() ].c_str() );
			}
		}
	}
	catch ( const std::exception& e )
	{
		*pStatus = ARC_STATUS_ERROR;

		ArcSprintf( g_szErrMsg, ARC_ERROR_MSG_SIZE, "%s", e.what() );
	}

	return const_cast<const char**>( g_pDeviceList );
}


// +----------------------------------------------------------------------------
// |  FreeDeviceStringList
// +----------------------------------------------------------------------------
// |  Frees the internal gen3 list. Must be called after getDeviceStringList()
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API void ArcDevice_FreeDeviceStringList()
{
	if ( g_pDeviceList != nullptr )
	{
		for ( int i=0; i<g_dDeviceCount; i++ )
		{
			delete [] g_pDeviceList[ i ];
		}

		delete [] g_pDeviceList;
	}

	g_pDeviceList = nullptr;
}


// +----------------------------------------------------------------------------
// |  isOpen
// +----------------------------------------------------------------------------
// |  Returns 'true' if connected to a gen3; 'false' otherwise.
// |
// |  <OUT> -> pStatus : Status equals ARC_STATUS_OK or ARC_STATUS_ERROR
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API unsigned int ArcDevice_IsOpen( int* pStatus )
{
	unsigned int uiOpen = 0;

	*pStatus = ARC_STATUS_OK;

	try
	{
		if ( g_pCDevice != nullptr )
		{
			uiOpen = ( g_pCDevice.get()->isOpen() == true ? 1 : 0 );
		}
	}
	catch ( const std::exception& e )
	{
		*pStatus = ARC_STATUS_ERROR;

		ArcSprintf( g_szErrMsg, ARC_ERROR_MSG_SIZE, "%s", e.what() );
	}

	return uiOpen;
}


// +----------------------------------------------------------------------------
// |  open
// +----------------------------------------------------------------------------
// |  Opens a connection to the gen3 driver associated with the specified
// |  gen3.
// |
// |  Returns ARC_STATUS_ERROR on error
// |
// |  <IN>  -> uiDeviceNumber - Device number
// |  <OUT> -> pStatus       - Status equals ARC_STATUS_OK or ARC_STATUS_ERROR
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API void ArcDevice_Open( unsigned int uiDeviceNumber, int* pStatus )
{
	*pStatus = ARC_STATUS_OK;

	try
	{
		const char** pszDevList = ArcDevice_GetDeviceStringList( pStatus );

		if ( *pStatus == ARC_STATUS_ERROR )
		{
			THROW( g_szErrMsg );
		}

		auto pTokens = arc::gen3::CArcBase::splitString( pszDevList[ uiDeviceNumber ] );

		std::string sDevice = pTokens->at( 0 );

		// Dump "Device" Text
				
		auto iDevNum = std::stoi( pTokens->at( 2 ) );

		if ( sDevice.find( "PCIe" ) != std::string::npos )
		{
			g_pCDevice.reset( new arc::gen3::CArcPCIe() );
		}

		else if ( sDevice.find( "PCI" ) != std::string::npos )
		{
			g_pCDevice.reset( new arc::gen3::CArcPCI() );
		}

		else
		{
			THROW( "( ArcDevice_Open ): No ARC device found!" );
		}

		VERIFY_CLASS_PTR( g_pCDevice )

		g_pCDevice.get()->open( iDevNum );
	}
	catch ( const std::exception& e )
	{
		*pStatus = ARC_STATUS_ERROR;

		ArcSprintf( g_szErrMsg, ARC_ERROR_MSG_SIZE, "%s", e.what() );
	}

	ArcDevice_FreeDeviceStringList();
}


// +----------------------------------------------------------------------------
// |  Open_I
// +----------------------------------------------------------------------------
// |  This version first calls open, then mapCommonBuffer if open
// |  succeeded. Basically, this function just combines the other two.
// |
// |  Throws std::runtime_error on error
// |
// |  <IN>  -> uiDeviceNumber - PCI gen3 number
// |  <IN>  -> uiBytes        - The size of the kernel image buffer in bytes
// |  <OUT> -> pStatus       - Status equals ARC_STATUS_OK or ARC_STATUS_ERROR
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API void ArcDevice_Open_I( unsigned int uiDeviceNumber, unsigned int uiBytes, int* pStatus )
{
	*pStatus = ARC_STATUS_OK;

	try
	{
		const char** pszDevList = ArcDevice_GetDeviceStringList( pStatus );

		if ( *pStatus == ARC_STATUS_ERROR )
		{
			THROW( g_szErrMsg );
		}

		auto pTokens = arc::gen3::CArcBase::splitString( pszDevList[ uiDeviceNumber ] );

		std::string sDevice = pTokens->at( 0 );

		// Dump "Device" Text
				
		auto iDevNum = std::stoi( pTokens->at( 2 ) );

		if ( sDevice.find( "PCIe" ) != std::string::npos )
		{
			g_pCDevice.reset( new arc::gen3::CArcPCIe() );
		}

		else if ( sDevice.find( "PCI" ) != std::string::npos )
		{
			g_pCDevice.reset( new arc::gen3::CArcPCI() );
		}

		else
		{
			THROW( "( ArcDevice_Open_I ): No ARC device found!" );
		}

		VERIFY_CLASS_PTR( g_pCDevice )

		g_pCDevice.get()->open( iDevNum, uiBytes );
	}
	catch ( const std::exception& e )
	{
		*pStatus = ARC_STATUS_ERROR;

		ArcSprintf( g_szErrMsg, ARC_ERROR_MSG_SIZE, "%s", e.what() );
	}

	ArcDevice_FreeDeviceStringList();
}


// +----------------------------------------------------------------------------
// |  open
// +----------------------------------------------------------------------------
// |  Convenience method. This version first calls open, then mapCommonBuffer
// |  if open succeeded. Basically, this function just combines the other two.
// |
// |  Throws std::runtime_error on error
// |
// |  <IN>  -> uiDeviceNumber - PCI gen3 number
// |  <IN>  -> uiRows         - The image buffer row size ( in pixels )
// |  <IN>  -> uiCols         - The image buffer column size ( in pixels )
// |  <OUT> -> pStatus       - Status equals ARC_STATUS_OK or ARC_STATUS_ERROR
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API
void ArcDevice_Open_II( unsigned int uiDeviceNumber, unsigned int uiRows, unsigned int uiCols, int* pStatus )
{
	*pStatus = ARC_STATUS_OK;

	try
	{
		const char** pszDevList = ArcDevice_GetDeviceStringList( pStatus );

		if ( *pStatus == ARC_STATUS_ERROR )
		{
			THROW( g_szErrMsg );
		}

		auto pTokens = arc::gen3::CArcBase::splitString( pszDevList[ uiDeviceNumber ] );

		std::string sDevice = pTokens->at( 0 );

		// Dump "Device" Text
				
		auto iDevNum = std::stoi( pTokens->at( 2 ) );

		if ( sDevice.find( "PCIe" ) != std::string::npos )
		{
			g_pCDevice.reset( new arc::gen3::CArcPCIe() );
		}

		else if ( sDevice.find( "PCI" ) != std::string::npos )
		{
			g_pCDevice.reset( new arc::gen3::CArcPCI() );
		}

		else
		{
			THROW( "( ArcDevice_Open_I ): No ARC device found!" );
		}

		VERIFY_CLASS_PTR( g_pCDevice )

		g_pCDevice.get()->open( iDevNum, uiRows, uiCols );
	}
	catch ( const std::exception& e )
	{
		*pStatus = ARC_STATUS_ERROR;

		ArcSprintf( g_szErrMsg, ARC_ERROR_MSG_SIZE, "%s", e.what() );
	}

	ArcDevice_FreeDeviceStringList();
}


// +----------------------------------------------------------------------------
// |  close
// +----------------------------------------------------------------------------
// |  Closes the currently open driver that was opened with a call to open.
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API void ArcDevice_Close( void )
{
	ArcDevice_FreeDeviceStringList();

	if ( g_pCDevice.get() != nullptr )
	{
		g_pCDevice.get()->close();

		g_pCDevice.reset( 0 );
	}
}


// +----------------------------------------------------------------------------
// |  reset
// +----------------------------------------------------------------------------
// |  Resets the PCIe board.
// |
// |  <OUT> -> pStatus - Status equals ARC_STATUS_OK or ARC_STATUS_ERROR
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API void ArcDevice_Reset( int* pStatus )
{
	*pStatus = ARC_STATUS_OK;

	try
	{
		VERIFY_CLASS_PTR( g_pCDevice )

		g_pCDevice.get()->reset();
	}
	catch ( const std::exception& e )
	{
		*pStatus = ARC_STATUS_ERROR;

		ArcSprintf( g_szErrMsg, ARC_ERROR_MSG_SIZE, "%s", e.what() );
	}
}


// +----------------------------------------------------------------------------
// |  mapCommonBuffer
// +----------------------------------------------------------------------------
// |  Map the gen3 driver image buffer.
// |
// |  <IN>  -> bBytes - The number of bytes to map as an image buffer. Not
// |                    used by PCI/e.
// |  <OUT> -> pStatus - Status equals ARC_STATUS_OK or ARC_STATUS_ERROR
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API void ArcDevice_MapCommonBuffer( unsigned int uiBytes, int* pStatus )
{
	*pStatus = ARC_STATUS_OK;

	try
	{
		VERIFY_CLASS_PTR( g_pCDevice )

		g_pCDevice.get()->mapCommonBuffer( uiBytes );
	}
	catch ( const std::exception& e )
	{
		*pStatus = ARC_STATUS_ERROR;

		ArcSprintf( g_szErrMsg, ARC_ERROR_MSG_SIZE, "%s", e.what() );
	}
}


// +----------------------------------------------------------------------------
// |  unMapCommonBuffer
// +----------------------------------------------------------------------------
// |  Un-Maps the gen3 driver image buffer.
// |
// |  <OUT> -> pStatus - Status equals ARC_STATUS_OK or ARC_STATUS_ERROR
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API void ArcDevice_UnMapCommonBuffer( int* pStatus )
{
	*pStatus = ARC_STATUS_OK;

	try
	{
		VERIFY_CLASS_PTR( g_pCDevice )

		g_pCDevice.get()->unMapCommonBuffer();
	}
	catch ( const std::exception& e )
	{
		*pStatus = ARC_STATUS_ERROR;

		ArcSprintf( g_szErrMsg, ARC_ERROR_MSG_SIZE, "%s", e.what() );
	}
}


// +----------------------------------------------------------------------------
// |  reMapCommonBuffer
// +----------------------------------------------------------------------------
// |  Remaps the kernel image buffer by first calling unMapCommonBuffer if
// |  necessary and then calls mapCommonBuffer.
// |
// |  <OUT> -> pStatus - Status equals ARC_STATUS_OK or ARC_STATUS_ERROR
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API void ArcDevice_ReMapCommonBuffer( unsigned int uiBytes, int* pStatus )
{
	*pStatus = ARC_STATUS_OK;

	try
	{
		VERIFY_CLASS_PTR( g_pCDevice )

		g_pCDevice.get()->reMapCommonBuffer( uiBytes );
	}
	catch ( const std::exception& e )
	{
		*pStatus = ARC_STATUS_ERROR;

		ArcSprintf( g_szErrMsg, ARC_ERROR_MSG_SIZE, "%s", e.what() );
	}
}


// +----------------------------------------------------------------------------
// |  fillCommonBuffer
// +----------------------------------------------------------------------------
// |  Clears the image buffer by filling it with the specified value.
// |
// |  <IN>  -> uwValue - The 16-bit value to fill the buffer with. Default = 0
// |  <OUT> -> pStatus  - Status equals ARC_STATUS_OK or ARC_STATUS_ERROR
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API void ArcDevice_FillCommonBuffer( unsigned short uwValue, int* pStatus )
{
	*pStatus = ARC_STATUS_OK;

	try
	{
		VERIFY_CLASS_PTR( g_pCDevice )

		g_pCDevice.get()->fillCommonBuffer( uwValue );
	}
	catch ( const std::exception& e )
	{
		*pStatus = ARC_STATUS_ERROR;

		ArcSprintf( g_szErrMsg, ARC_ERROR_MSG_SIZE, "%s", e.what() );
	}
}


// +----------------------------------------------------------------------------
// |  commonBufferVA
// +----------------------------------------------------------------------------
// |  Returns the virtual address of the gen3 driver image buffer.
// |
// |  <OUT> -> pStatus - Status equals ARC_STATUS_OK or ARC_STATUS_ERROR
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API void* ArcDevice_CommonBufferVA( int* pStatus )
{
	std::uint8_t* pVA = nullptr;

	*pStatus = ARC_STATUS_OK;

	try
	{
		VERIFY_CLASS_PTR( g_pCDevice )

		pVA = g_pCDevice.get()->commonBufferVA();
	}
	catch ( const std::exception& e )
	{
		*pStatus = ARC_STATUS_ERROR;

		ArcSprintf( g_szErrMsg, ARC_ERROR_MSG_SIZE, "%s", e.what() );
	}

	return pVA;
}


// +----------------------------------------------------------------------------
// |  commonBufferPA
// +----------------------------------------------------------------------------
// |  Returns the physical address of the gen3 driver image buffer.
// |
// |  <OUT> -> pStatus - Status equals ARC_STATUS_OK or ARC_STATUS_ERROR
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API unsigned long long ArcDevice_CommonBufferPA( int* pStatus )
{
	std::uint64_t u64PA = 0;

	*pStatus = ARC_STATUS_OK;

	try
	{
		VERIFY_CLASS_PTR( g_pCDevice )

		u64PA = g_pCDevice.get()->commonBufferPA();
	}
	catch ( const std::exception& e )
	{
		*pStatus = ARC_STATUS_ERROR;

		ArcSprintf( g_szErrMsg, ARC_ERROR_MSG_SIZE, "%s", e.what() );
	}

	return u64PA;
}


// +----------------------------------------------------------------------------
// |  commonBufferSize
// +----------------------------------------------------------------------------
// |  Returns the gen3 driver image buffer size in bytes.
// |
// |  <OUT> -> pStatus - Status equals ARC_STATUS_OK or ARC_STATUS_ERROR
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API unsigned long long ArcDevice_CommonBufferSize( int* pStatus )
{
	std::uint64_t u64BufSize = 0;

	*pStatus = ARC_STATUS_OK;

	try
	{
		VERIFY_CLASS_PTR( g_pCDevice )

		u64BufSize = g_pCDevice.get()->commonBufferSize();
	}
	catch ( const std::exception& e )
	{
		*pStatus = ARC_STATUS_ERROR;

		ArcSprintf( g_szErrMsg, ARC_ERROR_MSG_SIZE, "%s", e.what() );
	}

	return u64BufSize;
}


// +----------------------------------------------------------------------------
// |  getId
// +----------------------------------------------------------------------------
// |  Returns the board id
// |
// |  <OUT> -> pStatus - Status equals ARC_STATUS_OK or ARC_STATUS_ERROR
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API unsigned int ArcDevice_GetId( int* pStatus )
{
	unsigned int uiId = 0;

	*pStatus = ARC_STATUS_OK;

	try
	{
		VERIFY_CLASS_PTR( g_pCDevice )

		uiId = g_pCDevice.get()->getId();
	}
	catch ( const std::exception& e )
	{
		*pStatus = ARC_STATUS_ERROR;

		ArcSprintf( g_szErrMsg, ARC_ERROR_MSG_SIZE, "%s", e.what() );
	}

	return uiId;
}


// +----------------------------------------------------------------------------
// |  getStatus
// +----------------------------------------------------------------------------
// |  Returns the PCIe status register value.
// |
// |  <OUT> -> pStatus - Status equals ARC_STATUS_OK or ARC_STATUS_ERROR
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API unsigned int ArcDevice_GetStatus( int* pStatus )
{
	std::uint32_t uiStatus = 0;

	*pStatus = ARC_STATUS_OK;

	try
	{
		VERIFY_CLASS_PTR( g_pCDevice )

		uiStatus = g_pCDevice.get()->getStatus();
	}
	catch ( const std::exception& e )
	{
		*pStatus = ARC_STATUS_ERROR;

		ArcSprintf( g_szErrMsg, ARC_ERROR_MSG_SIZE, "%s", e.what() );
	}

	return uiStatus;
}


// +----------------------------------------------------------------------------
// |  clearStatus
// +----------------------------------------------------------------------------
// |  Clears the PCIe status register.
// |
// |  <OUT> -> pStatus - Status equals ARC_STATUS_OK or ARC_STATUS_ERROR
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API void ArcDevice_ClearStatus( int* pStatus )
{
	*pStatus = ARC_STATUS_OK;

	try
	{
		VERIFY_CLASS_PTR( g_pCDevice )

		g_pCDevice.get()->clearStatus();
	}
	catch ( const std::exception& e )
	{
		*pStatus = ARC_STATUS_ERROR;

		ArcSprintf( g_szErrMsg, ARC_ERROR_MSG_SIZE, "%s", e.what() );
	}
}


// +----------------------------------------------------------------------------
// |  set2xFOTransmitter
// +----------------------------------------------------------------------------
// |  Sets the controller to use two fiber optic transmitters.
// |
// |  <IN>  -> uiOnOff  - True to enable dual transmitters; false otherwise.
// |  <OUT> -> pStatus - Status equals ARC_STATUS_OK or ARC_STATUS_ERROR
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API void ArcDevice_Set2xFOTransmitter( unsigned int uiOnOff, int* pStatus )
{
	*pStatus = ARC_STATUS_OK;

	try
	{
		VERIFY_CLASS_PTR( g_pCDevice )

		g_pCDevice.get()->set2xFOTransmitter( ( uiOnOff == 1 ? true : false ) );
	}
	catch ( const std::exception& e )
	{
		*pStatus = ARC_STATUS_ERROR;

		ArcSprintf( g_szErrMsg, ARC_ERROR_MSG_SIZE, "%s", e.what() );
	}
}


// +----------------------------------------------------------------------------
// |  loadDeviceFile
// +----------------------------------------------------------------------------
// |  Loads a gen3 file, such as pci.lod. This function is not valid for PCIe.
// |
// |  <OUT> -> pStatus - Status equals ARC_STATUS_OK or ARC_STATUS_ERROR
// +---------------------------------------------------------------------------
GEN3_CARCDEVICE_API void ArcDevice_LoadDeviceFile( const char* pszFile, int* pStatus )
{
	*pStatus = ARC_STATUS_OK;

	try
	{
		VERIFY_CLASS_PTR( g_pCDevice )

		g_pCDevice.get()->loadDeviceFile( pszFile );
	}
	catch ( const std::exception& e )
	{
		*pStatus = ARC_STATUS_ERROR;

		ArcSprintf( g_szErrMsg, ARC_ERROR_MSG_SIZE, "%s", e.what() );
	}
}


// +----------------------------------------------------------------------------
// |  command
// +----------------------------------------------------------------------------
// |  Send a command to the controller timing or utility board. Returns the
// |  controller reply, typically DON.
// |
// |  Throws std::runtime_error on error
// |
// |  <IN>  -> uiBoardId - command board id ( TIM or UTIL )
// |  <IN>  -> uiCommand - Board command
// |  <OUT> -> pStatus  - Status equals ARC_STATUS_OK or ARC_STATUS_ERROR
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API unsigned int ArcDevice_Command( unsigned int uiBoardId, unsigned int uiCommand, int* pStatus )
{
	return ArcDevice_Command_IIII( uiBoardId,
								   uiCommand,
								   DEVICE_NOPARAM,
								   DEVICE_NOPARAM,
								   DEVICE_NOPARAM,
								   DEVICE_NOPARAM,
								   pStatus );
}


// +----------------------------------------------------------------------------
// |  Command_I
// +----------------------------------------------------------------------------
// |  Send a command to the controller timing or utility board. Returns the
// |  controller reply, typically DON.
// |
// |  Throws std::runtime_error on error
// |
// |  <IN>  -> uiBoardId - command board id ( TIM or UTIL )
// |  <IN>  -> uiCommand - Board command
// |  <IN>  -> uiArg1    - command arguments
// |  <OUT> -> pStatus  - Status equals ARC_STATUS_OK or ARC_STATUS_ERROR
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API unsigned int ArcDevice_Command_I( unsigned int uiBoardId, unsigned int uiCommand, unsigned int uiArg1, int* pStatus )
{
	return ArcDevice_Command_IIII( uiBoardId,
								   uiCommand,
								   uiArg1,
								   DEVICE_NOPARAM,
								   DEVICE_NOPARAM,
								   DEVICE_NOPARAM,
								   pStatus );
}

// +----------------------------------------------------------------------------
// |  Command_II
// +----------------------------------------------------------------------------
// |  Send a command to the controller timing or utility board. Returns the
// |  controller reply, typically DON.
// |
// |  Throws std::runtime_error on error
// |
// |  <IN>  -> uiBoardId - command board id ( TIM or UTIL )
// |  <IN>  -> uiCommand - Board command
// |  <IN>  -> uiArg1/2  - command arguments
// |  <OUT> -> pStatus  - Status equals ARC_STATUS_OK or ARC_STATUS_ERROR
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API unsigned int ArcDevice_Command_II( unsigned int uiBoardId, unsigned int uiCommand, unsigned int uiArg1, unsigned int uiArg2, int* pStatus )
{
	return ArcDevice_Command_IIII( uiBoardId,
								   uiCommand,
								   uiArg1,
								   uiArg2,
								   DEVICE_NOPARAM,
								   DEVICE_NOPARAM,
								   pStatus );
}


// +----------------------------------------------------------------------------
// |  Command_III
// +----------------------------------------------------------------------------
// |  Send a command to the controller timing or utility board. Returns the
// |  controller reply, typically DON.
// |
// |  Throws std::runtime_error on error
// |
// |  <IN>  -> uiBoardId  - command board id ( TIM or UTIL )
// |  <IN>  -> uiCommand  - Board command
// |  <IN>  -> uiArg1/2/3 - command arguments
// |  <OUT> -> pStatus   - Status equals ARC_STATUS_OK or ARC_STATUS_ERROR
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API unsigned int ArcDevice_Command_III( unsigned int uiBoardId, unsigned int uiCommand, unsigned int uiArg1, unsigned int uiArg2, unsigned int uiArg3, int* pStatus )
{
	return ArcDevice_Command_IIII( uiBoardId,
								   uiCommand,
								   uiArg1,
								   uiArg2,
								   uiArg3,
								   DEVICE_NOPARAM,
								   pStatus );
}


// +----------------------------------------------------------------------------
// |  Command_IIII
// +----------------------------------------------------------------------------
// |  Send a command to the controller timing or utility board. Returns the
// |  controller reply, typically DON.
// |
// |  Throws std::runtime_error on error
// |
// |  <IN>  -> uiBoardId    - command board id ( TIM or UTIL )
// |  <IN>  -> uiCommand    - Board command
// |  <IN>  -> uiArg1/2/3/4 - command arguments
// |  <OUT> -> pStatus     - Status equals ARC_STATUS_OK or ARC_STATUS_ERROR
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API unsigned int ArcDevice_Command_IIII( unsigned int uiBoardId, unsigned int uiCommand, unsigned int uiArg1,
														 unsigned int uiArg2, unsigned int uiArg3, unsigned int uiArg4,
														 int* pStatus )
{
	std::uint32_t uiReply = 0;

	*pStatus = ARC_STATUS_OK;

	try
	{
		VERIFY_CLASS_PTR( g_pCDevice )

		uiReply = g_pCDevice.get()->command( { uiBoardId,
											   uiCommand,
											   uiArg1,
											   uiArg2,
											   uiArg3,
											   uiArg4 } );
	}
	catch ( const std::exception& e )
	{
		*pStatus = ARC_STATUS_ERROR;

		ArcSprintf( g_szErrMsg, ARC_ERROR_MSG_SIZE, "%s", e.what() );
	}

	return uiReply;
}


// +----------------------------------------------------------------------------
// |  getControllerId
// +----------------------------------------------------------------------------
// |  Returns the controller ID or 'ERR' if none.
// |
// |  <OUT> -> pStatus - Status equals ARC_STATUS_OK or ARC_STATUS_ERROR
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API unsigned int ArcDevice_GetControllerId( int* pStatus )
{
	unsigned int uiCtlrId = 0;

	*pStatus = ARC_STATUS_OK;

	try
	{
		VERIFY_CLASS_PTR( g_pCDevice )

		uiCtlrId = g_pCDevice.get()->getControllerId();
	}
	catch ( const std::exception& e )
	{
		*pStatus = ARC_STATUS_ERROR;

		ArcSprintf( g_szErrMsg, ARC_ERROR_MSG_SIZE, "%s", e.what() );
	}

	return uiCtlrId;
}


// +----------------------------------------------------------------------------
// |  resetController
// +----------------------------------------------------------------------------
// |  Resets the controller.
// |
// |  <OUT> -> pStatus - Status equals ARC_STATUS_OK or ARC_STATUS_ERROR
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API void ArcDevice_ResetController( int* pStatus )
{
	*pStatus = ARC_STATUS_OK;

	try
	{
		VERIFY_CLASS_PTR( g_pCDevice )

		g_pCDevice.get()->resetController();
	}
	catch ( const std::exception& e )
	{
		*pStatus = ARC_STATUS_ERROR;

		ArcSprintf( g_szErrMsg, ARC_ERROR_MSG_SIZE, "%s", e.what() );
	}
}


// +----------------------------------------------------------------------------
// | isControllerConnected
// +----------------------------------------------------------------------------
// |  Returns 'true' if a controller is connected to the PCIe board.  This is
// |  for fiber optic A only.
// |
// |  <OUT> -> pStatus - Status equals ARC_STATUS_OK or ARC_STATUS_ERROR
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API unsigned int ArcDevice_IsControllerConnected( int* pStatus )
{
	unsigned int uiConnected = 0;

	*pStatus = ARC_STATUS_OK;

	try
	{
		VERIFY_CLASS_PTR( g_pCDevice )

		uiConnected = ( g_pCDevice.get()->isControllerConnected() == true ? 1 : 0 );
	}
	catch ( const std::exception& e )
	{
		*pStatus = ARC_STATUS_ERROR;

		ArcSprintf( g_szErrMsg, ARC_ERROR_MSG_SIZE, "%s", e.what() );
	}

	return uiConnected;
}


// +----------------------------------------------------------------------------
// |  setupController
// +----------------------------------------------------------------------------
// |  This is a convenience function that performs a controller setup given
// |  the specified parameters.
// |
// |  <IN>  -> uiReset      - 'true' to reset the controller.
// |  <IN>  -> uiTDL        - 'true' to send TDLs to the PCI board and any board
// |                          whose .lod file is not NULL.
// |  <IN>  -> uiPower      - 'true' to power on the controller.
// |  <IN>  -> uiRows       -  The image row dimension (in pixels).
// |  <IN>  -> uiCols       -  The image col dimension (in pixels).
// |  <IN>  -> pszTimFile  - The timing board file to load (.lod file).
// |  <IN>  -> pszUtilFile - The utility board file to load (.lod file).
// |  <IN>  -> pszPciFile  - The pci board file to load (.lod file).
// |  <OUT> -> pStatus     - Status equals ARC_STATUS_OK or ARC_STATUS_ERROR
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API void ArcDevice_SetupController( unsigned int uiReset, unsigned int uiTDL, unsigned int uiPower, unsigned int uiRows, unsigned int uiCols,
													const char* pszTimFile, const char* pszUtilFile, const char* pszPciFile, int* pStatus )
{
	*pStatus = ARC_STATUS_OK;

	try
	{
		VERIFY_CLASS_PTR( g_pCDevice )

		g_pCDevice.get()->setupController( ( uiReset == 1 ? true : false ),
										   ( uiTDL == 1 ? true : false ),
										   ( uiPower == 1 ? true : false ),
										     uiRows,
										     uiCols,
										   ( pszTimFile == nullptr ? "" : pszTimFile ),
										   ( pszUtilFile == nullptr ? "" : pszUtilFile ),
										   ( pszPciFile == nullptr ? "" : pszPciFile ),
										   false );
	}
	catch ( const std::exception& e )
	{
		*pStatus = ARC_STATUS_ERROR;

		ArcSprintf( g_szErrMsg, ARC_ERROR_MSG_SIZE, "%s", e.what() );
	}
}


// +----------------------------------------------------------------------------
// |  loadControllerFile
// +----------------------------------------------------------------------------
// |  Loads a SmallCam/GenI/II/III timing or utility file (.lod) into the
// |  specified board.
// |
// |  <IN>  -> pszFilename - The SMALLCAM or GENI/II/III TIM or UTIL lod file
// |                         to load.
// |  <IN>  -> uiValidate   - Set to 'true' if the download should be read back
// |                         and checked after every write.
// |  <OUT> -> pStatus     - Status equals ARC_STATUS_OK or ARC_STATUS_ERROR
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API void ArcDevice_LoadControllerFile( const char* pszFilename, unsigned int uiValidate, int* pStatus )
{
	*pStatus = ARC_STATUS_OK;

	try
	{
		VERIFY_CLASS_PTR( g_pCDevice )

		g_pCDevice.get()->loadControllerFile( pszFilename, ( uiValidate == 1 ? true : false ), false );
	}
	catch ( const std::exception& e )
	{
		*pStatus = ARC_STATUS_ERROR;

		ArcSprintf( g_szErrMsg, ARC_ERROR_MSG_SIZE, "%s", e.what() );
	}
}


// +----------------------------------------------------------------------------
// | setImageSize
// +----------------------------------------------------------------------------
// | Sets the image size in pixels on the controller. This is used during setup,
// | subarray, binning, continuous readout, etc., whenever the image size is
// | changed. This method will attempt to re-map the image buffer if the
// | specified image size is greater than that of the image buffer.
// |
// |  Throws std::runtime_error on error
// |
// |  <IN>  -> uiRows   - The number of rows in the image.
// |  <IN>  -> uiCols   - The number of columns in the image.
// |  <OUT> -> pStatus - Status equals ARC_STATUS_OK or ARC_STATUS_ERROR
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API void ArcDevice_SetImageSize( unsigned int uiRows, unsigned int uiCols, int* pStatus )
{
	*pStatus = ARC_STATUS_OK;

	try
	{
		VERIFY_CLASS_PTR( g_pCDevice )

		g_pCDevice.get()->setImageSize( uiRows, uiCols );
	}
	catch ( const std::exception& e )
	{
		*pStatus = ARC_STATUS_ERROR;

		ArcSprintf( g_szErrMsg, ARC_ERROR_MSG_SIZE, "%s", e.what() );
	}
}


// +----------------------------------------------------------------------------
// | getImageRows
// +----------------------------------------------------------------------------
// | Returns the image row size (pixels) that has been set on the controller.
// |
// |  <OUT> -> pStatus - Status equals ARC_STATUS_OK or ARC_STATUS_ERROR
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API unsigned int ArcDevice_GetImageRows( int* pStatus )
{
	std::uint32_t uiRows = 0;

	*pStatus = ARC_STATUS_OK;

	try
	{
		VERIFY_CLASS_PTR( g_pCDevice )

		uiRows = g_pCDevice.get()->getImageRows();
	}
	catch ( const std::exception& e )
	{
		*pStatus = ARC_STATUS_ERROR;

		ArcSprintf( g_szErrMsg, ARC_ERROR_MSG_SIZE, "%s", e.what() );
	}

	return uiRows;
}


// +----------------------------------------------------------------------------
// | getImageCols
// +----------------------------------------------------------------------------
// | Returns the image column size (pixels) that has been set on the controller.
// |
// | <OUT> -> pStatus - Status equals ARC_STATUS_OK or ARC_STATUS_ERROR
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API unsigned int ArcDevice_GetImageCols( int* pStatus )
{
	std::uint32_t uiCols = 0;

	*pStatus = ARC_STATUS_OK;

	try
	{
		VERIFY_CLASS_PTR( g_pCDevice )

		uiCols = g_pCDevice.get()->getImageCols();
	}
	catch ( const std::exception& e )
	{
		*pStatus = ARC_STATUS_ERROR;

		ArcSprintf( g_szErrMsg, ARC_ERROR_MSG_SIZE, "%s", e.what() );
	}

	return uiCols;
}


// +----------------------------------------------------------------------------
// |  getCCParams
// +----------------------------------------------------------------------------
// |  Returns the available configuration parameters. The parameter bit
// |  definitions are specified in ArcDefs.h.
// |
// |  <OUT> -> pStatus - Status equals ARC_STATUS_OK or ARC_STATUS_ERROR
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API unsigned int ArcDevice_GetCCParams( int* pStatus )
{
	std::uint32_t uiCCParams = 0;

	*pStatus = ARC_STATUS_OK;

	try
	{
		VERIFY_CLASS_PTR( g_pCDevice )

		uiCCParams = g_pCDevice.get()->getCCParams();
	}
	catch ( const std::exception& e )
	{
		*pStatus = ARC_STATUS_ERROR;

		ArcSprintf( g_szErrMsg, ARC_ERROR_MSG_SIZE, "%s", e.what() );
	}

	return uiCCParams;
}


// +----------------------------------------------------------------------------
// |  isCCParamSupported
// +----------------------------------------------------------------------------
// |  Returns 'true' if the specified configuration parameter is available on
// |  the controller.  Returns 'false' otherwise. The parameter bit definitions
// |  are specified in ArcDefs.h.
// |
// |  <IN>  -> uiParameter - The controller parameter to check.
// |  <OUT> -> pStatus    - Status equals ARC_STATUS_OK or ARC_STATUS_ERROR
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API unsigned int ArcDevice_IsCCParamSupported( int uiParameter, int* pStatus )
{
	std::uint32_t uiSupported = 0;

	*pStatus = ARC_STATUS_OK;

	try
	{
		VERIFY_CLASS_PTR( g_pCDevice )

		uiSupported = ( g_pCDevice.get()->isCCParamSupported( uiParameter ) ? 1 : 0 );
	}
	catch ( const std::exception& e )
	{
		*pStatus = ARC_STATUS_ERROR;

		ArcSprintf( g_szErrMsg, ARC_ERROR_MSG_SIZE, "%s", e.what() );
	}

	return uiSupported;
}


// +----------------------------------------------------------------------------
// |  isCCD
// +----------------------------------------------------------------------------
// |  Returns false if the controller contains an IR video processor board.
// |  Returns true otherwise.
// |
// |  <OUT> -> pStatus - Status equals ARC_STATUS_OK or ARC_STATUS_ERROR
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API unsigned int ArcDevice_IsCCD( int* pStatus )
{
	std::uint32_t uiIsCCD = 0;

	*pStatus = ARC_STATUS_OK;

	try
	{
		VERIFY_CLASS_PTR( g_pCDevice )

		uiIsCCD = ( g_pCDevice.get()->isCCD() ? 1 : 0 );
	}
	catch ( const std::exception& e )
	{
		*pStatus = ARC_STATUS_ERROR;

		ArcSprintf( g_szErrMsg, ARC_ERROR_MSG_SIZE, "%s", e.what() );
	}

	return uiIsCCD;
}


// +----------------------------------------------------------------------------
// |  isBinningSet
// +----------------------------------------------------------------------------
// |  Returns 'true' if binning is set on the controller, returns 'false'
// |  otherwise.
// |
// |  <OUT> -> pStatus - Status equals ARC_STATUS_OK or ARC_STATUS_ERROR
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API unsigned int ArcDevice_IsBinningSet( int* pStatus )
{
	std::uint32_t uiIsBin = 0;

	*pStatus = ARC_STATUS_OK;

	try
	{
		VERIFY_CLASS_PTR( g_pCDevice )

		uiIsBin = ( g_pCDevice.get()->isBinningSet() ? 1 : 0 );
	}
	catch ( const std::exception& e )
	{
		*pStatus = ARC_STATUS_ERROR;

		ArcSprintf( g_szErrMsg, ARC_ERROR_MSG_SIZE, "%s", e.what() );
	}

	return uiIsBin;
}


// +----------------------------------------------------------------------------
// |  setBinning
// +----------------------------------------------------------------------------
// |  Sets the camera controller binning factors for both the rows and columns.
// |  Binning causes each axis to combine xxxFactor number of pixels. For
// |  example, if rows = 1000, cols = 1000, rowFactor = 2, colFactor = 4
// |  then binRows = 500, binCols = 250.
// |
// |  <IN>  -> uiRows      - The pre-binning number of image rows
// |  <IN>  -> uiCols      - The pre-binning number of images columns
// |  <IN>  -> uiRowFactor - The binning row factor
// |  <IN>  -> uiColFactor - The binning column factor
// |  <OUT> -> pBinRows   - Pointer that will get binned image row value
// |  <OUT> -> pBinCols   - Pointer that will get binned image col value
// |  <OUT> -> pStatus    - Status equals ARC_STATUS_OK or ARC_STATUS_ERROR
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API void ArcDevice_SetBinning( unsigned int uiRows, unsigned int uiCols, unsigned int uiRowFactor, unsigned int uiColFactor, unsigned int* pBinRows, unsigned int* pBinCols, int* pStatus )
{
	*pStatus = ARC_STATUS_OK;

	try
	{
		VERIFY_CLASS_PTR( g_pCDevice )

		g_pCDevice.get()->setBinning( uiRows,
									  uiCols,
									  uiRowFactor,
									  uiColFactor,
									  pBinRows,
									  pBinCols );
	}
	catch ( const std::exception& e )
	{
		*pStatus = ARC_STATUS_ERROR;

		ArcSprintf( g_szErrMsg, ARC_ERROR_MSG_SIZE, "%s", e.what() );
	}
}


// +----------------------------------------------------------------------------
// |  UnsetBinning
// +----------------------------------------------------------------------------
// |  Unsets the camera controller binning factors previously set by a call to
// |  CArcDevice::setBinning().
// |
// |  <IN>  -> uiRows   - The image rows to set on the controller 
// |  <IN>  -> uiCols   - The image cols to set on the controller
// |  <OUT> -> pStatus - Status equals ARC_STATUS_OK or ARC_STATUS_ERROR
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API void ArcDevice_UnSetBinning( int uiRows, int uiCols, int* pStatus )
{
	*pStatus = ARC_STATUS_OK;

	try
	{
		VERIFY_CLASS_PTR( g_pCDevice )

		g_pCDevice.get()->unSetBinning( uiRows, uiCols );
	}
	catch ( const std::exception& e )
	{
		*pStatus = ARC_STATUS_ERROR;

		ArcSprintf( g_szErrMsg, ARC_ERROR_MSG_SIZE, "%s", e.what() );
	}
}


// +----------------------------------------------------------------------------
// |  setSubArray
// +----------------------------------------------------------------------------
// |  Sets the camera controller to sub-array mode. All parameters are in
// |  pixels.
// |
// |	+-----------------------+------------+
// |	|						|            |
// |	|          BOX	        |    BIAS    |
// |    |<----------------5------->          |
// |    |        <--4-->        |  |         |
// |	|       +-------+^      |  +-------+ |
// |	|       |       ||      |  |       | |
// |	|<------|-->2   |3      |  |<--6-->| |
// |	|       |   ^   ||		|  |	   | |
// |	|       +---|---+v      |  +-------+ |
// |	|			|	        |            |
// |	|			|	        |            |
// |	|			1	        |            |
// |	|			|	        |            |
// |	|			v	        |            |
// |	+-----------------------+------------+
// |
// |  <OUT> -> dOldRows - Returns the original row size set on the controller
// |  <OUT> -> dOldCols - Returns the original col size set on the controller
// |  <IN>  -> #1 - uiRow - The row # of the center of the sub-array in pixels
// |  <IN>  -> #2 - uiCol - The col # of the center of the sub-array in pixels
// |  <IN>  -> #3 - uiSubRows - The sub-array row count in pixels
// |  <IN>  -> #4 - uiSubCols - The sub-array col count in pixels
// |  <IN>  -> #5 - uiBiasOffset - The offset of the bias region in pixels
// |  <IN>  -> #6 - dBiasCols - The col count of the bias region in pixels
// |  <OUT> -> pStatus - Status equals ARC_STATUS_OK or ARC_STATUS_ERROR
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API void ArcDevice_SetSubArray( unsigned int* pOldRows, unsigned int* pOldCols, unsigned int uiRow, unsigned int uiCol, unsigned int uiSubRows,
												unsigned int uiSubCols, unsigned int uiBiasOffset, unsigned int uiBiasWidth, int* pStatus )
{
	*pStatus = ARC_STATUS_OK;

	try
	{
		VERIFY_CLASS_PTR( g_pCDevice )

		g_pCDevice.get()->setSubArray( *pOldRows,
									   *pOldCols,
									   uiRow,
									   uiCol,
									   uiSubRows,
									   uiSubCols,
									   uiBiasOffset,
									   uiBiasWidth );
	}
	catch ( const std::exception& e )
	{
		*pStatus = ARC_STATUS_ERROR;

		ArcSprintf( g_szErrMsg, ARC_ERROR_MSG_SIZE, "%s", e.what() );
	}
}


// +----------------------------------------------------------------------------
// |  unSetSubArray
// +----------------------------------------------------------------------------
// |  Unsets the camera controller from sub-array mode.
// |
// |  Throws std::runtime_error on error
// |
// |  <IN>  -> uiRows   - The row size to set on the controller
// |  <IN>  -> uiCols   - The col size to set on the controller
// |  <OUT> -> pStatus - Status equals ARC_STATUS_OK or ARC_STATUS_ERROR
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API void ArcDevice_UnSetSubArray( unsigned int uiRows, unsigned int uiCols, int* pStatus )
{
	*pStatus = ARC_STATUS_OK;

	try
	{
		VERIFY_CLASS_PTR( g_pCDevice )

		g_pCDevice.get()->unSetSubArray( uiRows, uiCols );
	}
	catch ( const std::exception& e )
	{
		*pStatus = ARC_STATUS_ERROR;

		ArcSprintf( g_szErrMsg, ARC_ERROR_MSG_SIZE, "%s", e.what() );
	}
}


// +----------------------------------------------------------------------------
// |  isSyntheticImageMode
// +----------------------------------------------------------------------------
// |  Returns true if synthetic readout is turned on. Otherwise, returns
// |  'false'. A synthetic image looks like: 0, 1, 2, ..., 65535, 0, 1, 2,
// |  ..., 65353, etc
// |
// |  <OUT> -> pStatus - Status equals ARC_STATUS_OK or ARC_STATUS_ERROR
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API unsigned int ArcDevice_IsSyntheticImageMode( int* pStatus )
{
	std::uint32_t uiIsSyn = 0;

	*pStatus = ARC_STATUS_OK;

	try
	{
		VERIFY_CLASS_PTR( g_pCDevice )

		uiIsSyn = ( g_pCDevice.get()->isSyntheticImageMode() ? 1 : 0 );
	}
	catch ( const std::exception& e )
	{
		*pStatus = ARC_STATUS_ERROR;

		ArcSprintf( g_szErrMsg, ARC_ERROR_MSG_SIZE, "%s", e.what() );
	}

	return uiIsSyn;
}


// +----------------------------------------------------------------------------
// |  setSyntheticImageMode
// +----------------------------------------------------------------------------
// |  If mode is 'true', then synthetic readout will be turned on. Set mode
// |  to 'false' to turn it off. A synthetic image looks like: 0, 1, 2, ...,
// |  65535, 0, 1, 2, ..., 65353, etc
// |
// |  <IN>  -> uiMode   - 'true' to turn it on, 'false' to turn if off.
// |  <OUT> -> pStatus - Status equals ARC_STATUS_OK or ARC_STATUS_ERROR
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API void ArcDevice_SetSyntheticImageMode( unsigned int uiMode, int* pStatus )
{
	*pStatus = ARC_STATUS_OK;

	try
	{
		VERIFY_CLASS_PTR( g_pCDevice )

		g_pCDevice.get()->setSyntheticImageMode( ( uiMode == 1 ? true : false ) );
	}
	catch ( const std::exception& e )
	{
		*pStatus = ARC_STATUS_ERROR;

		ArcSprintf( g_szErrMsg, ARC_ERROR_MSG_SIZE, "%s", e.what() );
	}
}


// +----------------------------------------------------------------------------
// |  setOpenShutter
// +----------------------------------------------------------------------------
// |  Sets whether or not the shutter will open when an exposure is started
// |  ( using SEX ).
// |
// |  <IN>  -> shouldOpen - Set to 'true' if the shutter should open during
// |                        expose. Set to 'false' to keep the shutter closed.
// |  <OUT> -> pStatus    - Status equals ARC_STATUS_OK or ARC_STATUS_ERROR
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API void ArcDevice_SetOpenShutter( unsigned int uiShouldOpen, int* pStatus )
{
	*pStatus = ARC_STATUS_OK;

	try
	{
		VERIFY_CLASS_PTR( g_pCDevice )

		g_pCDevice.get()->setOpenShutter( ( uiShouldOpen == 1 ? true : false ) );
	}
	catch ( const std::exception& e )
	{
		*pStatus = ARC_STATUS_ERROR;

		ArcSprintf( g_szErrMsg, ARC_ERROR_MSG_SIZE, "%s", e.what() );
	}
}


// +----------------------------------------------------------------------------
// |  expose
// +----------------------------------------------------------------------------
// |  Starts an exposure using the specified exposure time and whether or not
// |  to open the shutter. Callbacks for the elapsed exposure time and image
// |  data readout can be used.
// |
// |  <IN> -> fExpTime - The exposure time ( in seconds ).
// |  <IN> -> uiOpenShutter - Set to 'true' if the shutter should open during the
// |                         exposure. Set to 'false' to keep the shutter closed.
// |  <IN> -> uiRows - The image row size ( in pixels ).
// |  <IN> -> uiCols - The image column size ( in pixels ).
// |  <IN> -> fExpTime - The exposure time ( in seconds ).
// |  <IN> -> pExpIFace - Function pointer to CExpIFace class. NULL by default.
// |  <OUT> -> pStatus  - Status equals ARC_STATUS_OK or ARC_STATUS_ERROR
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API
void ArcDevice_Expose( float fExpTime, int uiRows, int uiCols, void ( *pExposeCall )( float ), void ( *pReadCall )( unsigned int ), int uiOpenShutter, int* pStatus )
{
	*pStatus = ARC_STATUS_OK;

	try
	{
		VERIFY_CLASS_PTR( g_pCDevice )

		IFExpose ExpIFace( pExposeCall, pReadCall );

		g_pCDevice.get()->expose( fExpTime,
								  uiRows,
								  uiCols,
								  g_bAbort,
								  &ExpIFace,
								  ( uiOpenShutter == 1 ? true : false ) );
	}
	catch ( const std::exception& e )
	{
		*pStatus = ARC_STATUS_ERROR;

		ArcSprintf( g_szErrMsg, ARC_ERROR_MSG_SIZE, "%s", e.what() );
	}
}


// +----------------------------------------------------------------------------
// |  stopExposure
// +----------------------------------------------------------------------------
// |  Stops the current exposure.
// |
// |  NOTE: The command is sent manually and NOT using the command() method
// |        because the command() method checks for readout and fails.
// |
// |  <OUT> -> pStatus  - Status equals ARC_STATUS_OK or ARC_STATUS_ERROR
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API void ArcDevice_StopExposure( int* pStatus )
{
	*pStatus = ARC_STATUS_OK;

	g_bAbort = true;

	try
	{
		VERIFY_CLASS_PTR( g_pCDevice )

		g_pCDevice.get()->stopExposure();
	}
	catch ( const std::exception& e )
	{
		*pStatus = ARC_STATUS_ERROR;

		ArcSprintf( g_szErrMsg, ARC_ERROR_MSG_SIZE, "%s", e.what() );
	}
}


// +----------------------------------------------------------------------------
// |  continuous
// +----------------------------------------------------------------------------
// |  This method can be called to start continuous readout.  A callback for 
// |  each frame read out can be used to process the frame.
// |
// |  <IN> -> uiRows - The image row size ( in pixels ).
// |  <IN> -> uiCols - The image column size ( in pixels ).
// |  <IN> -> uiNumOfFrames - The number of frames to take.
// |  <IN> -> fExpTime - The exposure time ( in seconds ).
// |  <IN> -> pFrameCall - Function pointer to callback for frame completion.
// |                       NULL by default.
// |  <IN> -> uiOpenShutter - 'true' to open the shutter during expose; 'false'
// |                         otherwise.
// |  <OUT> -> pStatus - Status equals ARC_STATUS_OK or ARC_STATUS_ERROR
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API
void ArcDevice_Continuous( unsigned int uiRows, unsigned int uiCols, unsigned int uiNumOfFrames, float fExpTime, 
						   void ( *pFrameCall )( unsigned int, unsigned int, unsigned int, unsigned int, void * ),
						   unsigned int uiOpenShutter, int* pStatus )
{
	*pStatus = ARC_STATUS_OK;

	g_bAbort = false;

	try
	{
		VERIFY_CLASS_PTR( g_pCDevice )

		IFConExp ConIFace( pFrameCall );

		g_pCDevice.get()->continuous( uiRows,
									  uiCols,
									  uiNumOfFrames,
									  fExpTime,
									  g_bAbort,
									  &ConIFace,
									  ( uiOpenShutter == 1 ? true : false ) );
	}
	catch ( const std::exception& e )
	{
		*pStatus = ARC_STATUS_ERROR;

		ArcSprintf( g_szErrMsg, ARC_ERROR_MSG_SIZE, "%s", e.what() );
	}
}


// +----------------------------------------------------------------------------
// |  stopContinuous
// +----------------------------------------------------------------------------
// |  Sends abort expose/readout and sets the controller back into single
// |  read mode.
// |
// |  <OUT> -> pStatus - Status equals ARC_STATUS_OK or ARC_STATUS_ERROR
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API void ArcDevice_StopContinuous( int* pStatus )
{
	*pStatus = ARC_STATUS_OK;

	g_bAbort = true;

	try
	{
		VERIFY_CLASS_PTR( g_pCDevice )

		g_pCDevice.get()->stopContinuous();
	}
	catch ( const std::exception& e )
	{
		*pStatus = ARC_STATUS_ERROR;

		ArcSprintf( g_szErrMsg, ARC_ERROR_MSG_SIZE, "%s", e.what() );
	}
}


// +----------------------------------------------------------------------------
// |  isReadout
// +----------------------------------------------------------------------------
// |  Returns 'true' if the controller is in readout.
// |
// |  <OUT> -> pStatus - Status equals ARC_STATUS_OK or ARC_STATUS_ERROR
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API unsigned int ArcDevice_IsReadout( int* pStatus )
{
	std::uint32_t uiIsROUT = 0;

	*pStatus = ARC_STATUS_OK;

	try
	{
		VERIFY_CLASS_PTR( g_pCDevice )

		uiIsROUT = ( g_pCDevice.get()->isReadout() ? 1 : 0 );
	}
	catch ( const std::exception& e )
	{
		*pStatus = ARC_STATUS_ERROR;

		ArcSprintf( g_szErrMsg, ARC_ERROR_MSG_SIZE, "%s", e.what() );
	}

	return uiIsROUT;
}


// +----------------------------------------------------------------------------
// |  getPixelCount
// +----------------------------------------------------------------------------
// |  Returns the current pixel count.
// |
// |  <OUT> -> pStatus - Status equals ARC_STATUS_OK or ARC_STATUS_ERROR
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API unsigned int ArcDevice_GetPixelCount( int* pStatus )
{
	std::uint32_t uiPixCnt = 0;

	*pStatus = ARC_STATUS_OK;

	try
	{
		VERIFY_CLASS_PTR( g_pCDevice )

		uiPixCnt = g_pCDevice.get()->getPixelCount();
	}
	catch ( const std::exception& e )
	{
		*pStatus = ARC_STATUS_ERROR;

		ArcSprintf( g_szErrMsg, ARC_ERROR_MSG_SIZE, "%s", e.what() );
	}

	return uiPixCnt;
}


// +----------------------------------------------------------------------------
// |  getCRPixelCount
// +----------------------------------------------------------------------------
// |  Returns the cumulative pixel count as when doing continuous readout.
// |  This method is used by user applications when reading super dArge images
// | ( greater than 4K x 4K ). This value increases across all frames being
// |  readout. This code needs to execute fast, so not pre-checking, such as 
// |  isControllerConnected() is done.
// |
// |  <OUT> -> pStatus - Status equals ARC_STATUS_OK or ARC_STATUS_ERROR
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API unsigned int ArcDevice_GetCRPixelCount( int* pStatus )
{
	std::uint32_t uiPixCnt = 0;

	*pStatus = ARC_STATUS_OK;

	try
	{
		VERIFY_CLASS_PTR( g_pCDevice )

		uiPixCnt = g_pCDevice.get()->getCRPixelCount();
	}
	catch ( const std::exception& e )
	{
		*pStatus = ARC_STATUS_ERROR;

		ArcSprintf( g_szErrMsg, ARC_ERROR_MSG_SIZE, "%s", e.what() );
	}

	return uiPixCnt;
}


// +----------------------------------------------------------------------------
// |  getFrameCount
// +----------------------------------------------------------------------------
// |  Returns the current frame count. The camera MUST be set for continuous
// |  readout for this to work. This code needs to execute fast, so no 
// |  pre-checking, such as isControllerConnected() is done.
// |
// |  <OUT> -> pStatus - Status equals ARC_STATUS_OK or ARC_STATUS_ERROR
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API unsigned int ArcDevice_GetFrameCount( int* pStatus )
{
	std::uint32_t uiFrameCnt = 0;

	*pStatus = ARC_STATUS_OK;

	try
	{
		VERIFY_CLASS_PTR( g_pCDevice )

		uiFrameCnt = g_pCDevice.get()->getFrameCount();
	}
	catch ( const std::exception& e )
	{
		*pStatus = ARC_STATUS_ERROR;

		ArcSprintf( g_szErrMsg, ARC_ERROR_MSG_SIZE, "%s", e.what() );
	}

	return uiFrameCnt;
}


// +----------------------------------------------------------------------------
// |  Check the specified value for error replies:
// |  TOUT, ROUT, HERR, ERR, SYR, RST
// +----------------------------------------------------------------------------
// |  Returns 'true' if the specified value matches 'TOUT, 'ROUT', 'HERR',
// |  'ERR', 'SYR' or 'RST'. Returns 'false' otherwise.
// |
// |  <IN>  -> uiWord   - The value to check
// |  <OUT> -> pStatus - Status equals ARC_STATUS_OK or ARC_STATUS_ERROR
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API unsigned int ArcDevice_ContainsError( unsigned int uiWord, int* pStatus )
{
	std::uint32_t uiError = 0;

	*pStatus = ARC_STATUS_OK;

	try
	{
		VERIFY_CLASS_PTR( g_pCDevice )

		uiError = ( g_pCDevice.get()->containsError( uiWord ) ? 1 : 0 );
	}
	catch ( const std::exception& e )
	{
		*pStatus = ARC_STATUS_ERROR;

		ArcSprintf( g_szErrMsg, ARC_ERROR_MSG_SIZE, "%s", e.what() );
	}

	return uiError;
}


// +----------------------------------------------------------------------------
// |  Check that the specified value falls within the specified range.
// +----------------------------------------------------------------------------
// |  Returns 'true' if the specified value falls within the specified range.
// |  Returns 'false' otherwise.
// |
// |  <IN>  -> uiWord    - The value to check
// |  <IN>  -> uiWordMin - The minimum range value ( not inclusive )
// |  <IN>  -> uiWordMax - The maximum range value ( not inclusive )
// |  <OUT> -> pStatus  - Status equals ARC_STATUS_OK or ARC_STATUS_ERROR
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API unsigned int ArcDevice_ContainsError_I( unsigned int uiWord, unsigned int uiWordMin, unsigned int uiWordMax, int* pStatus )
{
	std::uint32_t uiError = 0;

	*pStatus = ARC_STATUS_OK;

	try
	{
		VERIFY_CLASS_PTR( g_pCDevice )

		uiError = ( g_pCDevice.get()->containsError( uiWord, uiWordMin, uiWordMax ) ? 1 : 0 );
	}
	catch ( const std::exception& e )
	{
		*pStatus = ARC_STATUS_ERROR;

		ArcSprintf( g_szErrMsg, ARC_ERROR_MSG_SIZE, "%s", e.what() );
	}

	return uiError;
}


// +----------------------------------------------------------------------------
// |  getNextLoggedCmd
// +----------------------------------------------------------------------------
// |  Pops the first message from the command logger and returns it. If the
// |  logger is empty, then NULL is returned.
// |
// |  <OUT> -> pStatus - Status equals ARC_STATUS_OK or ARC_STATUS_ERROR
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API const char* ArcDevice_GetNextLoggedCmd( int* pStatus )
{
	*pStatus = ARC_STATUS_OK;

	try
	{
		VERIFY_CLASS_PTR( g_pCDevice )

		ArcSprintf( g_szTmpBuf,
					ARC_MSG_SIZE,
					"%s",
					g_pCDevice.get()->getNextLoggedCmd().c_str() );
	}
	catch ( const std::exception& e )
	{
		*pStatus = ARC_STATUS_ERROR;

		ArcSprintf( g_szErrMsg, ARC_ERROR_MSG_SIZE, "%s", e.what() );
	}

	return g_szTmpBuf;
}


// +----------------------------------------------------------------------------
// |  getLoggedCmdCount
// +----------------------------------------------------------------------------
// |  Returns the available command message count.
// |
// |  <OUT> -> pStatus - Status equals ARC_STATUS_OK or ARC_STATUS_ERROR
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API unsigned int ArcDevice_GetLoggedCmdCount( int* pStatus )
{
	std::uint32_t uiCount = 0;

	*pStatus = ARC_STATUS_OK;

	try
	{
		VERIFY_CLASS_PTR( g_pCDevice )

		uiCount = g_pCDevice.get()->getLoggedCmdCount();
	}
	catch ( const std::exception& e )
	{
		*pStatus = ARC_STATUS_ERROR;

		ArcSprintf( g_szErrMsg, ARC_ERROR_MSG_SIZE, "%s", e.what() );
	}

	return uiCount;
}


// +----------------------------------------------------------------------------
// |  setLogCmds
// +----------------------------------------------------------------------------
// |  Turns command logging on/off. This logging can be used for debugging to
// |  see command details in the following form:
// |
// |  <header> <cmd> <arg1> ... <arg4> -> <controller reply>
// |  Example: 0x203 TDL 0x112233 -> 0x444E4F
// |
// |  <IN>  -> uiOnOff  - 'true' to turn loggin on; 'false' otherwise.
// |  <OUT> -> pStatus - Status equals ARC_STATUS_OK or ARC_STATUS_ERROR
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API void ArcDevice_SetLogCmds( int uiOnOff, int* pStatus )
{
	*pStatus = ARC_STATUS_OK;

	try
	{
		VERIFY_CLASS_PTR( g_pCDevice )

		g_pCDevice.get()->setLogCmds( ( uiOnOff == 1 ? true : false ) );
	}
	catch ( const std::exception& e )
	{
		*pStatus = ARC_STATUS_ERROR;

		ArcSprintf( g_szErrMsg, ARC_ERROR_MSG_SIZE, "%s", e.what() );
	}
}


// +----------------------------------------------------------------------------
// |  getArrayTemperature
// +----------------------------------------------------------------------------
// |  Returns the array temperature.
// |
// |  Returns the average temperature value ( in Celcius ). The temperature is
// |  read m_gTmpCtrl_SDNumberOfReads times and averaged. Also, for a read to be
// |  included in the average the difference between the target temperature and
// |  the actual temperature must be less than the tolerance specified by
// |  gTmpCtrl_SDTolerance.
// |
// |  <OUT> -> pStatus - Status equals ARC_STATUS_OK or ARC_STATUS_ERROR
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API double ArcDevice_GetArrayTemperature( int* pStatus )
{
	double gTemp = 0.0;

	*pStatus = ARC_STATUS_OK;

	try
	{
		VERIFY_CLASS_PTR( g_pCDevice )

		gTemp = g_pCDevice.get()->getArrayTemperature();
	}
	catch ( const std::exception& e )
	{
		*pStatus = ARC_STATUS_ERROR;

		ArcSprintf( g_szErrMsg, ARC_ERROR_MSG_SIZE, "%s", e.what() );
	}

	return gTemp;
}


// +----------------------------------------------------------------------------
// |  getArrayTemperatureDN
// +----------------------------------------------------------------------------
// |  Returns the digital number associated with the current array temperature
// |  value.
// |
// |  <OUT> -> pStatus - Status equals ARC_STATUS_OK or ARC_STATUS_ERROR
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API double ArcDevice_GetArrayTemperatureDN( int* pStatus )
{
	double gTempDN = 0.0;

	*pStatus = ARC_STATUS_OK;

	try
	{
		VERIFY_CLASS_PTR( g_pCDevice )

		gTempDN = g_pCDevice.get()->getArrayTemperatureDN();
	}
	catch ( const std::exception& e )
	{
		*pStatus = ARC_STATUS_ERROR;

		ArcSprintf( g_szErrMsg, ARC_ERROR_MSG_SIZE, "%s", e.what() );
	}

	return gTempDN;
}


// +----------------------------------------------------------------------------
// |  setArrayTemperature
// +----------------------------------------------------------------------------
// |  Sets the array temperature to regulate around.
// |
// |  <IN>  -> gTempVal - The temperature value ( in Celcius ) to regulate
// |                      around.
// |  <OUT> -> pStatus  - Status equals ARC_STATUS_OK or ARC_STATUS_ERROR
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API void ArcDevice_SetArrayTemperature( double gTempVal, int* pStatus )
{
	*pStatus = ARC_STATUS_OK;

	try
	{
		VERIFY_CLASS_PTR( g_pCDevice )

		g_pCDevice.get()->setArrayTemperature( gTempVal );
	}
	catch ( const std::exception& e )
	{
		*pStatus = ARC_STATUS_ERROR;

		ArcSprintf( g_szErrMsg, ARC_ERROR_MSG_SIZE, "%s", e.what() );
	}
}


// +----------------------------------------------------------------------------
// |  loadTemperatureCtrlData
// +----------------------------------------------------------------------------
// |  Loads temperature control constants from the specified file.  The default
// |  constants are stored in TempCtrl.h and cannot be permanently overwritten.
// |
// |  <IN> -> pszFilename  - The filename of the temperature control constants
// |                         file.
// |  <OUT> -> pStatus     - Status equals ARC_STATUS_OK or ARC_STATUS_ERROR
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API
void ArcDevice_LoadTemperatureCtrlData( const char* pszFilename, int* pStatus )
{
	*pStatus = ARC_STATUS_OK;

	try
	{
		VERIFY_CLASS_PTR( g_pCDevice )

		g_pCDevice.get()->loadTemperatureCtrlData( pszFilename );
	}
	catch ( const std::exception& e )
	{
		*pStatus = ARC_STATUS_ERROR;

		ArcSprintf( g_szErrMsg, ARC_ERROR_MSG_SIZE, "%s", e.what() );
	}
}


// +----------------------------------------------------------------------------
// |  saveTemperatureCtrlData
// +----------------------------------------------------------------------------
// |  Saves the current temperature control constants to the specified file.
// |  These may be different from the values in TempCtrl.h if another
// |  temperature control file has been previously loaded.
// |
// |  Throws std::runtime_error on error
// |
// |  <IN> -> pszFilename  - The filename of the temperature control constants
// |                         file to save.
// |  <OUT> -> pStatus     - Status equals ARC_STATUS_OK or ARC_STATUS_ERROR
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API
void ArcDevice_SaveTemperatureCtrlData( const char* pszFilename, int* pStatus )
{
	*pStatus = ARC_STATUS_OK;

	try
	{
		VERIFY_CLASS_PTR( g_pCDevice )

		g_pCDevice.get()->saveTemperatureCtrlData( pszFilename );
	}
	catch ( const std::exception& e )
	{
		*pStatus = ARC_STATUS_ERROR;

		ArcSprintf( g_szErrMsg, ARC_ERROR_MSG_SIZE, "%s", e.what() );
	}
}


// +----------------------------------------------------------------------------
// | GetLastError
// +----------------------------------------------------------------------------
// | Returns the last error message reported.
// +----------------------------------------------------------------------------
GEN3_CARCDEVICE_API const char* ArcDevice_GetLastError()
{
	return const_cast<const char *>( g_szErrMsg );
}
