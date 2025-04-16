#ifndef _CARC_PCI_H_
#define _CARC_PCI_H_

#ifdef _WINDOWS
#pragma warning( disable: 4251 )
#endif

#include <vector>
#include <string>
#include <memory>

#include <CArcDeviceDllMain.h>
#include <CArcPCIBase.h>
#include <CArcStringList.h>




namespace arc
{
	namespace gen3
	{

		class GEN3_CARCDEVICE_API CArcPCI : public CArcPCIBase
		{
			public:
				 CArcPCI( void );
				~CArcPCI( void );

				const std::string toString( void );

				//  CArcPCIBase methods
				// +-------------------------------------------------+
				std::uint32_t  getCfgSpByte( std::uint32_t uiOffset );
				std::uint32_t  getCfgSpWord( std::uint32_t uiOffset );
				std::uint32_t  getCfgSpDWord( std::uint32_t uiOffset );

				void setCfgSpByte( std::uint32_t uiOffset, std::uint32_t uiValue );
				void setCfgSpWord( std::uint32_t uiOffset, std::uint32_t uiValue );
				void setCfgSpDWord( std::uint32_t uiOffset, std::uint32_t uiValue );

				void getCfgSp( void );
				void getBarSp( void );


				//  Device access
				// +-------------------------------------------------+
				static void findDevices( void );
				static std::uint32_t  deviceCount( void );
				static const std::string* getDeviceStringList( void );

				bool isOpen( void );
				void open( std::uint32_t uiDeviceNumber = 0 );
				void open( std::uint32_t uiDeviceNumber, std::uint32_t uiBytes );
				void open( std::uint32_t uiDeviceNumber, std::uint32_t uiRows, std::uint32_t uiCols );
				void close( void );
				void reset( void );

				bool getCommonBufferProperties( void );
				void mapCommonBuffer( std::uint32_t uiBytes = 0 );
				void unMapCommonBuffer( void );

				std::uint32_t getId( void );
				std::uint32_t getStatus( void );
				void clearStatus( void );

				void set2xFOTransmitter( bool bOnOff );
				void loadDeviceFile( const std::string& sFilename );


				//  Setup & General commands
				// +-------------------------------------------------+
				std::uint32_t command( const std::initializer_list<std::uint32_t>& tCmdList );

				std::uint32_t getControllerId( void );
				void resetController( void );
				bool isControllerConnected( void );


				//  expose commands
				// +-------------------------------------------------+
				void stopExposure( void );
				bool isReadout( void );
				std::uint32_t getPixelCount( void );
				std::uint32_t getCRPixelCount( void );
				std::uint32_t getFrameCount( void );


				//  PCI only commands
				// +-------------------------------------------------+
				void setHCTR( std::uint32_t uiVal );
				std::uint32_t getHSTR( void );
				std::uint32_t getHCTR( void );

				std::uint32_t PCICommand( std::uint32_t uiCommand );
				std::uint64_t ioctlDevice64( std::uint32_t uiIoctlCmd, std::uint32_t uiArg = CArcDevice::NOPARAM );
				std::uint32_t ioctlDevice( std::uint32_t uiIoctlCmd, std::uint32_t uiArg = CArcDevice::NOPARAM );
				std::uint32_t ioctlDevice( std::uint32_t uiIoctlCmd, const std::initializer_list<std::uint32_t>& tArgList );


				//  Driver ioctl commands
				// +------------------------------------------------------------------------------
				static const std::uint32_t ASTROPCI_GET_HCTR			= 0x01;
				static const std::uint32_t ASTROPCI_GET_PROGRESS		= 0x02;
				static const std::uint32_t ASTROPCI_GET_DMA_ADDR		= 0x03;
				static const std::uint32_t ASTROPCI_GET_HSTR			= 0x04;
				static const std::uint32_t ASTROPCI_MEM_MAP				= 0x05;
				static const std::uint32_t ASTROPCI_GET_DMA_SIZE		= 0x06;
				static const std::uint32_t ASTROPCI_GET_FRAMES_READ		= 0x07;
				static const std::uint32_t ASTROPCI_HCVR_DATA			= 0x10;
				static const std::uint32_t ASTROPCI_SET_HCTR			= 0x11;
				static const std::uint32_t ASTROPCI_SET_HCVR			= 0x12;
				static const std::uint32_t ASTROPCI_PCI_DOWNLOAD		= 0x13;
				static const std::uint32_t ASTROPCI_PCI_DOWNLOAD_WAIT	= 0x14;
				static const std::uint32_t ASTROPCI_COMMAND				= 0x15;
				static const std::uint32_t ASTROPCI_MEM_UNMAP			= 0x16;
				static const std::uint32_t ASTROPCI_ABORT				= 0x17;
				static const std::uint32_t ASTROPCI_CONTROLLER_DOWNLOAD	= 0x19;
				static const std::uint32_t ASTROPCI_GET_CR_PROGRESS		= 0x20;
				static const std::uint32_t ASTROPCI_GET_DMA_LO_ADDR		= 0x21;
				static const std::uint32_t ASTROPCI_GET_DMA_HI_ADDR		= 0x22;
				static const std::uint32_t ASTROPCI_GET_CONFIG_BYTE		= 0x30;
				static const std::uint32_t ASTROPCI_GET_CONFIG_WORD		= 0x31;
				static const std::uint32_t ASTROPCI_GET_CONFIG_DWORD	= 0x32;
				static const std::uint32_t ASTROPCI_SET_CONFIG_BYTE		= 0x33;
				static const std::uint32_t ASTROPCI_SET_CONFIG_WORD		= 0x34;
				static const std::uint32_t ASTROPCI_SET_CONFIG_DWORD	= 0x35;


				//  Status register ( HSTR ) constants
				// +------------------------------------------------------------------------------
				static const std::uint32_t HTF_BIT_MASK					= 0x00000038;

				typedef enum class ePCIStatusType : std::uint32_t
				{
					TIMEOUT_STATUS = 0,
					DONE_STATUS,
					READ_REPLY_STATUS,
					ERROR_STATUS,
					SYSTEM_RESET_STATUS,
					READOUT_STATUS,
					BUSY_STATUS
				} ePCIStatus;


				//  PCI commands
				// +----------------------------------------------------------------------------
				static const std::uint32_t PCI_RESET					= 0x8077;
				static const std::uint32_t ABORT_READOUT				= 0x8079;
				static const std::uint32_t BOOT_EEPROM					= 0x807B;
				static const std::uint32_t READ_HEADER					= 0x81;
				static const std::uint32_t RESET_CONTROLLER				= 0x87;
				static const std::uint32_t INITIALIZE_IMAGE_ADDRESS		= 0x91;
				static const std::uint32_t WRITE_COMMAND				= 0xB1;


			private:

				std::uint32_t getContinuousImageSize( std::uint32_t uiImageSize );
				std::uint32_t smallCamDLoad( std::uint32_t uiBoardId, std::vector<std::uint32_t>* pvData );
				void loadGen23ControllerFile( const std::string& sFilename, bool bValidate, const bool& bAbort = false );
				void setByteSwapping( void );

				const std::string formatPCICommand( std::uint32_t uiCmd, std::uint64_t uiReply, std::uint32_t uiArg = CArcDevice::NOPARAM, bool bGetSysErr = false );
				const std::string formatPCICommand( std::uint32_t uiCmd, std::uint64_t uiReply, const std::initializer_list<std::uint32_t>& tArgList, bool bGetSysErr = false );

				std::unique_ptr<CArcStringList> getHSTRBitList( std::uint32_t uiData, bool bDrawSeparator = false );

				//  Smart pointer array deleter
				// +--------------------------------------------------------------------+
				template<typename T> static void ArrayDeleter( T* p );

				static std::unique_ptr<std::vector<arc::gen3::device::ArcDev_t>> m_vDevList;

				static std::shared_ptr<std::string[]> m_psDevList;
		};

	}	// end gen3 namespace
}	// end arc namespace

#endif	// _CARC_PCI_H_
