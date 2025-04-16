// +----------------------------------------------------------------------+
//  CArcPCIe.cpp : Defines the exported functions for the DLL application.
// +----------------------------------------------------------------------+
//
// KNOWN PROBLEMS:
//
// 1. Dec 15, 2010 - Using DS9 to display image buffer data and then
//    switching devices prevents old gen3 from being closed, thus the
//    module use count doesn't get decremented.  This will result in a
//    "gen3 busy" error.  Only fix is to terminate the application.
//    I think DS9 doesn't release image buffer data, which prevents the
//    close() system call from being called.
//
//    Symptoms:
//    1. open gen3 0, then switch to gen3 1, then back to gen3 0
//       works; UNLESS DS9 is started and images taken and displayed.
//       THEN, gen3 used to take images will not be closed and module
//       use count ( as seen by /sbin/lsmod ) doesn't decrement.
//    2. The system function close() doesn't call the driver xxxClose
//       function when DS9 is used to display images as described in 1.
// +----------------------------------------------------------------------+

#ifdef _WINDOWS
	#define INITGUID
#endif

#include <iostream>
#include <fstream>
#include <sstream>
#include <memory>
#include <list>
#include <cstddef>
#include <algorithm>
#include <array>
#include <ctime>
#include <ios>

#include <CArcBase.h>
#include <CArcStringList.h>
#include <CArcPCIe.h>
#include <Reg9056.h>
#include <PCIRegs.h>
#include <ArcOSDefs.h>
#include <ArcDefs.h>

#ifdef _WINDOWS
	#include <setupapi.h>
	#include <devguid.h>
	#include <regstr.h>
	#include <AstroPCIeGUID.h>
#endif

#if defined( linux ) || defined( __linux )
	#include <sys/ioctl.h>
	#include <sys/types.h>
	#include <sys/stat.h>
	#include <sys/mman.h>
	#include <dirent.h>
	#include <fcntl.h>
	#include <errno.h>
	#include <cstring>
#endif

#ifdef __APPLE__
	#include <sys/types.h>
	#include <sys/mman.h>
#endif


//#include <fstream>
//std::ofstream dbgStream2( "CArcPCIe_Debug.txt" );


namespace arc
{
	namespace gen3
	{

		// +----------------------------------------------------------------------------
		// |  PCIe Device Info
		// +----------------------------------------------------------------------------
		#if defined( linux ) || defined( __linux )

			#define DEVICE_DIR			"/dev/"
			#define DEVICE_NAME			"AstroPCIe"
			#define DEVICE_NAME_ALT		"Arc66PCIe"

		#elif defined( __APPLE__ )

			#define AstroPCIeClassName		 com_arc_driver_Arc66PCIe
			#define kAstroPCIeClassName		"com_arc_driver_Arc66PCIe"

		#endif


		// +----------------------------------------------------------------------------
		// |  Macro to validate gen3 and vendor id's
		// +----------------------------------------------------------------------------
		#define VALID_DEV_VEN_ID( devVenId )	\
						( ( devVenId == 0x9056 || devVenId == 0x10B5 ) ? 1 : 0 )



		// +----------------------------------------------------------------------------
		// |  Macro to verify command values as 24-bits. This is necessary to prevent
		// |  the 'AC' pre-amble from being OR'd with other bits and being lost.
		// +----------------------------------------------------------------------------
		#define VERIFY_24BITS( clazz, method, value )												\
						if ( ( value & 0xFF000000 ) != 0 )											\
						{																			\
							THROW( "Data value %u [ 0x%X ] too large! Must be 24-bits or less!",	\
													  value, value );								\
						}


		// +----------------------------------------------------------------------------
		// |  PLX Register Structure
		// +----------------------------------------------------------------------------
		typedef struct PLX_REG_ITEM
		{
			std::uint32_t	uiAddr;
			std::string		sText;
		} PLXRegItem;



		// +----------------------------------------------------------------------------
		// |  PLX BAR Address To String Translation Tables
		// +----------------------------------------------------------------------------
		std::string LCRMapName       =   "Local Config (BAR0)";
		PLXRegItem LCRMap[ 18 ] = { { PCI9056_SPACE0_RANGE, "Direct Slave Local Address Space 0 Range" },
									{ PCI9056_SPACE0_REMAP, "Direct Slave Local Address Space 0 ( Remap )" },
									{ PCI9056_LOCAL_DMA_ARBIT, "Mode/DMA Arbitration" },
									{ PCI9056_ENDIAN_DESC, "Local Misc Ctrl 2/EEPROM Addr Boundary/Local Misc Ctrl 1/Endian Descriptor" },
									{ PCI9056_EXP_ROM_RANGE, "Direct Slave Expansion ROM Range" },
									{ PCI9056_EXP_ROM_REMAP, "Direct Slave Exp ROM Local Base Addr (Remap) & BREQo Ctrl" },
									{ PCI9056_SPACE0_ROM_DESC, "Local Addr Space 0/Expansion ROM Bus Region Descriptor" },
									{ PCI9056_DM_RANGE, "Local Range Direct Master-to-PCIe" },
									{ PCI9056_DM_MEM_BASE, "Local Base Addr Direct Master-to-PCIe Memory" },
									{ PCI9056_DM_IO_BASE, "Local Base Addr Direct Master-to-PCIe I/O Configuration" },
									{ PCI9056_DM_PCI_MEM_REMAP, "PCIe Base Addr (Remap) Master-to-PCIe Memory" },
									{ PCI9056_DM_PCI_IO_CONFIG, "PCI Config Addr Direct Master-to-PCIe I/O Configuration" },
									{ PCI9056_SPACE1_RANGE, "Direct Slave Local Addr Space 1 Range" },
									{ PCI9056_SPACE1_REMAP, "Direct Slave Local Addr Space 1 Local Base Addr (Remap)" },
									{ PCI9056_SPACE1_DESC, "Local Addr Space 1 Bus Region Descriptor" },
									{ PCI9056_DM_DAC, "Direct Master PCIe Dual Addr Cycles Upper Addr" },
									{ PCI9056_ARBITER_CTRL, "Internal Arbiter Control" },
									{ PCI9056_ABORT_ADDRESS, "PCI Abort Address" } };

		std::string RTRMapName       =   "Runtime Regs (BAR0)";
		PLXRegItem RTRMap[ 14 ] = { { PCI9056_MAILBOX0, "Mailbox 0" },
									{ PCI9056_MAILBOX1, "Mailbox 1" },
									{ PCI9056_MAILBOX2, "Mailbox 2" },
									{ PCI9056_MAILBOX3, "Mailbox 3" },
									{ PCI9056_MAILBOX4, "Mailbox 4" },
									{ PCI9056_MAILBOX5, "Mailbox 5" },
									{ PCI9056_MAILBOX6, "Mailbox 6" },
									{ PCI9056_MAILBOX7, "Mailbox 7" },
									{ PCI9056_LOCAL_DOORBELL, "PCIe-to-Local Doorbell" },
									{ PCI9056_PCI_DOORBELL, "Local-to-PCIe Doorbell" },
									{ PCI9056_INT_CTRL_STAT, "Interrupt Control/Status" },
									{ PCI9056_EEPROM_CTRL_STAT, "Serial EEPROM Ctrl, PCI Cmd Codes, User I/O Ctrl, Init Ctrl" },
									{ PCI9056_PERM_VENDOR_ID, "Device ID / Vendor ID" },
									{ PCI9056_REVISION_ID, "Reserved / PCI Hardwired Revision ID" } };

		std::string DMAMapName       =   "DMA Regs (BAR0)";
		PLXRegItem DMAMap[ 15 ] = { { PCI9056_DMA0_MODE, "DMA Channel 0 Mode" },
									{ PCI9056_DMA0_PCI_ADDR, "DMA Channel 0 PCIe Address" },
									{ PCI9056_DMA0_LOCAL_ADDR, "DMA Channel 0 Local Address" },
									{ PCI9056_DMA0_COUNT, "DMA Channel 0 Transfer Size (Bytes)" },
									{ PCI9056_DMA0_DESC_PTR, "DMA Channel 0 Descriptor Pointer" },
									{ PCI9056_DMA1_MODE, "DMA Channel 1 Mode" },
									{ PCI9056_DMA1_PCI_ADDR, "DMA Channel 1 PCIe Address" },
									{ PCI9056_DMA1_LOCAL_ADDR, "DMA Channel 1 Local Address" },
									{ PCI9056_DMA1_COUNT, "DMA Channel 1 Transfer Size (Bytes)" },
									{ PCI9056_DMA1_DESC_PTR, "DMA Channel 1 Descriptor Pointer" },
									{ PCI9056_DMA_COMMAND_STAT, "Reserved / DMA Ch 1 Cmd-Status / DMA Ch 0 Cmd-Status" },
									{ PCI9056_DMA_ARBIT, "DMA Arbitration" },
									{ PCI9056_DMA_THRESHOLD, "DMA Threshold" },
									{ PCI9056_DMA0_PCI_DAC, "DMA Channel 0 PCIe Dual Addr Cycle Upper Addr" },
									{ PCI9056_DMA1_PCI_DAC, "DMA Channel 1 PCIe Dual Addr Cycle Upper Addr" } };

		std::string MSQMapName       =   "Msg Q Regs (BAR0)";
		PLXRegItem MSQMap[ 13 ] = { { PCI9056_OUTPOST_INT_STAT, "Outbound Post Queue Interrupt Status" },
									{ PCI9056_OUTPOST_INT_MASK, "Outbound Post Queue Interrupt Mask" },
									{ PCI9056_MU_CONFIG, "Messaging Queue Configuration" },
									{ PCI9056_FIFO_BASE_ADDR, "Queue Base Address" },
									{ PCI9056_INFREE_HEAD_PTR, "Inbound Free Head Pointer" },
									{ PCI9056_INFREE_TAIL_PTR, "Inbound Free Tail Pointer" },
									{ PCI9056_INPOST_HEAD_PTR, "Inbound Post Head Pointer" },
									{ PCI9056_INPOST_TAIL_PTR, "Inbound Post Tail Pointer" },
									{ PCI9056_OUTFREE_HEAD_PTR, "Outbound Free Head Pointer" },
									{ PCI9056_OUTFREE_TAIL_PTR, "Outbound Free Tail Pointer" },
									{ PCI9056_OUTPOST_HEAD_PTR, "Outbound Post Head Pointer" },
									{ PCI9056_OUTPOST_TAIL_PTR, "Outbound Post Tail Pointer" },
									{ PCI9056_FIFO_CTRL_STAT, "Reserved / Queue Control-Status" } };


		// +----------------------------------------------------------------------------
		// |  Device Name Sorting Function
		// +----------------------------------------------------------------------------
		bool DevListSort( arc::gen3::device::ArcDev_t i, arc::gen3::device::ArcDev_t j )
		{
			return ( ( i.sName.compare( j.sName ) ) != 0 );
		}


		// +----------------------------------------------------------------------------
		// |  Static Class Member Initialization
		// +----------------------------------------------------------------------------
		std::unique_ptr<std::vector<arc::gen3::device::ArcDev_t>> CArcPCIe::m_vDevList;
		std::shared_ptr<std::string> CArcPCIe::m_psDevList;


		// +---------------------------------------------------------------------------+
		// | ArrayDeleter                                                              |
		// +---------------------------------------------------------------------------+
		// | Called by std::shared_ptr to delete the temporary image buffer.      |
		// | This method should NEVER be called directly by the user.                  |
		// +---------------------------------------------------------------------------+
		template<typename T> void CArcPCIe::ArrayDeleter( T* p )
		{
			if ( p != nullptr )
			{
				delete [] p;
			}
		}


		// +----------------------------------------------------------------------------
		// |  Constructor
		// +----------------------------------------------------------------------------
		// |  See CArcPCIe.h for the class definition
		// +----------------------------------------------------------------------------
		CArcPCIe::CArcPCIe( void )
		{
			m_hDevice = INVALID_HANDLE_VALUE;
		}


		// +----------------------------------------------------------------------------
		// |  Destructor
		// +----------------------------------------------------------------------------
		CArcPCIe::~CArcPCIe( void )
		{
			close();
		}


		// +----------------------------------------------------------------------------
		// |  toString
		// +----------------------------------------------------------------------------
		// |  Returns a std::string that represents the gen3 controlled by this library.
		// +----------------------------------------------------------------------------
		const std::string CArcPCIe::toString()
		{
			return std::string( "PCIe [ ARC-66 / 67 ]" );
		}


		// +----------------------------------------------------------------------------
		// |  findDevices
		// +----------------------------------------------------------------------------
		// |  Searches for available ARC, Inc PCIe devices and stores the list, which
		// |  can be accessed via gen3 number ( 0,1,2... ).
		// |
		// |  Throws std::runtime_error on error
		// +----------------------------------------------------------------------------
		void CArcPCIe::findDevices()
		{
			if ( m_vDevList == nullptr )
			{
				m_vDevList.reset( new std::vector<arc::gen3::device::ArcDev_t>() );
			}

			if ( m_vDevList == nullptr )
			{
				THROW( "Failed to allocate resources for PCIe device list!" );
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

				if ( !m_vDevList->empty() )
				{
					m_vDevList->clear();

					if ( m_vDevList->size() > 0 )
					{
						THROW( "Failed to free existing device list!" );
					}
				}

			#endif

			#ifdef _WINDOWS

				PSP_DEVICE_INTERFACE_DETAIL_DATA DeviceInterfaceDetailData = nullptr;
				SP_DEVICE_INTERFACE_DATA DeviceInterfaceData;
				HDEVINFO HardwareDeviceInfo;

				BOOL  bResult		   = FALSE;
				DWORD dwRequiredLength = 0;
				DWORD dwMemberIndex	   = 0;

				HardwareDeviceInfo = SetupDiGetClassDevs( ( LPGUID )&GUID_DEVINTERFACE_ARC_PCIE,
														   nullptr,
														   nullptr,
														  ( DIGCF_PRESENT | DIGCF_DEVICEINTERFACE ) );

				if ( HardwareDeviceInfo == INVALID_HANDLE_VALUE )
				{
					THROW( "SetupDiGetClassDevs failed!" );
				}

				DeviceInterfaceData.cbSize = sizeof( SP_DEVICE_INTERFACE_DATA );

				while ( 1 )
				{
					bResult = SetupDiEnumDeviceInterfaces( HardwareDeviceInfo,
														   0,
														   ( LPGUID )&GUID_DEVINTERFACE_ARC_PCIE,
														   dwMemberIndex,
														   &DeviceInterfaceData );

					if ( bResult == FALSE )
					{
						SetupDiDestroyDeviceInfoList( HardwareDeviceInfo );
						break;
					}

					SetupDiGetDeviceInterfaceDetail( HardwareDeviceInfo,
													 &DeviceInterfaceData,
													 nullptr,
													 0,
													 &dwRequiredLength,
													 nullptr );

					DeviceInterfaceDetailData = ( PSP_DEVICE_INTERFACE_DETAIL_DATA )
													LocalAlloc( LMEM_FIXED, dwRequiredLength );

					if ( DeviceInterfaceDetailData == nullptr )
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
															   nullptr );

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
																		 IOServiceMatching( kAstroPCIeClassName ),
																		 &iterator );

				if ( kernResult != KERN_SUCCESS )
				{
					THROW( "IOServiceGetMatchingServices failed: 0x%X",	kernResult );
				}

				while ( ( service = IOIteratorNext( iterator ) ) != IO_OBJECT_nullptr )
				{
					ArcDev_t tArcDev;

					tArcDev.sName			= kAstroPCIeClassName;
					tArcDev.tService		= service;

					m_vDevList->push_back( tArcDev );
				}

				//
				// Release the io_iterator_t now that we're done with it.
				//
				IOObjectRelease( iterator );


			#else	// LINUX

				struct dirent* pDirEntry = nullptr;
				DIR	*pDir = nullptr;

				pDir = opendir( DEVICE_DIR );

				if ( pDir == nullptr )
				{
					THROW( "Failed to open dir: %s", DEVICE_DIR );
				}
				else
				{
					while ( ( pDirEntry = readdir( pDir ) ) != nullptr )
					{
						std::string sDirEntry( pDirEntry->d_name );

						if ( sDirEntry.find( DEVICE_NAME ) != std::string::npos || sDirEntry.find( DEVICE_NAME_ALT ) != std::string::npos )
						{
							arc::gen3::device::ArcDev_t tArcDev;

							tArcDev.sName   = DEVICE_DIR + sDirEntry;
							
							m_vDevList->push_back( tArcDev );
						}
					}

					closedir( pDir );
				}

			#endif

			//
			// Make sure the list isn't empty
			//
			if ( m_vDevList->empty() )
			{
					THROW( "No device bindings exist! Make sure an ARC, Inc PCIe card is installed!" );
			}
		}


		// +----------------------------------------------------------------------------
		// |  deviceCount
		// +----------------------------------------------------------------------------
		// |  Returns the number of items in the gen3 list. Must be called after
		// |  findDevices().
		// +----------------------------------------------------------------------------
		std::uint32_t CArcPCIe::deviceCount( void )
		{
			return static_cast<int>( m_vDevList->size() );
		}


		// +----------------------------------------------------------------------------
		// |  getDeviceStringList
		// +----------------------------------------------------------------------------
		// |  Returns a std::string list representation of the gen3 list. Must be called
		// |  after GetDeviceList().
		// |
		// |  Throws std::runtime_error on error
		// +----------------------------------------------------------------------------
		const std::string* CArcPCIe::getDeviceStringList( void )
		{
			if ( !m_vDevList->empty() )
			{
				m_psDevList.reset( new std::string[ m_vDevList->size() ], &CArcPCIe::ArrayDeleter<std::string> );

				for ( std::string::size_type i = 0; i < m_vDevList->size(); i++ )
				{
					std::ostringstream oss;

					oss << "PCIe Device "
						<< i
					#ifdef _WINDOWS
						<< std::ends;
					#else
						<< m_vDevList->at( i ).sName
						<< std::ends;
					#endif

					( m_psDevList.get() )[ i ] = std::string( oss.str() );
				}
			}

			else
			{
				m_psDevList.reset( new std::string[ 1 ], &CArcPCIe::ArrayDeleter<std::string> );

				( m_psDevList.get() )[ 0 ] = std::string( "No Devices Found!" );
		   }

			return const_cast<const std::string *>( m_psDevList.get() );
		}


		// +----------------------------------------------------------------------------
		// |  isOpen
		// +----------------------------------------------------------------------------
		// |  Returns 'true' if connected to PCIe gen3; 'false' otherwise.
		// |
		// |  Throws NOTHING on error. No error handling.
		// +----------------------------------------------------------------------------
		bool CArcPCIe::isOpen( void )
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
		void CArcPCIe::open( std::uint32_t uiDeviceNumber )
		{
			if ( uiDeviceNumber < 0 || uiDeviceNumber > static_cast<std::uint32_t>( m_vDevList->size() ) )
			{
				THROW( "Invalid device number: %u",	uiDeviceNumber );
			}

			if ( isOpen() )
			{
				THROW( "Device already open, call close() first!" );
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
				THROW( "Failed to open device ( %s ) : %e", sDeviceName.c_str(), arc::gen3::CArcBase::getSystemError() );
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

			//
			//  Clear the status register
			// +-------------------------------------------------------+
			clearStatus();
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
		void CArcPCIe::open( std::uint32_t uiDeviceNumber, std::uint32_t uiBytes )
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
		void CArcPCIe::open( std::uint32_t uiDeviceNumber, std::uint32_t uiRows, std::uint32_t uiCols )
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
		void CArcPCIe::close( void )
		{
			//
			// Prevents access violation from code that follows
			//
			bool bOldStoreCmds = m_bStoreCmds;
			m_bStoreCmds       = false;

			unMapCommonBuffer();

			Arc_CloseHandle( m_hDevice );

			m_uiCCParam   = 0;
			m_hDevice    = INVALID_HANDLE_VALUE;
			m_bStoreCmds = bOldStoreCmds;
		}


		// +----------------------------------------------------------------------------
		// |  reset
		// +----------------------------------------------------------------------------
		// |  Resets the PCIe board.
		// |
		// |  Throws NOTHING on error. No error handling.
		// +----------------------------------------------------------------------------
		void CArcPCIe::reset( void )
		{
			writeBar( arc::gen3::device::ePCIeRegs::DEV_REG_BAR,
					  static_cast<std::uint32_t>( arc::gen3::device::ePCIeRegOffsets::REG_RESET ),
					  1 );

			//
			// Check that the status is now idle
			//
			std::uint32_t uiStatus = readBar( arc::gen3::device::ePCIeRegs::DEV_REG_BAR,
											  static_cast< std::uint32_t >( arc::gen3::device::ePCIeRegOffsets::REG_STATUS ) );

			if ( !PCIe_STATUS_IDLE( uiStatus ) )
			{
				THROW( "Reset failed! Device status not idle: 0x%X", uiStatus );
			}
		}


		// +----------------------------------------------------------------------------
		// |  mapCommonBuffer
		// +----------------------------------------------------------------------------
		// |  Map the gen3 driver image buffer.
		// |
		// |  Throws std::runtime_error on error
		// |
		// |  <IN>  -> bBytes - The number of bytes to map as an image buffer. Not
		// |                    used by PCI/e.
		// +----------------------------------------------------------------------------
		void CArcPCIe::mapCommonBuffer( std::uint32_t uiBytes )
		{
			if ( uiBytes <= 0 )
			{
				THROW( "Invalid buffer size: %u. Must be greater than zero!", uiBytes );		
			}

			std::cout << "MAP bytes -> " << uiBytes << std::endl;

			//  Map the kernel buffer
			// +----------------------------------------------------+
			m_tImgBuffer.pUserAddr = ( std::uint16_t* )Arc_MMap( m_hDevice, ARC_MEM_MAP, size_t( uiBytes ) );

			std::cout << "MAP m_tImgBuffer.pUserAddr -> " << std::hex << ( std::uint64_t )m_tImgBuffer.pUserAddr << std::dec << std::endl;

			if ( m_tImgBuffer.pUserAddr == MAP_FAILED )
			{
				auto iErrorCode = arc::gen3::CArcBase::getSystemError();

				if ( iErrorCode != 0 )
				{
					arc::gen3::CArcBase::zeroMemory( &m_tImgBuffer, sizeof( arc::gen3::device::ImgBuf_t ) );

					THROW( "Failed to map image buffer : %e", arc::gen3::CArcBase::getSystemError() );
				}
			}

			bool bSuccess = getCommonBufferProperties();

			if ( !bSuccess )
			{
				THROW( "Failed to read image buffer size : %e", arc::gen3::CArcBase::getSystemError() );		
			}

			if ( m_tImgBuffer.ulSize < size_t( uiBytes ) )
			{
				std::ostringstream oss;

				oss << "Failed to allocate buffer of the correct size.\nWanted: "
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
		void CArcPCIe::unMapCommonBuffer( void )
		{
			if ( m_tImgBuffer.pUserAddr != ( void * )nullptr )
			{
				Arc_MUnMap( m_hDevice, ARC_MEM_UNMAP, m_tImgBuffer.pUserAddr, static_cast<std::int32_t>( m_tImgBuffer.ulSize ) );
			}

			CArcBase::zeroMemory( &m_tImgBuffer, sizeof( arc::gen3::device::ImgBuf_t ) );
		}


		// +----------------------------------------------------------------------------
		// |  getId
		// +----------------------------------------------------------------------------
		// |  Returns the PCIe board id, which should be 'ARC6'
		// +----------------------------------------------------------------------------
		std::uint32_t CArcPCIe::getId( void )
		{
			return readBar( arc::gen3::device::ePCIeRegs::DEV_REG_BAR,
							static_cast< std::uint32_t >( arc::gen3::device::ePCIeRegOffsets::REG_ID_HI ) );
		}


		// +----------------------------------------------------------------------------
		// |  getStatus
		// +----------------------------------------------------------------------------
		// |  Returns the PCIe status register value.
		// +----------------------------------------------------------------------------
		std::uint32_t CArcPCIe::getStatus( void )
		{
			return readBar( arc::gen3::device::ePCIeRegs::DEV_REG_BAR,
							static_cast< std::uint32_t >( arc::gen3::device::ePCIeRegOffsets::REG_STATUS ) );
		}


		// +----------------------------------------------------------------------------
		// |  clearStatus
		// +----------------------------------------------------------------------------
		// |  Clears the PCIe status register.
		// +----------------------------------------------------------------------------
		void CArcPCIe::clearStatus( void )
		{
			writeBar( arc::gen3::device::ePCIeRegs::DEV_REG_BAR,
					  static_cast< std::uint32_t >( arc::gen3::device::ePCIeRegOffsets::REG_STATUS ),
					  PCIe_STATUS_CLEAR_ALL );
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
		void CArcPCIe::set2xFOTransmitter( bool bOnOff )
		{
			std::uint32_t uiReply = 0;

			if ( bOnOff )
			{
				if ( ( uiReply = command( { TIM_ID, XMT, 1 }  ) ) != DON )
				{
					THROW( "Failed to SET use of 2x fiber optic transmitters on controller, reply: 0x%X", uiReply );
				}

				writeBar( arc::gen3::device::ePCIeRegs::DEV_REG_BAR,
						  static_cast< std::uint32_t >( arc::gen3::device::ePCIeRegOffsets::REG_FIBER_2X_CTRL ),
						  static_cast< std::uint32_t >( arc::gen3::device::eFiber2x::FIBER_2X_ENABLE ) );
			}
			else
			{
				if ( ( uiReply = command( { TIM_ID, XMT, 0 } ) ) != DON )
				{
					THROW( "Failed to CLEAR use of 2x fiber optic transmitters on controller, reply: 0x%X", uiReply );
				}

				writeBar( arc::gen3::device::ePCIeRegs::DEV_REG_BAR,
						  static_cast< std::uint32_t >( arc::gen3::device::ePCIeRegOffsets::REG_FIBER_2X_CTRL ),
						  static_cast<std::uint32_t>( arc::gen3::device::eFiber2x::FIBER_2X_DISABLE ) );
			}
		}


		// +----------------------------------------------------------------------------
		// |  loadDeviceFile
		// +----------------------------------------------------------------------------
		// |  Not used by PCIe.
		// +----------------------------------------------------------------------------
		void CArcPCIe::loadDeviceFile( const std::string& sFile )
		{
			THROW( "Method not available for PCIe!" );
		}


		// +----------------------------------------------------------------------------
		// |  command
		// +----------------------------------------------------------------------------
		// |  Send a command to the controller timing or utility board. Returns the
		// |  controller reply, typically DON.
		// |
		// |  Throws std::runtime_error on error
		// |
		// |  <IN>  -> uiBoardId       - command board id ( TIM or UTIL )
		// |  <IN>  -> dCommand       - Board command
		// |  <IN>  -> dArg1 to dArg4 - command arguments ( optional )
		// +----------------------------------------------------------------------------
		std::uint32_t CArcPCIe::command( const std::initializer_list<std::uint32_t>& tCmdList )
		{
			std::uint32_t uiHeader = 0;
			std::uint32_t uiReply  = 0;

			//
			//  Report error if gen3 reports readout in progress
			// +------------------------------------------------------+
			auto uiValue = readBar( arc::gen3::device::ePCIeRegs::DEV_REG_BAR, static_cast< std::uint32_t >( arc::gen3::device::ePCIeRegOffsets::REG_STATUS ) );

			if ( PCIe_STATUS_READOUT( uiValue ) )
			{
				THROW( "Device reports readout in progress! Status: 0x%X",
						readBar( arc::gen3::device::ePCIeRegs::DEV_REG_BAR,
						static_cast< std::uint32_t >( arc::gen3::device::ePCIeRegOffsets::REG_STATUS ) ) );
			}

			//
			//  Clear Status Register
			// +-------------------------------------------------+
			clearStatus();

			try
			{
				std::uint32_t uiOffset = static_cast< std::uint32_t >( arc::gen3::device::ePCIeRegOffsets::REG_CMD_HEADER );

				for ( auto it = tCmdList.begin(); it != tCmdList.end(); it++ )
				{
					if ( it == tCmdList.begin() )
					{
						uiHeader = static_cast<std::uint32_t>( ( *it << 8 ) | tCmdList.size() );

						VERIFY_24BITS( "CArcPCIe", "Command", uiHeader )

						writeBar( arc::gen3::device::ePCIeRegs::DEV_REG_BAR,
								  uiOffset,
								  ( 0xAC000000 | uiHeader ) );
					}

					else
					{
						VERIFY_24BITS( "CArcPCIe", "Command", *it )

						writeBar( arc::gen3::device::ePCIeRegs::DEV_REG_BAR,
								  uiOffset,
								  ( 0xAC000000 | *it ) );
					}

					uiOffset += 4;
				}
			}
			catch ( ... )
			{
				if ( m_bStoreCmds )
				{
					m_pCLog->put( CArcBase::iterToString( tCmdList.begin(), tCmdList.end() ).c_str() );
				}

				throw;
			}

			//
			//  Return the reply
			// +-------------------------------------------------+
			try
			{
				uiReply = readReply();
			}
			catch ( const std::exception& e )
			{
				if ( m_bStoreCmds )
				{
					m_pCLog->put( CArcBase::iterToString( tCmdList.begin(), tCmdList.end() ).c_str() );
				}

				std::ostringstream oss;

				oss << e.what() << std::endl
					<< "Exception Details: 0x"
					<< std::hex << uiHeader << std::dec << " "
					<< CArcBase::iterToString( ( tCmdList.begin() + 1 ), tCmdList.end() )
					<< std::dec << std::endl;

				THROW( oss.str() );
			}

			//
			// Set the debug message queue.
			//
			if ( m_bStoreCmds )
			{
				m_pCLog->put( ( CArcBase::iterToString( tCmdList.begin(), tCmdList.end() ) + CArcBase::formatString( " -> 0x%X", uiReply ) ).c_str() );
			}

			if ( uiReply == CNR )
			{
				THROW( "Controller not ready! Verify controller has been setup! Reply: 0x%X", uiReply );
			}

			return uiReply;
		}


		// +----------------------------------------------------------------------------
		// |  getControllerId
		// +----------------------------------------------------------------------------
		// |  Returns the controller ID or 'ERR' if none.
		// |
		// |  Throws std::runtime_error on error
		// +----------------------------------------------------------------------------
		std::uint32_t CArcPCIe::getControllerId( void )
		{
			std::uint32_t uiReply = 0;

			//
			//  Clear status register
			// +-------------------------------------------------+
			clearStatus();

			//
			//  Get the controller ID
			// +-------------------------------------------------+
			writeBar( arc::gen3::device::ePCIeRegs::DEV_REG_BAR,
					  static_cast< std::uint32_t >( arc::gen3::device::ePCIeRegOffsets::REG_CTLR_SPECIAL_CMD ),
					  static_cast< std::uint32_t >( arc::gen3::device::eRegCmds::CONTROLLER_GET_ID ) );

			//
			//  Read the reply
			//
			//  NOTE: Ignore any error, as Gen III will return
			//  a timeout, which will indicate a Gen III system.
			// +-------------------------------------------------+
			try
			{
				uiReply = readReply( 0.5 );
			}
			catch ( ... )
			{
				uiReply = 0;
			}

			return uiReply;
		}


		// +----------------------------------------------------------------------------
		// |  resetController
		// +----------------------------------------------------------------------------
		// |  Resets the controller.
		// |
		// |  Throws std::runtime_error on error
		// +----------------------------------------------------------------------------
		void CArcPCIe::resetController( void )
		{
			//
			//  Clear status register
			// +-------------------------------------------------+
			clearStatus();

			//
			//  reset the controller or DSP
			// +-------------------------------------------------+
			writeBar( arc::gen3::device::ePCIeRegs::DEV_REG_BAR,
					  static_cast< std::uint32_t >( arc::gen3::device::ePCIeRegOffsets::REG_CTLR_SPECIAL_CMD ),
					  static_cast< std::uint32_t >( arc::gen3::device::eRegCmds::CONTROLLER_RESET ) );

			//
			//  Read the reply
			// +-------------------------------------------------+
			auto uiReply = readReply();

			if ( uiReply != SYR )
			{
				THROW( "Failed to reset controller, reply: 0x%X", uiReply );
			}
		}


		// +----------------------------------------------------------------------------
		// | isControllerConnected
		// +----------------------------------------------------------------------------
		// |  Returns 'true' if a controller is connected to the PCIe board.  This is
		// |  for fiber optic A only.
		// |
		// |  Throws std::runtime_error on error
		// +----------------------------------------------------------------------------
		bool CArcPCIe::isControllerConnected( void )
		{
			return isFiberConnected();
		}


		// +----------------------------------------------------------------------------
		// | isFiberConnected
		// +----------------------------------------------------------------------------
		// |  Returns 'true' if the specified  PCIe fiber optic is connected to a
		// |  powered-on controller.
		// |
		// |  <IN> -> eFiberId - The ID of the fiber to check: CArcPCIe::FIBER_A or CArcPCIe::FIBER_B. Default: FIBER_A
		// |
		// |  Throws std::runtime_error on invalid parameter
		// +----------------------------------------------------------------------------
		bool CArcPCIe::isFiberConnected( arc::gen3::device::eFiber eFiberId )
		{
			bool bConnected = false;

			if ( eFiberId == arc::gen3::device::eFiber::FIBER_A )
			{
				bConnected = PCIe_STATUS_FIBER_A_CONNECTED( getStatus() );
			}

			else if ( eFiberId == arc::gen3::device::eFiber::FIBER_B )
			{
				bConnected = PCIe_STATUS_FIBER_B_CONNECTED( getStatus() );
			}

			else
			{
				THROW( "Invalid fiber id: %u", static_cast<std::uint32_t>( eFiberId ) );
			}

			return bConnected;
		}


		// +----------------------------------------------------------------------------
		// |  stopExposure
		// +----------------------------------------------------------------------------
		// |  Stops the current exposure.
		// |
		// |  NOTE: The command is sent manually and NOT using the command() method
		// |        because the command() method checks for readout and fails.
		// |
		// |  Throws std::runtime_error on error
		// +----------------------------------------------------------------------------
		void CArcPCIe::stopExposure( void )
		{
			//
			//  Send Header
			// +-------------------------------------------------+
			std::uint32_t uiHeader = ( ( TIM_ID << 8 ) | 2 );

			writeBar( arc::gen3::device::ePCIeRegs::DEV_REG_BAR,
					  static_cast< std::uint32_t >( arc::gen3::device::ePCIeRegOffsets::REG_CMD_HEADER ),
					  ( 0xAC000000 | uiHeader ) );

			//
			//  Send command
			// +-------------------------------------------------+
			writeBar( arc::gen3::device::ePCIeRegs::DEV_REG_BAR,
					  static_cast< std::uint32_t >( arc::gen3::device::ePCIeRegOffsets::REG_CMD_COMMAND ),
					  ( 0xAC000000 | ABR ) );

			//
			//  Read the reply
			// +-------------------------------------------------+
			std::uint32_t uiReply = readReply();

			if ( uiReply != DON )
			{
				THROW( "Failed to stop exposure/readout, reply: 0x%X", uiReply );
			}
		}


		// +----------------------------------------------------------------------------
		// |  isReadout
		// +----------------------------------------------------------------------------
		// |  Returns 'true' if the controller is in readout.
		// |
		// |  Throws std::runtime_error on error
		// +----------------------------------------------------------------------------
		bool CArcPCIe::isReadout( void )
		{
			return ( ( ( getStatus() & 0x4 ) > 0 ) ? true : false );
		}


		// +----------------------------------------------------------------------------
		// |  getPixelCount
		// +----------------------------------------------------------------------------
		// |  Returns the current pixel count.
		// |
		// |  Throws std::runtime_error on error
		// +----------------------------------------------------------------------------
		std::uint32_t CArcPCIe::getPixelCount( void )
		{
			std::uint32_t uiPixCnt = readBar( arc::gen3::device::ePCIeRegs::DEV_REG_BAR,
											  static_cast< std::uint32_t >( arc::gen3::device::ePCIeRegOffsets::REG_PIXEL_COUNT ) );

			if ( m_bStoreCmds )
			{
				m_pCLog->put( CArcBase::formatString( "[ PIXEL COUNT REG: 0x%X -> %u ]",
							  static_cast<std::uint32_t>( arc::gen3::device::ePCIeRegOffsets::REG_PIXEL_COUNT ),
							  uiPixCnt ).c_str() );
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
		// |  Throws std::runtime_error on error
		// +----------------------------------------------------------------------------
		std::uint32_t CArcPCIe::getCRPixelCount( void )
		{
			THROW( "Method not supported by PCIe!" );

			return 0;
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
		std::uint32_t CArcPCIe::getFrameCount( void )
		{
			std::uint32_t uiFrameCnt = readBar( arc::gen3::device::ePCIeRegs::DEV_REG_BAR,
											    static_cast< std::uint32_t >( arc::gen3::device::ePCIeRegOffsets::REG_FRAME_COUNT ) );

			if ( m_bStoreCmds )
			{
				m_pCLog->put( CArcBase::formatString( "[ FRAME COUNT REG: 0x%X -> %u ]",
							  static_cast<std::uint32_t>( arc::gen3::device::ePCIeRegOffsets::REG_FRAME_COUNT ),
							  uiFrameCnt ).c_str() );
			}

			return uiFrameCnt;
		}


		// +----------------------------------------------------------------------------
		// |  writeBar
		// +----------------------------------------------------------------------------
		// |  Writes a value to the specified PCI/e BAR offset using mapped registers.
		// |  Automatically calls WriteBarDirect() if no mappings exist.
		// |
		// |  Throws std::runtime_error on error
		// |
		// |  <IN> -> eBar    - The PCI BAR number ( 0 - 5 ).
		// |  <IN> -> bOffset - The offset into the specified BAR.
		// |  <IN> -> uiValue  - 32-bit value to write.
		// +----------------------------------------------------------------------------
		void CArcPCIe::writeBar( arc::gen3::device::ePCIeRegs eBar, std::uint32_t uiOffset, std::uint32_t uiValue )
		{
			if ( !isOpen() )
			{
				THROW_NO_DEVICE_ERROR();
			}

			std::array<std::uint32_t,3> tArgs = { static_cast<std::uint32_t>( eBar ), static_cast<std::uint32_t>( uiOffset ), uiValue };

			if ( static_cast< std::uint32_t >( eBar ) < ARC_MIN_BAR || static_cast< std::uint32_t >( eBar ) > ARC_MAX_BAR )
			{
				THROW( "Invalid BAR number: 0x%X", eBar );
			}

			auto iSuccess = Arc_IOCtl( m_hDevice, ARC_WRITE_BAR, tArgs.data(), ( tArgs.size() * sizeof( std::uint32_t ) ) );

			if ( !iSuccess )
			{
				THROW( "Writing 0x%X to 0x%X : 0x%X failed! %e", uiValue, eBar, uiOffset, arc::gen3::CArcBase::getSystemError() );
			}
		}


		// +----------------------------------------------------------------------------
		// |  readBar
		// +----------------------------------------------------------------------------
		// |  Read a value from the specified PCI/e BAR offset using mapped registers.
		// |  Automatically calls ReadBarDirect() if no mappings exist.
		// |
		// |  Throws std::runtime_error on error
		// |
		// |  <IN> -> eBar    - The PCI BAR number ( 0 - 5 ).
		// |  <IN> -> bOffset - The offset into the specified BAR.
		// +----------------------------------------------------------------------------
		std::uint32_t CArcPCIe::readBar( arc::gen3::device::ePCIeRegs eBar, std::uint32_t uiOffset )
		{
			if ( !isOpen() )
			{
				THROW_NO_DEVICE_ERROR();
			}

			std::array<std::uint32_t,2> tIn = { static_cast< std::uint32_t >( eBar ), static_cast< std::uint32_t >( uiOffset ) };

			if ( static_cast< std::uint32_t >( eBar ) < ARC_MIN_BAR || static_cast< std::uint32_t >( eBar ) > ARC_MAX_BAR )
			{
				THROW( "Invalid BAR number: 0x%X", eBar );
			}

			auto iSuccess = Arc_IOCtl( m_hDevice, ARC_READ_BAR, tIn.data(), ( tIn.size() * sizeof( std::uint32_t ) ) );

			if ( !iSuccess )
			{
				THROW( "Reading 0x%X : 0x%X failed! %e", eBar, uiOffset, arc::gen3::CArcBase::getSystemError() );
			}

			return tIn[ 0 ];
		}


		// +----------------------------------------------------------------------------
		// |  getCommonBufferProperties
		// +----------------------------------------------------------------------------
		// |  Fills in the image buffer structure with its properties, such as
		// |  physical address and size.
		// |
		// |  Throws NOTHING on error. No error handling.
		// +----------------------------------------------------------------------------
		bool CArcPCIe::getCommonBufferProperties( void )
		{
			std::uint64_t uiProps[] = { 0, 0 };

			if ( !isOpen() )
			{
				THROW_NO_DEVICE_ERROR();
			}

			auto iSuccess = Arc_IOCtl( m_hDevice, ARC_BUFFER_PROP, uiProps, ( sizeof( std::uint64_t ) * ( sizeof( uiProps ) / sizeof( std::uint64_t ) ) ) );

			if ( !iSuccess )
			{
				return false;
			}

			m_tImgBuffer.ulPhysicalAddr = uiProps[ 0 ];
			m_tImgBuffer.ulSize         = static_cast<size_t>( uiProps[ 1 ] );

			return true;
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
		void CArcPCIe::loadGen23ControllerFile( const std::string& sFilename, bool bValidate, const bool& bAbort )
		{
			std::uint32_t	uiBoardId	= 0;
			std::uint32_t	uiType		= 0;
			std::uint32_t	uiAddr		= 0;
			std::uint32_t	uiData		= 0;
			std::uint32_t	uiReply		= 0;
			char			typeChar	= ' ';
			bool			bIsCLodFile	= false;

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
				THROW( "Cannot open file: %s", sFilename.c_str() );
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
				THROW( "Invalid file. Missing 'TIMBOOT/CRT' or 'UTILBOOT' std::string." );
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
				THROW( "Stop ('STP') controller failed. Reply: 0x%X", uiReply );
			}

			if ( bAbort ) { inFile.close(); return; }

			//
			// Set the PCI status bit #1 (X:0 bit 1 = 1).
			// -------------------------------------------
			// Not Used by PCIe

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
					auto pTokens = CArcBase::splitString( sLine );

					// Dump _DATA std::string

					//
					// Get the memory type and start address
					// ---------------------------------------------
					typeChar = pTokens->at( 1 ).at( 0 );
					uiAddr   = static_cast<std::uint32_t>( std::stol( pTokens->at( 2 ), 0, 16 ) );

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

							auto pDataTokens = CArcBase::splitString( sLine );

							for ( auto it = pDataTokens->begin(); it != pDataTokens->end(); it++ )
							{
								if ( bAbort ) { inFile.close(); return; }

								uiData = static_cast<std::uint32_t>( std::stol( *it, 0, 16 ) );

								//
								// Write the data to the controller.
								// --------------------------------------------------------------
								uiReply = command( { uiBoardId, WRM, ( uiType | uiAddr ), uiData } );

								if ( uiReply != DON )
								{
									THROW( "Write ('WRM') to controller %s board failed. WRM 0x%X 0x%X -> 0x%X",
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
										THROW( "Write ('WRM') to controller %s board failed. RDM 0x%X -> 0x%X [ Expected: 0x%X ]",
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
			// Not Used with PCIe


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
					THROW( "Jump from boot code failed. Reply: 0x%X", uiReply );
				}
			}
		}


		// +----------------------------------------------------------------------------
		// |  getContinuousImageSize
		// +----------------------------------------------------------------------------
		// |  Returns the boundary adjusted image size for continuous readout.  The PCIe
		// |  card ( ARC-66/67 ) requires no boundary adjustments and writes data
		// |  continuously.
		// |
		// |  Throws std::runtime_error on error
		// |
		// |  <IN>  -> uiImageSize - The boundary adjusted image size ( in bytes ).
		// +----------------------------------------------------------------------------
		std::uint32_t CArcPCIe::getContinuousImageSize( std::uint32_t uiImageSize )
		{
			return uiImageSize;
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
		std::uint32_t CArcPCIe::smallCamDLoad( std::uint32_t uiBoardId, std::vector<std::uint32_t>* pvData )
		{
			std::uint32_t uiHeader = 0;
			std::uint32_t uiReply  = 0;

			//
			//  Report error if gen3 reports readout in progress
			// +------------------------------------------------------+
			auto uiValue = readBar( arc::gen3::device::ePCIeRegs::DEV_REG_BAR, static_cast< std::uint32_t >( arc::gen3::device::ePCIeRegOffsets::REG_STATUS ) );

			if ( PCIe_STATUS_READOUT( uiValue ) )
			{
				THROW( "Device reports readout in progress! Status: 0x%X",
						readBar( arc::gen3::device::ePCIeRegs::DEV_REG_BAR,
								 static_cast< std::uint32_t >( arc::gen3::device::ePCIeRegOffsets::REG_STATUS ) ) );
			}

			//
			//  Verify the size of the data, cannot be greater than 6
			// +------------------------------------------------------+
			if ( pvData->size() > 6 )
			{
				THROW( "Data vector too large: 0x%X! Must be less than 6!", pvData->size() );
			}

			//
			//  Verify the board id equals smallcam download id
			// +------------------------------------------------------+
			if ( uiBoardId != SMALLCAM_DLOAD_ID )
			{
				THROW( "Invalid board id: %u! Must be: %u",	uiBoardId, SMALLCAM_DLOAD_ID );
			}

			//
			//  Clear Status Register
			// +-------------------------------------------------+
			clearStatus();

			try
			{
				//
				//  Send Header
				// +-------------------------------------------------+
				uiHeader = ( ( uiBoardId << 8 ) | static_cast<int>( pvData->size() + 1 ) );

				VERIFY_24BITS( "CArcPCIe", "SmallCamDLoad", uiHeader )

				writeBar( arc::gen3::device::ePCIeRegs::DEV_REG_BAR,
						  static_cast< std::uint32_t >( arc::gen3::device::ePCIeRegOffsets::REG_CMD_HEADER ),
						  ( 0xAC000000 | uiHeader ) );

				//
				//  Send the data
				// +-------------------------------------------------+
				for ( std::vector<std::uint32_t>::size_type i = 0; i < pvData->size(); i++ )
				{
					VERIFY_24BITS( "CArcPCIe", "SmallCamDLoad", pvData->at( i ) )

					auto uiOffset = static_cast< std::uint32_t >( arc::gen3::device::ePCIeRegOffsets::REG_CMD_COMMAND ) + 
									( static_cast< std::uint32_t >( i ) * 4 );

					writeBar( arc::gen3::device::ePCIeRegs::DEV_REG_BAR, uiOffset, ( 0xAC000000 | pvData->at( i ) ) );
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
				uiReply = readReply();
			}
			catch ( const std::exception& e )
			{
				if ( m_bStoreCmds )
				{
					m_pCLog->put( formatDLoadString( uiReply, uiBoardId, pvData ).c_str() );
				}

				std::ostringstream oss;

				oss << e.what() << std::endl << "Exception Details: 0x" << std::hex << uiHeader << std::dec;

				for ( std::vector<std::uint32_t>::size_type i = 0; i < pvData->size(); i++ )
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
		// |  Sets the hardware byte-swapping if system architecture is solaris.
		// |  Otherwise, does nothing; compiles to empty function.
		// |
		// |  Throws std::runtime_error on error
		// +----------------------------------------------------------------------------
		void CArcPCIe::setByteSwapping( void )
		{
			// Not Used !!!!
		}

		// +----------------------------------------------------------------------------
		// |  readReply
		// +----------------------------------------------------------------------------
		// |  Reads the reply register value. This method will time-out if the
		// |  specified number of seconds passes before the reply received register
		// |  bit or an error bit ( PCIe time-out, header error, controller reset ) is
		// |  set.
		// |
		// |  Throws std::runtime_error on error
		// |
		// |  <IN> -> gTimeOutSecs - Seconds to wait before time-out occurs.
		// +----------------------------------------------------------------------------
		std::uint32_t CArcPCIe::readReply( double gTimeOutSecs )
		{
			std::uint32_t   uiStatus  = 0;
			std::uint32_t   uiReply   = 0;
			double			gDiffTime = 0.0;
			time_t			tStart    = time( nullptr );

			do
			{
				uiStatus = getStatus();

				if ( PCIe_STATUS_HDR_ERROR( uiStatus ) )
				{
					uiReply = HERR;

					break;
				}

				else if( PCIe_STATUS_CONTROLLER_RESET( uiStatus ) )
				{
					uiReply = SYR;
	
					break;
				}

				if ( ( gDiffTime = difftime( time( nullptr ), tStart ) ) > gTimeOutSecs )
				{
					THROW( "Time Out [ %f sec ] while waiting for status [ 0x%X ]!", gDiffTime, uiStatus );
				}

			} while ( !PCIe_STATUS_REPLY_RECVD( uiStatus ) );


			if ( uiReply != HERR && uiReply != SYR )
			{
				uiReply = readBar( arc::gen3::device::ePCIeRegs::DEV_REG_BAR,
								   static_cast< std::uint32_t >( arc::gen3::device::ePCIeRegOffsets::REG_CMD_REPLY ) );
			}

			return uiReply;
		}


		// +----------------------------------------------------------------------------
		// |  getCfgSpByte
		// +----------------------------------------------------------------------------
		// |  Returns the specified BYTE from the specified PCI configuration space
		// |  register.
		// |
		// |  <IN> -> uiOffset - The byte offset from the start of PCI config space
		// |
		// |  Throws std::runtime_error on error
		// +----------------------------------------------------------------------------
		std::uint32_t CArcPCIe::getCfgSpByte( std::uint32_t uiOffset )
		{
			std::uint32_t uiRegValue = uiOffset;

			if ( !isOpen() )
			{
				THROW_NO_DEVICE_ERROR();
			}

			auto iSuccess = Arc_IOCtl( m_hDevice, ARC_READ_CFG_8, &uiRegValue, sizeof( uiRegValue ) );

			if ( !iSuccess )
			{
				THROW( "Reading configuration BYTE offset 0x%X failed : %e", uiOffset, arc::gen3::CArcBase::getSystemError() );
			}

			return uiRegValue;
		}


		// +----------------------------------------------------------------------------
		// |  getCfgSpWord
		// +----------------------------------------------------------------------------
		// |  Returns the specified WORD from the specified PCI configuration space
		// |  register.
		// |
		// |  <IN> -> uiOffset - The byte offset from the start of PCI config space
		// |
		// |  Throws std::runtime_error on error
		// +----------------------------------------------------------------------------
		std::uint32_t CArcPCIe::getCfgSpWord( std::uint32_t uiOffset )
		{
			std::uint32_t uiRegValue = uiOffset;

			if ( !isOpen() )
			{
				THROW_NO_DEVICE_ERROR();
			}

			auto iSuccess = Arc_IOCtl( m_hDevice, ARC_READ_CFG_16, &uiRegValue, sizeof( uiRegValue ) );

			if ( !iSuccess )
			{
				THROW( "Reading configuration WORD offset 0x%X failed : %e", uiOffset, arc::gen3::CArcBase::getSystemError() );
			}

			return uiRegValue;
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
		std::uint32_t CArcPCIe::getCfgSpDWord( std::uint32_t uiOffset )
		{
			std::uint32_t uiRegValue = uiOffset;

			if ( !isOpen() )
			{
				THROW_NO_DEVICE_ERROR();
			}

			auto iSuccess = Arc_IOCtl( m_hDevice, ARC_READ_CFG_32, &uiRegValue, sizeof( uiRegValue ) );

			if ( !iSuccess )
			{
				THROW( "Reading configuration DWORD offset 0x%X failed : %e", uiOffset, arc::gen3::CArcBase::getSystemError() );
			}

			return uiRegValue;
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
		void CArcPCIe::setCfgSpByte( std::uint32_t uiOffset, std::uint32_t uiValue )
		{
			std::uint32_t uiArgs[] = { uiOffset, uiValue };

			if ( !isOpen() )
			{
				THROW_NO_DEVICE_ERROR();
			}

			auto iSuccess = Arc_IOCtl( m_hDevice, ARC_WRITE_CFG_8, uiArgs, sizeof( uiArgs ) );

			if ( !iSuccess )
			{
				THROW( "Writing configuration BYTE 0x%X to offset 0x%X failed : %e", uiValue, uiOffset, arc::gen3::CArcBase::getSystemError() );
			}
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
		void CArcPCIe::setCfgSpWord( std::uint32_t uiOffset, std::uint32_t uiValue )
		{
			std::uint32_t uiArgs[] = { uiOffset, uiValue };

			if ( !isOpen() )
			{
				THROW_NO_DEVICE_ERROR();
			}

			auto iSuccess = Arc_IOCtl( m_hDevice, ARC_WRITE_CFG_16, uiArgs, sizeof( uiArgs ) );

			if ( !iSuccess )
			{
				THROW( "Writing configuration WORD 0x%X to offset 0x%X failed : %e", uiValue, uiOffset, arc::gen3::CArcBase::getSystemError() );
			}
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
		void CArcPCIe::setCfgSpDWord( std::uint32_t uiOffset, std::uint32_t uiValue )
		{
			std::uint32_t uiArgs[] = { uiOffset, uiValue };

			if ( !isOpen() )
			{
				THROW_NO_DEVICE_ERROR();
			}

			auto iSuccess = Arc_IOCtl( m_hDevice, ARC_WRITE_CFG_32, uiArgs, sizeof( uiArgs ) );

			if ( !iSuccess )
			{
				THROW( "Writing configuration DWORD 0x%X to offset 0x%X failed : %e", uiValue, uiOffset, arc::gen3::CArcBase::getSystemError() );
			}
		}


		// +----------------------------------------------------------------------------
		// |  getCfgSp
		// +----------------------------------------------------------------------------
		// |  Reads and parses the entire PCIe configuration space header into readable
		// |  text and bit definitions that are stored in a member list variable. The
		// |  public methods of this class allow access to this list. This method will
		// |  create the member list if it doesn't already exist and clears it if it
		// |  does.
		// +----------------------------------------------------------------------------
		void CArcPCIe::getCfgSp( void )
		{
			std::uint32_t uiRegValue = 0;
			std::uint32_t uiRegAddr  = 0;

			if ( !isOpen() )
			{
				THROW_NO_DEVICE_ERROR();
			}

			CArcPCIBase::getCfgSp();

			uiRegAddr  = PCI9056_PM_CAP_ID;
			uiRegValue = getCfgSpDWord( uiRegAddr );
			addRegItem( m_pCfgSpList.get(),
						uiRegAddr,
						"Power Management Capability / Next Item Ptr / Capability ID",
						uiRegValue );

			uiRegAddr  = PCI9056_PM_CSR;
			uiRegValue = getCfgSpDWord( uiRegAddr );
			addRegItem( m_pCfgSpList.get(),
						uiRegAddr,
						"PM Cap: PM Data / Bridge Ext / PM Control & Status",
						uiRegValue );

			uiRegAddr  = PCI9056_HS_CAP_ID;
			uiRegValue = getCfgSpDWord( uiRegAddr );
			addRegItem( m_pCfgSpList.get(),
						uiRegAddr,
						"Hot Swap Capability / Next Item Pointer / Capability ID",
						uiRegValue );

			uiRegAddr  = PCI9056_VPD_CAP_ID;
			uiRegValue = getCfgSpDWord( uiRegAddr );
			addRegItem( m_pCfgSpList.get(),
						uiRegAddr,
						"VPD Capability / VPD Address / Next Item Ptr / Capability ID",
						uiRegValue );

			uiRegAddr  = PCI9056_VPD_DATA;
			uiRegValue = getCfgSpDWord( uiRegAddr );
			addRegItem( m_pCfgSpList.get(),
						uiRegAddr,
						"VPD Data",
						uiRegValue );
		}


		// +----------------------------------------------------------------------------
		// |  getBarSp
		// +----------------------------------------------------------------------------
		// |  Reads and parses the entire PCIe Base Address Registers (BAR) into
		// |  readable text and bit definitions that are stored in a member list
		// |  variable. The public methods of this class allow access to this list.
		// |  This method will create the member list if it doesn't already exist and
		// |  clears it if it does. NOTE: Not all BARS or PCI boards have data.
		// +----------------------------------------------------------------------------
		void CArcPCIe::getBarSp( void )
		{
			if ( !isOpen() )
			{
				THROW_NO_DEVICE_ERROR();
			}

			CArcPCIBase::getBarSp();

			//
			//  Access the register data
			// +-------------------------------------------------------+
			getLocalConfiguration();
		}


		// +----------------------------------------------------------------------------
		// |  getLocalConfiguration
		// +----------------------------------------------------------------------------
		// |  Reads and parses the entire PLX PCIe local registers located within BAR0.
		// +----------------------------------------------------------------------------
		void CArcPCIe::getLocalConfiguration( void )
		{
			std::uint32_t uiRegValue = 0;

			if ( m_pBarList == nullptr )
			{
				THROW( "Unable to read PCI/e base address register!" );
			}

			//
			// Get and Add PLX Local Configuration Registers
			//
			std::unique_ptr<PCIRegList> pList( new PCIRegList() );

			for ( auto i = 0; i < static_cast<int>( sizeof( LCRMap ) / sizeof( PLXRegItem ) ); i++ )
			{
				uiRegValue = readBar( arc::gen3::device::ePCIeRegs::LCL_CFG_BAR, LCRMap[ i ].uiAddr );

				addRegItem( pList.get(),
							LCRMap[ i ].uiAddr,
							LCRMap[ i ].sText.c_str(),
							uiRegValue );
			}

			addBarItem( LCRMapName, pList.release() );

			//
			// Get and Add PLX Runtime Registers
			//
			pList.reset( new PCIRegList() );

			for ( auto i = 0; i < static_cast<int>( sizeof( RTRMap ) / sizeof( PLXRegItem ) ); i++ )
			{
				uiRegValue = readBar( arc::gen3::device::ePCIeRegs::LCL_CFG_BAR, RTRMap[ i ].uiAddr );

				if ( RTRMap[ i ].uiAddr == PCI9056_PERM_VENDOR_ID )
				{
					addRegItem( pList.get(),
								RTRMap[ i ].uiAddr,
								RTRMap[ i ].sText.c_str(),
								uiRegValue,
								getDevVenBitList( uiRegValue ).release() );
				}

				else
				{
					addRegItem( pList.get(),
								RTRMap[ i ].uiAddr,
								RTRMap[ i ].sText.c_str(),
								uiRegValue );
				}
			}

			addBarItem( RTRMapName, pList.release() );

			//
			// Get and Add PLX DMA Registers
			//
			pList.reset( new PCIRegList() );

			for ( int i=0; i<int( sizeof( DMAMap ) / sizeof( PLXRegItem ) ); i++ )
			{
				uiRegValue = readBar( arc::gen3::device::ePCIeRegs::LCL_CFG_BAR, DMAMap[ i ].uiAddr );

				addRegItem( pList.get(),
							DMAMap[ i ].uiAddr,
							DMAMap[ i ].sText.c_str(),
							uiRegValue );
			}

			addBarItem( DMAMapName, pList.release() );

			//
			// Get and Add PLX Messaging Queue Registers
			//
			pList.reset( new PCIRegList() );

			for ( auto i = 0; i < static_cast<int>( sizeof( MSQMap ) / sizeof( PLXRegItem ) ); i++ )
			{
				uiRegValue = readBar( arc::gen3::device::ePCIeRegs::LCL_CFG_BAR, MSQMap[ i ].uiAddr );

				addRegItem( pList.get(),
							MSQMap[ i ].uiAddr,
							MSQMap[ i ].sText.c_str(),
							uiRegValue );
			}

			addBarItem( MSQMapName, pList.release() );

			return;
		}

	}	// end gen3 namespace 
}	// end arc namespace