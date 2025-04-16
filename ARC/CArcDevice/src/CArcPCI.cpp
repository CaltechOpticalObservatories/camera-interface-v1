#ifdef _WINDOWS
	#define INITGUID
#endif


#ifdef _WINDOWS
	#include <windows.h>
	#include <setupapi.h>
	#include <devguid.h>
	#include <regstr.h>
	#include <astropciGUID.h>

#elif defined( linux ) || defined( __linux ) || defined( __APPLE__ )
	#include <sys/types.h>
	#include <sys/mman.h>
	#include <sys/ioctl.h>
	#include <fcntl.h>
	#include <unistd.h>
	#include <errno.h>
	#include <dirent.h>
	#include <cstring>

#else						// Unix Systems Only
	#include <sys/types.h>
	#include <sys/mman.h>
	#include <fcntl.h>
	#include <unistd.h>
	#include <stropts.h>
	#include <errno.h>
	#include <dirent.h>
#endif

#include <iostream>

#include <sstream>
#include <fstream>
#include <cmath>
#include <ios>

#include <CArcBase.h>
#include <ArcOSDefs.h>
#include <CArcDevice.h>
#include <CArcPCI.h>
#include <ArcDefs.h>
#include <PCIRegs.h>


namespace arc
{
	namespace gen3
	{

		#if defined( linux ) || defined( __linux )

			#define DEVICE_DIR			"/dev/"
			#define DEVICE_NAME			"AstroPCI"
			#define DEVICE_NAME_ALT		"Arc64PCI"

		#elif defined( __APPLE__ )

			// +----------------------------------------------------------------------------
			// | Define driver names
			// +----------------------------------------------------------------------------
			#define AstroPCIClassName		 com_arc_driver_Arc64PCI
			#define kAstroPCIClassName		"com_arc_driver_Arc64PCI"

		// +----------------------------------------------------------------------------
		// |  Macro to validate gen3 and vendor id's
		// +----------------------------------------------------------------------------
		#define VALID_DEV_VEN_ID( devVenId )	\
					( ( devVenId == 0x1057 || devVenId == 0x1801 ) ? 1 : 0 )

		#endif


		// +----------------------------------------------------------------------------
		// |  PCI File Download Constants
		// +----------------------------------------------------------------------------
		#define HTF_MASK					0x200
		#define HTF_CLEAR_MASK				0xFFFFFCFF	// Bits 8 and 9
		#define MAX_PCI_COMM_TEST			3
		#define PCI_COM_TEST_VALUE			0xABC123


		//#include <fstream>
		//std::ofstream dbgStream( "CArcPCI_Debug.txt" );


		// +----------------------------------------------------------------------------
		// |  Initialize Static Class Members
		// +----------------------------------------------------------------------------
		std::unique_ptr<std::vector<arc::gen3::device::ArcDev_t>> arc::gen3::CArcPCI::m_vDevList;

		std::shared_ptr<std::string[]> CArcPCI::m_psDevList;


		// +---------------------------------------------------------------------------+
		// | ArrayDeleter                                                              |
		// +---------------------------------------------------------------------------+
		// | Called by std::shared_ptr to delete the temporary image buffer.      |
		// | This method should NEVER be called directly by the user.                  |
		// +---------------------------------------------------------------------------+
		template<typename T> void CArcPCI::ArrayDeleter( T* p )
		{
			if ( p != NULL )
			{
				delete [] p;
			}
		}


		// +----------------------------------------------------------------------------
		// |  Constructor
		// +----------------------------------------------------------------------------
		// |  See CArcPCI.h for the class definition
		// +----------------------------------------------------------------------------
		CArcPCI::CArcPCI( void )
		{
			m_hDevice = INVALID_HANDLE_VALUE;
		}


		// +----------------------------------------------------------------------------
		// |  Destructor
		// +----------------------------------------------------------------------------
		CArcPCI::~CArcPCI( void )
		{
			close();
		}


		// +----------------------------------------------------------------------------
		// |  toString
		// +----------------------------------------------------------------------------
		// |  Returns a std::string that represents the gen3 controlled by this library.
		// +----------------------------------------------------------------------------
		const std::string CArcPCI::toString( void )
		{
			return std::string( "PCI [ ARC-63 / 64 ]" );
		}


		// +----------------------------------------------------------------------------
		// |  findDevices
		// +----------------------------------------------------------------------------
		// |  Searches for available ARC, Inc PCI devices and stores the list, which
		// |  can be accessed via gen3 number ( 0,1,2... ).
		// |
		// |  Throws std::runtime_error on error
		// +----------------------------------------------------------------------------
		void CArcPCI::findDevices( void )
		{
			if ( m_vDevList == nullptr )
			{
				m_vDevList.reset( new std::vector<arc::gen3::device::ArcDev_t>() );
			}

			if ( m_vDevList == nullptr )
			{
				THROW( "(CArcPCI::findDevices) Failed to allocate resources for PCI device list!" );
			}

			#ifdef __APPLE__
	
				if ( !m_vDevList->empty() )
				{
					//
					// Don't generate a new list on MAC, the stored service
					// object is currently in use by the open gen3.
					//
					return;
				}
	
			#else
	
				//
				// Dump any existing bindings
				//
				if ( !m_vDevList->empty() )
				{
					m_vDevList->clear();
		
					if ( m_vDevList->size() > 0 )
					{
						THROW( "(CArcPCI::findDevices) Failed to free existing device list!" );
					}
				}
	
			#endif


			#ifdef _WINDOWS

				PSP_DEVICE_INTERFACE_DETAIL_DATA DeviceInterfaceDetailData = NULL;
				SP_DEVICE_INTERFACE_DATA DeviceInterfaceData;
				HDEVINFO HardwareDeviceInfo;

				BOOL  bResult		   = FALSE;
				DWORD dwRequiredLength = 0;
				DWORD dwMemberIndex	   = 0;

				HardwareDeviceInfo = SetupDiGetClassDevs( ( LPGUID )&GUID_DEVINTERFACE_ARC_PCI,
														   NULL,
														   NULL,
														  ( DIGCF_PRESENT | DIGCF_DEVICEINTERFACE ) );

				if ( HardwareDeviceInfo == INVALID_HANDLE_VALUE )
				{
					THROW( "(CArcPCI::findDevices) SetupDiGetClassDevs failed!" );
				}

				DeviceInterfaceData.cbSize = sizeof( SP_DEVICE_INTERFACE_DATA );

				while ( 1 )
				{
					bResult = SetupDiEnumDeviceInterfaces( HardwareDeviceInfo,
														   0,
														   ( LPGUID )&GUID_DEVINTERFACE_ARC_PCI,
														   dwMemberIndex,
														   &DeviceInterfaceData );

					if ( bResult == FALSE )
					{
						SetupDiDestroyDeviceInfoList( HardwareDeviceInfo );
						break;
					}

					SetupDiGetDeviceInterfaceDetail( HardwareDeviceInfo,
													 &DeviceInterfaceData,
													 NULL,
													 0,
													 &dwRequiredLength,
													 NULL );

					DeviceInterfaceDetailData = ( PSP_DEVICE_INTERFACE_DETAIL_DATA )
													LocalAlloc( LMEM_FIXED, dwRequiredLength );

					if ( DeviceInterfaceDetailData == NULL )
					{
						SetupDiDestroyDeviceInfoList( HardwareDeviceInfo );
						break;
					}

					DeviceInterfaceDetailData->cbSize = sizeof( SP_DEVICE_INTERFACE_DETAIL_DATA );

					bResult = SetupDiGetDeviceInterfaceDetail( HardwareDeviceInfo,
															   &DeviceInterfaceData,
															   DeviceInterfaceDetailData,
															   dwRequiredLength,
															   &dwRequiredLength,
															   NULL );

					if ( bResult == FALSE )
					{
						SetupDiDestroyDeviceInfoList( HardwareDeviceInfo );
						LocalFree( DeviceInterfaceDetailData );
						break;
					}

					arc::gen3::device::ArcDev_t tArcDev;

					tArcDev.sName = CArcBase::convertWideToAnsi( ( LPWSTR )DeviceInterfaceDetailData->DevicePath );

					m_vDevList->push_back( tArcDev );

					dwMemberIndex++;
				}

			#elif defined( __APPLE__ )
	
				io_service_t  service;
				io_iterator_t iterator;
	
				//
				// Look up the objects we wish to open. This example uses simple class
				// matching (IOServiceMatching()) to find instances of the class defined by the kext.
				//
				// Because Mac OS X has no weak-linking support in the kernel, the only way to
				// support mutually-exclusive KPIs is to provide separate kexts. This in turn means that the
				// separate kexts must have their own unique CFBundleIdentifiers and I/O Kit class names.
				//
				// This sample shows how to do this in the SimpleUserClient and SimpleUserClient_10.4 Xcode targets.
				//
				// From the userland perspective, a process must look for any of the class names it is prepared to talk to.
				//
				// This creates an io_iterator_t of all instances of our driver that exist in the I/O Registry.
				//
				kern_return_t kernResult = IOServiceGetMatchingServices( kIOMasterPortDefault,
																		IOServiceMatching( kAstroPCIClassName ),
																		&iterator );
	
				if ( kernResult != KERN_SUCCESS )
				{
					THROW( "(CArcPCI) IOServiceGetMatchingServices failed: 0x%X", kernResult );
				}
	
				while ( ( service = IOIteratorNext( iterator ) ) != IO_OBJECT_NULL )
				{
					arc::gen3::device::ArcDev_t tArcDev;
		
					tArcDev.sName			= kAstroPCIClassName;
					tArcDev.tService		= service;
		
					m_vDevList->push_back( tArcDev );
				}
	
				//
				// Release the io_iterator_t now that we're done with it.
				//
				IOObjectRelease( iterator );
	
			#else	// LINUX
	
				struct dirent *pDirEntry = NULL;
				DIR *pDir = NULL;

				pDir = opendir( DEVICE_DIR );

				if ( pDir == NULL )
				{
					THROW( "(CArcPCI) Failed to open dir: %s", DEVICE_DIR );
				}

				else
				{
					while ( ( pDirEntry = readdir( pDir ) ) != NULL )
					{
						std::string sDirEntry( pDirEntry->d_name );

						if ( ( sDirEntry.find( DEVICE_NAME ) != std::string::npos ||
							   sDirEntry.find( DEVICE_NAME_ALT ) != std::string::npos ) &&
							   sDirEntry.find( "PCIe" ) == std::string::npos )
						{
							arc::gen3::device::ArcDev_t tArcDev;
							tArcDev.sName = DEVICE_DIR + sDirEntry;
							m_vDevList->push_back( tArcDev );
						}
					}

					closedir( pDir );
				}

			#endif

			//
			// Make sure the bindings exist
			//
			if ( m_vDevList->empty() )
			{
				THROW( "(CArcPCI) No device bindings exist! Make sure an ARC, Inc PCI card is installed!" );
			}
		}


		// +----------------------------------------------------------------------------
		// |  deviceCount
		// +----------------------------------------------------------------------------
		// |  Returns the number of items in the gen3 list. Must be called after
		// |  findDevices().
		// +----------------------------------------------------------------------------
		std::uint32_t CArcPCI::deviceCount( void )
		{
			return static_cast<int>( m_vDevList->size() );
		}


		// +----------------------------------------------------------------------------
		// |  getDeviceStringList
		// +----------------------------------------------------------------------------
		// |  Returns a std::string list representation of the gen3 list. Must be called
		// |  after findDevices().
		// |
		// |  Throws std::runtime_error on error
		// +----------------------------------------------------------------------------
		const std::string* CArcPCI::getDeviceStringList( void )
		{
			if ( !m_vDevList->empty() )
			{
				m_psDevList.reset( new std::string[ m_vDevList->size() ], &CArcPCI::ArrayDeleter<std::string> );

				if ( m_psDevList == nullptr )
				{
					THROW( "(CArcPCI) Failed to allocate storage for PCI device std::string list" );
				}

				for ( decltype( m_vDevList->size() ) i = 0; i < m_vDevList->size(); i++ )
				{
					std::ostringstream oss;

		#ifdef __APPLE__
					oss << "PCI Device " << i << m_vDevList->at( i ).sName << ends;
		#else
					oss << "PCI Device " << i << std::ends;
		#endif
					m_psDevList[ i ] = oss.str();
				}
			}

			else
			{
				m_psDevList.reset( new std::string[ 1 ], &CArcPCI::ArrayDeleter<std::string> );

				if ( m_psDevList == nullptr )
				{
					THROW( "(CArcPCI) Failed to allocate storage for PCI device std::string list" );
				}

				m_psDevList[ 0 ] = std::string( "No Devices Found!" );
			}

			return const_cast<const std::string*>( m_psDevList.get() );
		}


		// +----------------------------------------------------------------------------
		// |  isOpen
		// +----------------------------------------------------------------------------
		// |  Returns 'true' if connected to PCI gen3; 'false' otherwise.
		// |
		// |  Throws NOTHING on error. No error handling.
		// +----------------------------------------------------------------------------
		bool CArcPCI::isOpen( void )
		{
			return CArcDevice::isOpen();
		}


		// +----------------------------------------------------------------------------
		// |  open
		// +----------------------------------------------------------------------------
		// |  Opens a connection to the gen3 driver associated with the specified
		// |  gen3.
		// |
		// |  Throws std::runtime_error on error
		// |
		// |  <IN>  -> uiDeviceNumber - Device number
		// +----------------------------------------------------------------------------
		void CArcPCI::open( std::uint32_t uiDeviceNumber )
		{
			if ( isOpen() )
			{
				THROW( "(CArcPCI::open) Device already open, call close() first!" );
			}

			//
			// Make sure the bindings exist
			//
			if ( m_vDevList->empty() )
			{
				THROW( "(CArcPCI::open) No device bindings exist!" );
			}

			// Verify gen3 number
			//
			if ( uiDeviceNumber < 0 || uiDeviceNumber > static_cast<std::uint32_t>( m_vDevList->size() ) )
			{
				THROW( "(CArcPCI::open) Invalid device number: %u", uiDeviceNumber );
			}

			std::string sDeviceName = m_vDevList->at( uiDeviceNumber ).sName;

		#ifdef __APPLE__
	
			io_service_t tService = m_vDevList->at( uiDeviceNumber ).tService;

			Arc_OpenHandle( m_hDevice, &tService );
	
		#else

			Arc_OpenHandle( m_hDevice, sDeviceName.c_str() );
	
		#endif
	
			if ( m_hDevice == INVALID_HANDLE_VALUE )
			{
				THROW( "(CArcPCI::open) Failed to open device ( %s ) : %e", sDeviceName.c_str(), arc::gen3::CArcBase::getSystemError() );
			}

			// EXTREMELY IMPORTANT
			//
			// Prevent a forking problem. Forking a new process results in the
			// gen3 file descriptor being copied to the child process, which
			// results in problems when later trying to close the gen3 from
			// the parent process.
			//
			// Example: The CArcDisplay::Launch() method will fork a new DS9.
			// The parent and child processes now hold copies of the gen3 file
			// descriptor. The OS marks the gen3 usage count as being two;
			// instead of one. When the parent then closes and tries to re-open
			// the same gen3 it will fail, because the usage count prevents
			// the gen3 from closing ( since it's greater than zero ). In fact,
			// calling close( hdev ) will not result in the driver close/release
			// function being called because the usage count is not zero.  The
			// fcntl() function sets the gen3 file descriptor to close on exec(),
			// which is used in conjunction with the fork() call, and causes the
			// child's copies of the gen3 descriptor to be closed before exec()
			// is called.
		#if defined( linux ) || defined( __linux )

			fcntl( m_hDevice, F_SETFD, FD_CLOEXEC );

		#endif
		}

		// +----------------------------------------------------------------------------
		// |  open
		// +----------------------------------------------------------------------------
		// |  This version first calls open, then mapCommonBuffer if open
		// |  succeeded. Basically, this function just combines the other two.
		// |
		// |  Throws std::runtime_error on error
		// |
		// |  <IN>  -> uiDeviceNumber - PCI gen3 number
		// |  <IN>  -> uiBytes - The size of the kernel image buffer in bytes
		// +----------------------------------------------------------------------------
		void CArcPCI::open( std::uint32_t uiDeviceNumber, std::uint32_t uiBytes )
		{
			open( uiDeviceNumber );

			mapCommonBuffer( uiBytes );
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
		// +----------------------------------------------------------------------------
		void CArcPCI::open( std::uint32_t uiDeviceNumber, std::uint32_t uiRows, std::uint32_t uiCols )
		{
			open( uiDeviceNumber );

			mapCommonBuffer( ( uiRows * uiCols * sizeof( std::uint16_t ) ) );
		}
		

		// +----------------------------------------------------------------------------
		// |  close
		// +----------------------------------------------------------------------------
		// |  Closes the currently open driver that was opened with a call to
		// |  open.
		// |
		// |  Throws NOTHING on error. No error handling.
		// +----------------------------------------------------------------------------
		void CArcPCI::close( void )
		{
			//
			// Prevents access violation from code that follows
			//
			bool bOldStoreCmds = m_bStoreCmds;
			m_bStoreCmds       = false;

			unMapCommonBuffer();

			Arc_CloseHandle( m_hDevice );

			m_hDevice    = INVALID_HANDLE_VALUE;
			m_bStoreCmds = bOldStoreCmds;
		}


		// +----------------------------------------------------------------------------
		// |  reset
		// +----------------------------------------------------------------------------
		// |  Resets the PCI board.
		// |
		// |  Throws NOTHING on error. No error handling.
		// +----------------------------------------------------------------------------
		void CArcPCI::reset( void )
		{
			std::uint32_t uiReply = PCICommand( PCI_RESET );

			if ( uiReply != DON )
			{
				THROW( "(CArcPCI::reset) PCI reset failed! Expected: 'DON' [ 0x444F4E ], Received: 0x%X", uiReply );
			}
		}


		// +----------------------------------------------------------------------------
		// |  mapCommonBuffer
		// +----------------------------------------------------------------------------
		// |  Map the gen3 driver image buffer.
		// |
		// |  Throws std::runtime_error on error
		// |
		// |  <IN>  -> uiBytes - The number of bytes to map as an image buffer. Not used by PCI.
		// +----------------------------------------------------------------------------
		void CArcPCI::mapCommonBuffer( std::uint32_t uiBytes )
		{
			if ( uiBytes <= 0 )
			{
				THROW( "(CArcPCI::mapCommonBuffer) Invalid buffer size: %u. Must be greater than zero!", uiBytes );
			}

			m_tImgBuffer.pUserAddr = ( std::uint16_t* )Arc_MMap( m_hDevice, ASTROPCI_MEM_MAP, size_t( uiBytes ) );

			if ( m_tImgBuffer.pUserAddr == MAP_FAILED )
			{
				arc::gen3::CArcBase::zeroMemory( &m_tImgBuffer, sizeof( arc::gen3::device::ImgBuf_t ) );

				THROW( "(CArcPCI::mapCommonBuffer) Failed to map image buffer : [ %d ] %e", arc::gen3::CArcBase::getSystemError(), arc::gen3::CArcBase::getSystemError() );
			}

			bool bSuccess = getCommonBufferProperties();

			if ( !bSuccess )
			{
				THROW( "(CArcPCI::mapCommonBuffer) Failed to read image buffer size : [ %d ] %e", arc::gen3::CArcBase::getSystemError(), arc::gen3::CArcBase::getSystemError() );
			}

			std::cout << "(arc::gen3::CArcPCI::mapCommonBuffer) PCI VA=" << std::hex << std::showbase << m_tImgBuffer.pUserAddr 
				  << " PA=" << std::hex << std::showbase << m_tImgBuffer.ulPhysicalAddr
				  << " size=" << std::dec << m_tImgBuffer.ulSize << std::endl;

			if ( m_tImgBuffer.ulSize < size_t( uiBytes ) )
			{
				std::ostringstream oss;

				oss << "(CArcPCI::mapCommonBuffer) Failed to allocate buffer of the correct size.\nWanted: "
					<< uiBytes << " bytes [ " << ( uiBytes / 1E6 ) << "MB ] - Received: "
					<< m_tImgBuffer.ulSize << " bytes [ " << ( m_tImgBuffer.ulSize / 1E6 )
					<< "MB ]";

				THROW( oss.str() );		
			}
		}


		// +----------------------------------------------------------------------------
		// |  unMapCommonBuffer
		// +----------------------------------------------------------------------------
		// |  Un-Maps the gen3 driver image buffer.
		// |
		// |  Throws NOTHING
		// +----------------------------------------------------------------------------
		void CArcPCI::unMapCommonBuffer( void )
		{
			if ( m_tImgBuffer.pUserAddr != ( void * )nullptr )
			{
				Arc_MUnMap( m_hDevice, ASTROPCI_MEM_UNMAP, m_tImgBuffer.pUserAddr, static_cast<std::int32_t>( m_tImgBuffer.ulSize ) );
			}
	
			arc::gen3::CArcBase::zeroMemory( &m_tImgBuffer, sizeof( arc::gen3::device::ImgBuf_t ) );
		}


		// +----------------------------------------------------------------------------
		// |  getId
		// +----------------------------------------------------------------------------
		// |  Returns 0, since the PCI board has no ID!
		// +----------------------------------------------------------------------------
		std::uint32_t CArcPCI::getId( void )
		{
			return 0;
		}


		// +----------------------------------------------------------------------------
		// |  getStatus
		// +----------------------------------------------------------------------------
		// |  Returns the current value of the PCI boards HTF ( Host Transfer Flags )
		// |  bits from the HSTR register.
		// |
		// |  Throws std::runtime_error on error
		// +----------------------------------------------------------------------------
		std::uint32_t CArcPCI::getStatus( void )
		{
			auto uiRetVal = getHSTR();

			uiRetVal = ( ( uiRetVal & HTF_BIT_MASK ) >> 3 );

			return uiRetVal;
		}


		// +----------------------------------------------------------------------------
		// |  clearStatus
		// +----------------------------------------------------------------------------
		// |  Clears the PCI status register.
		// +----------------------------------------------------------------------------
		void CArcPCI::clearStatus( void )
		{
			// Not Used ????
		}


		// +----------------------------------------------------------------------------
		// |  set2xFOTransmitter
		// +----------------------------------------------------------------------------
		// |  Sets the controller to use two fiber optic transmitters.
		// |
		// |  Throws std::runtime_error on error
		// |
		// |  <IN> -> bOnOff - True to enable dual transmitters; false otherwise.
		// +----------------------------------------------------------------------------
		void CArcPCI::set2xFOTransmitter( bool bOnOff )
		{
			std::uint32_t uiReply = 0;

			if ( bOnOff )
			{
				if ( ( uiReply = command( { TIM_ID, XMT, 1 } ) ) != DON )
				{
					THROW( "Failed to SET use of 2x fiber optic transmitters on controller, reply: 0x%X", uiReply );
				}
			}

			else
			{
				if ( ( uiReply = command( { TIM_ID, XMT, 0 } ) ) != DON )
				{
					THROW( "Failed to CLEAR use of 2x fiber optic transmitters on controller, reply: 0x%X", uiReply );
				}
			}
		}


		// +----------------------------------------------------------------------------
		// |  loadDeviceFile
		// +----------------------------------------------------------------------------
		// |  Loads a PCI '.lod' file into the PCI boards DSP for execution, which
		// |  begins immediately following upload.
		// |
		// |  Throws std::runtime_error on error
		// |
		// |  <IN> -> sFile - The PCI '.lod' file to load.
		// +----------------------------------------------------------------------------
		void CArcPCI::loadDeviceFile( const std::string& sFilename )
		{
			std::uint32_t uiHctrValue	= 0;
			std::uint32_t uiWordTotal	= 0;
			std::uint32_t uiWordCount	= 0;
			std::uint32_t uiStartAddr	= 0;
			std::uint32_t uiData		= 0;
			std::uint32_t uiFailedCount	= 0;
			bool bPCITransferModeSet	= false;

			std::string sLine;

			//
			// open the file for reading
			// -------------------------------------------------------------------
			std::ifstream inFile( sFilename.c_str() );

			if ( !inFile.is_open() )
			{
				THROW( "(CArcPCI::loadDeviceFile) Cannot open file: %s", sFilename.c_str() );
			}

			//
			// Check for valid PCI file
			// -------------------------------------------------------------------
			getline( inFile, sLine );

			if ( sLine.find( "PCI" ) == std::string::npos )
			{
				THROW( "(CArcPCI::loadDeviceFile) Invalid PCI file, no PCIBOOT std::string found." );
			}

			//
			// Verify gen3 connection
			// -------------------------------------------------------------------
			if ( !isOpen() )
			{
				THROW_NO_DEVICE_ERROR();
			}

			//
			// Set the PCI board HCTR bits. Bits 8 & 9 (HTF bits) are cleared to
			// allow 32-bit values to be written to the PCI board without loss
			// of bytes. The 32-bit values are broken up into two 16-bit values.
			// -------------------------------------------------------------------
			uiHctrValue = getHCTR();

			//
			// Clear the HTF bits (8 and 9) and bit3
			//
			uiHctrValue = uiHctrValue & HTF_CLEAR_MASK;

			//
			// Set the HTF bits
			//
			uiHctrValue = ( uiHctrValue | HTF_MASK );
			setHCTR( uiHctrValue );
			bPCITransferModeSet = true;

			//
			// Inform the DSP that new pci boot code will be downloaded
			//
			ioctlDevice( ASTROPCI_PCI_DOWNLOAD, 0 );

			//
			// Set the magic number that indicates a pci download
			//
			ioctlDevice( ASTROPCI_HCVR_DATA, 0x00555AAA );

			//
			// Write the data to the PCI
			//
			while ( !inFile.eof() )
			{
				getline( inFile, sLine );

				if ( sLine.find( "_DATA P" ) != std::string::npos )
				{
					getline( inFile, sLine );

					auto pTokens = arc::gen3::CArcBase::splitString( sLine );

					//
					// Set the number of words and start address
					//
					uiWordTotal = static_cast<std::uint32_t>( std::stol( pTokens->at( 0 ), 0, 16 ) );
					ioctlDevice( ASTROPCI_HCVR_DATA, uiWordTotal );

					uiStartAddr = static_cast< std::uint32_t >( std::stol( pTokens->at( 1 ), 0, 16 ) );
					ioctlDevice( ASTROPCI_HCVR_DATA, uiStartAddr );

					//
					// Throw away the next line (example: _DATA P 000002)
					//
					getline( inFile, sLine );

					//
					// Load the data
					//
					while ( uiWordCount < uiWordTotal )
					{
						//
						// Get the next line, this is the data start
						//
						getline( inFile, sLine );

						//
						// Check for intermixed "_DATA" strings
						//
						if ( sLine.find( "_DATA P" ) == std::string::npos )
						{
							auto pDataTokens = arc::gen3::CArcBase::splitString( sLine );

							for ( auto it = pDataTokens->begin(); it != pDataTokens->end(); it++ )
							{
								if ( uiWordCount >= uiWordTotal )
								{
									break;
								}

								uiData = static_cast<std::uint32_t>( std::stol( *it, 0, 16 ) );
								ioctlDevice( ASTROPCI_HCVR_DATA, uiData );
								uiWordCount++;
							}
						}	// End if _DATA P
					}	// End while wordCount < wordTotal
			
					break;

				}	// End if strstr != null
			}	// End while not eof

			//
			// Set the PCI data size transfer mode
			//
			if ( bPCITransferModeSet )
			{
				uiHctrValue = getHCTR();
				setHCTR( ( uiHctrValue & 0xCFF ) | 0x900 );
			}

			//
			// Wait for the PCI DSP to finish initialization
			//
			if ( bPCITransferModeSet )
			{
				auto uiRetVal = ioctlDevice( ASTROPCI_PCI_DOWNLOAD_WAIT, PCI_ID );

				//
				// Make sure a DON is received
				//
				if ( uiRetVal != DON )
				{
					THROW( "(CArcPCI::loadDeviceFile) PCI download failed. Reply: 0x%X", uiRetVal );
				}
			}

			//
			// Test PCI communications
			//
			std::uint32_t uiReply = 0;

			for ( auto i = 0; i < MAX_PCI_COMM_TEST; i++ )
			{
				uiData  = PCI_COM_TEST_VALUE * i;

				uiReply = command( { PCI_ID, TDL, uiData } );

				if ( uiReply != uiData )
				{
					uiFailedCount++;
				}
			}

			//
			// If ALL the communication tests failed, then report an error.
			//
			if ( uiFailedCount >= MAX_PCI_COMM_TEST )
			{
				THROW( "(CArcPCI::loadDeviceFile) PCI communications test failed." );
			}

			inFile.close();
		}


		// +----------------------------------------------------------------------------
		// |  command
		// +----------------------------------------------------------------------------
		// |  Send a command to the controller timing or utility board. Returns the
		// |  controller reply, typically DON.
		// |
		// |  Throws std::runtime_error on error
		// |
		// |  <IN>  -> uiBoardId       - command board id ( PCI, TIM or UTIL )
		// |  <IN>  -> dCommand       - Board command
		// |  <IN>  -> dArg0 to dArg3 - command arguments ( optional )
		// +----------------------------------------------------------------------------
		std::uint32_t CArcPCI::command( const std::initializer_list<std::uint32_t>& tCmdList )
		{
			if ( !isOpen() )
			{
				THROW( "(CArcPCI::command) Not connected to any device!" );
			}

			if ( tCmdList.size() > CTLR_CMD_MAX )
			{
				THROW( "(CArcPCI::command) Command list too large. Cannot exceed four arguments!" );
			}

			std::unique_ptr<std::uint32_t[]> pCmdData( new std::uint32_t[ CTLR_CMD_MAX ] );

			auto pInserter = pCmdData.get();

			for ( auto it = tCmdList.begin(); it != tCmdList.end(); it++ )
			{
				if ( it == tCmdList.begin() )
				{
					*pInserter = ( ( *it << 8 ) | static_cast<std::uint32_t>( tCmdList.size() ) );
				}

				else
				{
					*pInserter = *it;
				}

				pInserter++;
			}

			auto iSuccess = Arc_IOCtl( m_hDevice, ASTROPCI_COMMAND, pCmdData.get(), ( CTLR_CMD_MAX * sizeof( std::uint32_t ) ) );

			auto uiReply = pCmdData[ 0 ];

			if ( !iSuccess )
			{
				std::string sCmd = arc::gen3::CArcBase::iterToString( tCmdList.begin(), tCmdList.end() );
	
				if ( m_bStoreCmds )
				{
					m_pCLog->put( sCmd.c_str() );
				}

				sCmd.insert( 0, "(CArcPCI::command) " );
				THROW( sCmd );
			}

			// Set the debug message queue. Can't pass dCmdData[ 0 ] because
			// linux/unix systems overwrite this in the driver with the reply.
			if ( m_bStoreCmds )
			{
				std::string sCmd = arc::gen3::CArcBase::iterToString( tCmdList.begin(), tCmdList.end() );
	
				m_pCLog->put( sCmd.c_str() );
			}

			if ( uiReply == CNR )
			{
				THROW( "(CArcPCI::command) Controller not ready! Verify controller has been setup! Reply: 0x%X", uiReply );
			}

			return uiReply;
		}

		//std::uint32_t CArcPCI::command( int uiBoardId, int dCommand, int dArg0, int dArg1, int dArg2, int dArg3 )
		//{
		//	int dCmdData[ CTLR_CMD_MAX ]	= { -1 };
		//	int dNumberOfArgs				= 0;
		//	int uiHeader						= 0;
		//	int uiReply						= 0;

		//	if ( !isOpen() )
		//	{
		//		CArcTools::ThrowException( "CArcPCI",
		//								   "Command",
		//								   "Not connected to any device!" );
		//	}

		//	if      ( dArg0 == -1 ) dNumberOfArgs = 2;
		//	else if ( dArg1 == -1 ) dNumberOfArgs = 3;
		//	else if ( dArg2 == -1 ) dNumberOfArgs = 4;
		//	else if ( dArg3 == -1 ) dNumberOfArgs = 5;
		//	else dNumberOfArgs = 6;

		//	dHeader       = ( ( uiBoardId << 8 ) | dNumberOfArgs );
		//	dCmdData[ 0 ] = dHeader;
		//	dCmdData[ 1 ] = dCommand;
		//	dCmdData[ 2 ] = dArg0;
		//	dCmdData[ 3 ] = dArg1;
		//	dCmdData[ 4 ] = dArg2;
		//	dCmdData[ 5 ] = dArg3;

		//	int dSuccess = Arc_IOCtl( m_hDevice,
		//							  ASTROPCI_COMMAND,
		//							  dCmdData,
		//							  sizeof( dCmdData ) );

		//	uiReply = dCmdData[ 0 ];

		//	if ( !dSuccess )
		//	{
		//		std::string sCmd = CArcTools::CmdToString( uiReply,
		//											  uiBoardId,
		//											  dCommand,
		//											  dArg0,
		//											  dArg1,
		//											  dArg2,
		//											  dArg3,
		//											  Arc_ErrorCode() );
		//		if ( m_bStoreCmds )
		//		{
		//			m_pCLog.put( sCmd.c_str() );
		//		}

		//		CArcTools::ThrowException( "CArcPCI",
		//								   "Command",
		//									sCmd );
		//	}

		//	// Set the debug message queue. Can't pass dCmdData[ 0 ] because
		//	// linux/unix systems overwrite this in the driver with the reply.
		//	if ( m_bStoreCmds )
		//	{
		//		std::string sCmd = CArcTools::CmdToString( uiReply,
		//											  uiBoardId,
		//											  dCommand,
		//											  dArg0,
		//											  dArg1,
		//											  dArg2,
		//											  dArg3 );
		//		m_pCLog.put( sCmd.c_str() );
		//	}

		//	if ( uiReply == CNR )
		//	{
		//		CArcTools::ThrowException(
		//						"CArcPCI",
		//						"Command",
		//						"Controller not ready! Verify controller has been setup! Reply: 0x%X",
		//						 uiReply );
		//	}

		//	return uiReply;
		//}


		// +----------------------------------------------------------------------------
		// |  getControllerId
		// +----------------------------------------------------------------------------
		// |  Returns the controller ID or 'ERR' if none.
		// |
		// |  Throws std::runtime_error on error
		// +----------------------------------------------------------------------------
		std::uint32_t CArcPCI::getControllerId( void )
		{
			//
			//  Check the controller id  ... And YES,
			//  PCI_ID is correct for ARC-12!
			// +--------------------------------------+
			std::uint32_t uiId = command( { PCI_ID, SID } );

			if ( !IS_ARC12( uiId ) )
			{
				uiId = command( { TIM_ID, SID } );
			}

			return uiId;
		}


		// +----------------------------------------------------------------------------
		// |  resetController
		// +----------------------------------------------------------------------------
		// |  Resets the controller.
		// |
		// |  Throws std::runtime_error on error
		// +----------------------------------------------------------------------------
		void CArcPCI::resetController( void )
		{
			std::uint32_t uiRetVal = PCICommand( RESET_CONTROLLER );

			if ( uiRetVal != SYR )
			{
				THROW( "(CArcPCI::resetController) Reset controller failed. Reply: 0x%X", uiRetVal );
			}
		}


		// +----------------------------------------------------------------------------
		// | isControllerConnected
		// +----------------------------------------------------------------------------
		// |  Returns 'true' if a controller is connected to the PCI board. This is
		// |  tested by sending a TDL to the controller. If it succeeds, then the
		// |  controller is ready.
		// |
		// |  Throws std::runtime_error on error ( indirectly via command() )
		// +----------------------------------------------------------------------------
		bool CArcPCI::isControllerConnected( void )
		{
			bool bIsSetup			= false;
			std::uint32_t uiVal     = 0x112233;
			std::uint32_t uiRetVal  = 0;

			try
			{
				uiRetVal = command( { TIM_ID, TDL, uiVal } );

				if ( uiRetVal == uiVal )
				{
					bIsSetup = true;
				}
			}
			catch ( ... ) {}

			return bIsSetup;
		}


		// +----------------------------------------------------------------------------
		// |  stopExposure
		// +----------------------------------------------------------------------------
		// |  Stops the current exposure.
		// |
		// |  Throws std::runtime_error on error
		// +----------------------------------------------------------------------------
		void CArcPCI::stopExposure( void )
		{
			PCICommand( ABORT_READOUT );
		}


		// +----------------------------------------------------------------------------
		// |  isReadout
		// +----------------------------------------------------------------------------
		// |  Returns 'true' if the controller is in readout.
		// |
		// |  Throws std::runtime_error on error
		// +----------------------------------------------------------------------------
		bool CArcPCI::isReadout( void )
		{
			std::uint32_t uiStatus = ( ( getHSTR() & HTF_BIT_MASK ) >> 3 );

			return ( uiStatus == static_cast<std::uint32_t>( ePCIStatus::READOUT_STATUS ) ? true : false );
		}


		// +----------------------------------------------------------------------------
		// |  getPixelCount
		// +----------------------------------------------------------------------------
		// |  Returns the current pixel count.
		// |
		// |  Throws std::runtime_error on error
		// +----------------------------------------------------------------------------
		std::uint32_t CArcPCI::getPixelCount( void )
		{
			std::uint32_t uiRetVal = 0;

			//
			// Verify gen3 connection
			// -------------------------------------------------------------------
			if ( !isOpen() )
			{
				THROW_NO_DEVICE_ERROR();
			}

			else
			{
				uiRetVal = ioctlDevice( ASTROPCI_GET_HCTR );
				uiRetVal = ioctlDevice( ASTROPCI_GET_DMA_ADDR );
				uiRetVal = ioctlDevice( ASTROPCI_GET_HSTR );
				uiRetVal = ioctlDevice( ASTROPCI_GET_DMA_SIZE );
				uiRetVal = ioctlDevice( ASTROPCI_GET_FRAMES_READ );
				uiRetVal = ioctlDevice( ASTROPCI_GET_PROGRESS );
			}

			return uiRetVal;
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
		// |  Throws std::runtime_error on error
		// +----------------------------------------------------------------------------
		std::uint32_t CArcPCI::getCRPixelCount( void )
		{
			return ioctlDevice( ASTROPCI_GET_CR_PROGRESS );
		}


		// +----------------------------------------------------------------------------
		// |  getFrameCount
		// +----------------------------------------------------------------------------
		// |  Returns the current frame count. The camera MUST be set for continuous
		// |  readout for this to work. This code needs to execute fast, so no 
		// |  pre-checking, such as isControllerConnected() is done.
		// |
		// |  Throws std::runtime_error on error
		// +----------------------------------------------------------------------------
		std::uint32_t CArcPCI::getFrameCount( void )
		{
			return ioctlDevice( ASTROPCI_GET_FRAMES_READ );
		}


		// +----------------------------------------------------------------------------
		// |  setHCTR
		// +----------------------------------------------------------------------------
		// |  Sets the current value of the PCI boards HCTR register.
		// |
		// |  Throws std::runtime_error on error
		// |
		// |  <IN> -> uiVal - The value to set the PCI board HCTR register to.
		// +----------------------------------------------------------------------------
		void CArcPCI::setHCTR( std::uint32_t uiVal )
		{
			//
			// Verify gen3 connection
			// -------------------------------------------------------------------
			if ( !isOpen() )
			{
				THROW_NO_DEVICE_ERROR();
			}

			else
			{
				ioctlDevice( ASTROPCI_SET_HCTR, uiVal );
			}
		}


		// +----------------------------------------------------------------------------
		// |  getHSTR
		// +----------------------------------------------------------------------------
		// |  Returns the current value of the PCI boards HSTR register.
		// |
		// |  Throws std::runtime_error on error
		// +----------------------------------------------------------------------------
		std::uint32_t CArcPCI::getHSTR()
		{
			std::uint32_t uiRetVal = 0;

			//
			// Verify gen3 connection
			// -------------------------------------------------------------------
			if ( !isOpen() )
			{
				THROW_NO_DEVICE_ERROR();
			}

			else
			{
				uiRetVal = ioctlDevice( ASTROPCI_GET_HSTR );
			}

			return uiRetVal;
		}


		// +----------------------------------------------------------------------------
		// |  getHCTR
		// +----------------------------------------------------------------------------
		// |  Returns the current value of the PCI boards HCTR register.
		// |
		// |  Throws std::runtime_error on error
		// +----------------------------------------------------------------------------
		std::uint32_t CArcPCI::getHCTR( void )
		{
			std::uint32_t uiRetVal = 0;

			if ( !isOpen() )
			{
				THROW_NO_DEVICE_ERROR();
			}

			else
			{
				uiRetVal = ioctlDevice( ASTROPCI_GET_HCTR );
			}

			return uiRetVal;
		}


		// +----------------------------------------------------------------------------
		// |  PCICommand
		// +----------------------------------------------------------------------------
		// |  Send a command to the PCI board.
		// |
		// |  Throws std::runtime_error on error
		// |
		// |  <IN>  -> command - PCI command
		// +----------------------------------------------------------------------------
		std::uint32_t CArcPCI::PCICommand( std::uint32_t uiCommand )
		{
			if ( !isOpen() )
			{
				THROW_NO_DEVICE_ERROR();
			}

			std::uint32_t uiRetVal = uiCommand;

			auto iSuccess = Arc_IOCtl( m_hDevice, ASTROPCI_SET_HCVR, &uiRetVal, sizeof( uiRetVal ) );

			if ( !iSuccess )
			{
				if ( m_bStoreCmds )
				{
					std::string sCmd = formatPCICommand( uiCommand, uiRetVal );

					m_pCLog->put( sCmd.c_str() );
				}

				const std::string sErr = formatPCICommand( uiCommand, uiRetVal, true );

				THROW( sErr.c_str() );
			}

			// Set the debug message queue
			if ( m_bStoreCmds )
			{
				std::string sCmd = formatPCICommand( uiCommand, uiRetVal );

				m_pCLog->put( sCmd.c_str() );
			}

			return uiRetVal;
		}


		// +----------------------------------------------------------------------------
		// |  loadGen23ControllerFile
		// +----------------------------------------------------------------------------
		// |  Loads a timing or utility file (.lod) into a GenII or GenIII controller.
		// |
		// |  Throws std::runtime_error on error
		// |
		// |  <IN> -> sFilename   - The TIM or UTIL lod file to load.
		// |  <IN> -> bValidate   - Set to 1 if the download should be read back and
		// |                        checked after every write.
		// |  <IN> -> bAbort      - 'true' to stop; 'false' otherwise. Default: false
		// +----------------------------------------------------------------------------
		void CArcPCI::loadGen23ControllerFile( const std::string& sFilename, bool bValidate, const bool& bAbort )
		{
			std::uint32_t  uiBoardId		= 0;
			std::uint32_t  uiType			= 0;
			std::uint32_t  uiAddr			= 0;
			std::uint32_t  uiData			= 0;
			std::uint32_t  uiReply			= 0;
			std::uint32_t  uiPciStatus		= 0;
			char           typeChar			= ' ';
			bool           bPciStatusSet	= false;
			bool           bIsCLodFile  	= false;

			std::string sLine;

			if ( bAbort ) { return; }

			//
			// Verify gen3 connection
			// -------------------------------------------------------------------
			if ( !isOpen() )
			{
				THROW_NO_DEVICE_ERROR();
			}

			if ( bAbort ) { return; }

			//
			// open the file for reading.
			// -------------------------------------------------------------------
			std::ifstream inFile( sFilename.c_str() );

			if ( !inFile.is_open() )
			{
				THROW( "(CArcPCI) Cannot open file: %s", sFilename.c_str() );
			}

			if ( bAbort ) { inFile.close(); return; }

			//
			// Check for valid TIM or UTIL file
			// -------------------------------------------------------------------
			getline( inFile, sLine );

			if ( sLine.find( "TIM" ) != std::string::npos )
			{
				uiBoardId = TIM_ID;
			}

			else if ( sLine.find( "CRT" ) != std::string::npos )
			{
				uiBoardId = TIM_ID;
				bIsCLodFile = true;
			}

			else if ( sLine.find( "UTIL" ) != std::string::npos )
			{
				uiBoardId = UTIL_ID;
			}

			else
			{
				THROW( "(CArcPCI) Invalid file. Missing 'TIMBOOT/CRT' or 'UTILBOOT' std::string." );
			}

			if ( bAbort ) { inFile.close(); return; }

			//
			// First, send the stop command. Otherwise, the controller crashes
			// because it is downloading and executing code while you try to
			// overwrite it.
			// -----------------------------------------------------------------
			uiReply = command( { TIM_ID, STP } );

			if ( uiReply != DON )
			{
				THROW( "(CArcPCI) Stop ('STP') controller failed. Reply: 0x%X", uiReply );
			}

			if ( bAbort ) { inFile.close(); return; }

			//
			// Set the PCI status bit #1 (X:0 bit 1 = 1).
			// -------------------------------------------
			bPciStatusSet = true;

			uiPciStatus = command( { PCI_ID, RDM, ( X_MEM | 0 ) } );

			uiReply = command( { PCI_ID, WRM, ( X_MEM | 0 ), ( uiPciStatus | 0x00000002 ) } );

			if ( uiReply != DON )
			{
				THROW( "(CArcPCI) Set PCI status bit 1 failed. Reply: 0x%X", uiReply );
			}

			//
			// Read in the file one line at a time
			// --------------------------------------
			while ( !inFile.eof() )
			{
				if ( bAbort ) { inFile.close(); return; }

				getline( inFile, sLine );

				//
				// Only "_DATA" blocks are valid for download
				// ---------------------------------------------
				if ( sLine.find( '_' ) == 0 && sLine.find( "_DATA " ) != std::string::npos )
				{
					auto pTokens = arc::gen3::CArcBase::splitString( sLine );

					// Dump _DATA std::string ( first std::string )

					//
					// Get the memory type and start address
					// ---------------------------------------------
					typeChar = pTokens->at( 1 ).at( 0 );
					uiAddr   = static_cast< std::uint32_t >( std::stol( pTokens->at( 2 ), 0, 16 ) );

					//
					// The start address must be less than MAX_DSP_START_LOAD_ADDR
					// -------------------------------------------------------------
					if ( uiAddr < MAX_DSP_START_LOAD_ADDR )
					{
						//
						// Set the DSP memory type
						// ----------------------------------
						if      ( typeChar == 'X' ) uiType = X_MEM;
						else if ( typeChar == 'Y' ) uiType = Y_MEM;
						else if ( typeChar == 'P' ) uiType = P_MEM;
						else if ( typeChar == 'R' ) uiType = R_MEM;

						//
						// Read the data block
						// ----------------------------------
						while ( !inFile.eof() && inFile.peek() != '_' )
						{
							if ( bAbort ) { inFile.close(); return; }

							getline( inFile, sLine );

							auto pBlockTokens = arc::gen3::CArcBase::splitString( sLine );

							for ( auto it = pBlockTokens->begin(); it != pBlockTokens->end(); it++ )
							{
								if ( bAbort ) { inFile.close(); return; }

								uiData = static_cast< std::uint32_t >( std::stol( *it, 0, 16 ) );

								//
								// Write the data to the controller.
								// --------------------------------------------------------------
								uiReply = command( { uiBoardId, WRM, ( uiType | uiAddr ), uiData } );

								if ( uiReply != DON )
								{
									THROW( "(CArcPCI) Write ('WRM') to controller %s board failed. WRM 0x%X 0x%X -> 0x%X",
											( uiBoardId == TIM_ID ? "TIMING" : "UTILITY" ),
											( uiType | uiAddr ),
											 uiData,
											 uiReply );
								}

								if ( bAbort ) { inFile.close(); return; }

								//
								// Validate the data if required.
								// --------------------------------------------------------------
								if ( bValidate )
								{
									uiReply = command( { uiBoardId, RDM, ( uiType | uiAddr ) } );

									if ( uiReply != uiData )
									{
										THROW( "(CArcPCI) Write ('WRM') to controller %s board failed. RDM 0x%X -> 0x%X [ Expected: 0x%X ]",
												( uiBoardId == TIM_ID ? "TIMING" : "UTILITY" ),
												( uiType | uiAddr ),
												 uiReply,
												 uiData );
									}
								}	// End if 'validate'

								uiAddr++;

							} // while tokenizer next
						} // if not EOF or '_'
					}	// if address < 0x4000
				}	// if '_DATA'
			}	// if not EOF

			inFile.close();

			if ( bAbort ) { return; }

			//
			// Clear the PCI status bit #1 (X:0 bit 1 = 0)
			// --------------------------------------------------------------
			//
			// NOTE: This should be undone for sure. Otherwise, the board may
			// be stuck until the computer is turned off and unplugged!! Also,
			// "cmdStatus" is used in place of "status" because this set of
			// commands may override "status" with API_OK, when it should be
			// API_ERROR from a previous error.
	
			if ( bPciStatusSet )
			{
				auto uiPciStatus = command( { PCI_ID, RDM, ( X_MEM | 0 ) } );

				uiReply = command( { PCI_ID, WRM, ( X_MEM | 0 ), ( uiPciStatus & 0xFFFFFFFD ) } );

				if ( uiReply != DON )
				{
					THROW( "(CArcPCI) Clear PCI status bit 1 failed. Reply: 0x%X", uiReply );
				}
			}

			if ( bAbort ) { return; }

			//
			//  Tell the TIMING board to jump from boot code to
			//  the uploaded application. Don't check the reply,
			//  since it doesn't return one.
			// +------------------------------------------------+
			if ( bIsCLodFile )
			{
				uiReply = command( { TIM_ID, JDL } );

				if ( uiReply != DON )
				{
					THROW( "(CArcPCI) Jump from boot code failed. Reply: 0x%X", uiReply );
				}
			}
		}


		// +----------------------------------------------------------------------------
		// |  getContinuousImageSize
		// +----------------------------------------------------------------------------
		// |  Returns the boundary adjusted image size for continuous readout.  The PCI
		// |  card ( ARC-63/64 ) requires that each image within the image buffer starts
		// |  on a 1024 byte boundary.
		// |
		// |  Throws std::runtime_error on error
		// |
		// |  <IN>  -> uiImageSize - The boundary adjusted image size ( in bytes ).
		// +----------------------------------------------------------------------------
		std::uint32_t CArcPCI::getContinuousImageSize( std::uint32_t uiImageSize )
		{
			std::uint32_t uiBoundedImageSize = 0;

			if ( ( uiImageSize & 0x3FF ) != 0 )
			{
				uiBoundedImageSize = ( uiImageSize - ( uiImageSize & 0x3FF ) + 1024 );
			}
			else
			{
				uiBoundedImageSize = uiImageSize;
			}

			return uiBoundedImageSize;
		}


		// +----------------------------------------------------------------------------
		// |  smallCamDLoad
		// +----------------------------------------------------------------------------
		// |  Sends a .lod download file data stream of up to 6 values to the SmallCam
		// |  controller.
		// |
		// |  Throws std::runtime_error on error
		// |
		// |  <IN>  -> uiBoardId  - Must be SMALLCAM_DLOAD_ID
		// |  <IN>  -> pvData     - Data vector
		// +----------------------------------------------------------------------------
		std::uint32_t CArcPCI::smallCamDLoad( std::uint32_t uiBoardId, std::vector<std::uint32_t>* pvData )
		{
			std::uint32_t uiHeader	= 0;
			std::uint32_t uiReply	= 0;

			//
			//  Report error if gen3 reports readout in progress
			// +------------------------------------------------------+
			if ( isReadout() )
			{
				THROW( "(CArcPCI) Device reports readout in progress! Status: 0x%X", getStatus() );
			}

			//
			//  Verify the size of the data, cannot be greater than 6
			// +------------------------------------------------------+
			if ( pvData->size() > 6 )
			{
				THROW( "(CArcPCI) Data vector too large: 0x%X! Must be less than 6!",	pvData->size() );
			}

			//
			//  Verify the board id equals smallcam download id
			// +------------------------------------------------------+
			if ( uiBoardId != SMALLCAM_DLOAD_ID )
			{
				THROW( "(CArcPCI) Invalid board id: %u! Must be: %u",	uiBoardId, SMALLCAM_DLOAD_ID );
			}

			try
			{
				//
				//  Send Header
				// +-------------------------------------------------+
				uiHeader = ( ( uiBoardId << 8 ) | static_cast<int>( pvData->size() + 1 ) );

				ioctlDevice( ASTROPCI_HCVR_DATA, uiHeader );

				//
				//  Send the data
				// +-------------------------------------------------+
				for ( decltype( pvData->size() ) i = 0; i < pvData->size(); i++ )
				{
					ioctlDevice( ASTROPCI_HCVR_DATA, ( 0xAC000000 | pvData->at( i ) ) );
				}
			}
			catch ( ... )
			{
				if ( m_bStoreCmds )
				{
					m_pCLog->put( formatDLoadString( uiReply, uiBoardId, pvData ).c_str() );
				}

				throw;
			}

			//
			//  Return the reply
			// +-------------------------------------------------+
			try
			{
				uiReply = ( ( getHSTR() & HTF_BIT_MASK ) >> 3 );
			}
			catch ( const std::exception& e )
			{
				if ( m_bStoreCmds )
				{
					m_pCLog->put( formatDLoadString( uiReply, uiBoardId, pvData ).c_str() );
				}

				std::ostringstream oss;

				oss << "(CArcPCI) " << e.what() << std::endl << "Exception Details:" << std::endl << "0x" << std::hex << uiHeader << std::dec;

				for ( decltype( pvData->size() ) i = 0; i < pvData->size(); i++ )
				{
					oss << " 0x" << std::hex << pvData->at( i ) << std::dec;
				}

				oss	<< std::ends;

				THROW( oss.str() );
			}

			//
			// Set the debug message queue.
			//
			if ( m_bStoreCmds )
			{
				m_pCLog->put( formatDLoadString( uiReply, uiBoardId, pvData ).c_str() );
			}

			return uiReply;
		}

		// +----------------------------------------------------------------------------
		// |  setByteSwapping
		// +----------------------------------------------------------------------------
		// |  Turns the PCI hardware byte-swapping on if system architecture is solaris.
		// |  Otherwise, does nothing; compiles to empty function.
		// |
		// |  Throws std::runtime_error on error
		// +----------------------------------------------------------------------------
		void CArcPCI::setByteSwapping( void )
		{
			#if !defined( _WINDOWS ) && !defined( linux ) && !defined( __linux ) && !defined( __APPLE__ )

				//
				// Test if byte swapping is available
				//
				std::uint32_t uiReply = command( { PCI_ID, TBS } );

				if ( containsError( uiReply ) )
				{
					std::string sCmdMsg = CArcTools::CmdToString( uiReply, PCI_ID, TBS );

					THROW( sCmdMsg );
				}

				//
				// Turn hardware byte swapping ON
				//
				uiReply = command( { PCI_ID, SBS, 1 } );

				if ( containsError( uiReply ) )
				{
					std::string sCmdMsg = CArcTools::CmdToString( uiReply, PCI_ID, SBS, 1 );

					THROW( sCmdMsg );
				}

				if ( m_bStoreCmds )
				{
					m_pCLog->put( ( char * )"Hardware byte swapping on!" );
				}

			#endif
		}


		// +----------------------------------------------------------------------------
		// |  ioctlDevice64
		// +----------------------------------------------------------------------------
		// |  Send a command to the gen3 driver. Returns the gen3 driver reply.
		// |
		// |  Throws std::runtime_error on error
		// |
		// |  <IN>  -> uiIoctlCmd  - command board id (TIM or UTIL)
		// |  <IN>  -> uiArg       - command argument (optional)
		// +----------------------------------------------------------------------------
		std::uint64_t CArcPCI::ioctlDevice64( std::uint32_t uiIoctlCmd, std::uint32_t uiArg )
		{
			if ( !isOpen() )
			{
				THROW( "(CArcPCI) Not conntected to any device." );
			}

			std::uint64_t uiRetVal = uiArg;

			auto iSuccess = Arc_IOCtl( m_hDevice, uiIoctlCmd, &uiRetVal, sizeof( uiRetVal ) );

			if ( !iSuccess )
			{
				if ( m_bStoreCmds )
				{
					std::string sCmd = formatPCICommand( uiIoctlCmd, uiRetVal, uiArg );

					m_pCLog->put( sCmd.c_str() );
				}

				THROW( "(CArcPCI::ioctlDevice64) Ioctl failed cmd: 0x%X arg: 0x%X : %e", uiIoctlCmd, uiArg, true );
			}

			// Set the debug message queue
			if ( m_bStoreCmds )
			{
				std::string sCmd = formatPCICommand( uiIoctlCmd, uiRetVal, uiArg );

				m_pCLog->put( sCmd.c_str() );
			}

			return uiRetVal;
		}


		// +----------------------------------------------------------------------------
		// |  ioctlDevice
		// +----------------------------------------------------------------------------
		// |  Send a command to the gen3 driver. Returns the gen3 driver reply.
		// |
		// |  Throws std::runtime_error on error
		// |
		// |  <IN>  -> uiIoctlCmd  - command board id (TIM or UTIL)
		// |  <IN>  -> uiArg       - command argument (optional)
		// +----------------------------------------------------------------------------
		std::uint32_t CArcPCI::ioctlDevice( std::uint32_t uiIoctlCmd, std::uint32_t uiArg )
		{
			if ( !isOpen() )
			{
				THROW( "(CArcPCI) Not conntected to any device." );
			}

			std::uint32_t uiRetVal = uiArg;

			auto iSuccess = Arc_IOCtl( m_hDevice, uiIoctlCmd, &uiRetVal, sizeof( uiRetVal ) );

			if ( !iSuccess )
			{
				if ( m_bStoreCmds )
				{
					std::string sCmd = formatPCICommand( uiIoctlCmd, uiRetVal, uiArg );

					m_pCLog->put( sCmd.c_str() );
				}

				THROW( "(CArcPCI::ioctlDevice) Ioctl failed cmd: 0x%X arg: 0x%X : %e",	uiIoctlCmd,	uiArg, arc::gen3::CArcBase::getSystemError() );
			}

			// Set the debug message queue
			if ( m_bStoreCmds )
			{
					std::string sCmd = formatPCICommand( uiIoctlCmd, uiRetVal, uiArg );

					m_pCLog->put( sCmd.c_str() );
			}

			return uiRetVal;
		}


		// +----------------------------------------------------------------------------
		// |  ioctlDevice
		// +----------------------------------------------------------------------------
		// |  Send a command to the gen3 driver. Returns the gen3 driver reply.
		// |
		// |  Throws std::runtime_error on error
		// |
		// |  <IN>  -> uiArg      - Array of command arguments
		// |  <IN>  -> uiArgCount - The number of arguments in the dArg array parameter
		// +----------------------------------------------------------------------------
		std::uint32_t CArcPCI::ioctlDevice( std::uint32_t uiIoctlCmd, const std::initializer_list<std::uint32_t>& tArgList )
		{
			if ( !isOpen() )
			{
				THROW( "(CArcPCI) Not conntected to any device." );
			}

			std::unique_ptr<std::uint32_t[]> pArgs( new std::uint32_t[ tArgList.size() ] );

			if ( pArgs == nullptr )
			{
				THROW( "(CArcPCI) Failed to allocate argument buffer!" );
			}

			auto pInserter = pArgs.get();

			for ( auto it = tArgList.begin(); it != tArgList.end(); it++ )
			{
				*pInserter = *it;

				pInserter++;
			}

			auto iSuccess = Arc_IOCtl( m_hDevice, uiIoctlCmd, pArgs.get(), static_cast<std::uint32_t>( tArgList.size() ) );

			if ( !iSuccess )
			{
				if ( m_bStoreCmds )
				{
					std::string sCmd = formatPCICommand( uiIoctlCmd, pArgs[ 0 ], tArgList );

					m_pCLog->put( sCmd.c_str() );
				}

				std::ostringstream oss;

				oss << "(CArcPCI::ioctlDevice) Ioctl failed cmd: 0x" << std::hex << uiIoctlCmd << std::dec;

				for ( auto it = tArgList.begin(); it != tArgList.end(); it++ )
				{
					oss << " arg: 0x" << std::hex << *it << std::dec;
				}

				oss << " : " << arc::gen3::CArcBase::getSystemError() << std::ends;

				THROW( oss.str() );
			}

			// Set the debug message queue
			if ( m_bStoreCmds )
			{
				std::string sCmd = formatPCICommand( uiIoctlCmd, pArgs[ 0 ], tArgList );

				m_pCLog->put( sCmd.c_str() );
			}

			return pArgs[ 0 ];
		}


		// +----------------------------------------------------------------------------
		// |  getCommonBufferProperties
		// +----------------------------------------------------------------------------
		// |  Fills in the image buffer structure with its properties, such as
		// |  physical address and size.
		// |
		// |  Throws NOTHING on error. No error handling.
		// +----------------------------------------------------------------------------
		bool CArcPCI::getCommonBufferProperties()
		{
			try
			{
				m_tImgBuffer.ulSize = ioctlDevice64( ASTROPCI_GET_DMA_SIZE );

				m_tImgBuffer.ulPhysicalAddr = ioctlDevice64( ASTROPCI_GET_DMA_ADDR );
			}
			catch ( ... )
			{
				return false;
			}

			return true;
		}


		// +----------------------------------------------------------------------------
		// |  FormatPCICommand
		// +----------------------------------------------------------------------------
		// |  Formats an IOCTL command into a std::string that can be passed into a throw
		// |  exception method.
		// |
		// |  <IN> -> uiCmd      : The command value.
		// |  <IN> -> uiReply    : The received command reply value.
		// |  <IN> -> uiArg      : Any argument that may go with the specified command.
		// |  <IN> -> bGetSysErr : A system error number, used to get system message.
		// +----------------------------------------------------------------------------
		const std::string CArcPCI::formatPCICommand( std::uint32_t uiCmd, std::uint64_t uiReply, std::uint32_t uiArg, bool bGetSysErr )
		{
			std::ostringstream oss;

			oss.setf( std::ios::hex, std::ios::basefield );
			oss.setf( std::ios::uppercase );

			oss << "[ ";

				 if ( uiCmd == ASTROPCI_GET_HCTR ) { oss << "ASTROPCI_GET_HCTR"; }
			else if ( uiCmd == ASTROPCI_GET_PROGRESS ) { oss << "ASTROPCI_GET_PROGRESS"; }
			else if ( uiCmd == ASTROPCI_GET_DMA_ADDR ) { oss << "ASTROPCI_GET_DMA_ADDR"; }
			else if ( uiCmd == ASTROPCI_GET_HSTR ) { oss << "ASTROPCI_GET_HSTR"; }
			else if ( uiCmd == ASTROPCI_GET_DMA_SIZE ) { oss << "ASTROPCI_GET_DMA_SIZE"; }
			else if ( uiCmd == ASTROPCI_GET_FRAMES_READ ) { oss << "ASTROPCI_GET_FRAMES_READ"; }
			else if ( uiCmd == ASTROPCI_HCVR_DATA ) { oss << "ASTROPCI_HCVR_DATA"; }
			else if ( uiCmd == ASTROPCI_SET_HCTR ) { oss << "ASTROPCI_SET_HCTR"; }
			else if ( uiCmd == ASTROPCI_SET_HCVR ) { oss << "ASTROPCI_SET_HCVR"; }
			else if ( uiCmd == ASTROPCI_PCI_DOWNLOAD ) { oss << "ASTROPCI_PCI_DOWNLOAD"; }
			else if ( uiCmd == ASTROPCI_PCI_DOWNLOAD_WAIT ) { oss << "ASTROPCI_PCI_DOWNLOAD_WAIT"; }
			else if ( uiCmd == ASTROPCI_COMMAND ) { oss << "ASTROPCI_COMMAND"; }
			else if ( uiCmd == ASTROPCI_GET_CONFIG_BYTE ) { oss << "ASTROPCI_GET_CONFIG_BYTE"; }
			else if ( uiCmd == ASTROPCI_GET_CONFIG_WORD ) { oss << "ASTROPCI_GET_CONFIG_WORD"; }
			else if ( uiCmd == ASTROPCI_GET_CONFIG_DWORD ) { oss << "ASTROPCI_GET_CONFIG_DWORD"; }
			else if ( uiCmd == ASTROPCI_SET_CONFIG_BYTE ) { oss << "ASTROPCI_SET_CONFIG_BYTE"; }
			else if ( uiCmd == ASTROPCI_SET_CONFIG_WORD ) { oss << "ASTROPCI_SET_CONFIG_WORD"; }
			else if ( uiCmd == ASTROPCI_SET_CONFIG_DWORD ) { oss << "ASTROPCI_SET_CONFIG_DWORD"; }

		#ifdef _WINDOWS
			else if ( uiCmd == ASTROPCI_MEM_MAP ) { oss << "ASTROPCI_MEM_MAP"; }
			else if ( uiCmd == ASTROPCI_MEM_UNMAP ) { oss << "ASTROPCI_MEM_UNMAP"; }
		#endif

			else oss << "0x" << uiCmd;

			if ( uiArg != CArcDevice::NOPARAM )
			{
				oss << " 0x" << uiArg;
			}

			oss << " -> 0x" << uiReply << " ]";

			if ( bGetSysErr )
			{
				oss << std::endl << std::dec << CArcBase::getSystemMessage( CArcBase::getSystemError() );
			}

			oss << std::ends;

			return oss.str();
		}


		// +----------------------------------------------------------------------------
		// |  FormatPCICommand
		// +----------------------------------------------------------------------------
		// |  Formats an IOCTL command into a std::string that can be passed into a throw
		// |  exception method.
		// |
		// |  <IN> -> uiCmd      : The command value.
		// |  <IN> -> uiReply    : The received command reply value.
		// |  <IN> -> tArgList   : Array of arguments that go with the specified command.
		// |  <IN> -> bGetSysErr : A system error number, used to get system message.
		// +----------------------------------------------------------------------------
		const std::string CArcPCI::formatPCICommand( std::uint32_t uiCmd, std::uint64_t uiReply, const std::initializer_list<std::uint32_t>& tArgList, bool bGetSysErr )
		{
			std::ostringstream oss;

			oss.setf( std::ios::hex, std::ios::basefield );
			oss.setf( std::ios::uppercase );

			oss << "[ ";

				 if ( uiCmd == ASTROPCI_GET_HCTR ) { oss << "ASTROPCI_GET_HCTR"; }
			else if ( uiCmd == ASTROPCI_GET_PROGRESS ) { oss << "ASTROPCI_GET_PROGRESS"; }
			else if ( uiCmd == ASTROPCI_GET_DMA_ADDR ) { oss << "ASTROPCI_GET_DMA_ADDR"; }
			else if ( uiCmd == ASTROPCI_GET_HSTR ) { oss << "ASTROPCI_GET_HSTR"; }
			else if ( uiCmd == ASTROPCI_GET_DMA_SIZE ) { oss << "ASTROPCI_GET_DMA_SIZE"; }
			else if ( uiCmd == ASTROPCI_GET_FRAMES_READ ) { oss << "ASTROPCI_GET_FRAMES_READ"; }
			else if ( uiCmd == ASTROPCI_HCVR_DATA ) { oss << "ASTROPCI_HCVR_DATA"; }
			else if ( uiCmd == ASTROPCI_SET_HCTR ) { oss << "ASTROPCI_SET_HCTR"; }
			else if ( uiCmd == ASTROPCI_SET_HCVR ) { oss << "ASTROPCI_SET_HCVR"; }
			else if ( uiCmd == ASTROPCI_PCI_DOWNLOAD ) { oss << "ASTROPCI_PCI_DOWNLOAD"; }
			else if ( uiCmd == ASTROPCI_PCI_DOWNLOAD_WAIT ) { oss << "ASTROPCI_PCI_DOWNLOAD_WAIT"; }
			else if ( uiCmd == ASTROPCI_COMMAND ) { oss << "ASTROPCI_COMMAND"; }
			else if ( uiCmd == ASTROPCI_GET_CONFIG_BYTE ) { oss << "ASTROPCI_GET_CONFIG_BYTE"; }
			else if ( uiCmd == ASTROPCI_GET_CONFIG_WORD ) { oss << "ASTROPCI_GET_CONFIG_WORD"; }
			else if ( uiCmd == ASTROPCI_GET_CONFIG_DWORD ) { oss << "ASTROPCI_GET_CONFIG_DWORD"; }
			else if ( uiCmd == ASTROPCI_SET_CONFIG_BYTE ) { oss << "ASTROPCI_SET_CONFIG_BYTE"; }
			else if ( uiCmd == ASTROPCI_SET_CONFIG_WORD ) { oss << "ASTROPCI_SET_CONFIG_WORD"; }
			else if ( uiCmd == ASTROPCI_SET_CONFIG_DWORD ) { oss << "ASTROPCI_SET_CONFIG_DWORD"; }

		#ifdef _WINDOWS
			else if ( uiCmd == ASTROPCI_MEM_MAP ) { oss << "ASTROPCI_MEM_MAP"; }
			else if ( uiCmd == ASTROPCI_MEM_UNMAP ) { oss << "ASTROPCI_MEM_UNMAP"; }
		#endif

			else oss << "0x" << uiCmd;

			for ( auto it = tArgList.begin(); it != tArgList.end(); it++ )
			{
				oss << " 0x" << *it;
			}

			oss << " -> 0x" << uiReply << " ]";

			if ( bGetSysErr )
			{
				oss << std::endl << std::dec << CArcBase::getSystemMessage( CArcBase::getSystemError() );
			}

			oss << std::ends;

			return oss.str();
		}


		/////////////////////////////////////////////////////////////////////////////////////////////////
		//             PCI CONFIGURATION SPACE TESTS
		/////////////////////////////////////////////////////////////////////////////////////////////////
		// +----------------------------------------------------------------------------
		// |  getCfgSpByte
		// +----------------------------------------------------------------------------
		// |  Returns the specified BYTE from the specified PCI configuration space
		// |  register.
		// |
		// |  <IN> -> dOffset - The byte offset from the start of PCI config space
		// |
		// |  Throws std::runtime_error on error
		// +----------------------------------------------------------------------------
		std::uint32_t CArcPCI::getCfgSpByte( std::uint32_t uiOffset )
		{
			return ioctlDevice( ASTROPCI_GET_CONFIG_BYTE, uiOffset );
		}


		// +----------------------------------------------------------------------------
		// |  getCfgSpWord
		// +----------------------------------------------------------------------------
		// |  Returns the specified WORD from the specified PCI configuration space
		// |  register.
		// |
		// |  <IN> -> dOffset - The byte offset from the start of PCI config space
		// |
		// |  Throws std::runtime_error on error
		// +----------------------------------------------------------------------------
		std::uint32_t CArcPCI::getCfgSpWord( std::uint32_t uiOffset )
		{
			return ioctlDevice( ASTROPCI_GET_CONFIG_WORD, uiOffset );
		}


		// +----------------------------------------------------------------------------
		// |  getCfgSpDWord
		// +----------------------------------------------------------------------------
		// |  Returns the specified DWORD from the specified PCI configuration space
		// |  register.
		// |
		// |  <IN> -> uiOffset - The byte offset from the start of PCI config space
		// |
		// |  Throws std::runtime_error on error
		// +----------------------------------------------------------------------------
		std::uint32_t CArcPCI::getCfgSpDWord( std::uint32_t uiOffset )
		{
			return ioctlDevice( ASTROPCI_GET_CONFIG_DWORD, uiOffset );
		}


		// +----------------------------------------------------------------------------
		// |  setCfgSpByte
		// +----------------------------------------------------------------------------
		// |  Writes the specified BYTE to the specified PCI configuration space
		// |  register.
		// |
		// |  <IN> -> uiOffset - The byte offset from the start of PCI config space
		// |  <IN> -> uiValue  - The BYTE value to write
		// |
		// |  Throws std::runtime_error on error
		// +----------------------------------------------------------------------------
		void CArcPCI::setCfgSpByte( std::uint32_t uiOffset, std::uint32_t uiValue )
		{
			ioctlDevice( ASTROPCI_SET_CONFIG_BYTE, { uiOffset, uiValue } );
		}


		// +----------------------------------------------------------------------------
		// |  setCfgSpWord
		// +----------------------------------------------------------------------------
		// |  Writes the specified WORD to the specified PCI configuration space
		// |  register.
		// |
		// |  <IN> -> uiOffset - The byte offset from the start of PCI config space
		// |  <IN> -> uiValue  - The WORD value to write
		// |
		// |  Throws std::runtime_error on error
		// +----------------------------------------------------------------------------
		void CArcPCI::setCfgSpWord( std::uint32_t uiOffset, std::uint32_t uiValue )
		{
			ioctlDevice( ASTROPCI_SET_CONFIG_WORD, { uiOffset, uiValue } );
		}


		// +----------------------------------------------------------------------------
		// |  setCfgSpDWord
		// +----------------------------------------------------------------------------
		// |  Writes the specified DWORD to the specified PCI configuration space
		// |  register.
		// |
		// |  <IN> -> uiOffset - The byte offset from the start of PCI config space
		// |  <IN> -> uiValue  - The DWORD value to write
		// |
		// |  Throws std::runtime_error on error
		// +----------------------------------------------------------------------------
		void CArcPCI::setCfgSpDWord( std::uint32_t uiOffset, std::uint32_t uiValue )
		{
			ioctlDevice( ASTROPCI_SET_CONFIG_DWORD, { uiOffset, uiValue } );
		}


		// +----------------------------------------------------------------------------
		// |  getCfgSp
		// +----------------------------------------------------------------------------
		// |  Reads and parses the entire PCI configuration space header into readable
		// |  text and bit definitions that are stored in a member list variable. The
		// |  public methods of this class allow access to this list. This method will
		// |  create the member list if it doesn't already exist and clears it if it
		// |  does.
		// +----------------------------------------------------------------------------
		void CArcPCI::getCfgSp( void )
		{
			CArcPCIBase::getCfgSp();
		}


		// +----------------------------------------------------------------------------
		// |  getBarSp
		// +----------------------------------------------------------------------------
		// |  Reads and parses the entire PCI Base Address Registers (BAR) into readable
		// |  text and bit definitions that are stored in a member list variable. The
		// |  public methods of this class allow access to this list. This method will
		// |  create the member list if it doesn't already exist and clears it if it
		// |  does. NOTE: Not all BARS or PCI boards have data.
		// +----------------------------------------------------------------------------
		void CArcPCI::getBarSp( void )
		{
			CArcPCIBase::getBarSp();

			//
			//  Access the register data
			// +-------------------------------------------------------+
			std::unique_ptr<PCIRegList> pList( new PCIRegList() );

			int dRegValue = ioctlDevice( ASTROPCI_GET_HCTR );
			addRegItem( pList.get(),
						0x10,
						"Host Control Register ( HCTR )",
						dRegValue );

			dRegValue = ioctlDevice( ASTROPCI_GET_HSTR );
			addRegItem( pList.get(),
						0x14,
						"Host Status Register ( HSTR )",
						dRegValue,
						getHSTRBitList( dRegValue ).release() );

			addBarItem( "DSP Regs ( BAR0 )", pList.release() );
		}


		// +----------------------------------------------------------------------------
		// |  getHSTRBitList
		// +----------------------------------------------------------------------------
		// |  Sets the bit list strings for the PCI DSP HSTR register.
		// |
		// |  <IN> -> uiData  - The PCI cfg sp CLASS CODE and REV ID register value.
		// |  <IN> -> bDrawSeparator - 'true' to include a line separator within the
		// |                            bit list strings.
		// |
		// |  Throws std::runtime_error on error
		// +----------------------------------------------------------------------------
		std::unique_ptr<CArcStringList> CArcPCI::getHSTRBitList( std::uint32_t dData, bool bDrawSeparator )
		{
			std::unique_ptr<CArcStringList> pBitList( new CArcStringList() );

			if ( bDrawSeparator )
			{
				*pBitList << "____________________________________________________";
			}

			ePCIStatus uiHSTR = static_cast<ePCIStatus>( ( dData & HTF_BIT_MASK ) >> 3 );

			if ( uiHSTR == ePCIStatus::DONE_STATUS )
			{
				*pBitList << CArcBase::formatString( "Status: 0x%X [ DON ]", uiHSTR );
			}

			else if ( uiHSTR == ePCIStatus::READ_REPLY_STATUS )
			{
				*pBitList << CArcBase::formatString( "Status: 0x%X [ READ REPLY ]", uiHSTR );
			}

			else if ( uiHSTR == ePCIStatus::ERROR_STATUS )
			{
				*pBitList << CArcBase::formatString( "Status: 0x%X [ ERR ]", uiHSTR );
			}
	
			else if ( uiHSTR == ePCIStatus::SYSTEM_RESET_STATUS )
			{
				*pBitList << CArcBase::formatString( "Status: 0x%X [ SYR ]", uiHSTR );
			}

			else if ( uiHSTR == ePCIStatus::READOUT_STATUS )
			{
				*pBitList << CArcBase::formatString( "Status: 0x%X [ READOUT ]", uiHSTR );
			}

			else if ( uiHSTR == ePCIStatus::BUSY_STATUS )
			{
				*pBitList << CArcBase::formatString( "Status: 0x%X [ BUSY ]", uiHSTR );
			}

			else if ( uiHSTR == ePCIStatus::TIMEOUT_STATUS )
			{
				*pBitList << CArcBase::formatString( "Status: 0x%X [ IDLE / TIMEOUT ]", uiHSTR );
			}

			else
			{
				*pBitList << CArcBase::formatString( "Status: 0x%X [ UNKNOWN ]", uiHSTR );
			}

			return std::move( pBitList );
		}

	}	// end gen3 namespace
}	// end arc namespace
