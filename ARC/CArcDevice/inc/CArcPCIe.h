#ifndef _CARC_PCIE_H_
#define _CARC_PCIE_H_

#ifdef _WINDOWS
	#pragma warning( disable: 4251 )
#endif

#include <string>
#include <vector>
#include <memory>
#include <list>

#include <CArcDeviceDllMain.h>
#include <CArcBase.h>
#include <CArcPCIBase.h>




namespace arc
{
	namespace gen3
	{

		namespace device
		{

			//  Convenience names for base addr registers ( BAR )
			// +-------------------------------------------------+
			typedef enum class PCIeRegs : std::uint32_t
			{
				LCL_CFG_BAR = 0x00,		// Local Config Regs
				DEV_REG_BAR = 0x02			// Device Regs
			} ePCIeRegs;


			//  Device Register Offsets ( BAR2 )
			// +-------------------------------------------------+
			typedef enum class PCIeRegOffsets : std::uint32_t
			{
				REG_CMD_HEADER			= 0x00,
				REG_CMD_COMMAND			= 0x04,
				REG_CMD_ARG0			= 0x08,
				REG_CMD_ARG1			= 0x0C,
				REG_CMD_ARG2			= 0x10,
				REG_CMD_ARG3			= 0x14,
				REG_CMD_ARG4			= 0x18,
				REG_CTLR_SPECIAL_CMD	= 0x1C,
				REG_RESET				= 0x20,
				REG_INIT_IMG_ADDR		= 0x38,
				REG_FIBER_2X_CTRL		= 0x5C,
				REG_STATUS				= 0x60,
				REG_CMD_REPLY			= 0x64,
				REG_CTLR_ARG1			= 0x68,
				REG_CTLR_ARG2			= 0x6C,
				REG_PIXEL_COUNT			= 0x70,
				REG_FRAME_COUNT			= 0x74,
				REG_ID_LO				= 0x78,
				REG_ID_HI				= 0x7C
			} ePCIeRegOffsets;


			//  Special command Register Commands
			// +-------------------------------------------------+
			typedef enum class RegCmds : std::uint32_t
			{
				CONTROLLER_GET_ID = 0x09,
				CONTROLLER_RESET  = 0x0B
			} eRegCmds;


			//  Fiber Optic 2x Selector
			// +-------------------------------------------------+
			typedef enum class Fiber2x : std::uint32_t
			{
				FIBER_2X_DISABLE,
				FIBER_2X_ENABLE
			} eFiber2x;


			//  Fiber Optic Selector
			// +-------------------------------------------------+
			typedef enum class Fiber : std::uint32_t
			{
				FIBER_A,
				FIBER_B
			} eFiber;

		}	// end device namespace

		class GEN3_CARCDEVICE_API CArcPCIe : public CArcPCIBase
		{
			public:
				//  Constructor/Destructor
				// +-------------------------------------------------+
				CArcPCIe( void );

				virtual ~CArcPCIe( void );

				const std::string toString( void );


				//  PCI(e) configuration space access
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
				static std::uint32_t deviceCount( void );
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

				std::uint32_t  getControllerId( void );
				void resetController( void );
				bool isControllerConnected( void );
				bool isFiberConnected( arc::gen3::device::eFiber eFiberId = arc::gen3::device::eFiber::FIBER_A );


				//  expose commands
				// +-------------------------------------------------+
				void stopExposure( void );
				bool isReadout( void );
				std::uint32_t getPixelCount( void );
				std::uint32_t getCRPixelCount( void );
				std::uint32_t getFrameCount( void );


				//  PCIe only methods
				// +-------------------------------------------------+
				void writeBar( arc::gen3::device::ePCIeRegs eBar, std::uint32_t uiOffset, std::uint32_t uiValue );
				std::uint32_t readBar( arc::gen3::device::ePCIeRegs eBar, std::uint32_t uiOffset );
				std::uint32_t readReply( double fTimeOutSecs = 1.5 );


				//  PCIe Board ID Constant
				// +-------------------------------------------------+
				static const std::uint32_t ID				= 0x41524336;	// 'ARC6'

				//  Driver ioctl commands                                                      
				// +-----------------------------------------------------------------------------+
				static const std::uint32_t ARC_READ_BAR		=	0x01;	// Read PCI/e base address register
				static const std::uint32_t ARC_WRITE_BAR	=	0x02;	// Write PCI/e base address register
				static const std::uint32_t ARC_BAR_SIZE		=	0x03;	// Get PCI/e base address register size

				static const std::uint32_t ARC_READ_CFG_8	=	0x04;	// Read 8-bits of PCI/e config space
				static const std::uint32_t ARC_READ_CFG_16	=	0x05;	// Read 16-bits of PCI/e config space
				static const std::uint32_t ARC_READ_CFG_32	=	0x06;	// Read 32-bits of PCI/e config space

				static const std::uint32_t ARC_WRITE_CFG_8	=	0x07;	// Write 8-bits to PCI/e config space
				static const std::uint32_t ARC_WRITE_CFG_16	=	0x08;	// Write 16-bits to PCI/e config space
				static const std::uint32_t ARC_WRITE_CFG_32	=	0x09;	// Write 32-bits to PCI/e config space

				static const std::uint32_t ARC_BUFFER_PROP	=	0x0A;	// Get common buffer properties

				static const std::uint32_t ARC_MEM_MAP		=	0x0C;	// Maps BAR or common buffer
				static const std::uint32_t ARC_MEM_UNMAP	=	0x0D;	// UnMaps BAR or common buffer


			protected:

				static const std::uint32_t ARC_MIN_BAR		= 0;
				static const std::uint32_t ARC_MAX_BAR		= 5;

				std::uint32_t getContinuousImageSize( std::uint32_t uiImageSize );
				std::uint32_t smallCamDLoad( std::uint32_t uiBoardId, std::vector<std::uint32_t>* pvData );
				void loadGen23ControllerFile( const std::string& sFilename, bool bValidate, const bool& bAbort = false );
				void setByteSwapping( void );

				void getLocalConfiguration( void );

				//  Smart pointer array deleter
				// +--------------------------------------------------------------------+
				template<typename T> static void ArrayDeleter( T* p );

				static std::unique_ptr<std::vector<arc::gen3::device::ArcDev_t>> m_vDevList;
				static std::shared_ptr<std::string>								 m_psDevList;
		};


		// +----------------------------------------------------------------------------
		// |  Status Register Macros
		// +----------------------------------------------------------------------------
		#define PCIe_STATUS_CLEAR_ALL				0x7F

		#define PCIe_STATUS_REPLY_RECVD( dStatus )	\
							( ( dStatus & 0x00000003 ) == 2 ? true : false )

		#define PCIe_STATUS_CONTROLLER_RESET( dStatus )	\
							( ( dStatus & 0x00000008 ) > 0 ? true : false )

		#define PCIe_STATUS_READOUT( dStatus )	\
							( ( dStatus & 0x00000004 ) > 0 ? true : false )

		#define PCIe_STATUS_IDLE( dStatus )	\
							( ( dStatus & 0x00000003 ) == 0 ? true : false )

		#define PCIe_STATUS_CMD_SENT( dStatus )	\
							( ( dStatus & 0x00000003 ) == 1 ? true : false )

		#define PCIe_STATUS_IMG_READ_TIMEOUT( dStatus )	\
						( ( dStatus & 0x00000020 ) > 0 ? true : false )

		#define PCIe_STATUS_HDR_ERROR( dStatus )	\
							( ( dStatus & 0x00000010 ) > 0 ? true : false )

		#define PCIe_STATUS_FIBER_2X_RECEIVER( dStatus ) \
							( ( dStatus & 0x00000200 ) > 0 ? true : false )

		#define PCIe_STATUS_FIBER_A_CONNECTED( dStatus ) \
							( ( dStatus & 0x00000080 ) > 0 ? true : false )

		#define PCIe_STATUS_FIBER_B_CONNECTED( dStatus ) \
							( ( dStatus & 0x00000100 ) > 0 ? true : false )

	}	// end gen3 namespace
}	// end arc namespace


#endif
