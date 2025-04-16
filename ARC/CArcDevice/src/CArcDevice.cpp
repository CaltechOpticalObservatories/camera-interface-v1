#ifdef _WINDOWS
	#include <windows.h>
#else
	#include <sys/ioctl.h>
	#include <sys/mman.h>
	#include <unistd.h>
	#include <errno.h>
	#include <cstring>
	#include <cstdlib>
#endif

#include <memory>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <queue>
#include <cmath>
#include <iostream>     // for std::cerr

#include <CArcBase.h>
#include <CArcDevice.h>
#include <ArcOSDefs.h>
#include <ArcDefs.h>
#include <TempCtrl.h>


//#include <fstream>
//std::ofstream dbgStream( "CArcDevice_Debug.txt" );


namespace arc
{
	namespace gen3
	{

		const std::string CArcDevice::NO_FILE = "";


		// +----------------------------------------------------------------------------+
		// |  Shared Pointer Array Deleter                                              |
		// +----------------------------------------------------------------------------+
		template<typename T> class ArrayDeleter
		{
			public:
				void operator () ( T* p ) const
				{
					delete [] p;
				}
		}; 


		// +----------------------------------------------------------------------------+
		// |  Class Constructor                                                         |
		// +----------------------------------------------------------------------------+
		CArcDevice::CArcDevice( void )
		{
			m_hDevice    = INVALID_HANDLE_VALUE;
			m_uiCCParam   = 0;
			m_bStoreCmds = false;

			arc::gen3::CArcBase::zeroMemory( &m_tImgBuffer, sizeof( arc::gen3::device::ImgBuf_t ) );

			m_pCLog.reset( new arc::gen3::CArcLog() );

			setDefaultTemperatureValues();
		}


		// +----------------------------------------------------------------------------+
		// |  isOpen                                                                    |
		// +----------------------------------------------------------------------------+
		// |  Returns 'true' if connected to a gen3; 'false' otherwise.               |
		// |                                                                            |
		// |  Throws NOTHING on error. No error handling.                               |
		// +----------------------------------------------------------------------------+
		bool CArcDevice::isOpen( void )
		{
			return ( ( m_hDevice != INVALID_HANDLE_VALUE ) ? true : false );
		}


		// +----------------------------------------------------------------------------+
		// |  reMapCommonBuffer                                                         |
		// +----------------------------------------------------------------------------+
		// |  Remaps the kernel image buffer by first calling unMapCommonBuffer if      |
		// |  necessary and then calls mapCommonBuffer.                                 |
		// |                                                                            |
		// |  Throws std::runtime_error on error                                        |
		// +----------------------------------------------------------------------------+
		void CArcDevice::reMapCommonBuffer( std::uint32_t uiBytes )
		{
			unMapCommonBuffer();

			mapCommonBuffer( uiBytes );
		}


		// +----------------------------------------------------------------------------+
		// |  fillCommonBuffer                                                          |
		// +----------------------------------------------------------------------------+
		// |  Clears the image buffer by filling it with the specified value.           |
		// |                                                                            |
		// |  Throws std::runtime_error on error                                        |
		// |                                                                            |
		// |  <IN> -> uwValue - The 16-bit value to fill the buffer with. Default = 0   |
		// +----------------------------------------------------------------------------+
		void CArcDevice::fillCommonBuffer( std::uint16_t uwValue )
		{
			if ( m_tImgBuffer.pUserAddr == ( void * )nullptr )
			{
				THROW( "NULL image buffer! Check that a device is open and common buffer has been allocated and mapped!" );
			}

			auto pU16Buf = static_cast<std::uint16_t*>( m_tImgBuffer.pUserAddr );

			std::uint64_t uiPixCount = ( m_tImgBuffer.ulSize / sizeof( std::uint16_t ) );

			//  Perform a quicker fill if we can
			// +-------------------------------------------------+
			if ( ( uiPixCount % 4 ) == 0 )
			{
				for ( decltype( uiPixCount ) i = 0; i < uiPixCount; i += 4 )
				{
					pU16Buf[ i + 0 ] = uwValue;
					pU16Buf[ i + 1 ] = uwValue;
					pU16Buf[ i + 2 ] = uwValue;
					pU16Buf[ i + 3 ] = uwValue;
				}
			}

			//else
			//{
			//	for ( decltype( uiPixCount ) i = 0; i < uiPixCount; i++ )
			//	{
			//		pU16Buf[ i ] = uwValue;
			//	}
			//}
		}


		// +----------------------------------------------------------------------------+
		// |  commonBufferVA                                                            |
		// +----------------------------------------------------------------------------+
		// |  Returns the virtual address of the gen3 driver image buffer.            |
		// +----------------------------------------------------------------------------+
		std::uint8_t* CArcDevice::commonBufferVA( void )
		{
			return reinterpret_cast<std::uint8_t*>( m_tImgBuffer.pUserAddr );
		}


		// +----------------------------------------------------------------------------+
		// |  commonBufferPA                                                            |
		// +----------------------------------------------------------------------------+
		// |  Returns the physical address of the gen3 driver image buffer.           |
		// +----------------------------------------------------------------------------+
		std::uint64_t CArcDevice::commonBufferPA( void )
		{
			return m_tImgBuffer.ulPhysicalAddr;
		}


		// +----------------------------------------------------------------------------+
		// |  commonBufferSize                                                          |
		// +----------------------------------------------------------------------------+
		// |  Returns the gen3 driver image buffer size in bytes.                     |
		// +----------------------------------------------------------------------------+
		std::uint64_t CArcDevice::commonBufferSize( void )
		{
			return m_tImgBuffer.ulSize;
		}


		// +----------------------------------------------------------------------------+
		// |  setupController                                                           |
		// +----------------------------------------------------------------------------+
		// |  This is a convenience function that performs a controller setup given     |
		// |  the specified parameters.                                                 |
		// |                                                                            |
		// |  Throws std::runtime_error on error                                        |
		// |                                                                            |
		// |  <IN> -> bReset    - 'true' to reset the controller.                       |
		// |  <IN> -> bTdl      - 'true' to send TDLs to the PCI board and any board    |
		// |                       whose .lod file is not NULL.                         |
		// |  <IN> -> bPower    - 'true' to power on the controller.                    |
		// |  <IN> -> uiRows    -  The image row dimension (in pixels).                 |
		// |  <IN> -> uiCols    -  The image col dimension (in pixels).                 |
		// |  <IN> -> sTimFile  - The timing board file to load (.lod file).            |
		// |  <IN> -> sUtilFile - The utility board file to load (.lod file).           |
		// |  <IN> -> sPciFile  - The pci board file to load (.lod file).               |
		// |  <IN> -> bAbort    - 'true' to abort; 'false' otherwise. Default: false    |
		// +----------------------------------------------------------------------------+
		void CArcDevice::setupController( bool bReset, bool bTdl, bool bPower, std::uint32_t uiRows, std::uint32_t uiCols,
										  const std::string& sTimFile, const std::string& sUtilFile, const std::string& sPciFile,
										  const bool& bAbort )
		{
			std::uint32_t uiRetVal = 0;

			if ( bAbort ) { return; }

			//
			// Clear status register
			// +-------------------------------------------------+
			clearStatus();

			//
			// PCI download
			// +-------------------------------------------------+
			if ( !sPciFile.empty() )
			{
				loadDeviceFile( sPciFile );
			}

			if ( bAbort ) { return; }

			//
			// reset controller
			// +-------------------------------------------------+
			if ( bReset )
			{
				resetController();
			}

			if ( bAbort ) { return; }

			//
			// Hardware tests ( TDL )
			// +-------------------------------------------------+
			if ( bTdl )
			{
				//
				// PCI
				// +---------------------------------------------+
				for ( std::uint32_t i = 0; i < 1234; i++ )
				{
					if ( bAbort ) { return; }

					uiRetVal = command( { PCI_ID, TDL, i } );

					if ( uiRetVal != i )
					{
						THROW( "PCI TDL %d/1234 failed. Sent: %d Reply: %d", i,	i, uiRetVal );
					}
				}

				if ( bAbort ) { return; }

				//
				// TIM
				// +---------------------------------------------+
				if ( !sTimFile.empty() )
				{
					for ( std::uint32_t i = 0; i < 1234; i++ )
					{
						if ( bAbort ) { return; }

						uiRetVal = command( { TIM_ID, TDL, i } );

						if ( uiRetVal != i )
						{
							THROW( "TIM TDL %d/1234 failed. Sent: %d Reply: %u", i,	i, uiRetVal );
						}
					}
				}

				if ( bAbort ) { return; }

				//
				// UTIL
				// +---------------------------------------------+
				if ( !sUtilFile.empty() )
				{
					for ( std::uint32_t i = 0; i < 1234; i++ )
					{
						if ( bAbort ) { return; }

						uiRetVal = command( { UTIL_ID, TDL, i } );

						if ( uiRetVal != i )
						{
							THROW( "UTIL TDL %d/1234 failed. Sent: %d Reply: %u", i, i,	uiRetVal );
						}
					}
				}
			}

			if ( bAbort ) { return; }

			//
			// TIM download
			// +-------------------------------------------------+
			if ( !sTimFile.empty() )
			{
				loadControllerFile( sTimFile );
			}

			if ( bAbort ) { return; }

			//
			// UTIL download
			// +-------------------------------------------------+
			if ( !sUtilFile.empty() )
			{
				loadControllerFile( sUtilFile );
			}

			if ( bAbort ) { return; }

			//
			// Power on
			// +-------------------------------------------------+
			if ( bPower )
			{
				uiRetVal = command( { TIM_ID, PON } );

				if ( uiRetVal != DON )
				{
					THROW( "Power on failed! Reply: 0x%X", uiRetVal );
				}
			}

			if ( bAbort ) { return; }

			//
			// Set image dimensions
			// +-------------------------------------------------+
			if ( uiRows > 0 && uiCols > 0 )
			{
				setImageSize( uiRows, uiCols );
			}

			else
			{
				THROW( "Invalid image dimensions, rows: %u cols: %u", uiRows, uiCols );
			}
		}


		// +----------------------------------------------------------------------------+
		// |  selectOutputSource                                                        |
		// +----------------------------------------------------------------------------+
		// |  Calls the 3-letter command SOS to select the output source                |
		// |  specified board.                                                          |
		// |                                                                            |
		// |  Throws std::runtime_error on error                                        |
		// |                                                                            |
		// |  <IN> -> arg - argument for SOS command to select the readout amplifer     |
		// +----------------------------------------------------------------------------+
		void CArcDevice::selectOutputSource( std::uint32_t arg )
		{
			// select output source command
			auto uiRetVal = command( { TIM_ID, SOS, arg } );

			if ( uiRetVal != DON )
			{
				THROW( "Failed to set the output source (SOS). Reply: 0x%X", uiRetVal );
			}
		}


		// +----------------------------------------------------------------------------+
		// |  loadControllerFile                                                        |
		// +----------------------------------------------------------------------------+
		// |  Loads a SmallCam/GenI/II/III timing or utility file (.lod) into the       |
		// |  specified board.                                                          |
		// |                                                                            |
		// |  Throws std::runtime_error on error                                        |
		// |                                                                            |
		// |  <IN> -> sFilename - The SMALLCAM or GENI/II/III TIM or UTIL lod file to   |
		// |                      load.                                                 |
		// |  <IN> -> bValidate - Set to 'true' if the download should be read back     |
		// |                      and checked after every write.                        |
		// |  <IN> -> bAbort    - 'true' to stop; 'false' otherwise. Default: false     |
		// +----------------------------------------------------------------------------+
		void CArcDevice::loadControllerFile( const std::string& sFilename, bool bValidate, const bool& bAbort )
		{
			std::uint32_t uiReply = 0;

			if ( bAbort ) { return; }

			//
			// Set the PCI image byte-swapping if SUN hardware.
			//
			setByteSwapping();

			if ( bAbort ) { return; }

			//
			// Check the system id  ... And YES, PCI_ID is correct!
			//
			uiReply = getControllerId();

			if ( bAbort ) { return; }

			if ( IS_ARC12( uiReply ) )
			{
				resetController();

				if ( bAbort ) { return; }

				loadSmallCamControllerFile( sFilename, false, bAbort );
			}
			else
			{
				loadGen23ControllerFile( sFilename, bValidate, bAbort );
			}

			if ( bAbort ) { return; }

			//
			// Wait 10ms. The controller DSP takes 5ms to start processing
			// commands after the download is complete.  This time was
			// measured using the logic analyzer on a PCIe <-> SmallCam
			// system on Nov 16, 2011 at 12:50pm by Bob and Yoating.
			//
			// Without this delay the RCC command that follows will cause
			// problems that result in the DSP not responding to any further
			// commands.

			if ( bAbort ) { return; }

			//
			// Auto-get CC Params after setup
			//
			// Loop N number of times while waiting for the getCCParams
			// method to return a value other than CNR.  Any value other
			// than CNR will cause the code to continue on.  This is
			// primarily for use on a PCIe <-> SmallCam system.
			m_uiCCParam = CNR;

			int dTryCount = 0;

			while ( m_uiCCParam == CNR && dTryCount < 500 )
			{
				if ( bAbort ) { break; }

				try
				{
					m_uiCCParam = getCCParams();
				}
				catch ( ... )
				{
				}

				dTryCount++;

				std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
			}
		}

		// +----------------------------------------------------------------------------+
		// |  loadSmallCamControllerFile                                                |
		// +----------------------------------------------------------------------------+
		// |  Loads a timing or utility file (.lod) into a SmallCam controller.         |
		// |                                                                            |
		// |  Throws std::runtime_error on error                                        |
		// |                                                                            |
		// |  <IN> -> sFilename   - The TIM or UTIL lod file to load.                   |
		// |  <IN> -> bValidate   - Set to 1 if the download should be read back and    |
		// |                        checked after every write.                          |
		// |  <IN> -> bAbort      - 'true' to stop; 'false' otherwise. Default: false   |
		// +----------------------------------------------------------------------------+
		void CArcDevice::loadSmallCamControllerFile( const std::string& sFilename, bool bValidate, const bool& bAbort )
		{
			std::uint32_t uiAddr	= 0;
			std::uint32_t uiData	= 0;
			std::uint32_t uiReply	= 0;

			std::string sLine;

			std::unique_ptr<std::queue<std::uint32_t>>  pQ( new std::queue<std::uint32_t> );

			std::unique_ptr<std::vector<std::uint32_t>> pvData( new std::vector<std::uint32_t> );

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
			// Read in the file one line at a time
			// --------------------------------------
			while ( !inFile.eof() )
			{
				if ( bAbort ) { inFile.close(); return; }

				getline( inFile, sLine );

				//
				// Only "_DATA" blocks are valid for download
				// ---------------------------------------------
				if ( sLine.find( "_DATA " ) != std::string::npos )
				{
					auto pTokens = CArcBase::splitString( sLine );

					// Dump _DATA std::string

					//
					// Get the memory type and start address
					// ---------------------------------------------
					// Don't need to know memory type
					uiAddr = static_cast< std::uint32_t >( std::stol( pTokens->at( 2 ), 0, 16 ) );

					//
					// The start address must be less than MAX_DSP_START_LOAD_ADDR
					// -------------------------------------------------------------
					if ( uiAddr < MAX_DSP_START_LOAD_ADDR )
					{
						//
						// Read the data block and store it in a Q
						// ----------------------------------------
						while ( !inFile.eof() && inFile.peek() != '_' )
						{
							if ( bAbort ) { inFile.close(); return; }

							getline( inFile, sLine );
							
							auto pDataTokens = CArcBase::splitString( sLine );

							for ( auto it = pDataTokens->begin(); it != pDataTokens->end(); it++ )
							{
								if ( bAbort ) { inFile.close(); return; }

								uiData = static_cast< std::uint32_t >( std::stol( *it, 0, 16 ) );

								pQ->push( uiData );

								uiAddr++;

							} // while tokenizer next
						} // while not EOF or '_'
					}	// if address < 0x4000
				}	// if not '_DATA'
			}	// while not EOF

			//
			// Write the data from the Q
			// ----------------------------------------
			while ( !pQ->empty() )
			{
				if ( pvData->size() == 6 )
				{
					//
					// Write the data to the controller ( PCIe actually )
					// --------------------------------------------------------------
					uiReply = smallCamDLoad( SMALLCAM_DLOAD_ID, pvData.get() );

					if ( uiReply != DON )
					{
						THROW( "Write to controller TIMING board failed. %s", formatDLoadString( uiReply, SMALLCAM_DLOAD_ID, pvData.get() ).c_str() );
					}

					pvData->clear();
				}

				pvData->push_back( pQ->front() );

				pQ->pop();
			}

			//
			// Write 'extra' data to the controller ( PCIe actually )
			// --------------------------------------------------------------
			uiReply = smallCamDLoad( SMALLCAM_DLOAD_ID, pvData.get() );

			if ( uiReply != DON )
			{
				THROW( "Write to controller TIMING board failed. %s", formatDLoadString( uiReply, SMALLCAM_DLOAD_ID, pvData.get() ).c_str() );
			}

			inFile.close();
		}


		// +--------------------------------------------------------------------------------------------------------+
		// |  formatDLoadString                                                                                     |
		// +--------------------------------------------------------------------------------------------------------+
		// |  Method to bundle command values into a std::string that can be used to throw a std::runtime_error          |
		// |  exception.                                                                                            |
		// |                                                                                                        |
		// |  <IN> -> uiReply     : The received command reply value.                                               |
		// |  <IN> -> uiBoardId   : The command header.                                                             |
		// |  <IN> -> vData       : The data vector.                                                                |
		// +--------------------------------------------------------------------------------------------------------+
		const std::string CArcDevice::formatDLoadString( std::uint32_t uiReply, std::uint32_t uiBoardId, std::vector<std::uint32_t>* pvData )
		{
			if ( pvData == nullptr )
			{
				THROW( "Data parameter cannot be nullptr!" );
			}

			std::ostringstream oss;

			oss.setf( std::ios::hex, std::ios::basefield );
			oss.setf( std::ios::uppercase );

			oss << "[ 0x" << ( ( uiBoardId << 8 ) | ( pvData->size() + 1 ) );	// Header

			for ( auto it = pvData->begin(); it != pvData->end(); it++ )
			{
				oss << " 0x" << *it;
			}

			oss << " -> 0x" << uiReply << " ]";					// Reply

			return oss.str();
		}


		// +--------------------------------------------------------------------------------------------------------+
		// | setImageSize                                                                                           |
		// +--------------------------------------------------------------------------------------------------------+
		// | Sets the image size in pixels on the controller. This is used during setup, subarray, binning,         |
		// | continuous readout, etc., whenever the image size is changed. This method will attempt to re-map the   |
		// | image buffer if the specified image size is greater than that of the image buffer.                     |
		// |                                                                                                        |
		// |  Throws std::runtime_error on error                                                                    |
		// |                                                                                                        |
		// |  <IN> -> uiRows  - The number of rows in the image.                                                    |
		// |  <IN> -> uiCols  - The number of columns in the image.                                                 |
		// +--------------------------------------------------------------------------------------------------------+
		void CArcDevice::setImageSize( std::uint32_t uiRows, std::uint32_t uiCols )
		{
			std::uint32_t uiReply = 0;

			//
			// Rows
			// ---------------------------------------
			uiReply = command( { TIM_ID, WRM, ( Y_MEM | 2 ), uiRows } );

			if ( uiReply != DON )
			{
				THROW( "Write image rows: %u -> reply: 0x%X", uiRows, uiReply );
			}

			//
			// Cols
			// ---------------------------------------
			uiReply = command( { TIM_ID, WRM, ( Y_MEM | 1 ), uiCols } );

			if ( uiReply != DON )
			{
				THROW( "Write image cols: %u -> reply: 0x%X", uiCols, uiReply );
			}

			//
			// Attempt to remap the image buffer if needed
			//
		#ifdef _WINDOWS
			if ( ( uiRows * uiCols ) <= ( 4200 * 4200 ) )
			{
		#endif
				std::uint32_t uiNewSize = ( uiRows * uiCols * sizeof( std::uint16_t ) );

				if ( uiNewSize > commonBufferSize() )
				{
					reMapCommonBuffer( uiNewSize );
				}
		#ifdef _WINDOWS
			}
		#endif
		}


		// +--------------------------------------------------------------------------------------------------------+
		// | getImageRows                                                                                           |
		// +--------------------------------------------------------------------------------------------------------+
		// | Returns the image row size (pixels) that has been set on the controller.                               |
		// |                                                                                                        |
		// | Throws std::runtime_error on error                                                                     |
		// +--------------------------------------------------------------------------------------------------------+
		std::uint32_t CArcDevice::getImageRows( void )
		{
			std::uint32_t uiRows = 0;

			uiRows = command( { TIM_ID, RDM, ( Y_MEM | 2 ) } );

			if ( containsError( uiRows ) )
			{
				THROW( "Command failed!, reply: 0x%X", uiRows );
			}

			return uiRows;
		}


		// +--------------------------------------------------------------------------------------------------------+
		// | getImageCols                                                                                           |
		// +--------------------------------------------------------------------------------------------------------+
		// | Returns the image column size (pixels) that has been set on the controller.                            |
		// |                                                                                                        |
		// | Throws std::runtime_error on error                                                                     |
		// +--------------------------------------------------------------------------------------------------------+
		std::uint32_t CArcDevice::getImageCols( void )
		{
			std::uint32_t uiCols = 0;

			uiCols = command( { TIM_ID, RDM, ( Y_MEM | 1 ) } );

			if ( containsError( uiCols ) )
			{
				THROW( "Command failed!, reply: 0x%X", uiCols );
			}

			return uiCols;
		}


		// +--------------------------------------------------------------------------------------------------------+
		// |  getCCParams                                                                                           |
		// +--------------------------------------------------------------------------------------------------------+
		// |  Returns the available configuration parameters. The parameter bit definitions are specified in        |
		// |  ArcDefs.h.                                                                                            |
		// |                                                                                                        |
		// |  Throws std::runtime_error on error                                                                    |
		// +--------------------------------------------------------------------------------------------------------+
		std::uint32_t CArcDevice::getCCParams( void )
		{
			m_uiCCParam = command( { TIM_ID, RCC } );

			if ( containsError( m_uiCCParam ) )
			{
				THROW( "Read controller configuration parameters failed. Read: 0x%X", m_uiCCParam );
			}

			return m_uiCCParam;
		}


		// +--------------------------------------------------------------------------------------------------------+
		// |  isCCParamSupported                                                                                    |
		// +--------------------------------------------------------------------------------------------------------+
		// |  Returns 'true' if the specified configuration parameter is available on the controller.               |
		// |  Returns 'false' otherwise. The parameter bit definitions are specified in ArcDefs.h.                  |
		// |                                                                                                        |
		// |  <IN> -> uiParameter - The controller parameter to check.                                               |
		// +--------------------------------------------------------------------------------------------------------+
		bool CArcDevice::isCCParamSupported( std::uint32_t uiParameter )
		{
			bool bIsSupported = false;

			//
			// Read the CC Param word if it conains an error
			//
			if ( containsError( m_uiCCParam ) )
			{
				getCCParams();
			}

			//  Bits 0, 1, 2
			// +-------------------------------------------+
			if ( ( m_uiCCParam & 0x00000007 ) == uiParameter )
			{
				bIsSupported = true;
			}

			//  Bits 3, 4
			// +-------------------------------------------+
			else if ( ( m_uiCCParam & 0x00000018 ) == uiParameter )
			{
				bIsSupported = true;
			}

			//  Bits 5, 6
			// +-------------------------------------------+
			else if ( ( m_uiCCParam & 0x00000060 ) == uiParameter )
			{
				bIsSupported = true;
			}

			//  Bit 7
			// +-------------------------------------------+
			else if ( ( m_uiCCParam & 0x00000080 ) == uiParameter )
			{
				bIsSupported = true;
			}

			//  Bits 8, 9
			// +-------------------------------------------+
			else if ( ( m_uiCCParam & 0x00000300 ) == uiParameter )
			{
				bIsSupported = true;
			}

			//  Bit 10
			// +-------------------------------------------+
			else if ( ( m_uiCCParam & 0x00000400 ) == uiParameter )
			{
				bIsSupported = true;
			}

			//  Bit 11
			// +-------------------------------------------+
			else if ( ( m_uiCCParam & 0x00000800 ) == uiParameter )
			{
				bIsSupported = true;
			}

			//  Bits 12, 13
			// +-------------------------------------------+
			else if ( ( m_uiCCParam & 0x00003000 ) == uiParameter )
			{
				bIsSupported = true;
			}

			//  Bit 14
			// +-------------------------------------------+
			else if ( ( m_uiCCParam & 0x00004000 ) == uiParameter )
			{
				bIsSupported = true;
			}

			//  Bits 15, 16
			// +-------------------------------------------+
			else if ( ( m_uiCCParam & 0x00018000 ) == uiParameter )
			{
				bIsSupported = true;
			}

			//  Bits 17, 18, 19
			// +-------------------------------------------+
			else if ( ( m_uiCCParam & 0x000E0000 ) == uiParameter )
			{
				bIsSupported = true;
			}

			//  Bits 20, 21, 22, 23
			// +-------------------------------------------+
			else if ( ( m_uiCCParam & 0x00F00000 ) == uiParameter )
			{
				bIsSupported = true;
			}

			return bIsSupported;
		}


		// +--------------------------------------------------------------------------------------------------------+
		// |  isCCD                                                                                                 |
		// +--------------------------------------------------------------------------------------------------------+
		// |  Returns false if the controller contains an IR video processor board.                                 |
		// |  Returns true otherwise.                                                                               |
		// +--------------------------------------------------------------------------------------------------------+
		bool CArcDevice::isCCD( void )
		{
			bool bIRREV4 = isCCParamSupported( IRREV4 );
			bool bARC46  = isCCParamSupported( ARC46  );
			bool bIR8X   = isCCParamSupported( IR8X   );

			if ( bIRREV4 || bARC46 || bIR8X )
			{
				return false;
			}

			return true;
		}


		// +--------------------------------------------------------------------------------------------------------+
		// |  isBinningSet                                                                                          |
		// +--------------------------------------------------------------------------------------------------------+
		// |  Returns 'true' if binning is set on the controller, returns 'false' otherwise.                        |
		// +--------------------------------------------------------------------------------------------------------+
		bool CArcDevice::isBinningSet( void )
		{
			std::uint32_t uiBinFactor = 0;

			bool bIsSet  = true;

			//
			// Verify gen3 connection
			// -------------------------------------------------------------------
			if ( !isOpen() )
			{
				THROW_NO_DEVICE_ERROR();
			}

			//
			// Read the column factor from timing board Y:5
			// -------------------------------------------------------------
			uiBinFactor = command( { TIM_ID, RDM, ( Y_MEM | 0x5 ) } );

			if ( uiBinFactor == 1 )
			{
				//
				// Read the row factor from timing board Y:6
				// ---------------------------------------------------------
				uiBinFactor = command( { TIM_ID, RDM, ( Y_MEM | 0x6 ) } );

				if ( uiBinFactor == 1 )
				{
					bIsSet = false;
				}
			}

			return bIsSet;
		}


		// +--------------------------------------------------------------------------------------------------------+
		// |  setBinning
		// +--------------------------------------------------------------------------------------------------------+
		// |  Sets the camera controller binning factors for both the rows and columns.
		// |  Binning causes each axis to combine xxxFactor number of pixels. For
		// |  example, if rows = 1000, cols = 1000, rowFactor = 2, colFactor = 4
		// |  then binRows = 500, binCols = 250.
		// |
		// |  Throws std::runtime_error on error
		// |
		// |  <IN>  -> uiRows      - The pre-binning number of image rows
		// |  <IN>  -> uiCols      - The pre-binning number of images columns
		// |  <IN>  -> uiRowFactor - The binning row factor
		// |  <IN>  -> uiColFactor - The binning column factor
		// |  <OUT> -> pBinRows    - Pointer that will get binned image row value
		// |  <OUT> -> pBinCols    - Pointer that will get binned image col value
		// +--------------------------------------------------------------------------------------------------------+
		void CArcDevice::setBinning( std::uint32_t uiRows, std::uint32_t uiCols, std::uint32_t uiRowFactor, std::uint32_t uiColFactor, std::uint32_t* pBinRows, std::uint32_t* pBinCols )
		{
			std::uint32_t uiRetVal     = 0;
			std::uint32_t uiBinnedRows = uiRows;
			std::uint32_t uiBinnedCols = uiCols;

			//
			// Verify gen3 connection
			// -------------------------------------------------------------------
			if ( !isOpen() )
			{
				THROW_NO_DEVICE_ERROR();
			}

			//
			// Write the column factor to timing board Y:5 ( if different )
			// -------------------------------------------------------------
			uiRetVal = command( { TIM_ID, WRM, ( Y_MEM | 0x5 ), uiColFactor } );

			if ( uiRetVal != DON )
			{
				THROW( "Failed to set binning column factor ( %u ). Command reply: 0x%X", uiColFactor, uiRetVal );
			}

			uiBinnedCols = uiCols / uiColFactor;

			//
			// Write the row factor to timing board Y:6 ( if different )
			// -------------------------------------------------------------
			uiRetVal = command( { TIM_ID, WRM, ( Y_MEM | 0x6 ), uiRowFactor } );

			if ( uiRetVal != DON )
			{
				THROW( "Failed to set binning row factor ( %u ). Command reply: 0x%X", uiRowFactor, uiRetVal );
			}

			uiBinnedRows = uiRows / uiRowFactor;

			if ( pBinRows != nullptr ) { *pBinRows = uiBinnedRows; }
			if ( pBinCols != nullptr ) { *pBinCols = uiBinnedCols; }

			setImageSize( uiBinnedRows, uiBinnedCols );
		}


		// +----------------------------------------------------------------------------
		// |  UnsetBinning
		// +----------------------------------------------------------------------------
		// |  Unsets the camera controller binning factors previously set by a call to
		// |  CArcDevice::setBinning().
		// |
		// |  Throws std::runtime_error on error
		// |
		// |  <IN>  -> uiRows  - The image rows to set on the controller 
		// |  <IN>  -> uiCols  - The image cols to set on the controller
		// +----------------------------------------------------------------------------
		void CArcDevice::unSetBinning( std::uint32_t uiRows, std::uint32_t uiCols )
		{
			std::uint32_t uiRetVal = 0;

			//
			// Verify gen3 connection
			// -------------------------------------------------------------------
			if ( !isOpen() )
			{
				THROW_NO_DEVICE_ERROR();
			}

			//
			// Write the column factor to timing board Y:5
			// -------------------------------------------------------------
			uiRetVal = command( { TIM_ID, WRM, ( Y_MEM | 0x5 ), 1 } );

			if ( uiRetVal != DON )
			{
				THROW( "Failed to set binning column factor ( 1 ). Command reply: 0x%X", uiRetVal );
			}

			//
			// Write the row factor to timing board Y:6
			// -------------------------------------------------------------
			uiRetVal = command( { TIM_ID, WRM, ( Y_MEM | 0x6 ), 1 } );

			if ( uiRetVal != DON )
			{
				THROW( "Failed to set binning row factor ( 1 ). Command reply: 0x%X", uiRetVal );
			}

			//
			// Update the image dimensions on the controller
			// -------------------------------------------------------------
			setImageSize( uiRows, uiCols );
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
		// |  Throws std::runtime_error on error
		// |
		// |  <OUT> -> uiOldRows - Returns the original row size set on the controller
		// |  <OUT> -> uiOldCols - Returns the original col size set on the controller
		// |  <IN> -> #1 - uiRow - The row # of the center of the sub-array in pixels
		// |  <IN> -> #2 - uiCol - The col # of the center of the sub-array in pixels
		// |  <IN> -> #3 - uiSubRows - The sub-array row count in pixels
		// |  <IN> -> #4 - uiSubCols - The sub-array col count in pixels
		// |  <IN> -> #5 - uiBiasOffset - The offset of the bias region in pixels
		// |  <IN> -> #6 - uiBiasCols - The col count of the bias region in pixels
		// +----------------------------------------------------------------------------
		void CArcDevice::setSubArray( std::uint32_t& uiOldRows, std::uint32_t& uiOldCols, std::uint32_t uiRow, std::uint32_t uiCol,
									  std::uint32_t uiSubRows, std::uint32_t uiSubCols, std::uint32_t uiBiasOffset, std::uint32_t uiBiasCols )
		{
			std::uint32_t uiRetVal = 0;

			//
			// Verify gen3 connection
			// -------------------------------------------------------------------
			if ( !isOpen() )
			{
				THROW_NO_DEVICE_ERROR();
			}

			//
			// Get the current image dimensions
			//
			uiOldRows = getImageRows();
			uiOldCols = getImageCols();

			//
			// Set the new image dimensions
			//
			setImageSize( uiSubRows, uiSubCols + uiBiasCols );

			//
			// Set the sub-array size
			//
			uiRetVal = command( { TIM_ID, SSS, uiBiasCols, uiSubCols, uiSubRows } );

			if ( uiRetVal != DON )
			{
				THROW( "Failed to set sub-array SIZE on controller. Reply: 0x%X", uiRetVal );
			}

			//
			// Set the sub-array position
			//
			uiRetVal = command( { TIM_ID,
								  SSP,
								  uiRow - ( uiSubRows / 2 ),
								  uiCol - ( uiSubCols / 2 ),
								  uiBiasOffset - uiCol - ( uiSubCols / 2 ) } );

			if ( uiRetVal != DON )
			{
				THROW( "Failed to set sub-array POSITION on controller. Reply: 0x%X", uiRetVal );
			}
		}


		// +----------------------------------------------------------------------------
		// |  unSetSubArray
		// +----------------------------------------------------------------------------
		// |  Unsets the camera controller from sub-array mode.
		// |
		// |  Throws std::runtime_error on error
		// |
		// |  <IN> -> uiRows  - The row size to set on the controller
		// |  <IN> -> uiCols  - The col size to set on the controller
		// +----------------------------------------------------------------------------
		void CArcDevice::unSetSubArray( std::uint32_t uiRows, std::uint32_t uiCols )
		{
			std::uint32_t uiRetVal = 0;

			//
			// Verify gen3 connection
			// -------------------------------------------------------------------
			if ( !isOpen() )
			{
				THROW_NO_DEVICE_ERROR();
			}

			//
			// Set the new image dimensions
			//
			setImageSize( uiRows, uiCols );

			//
			// Set the sub-array size to zero
			//
			uiRetVal = command( { TIM_ID, SSS, 0, 0, 0 } );

			if ( uiRetVal != DON )
			{
				THROW( "Failed to set sub-array SIZE on controller. Reply: 0x%X", uiRetVal );
			}
		}


		// +----------------------------------------------------------------------------
		// |  isSyntheticImageMode
		// +----------------------------------------------------------------------------
		// |  Returns true if synthetic readout is turned on. Otherwise, returns
		// |  'false'. A synthetic image looks like: 0, 1, 2, ..., 65535, 0, 1, 2,
		// |  ..., 65353, etc
		// |
		// |  Throws std::runtime_error on error
		// +----------------------------------------------------------------------------
		bool CArcDevice::isSyntheticImageMode( void )
		{
			const std::uint32_t uiSynBitMask = 0x00000400;

			std::uint32_t uiStatus = 0;
			
			bool bIsSet = false;

			//
			// Read the controller status word from the TIM X:0
			//
			uiStatus = command( { TIM_ID, RDM, ( X_MEM | 0 ) } );

			if ( containsError( uiStatus ) )
			{
				THROW( "Failed to read controller status: 0x%X", uiStatus );
			}

			//
			// Check the PCI board for synthetic image mode. Bit 10
			// of TIM X:0 should be zero.
			//
			if ( ( uiStatus & uiSynBitMask ) == uiSynBitMask )
			{
				bIsSet = true;
			}

			return bIsSet;
		}


		// +----------------------------------------------------------------------------
		// |  setSyntheticImageMode
		// +----------------------------------------------------------------------------
		// |  If mode is 'true', then synthetic readout will be turned on. Set mode
		// |  to 'false' to turn it off. A synthetic image looks like: 0, 1, 2, ...,
		// |  65535, 0, 1, 2, ..., 65353, etc
		// |
		// |  Throws std::runtime_error on error
		// |
		// |  <IN> -> bMode  - 'true' to turn it on, 'false' to turn if off.
		// +----------------------------------------------------------------------------
		void CArcDevice::setSyntheticImageMode( bool bMode )
		{
			const std::uint32_t uiClearBitMask = 0x00000400;

			std::uint32_t uiStatus = 0;
			std::uint32_t uiReply  = 0;

			//
			// Read the controller status word from the TIM X:0
			//
			uiStatus = command( { TIM_ID, RDM, ( X_MEM | 0 ) } );

			if ( containsError( uiStatus ) )
			{
				THROW( "Failed to read controller status: 0x%X", uiStatus );
			}

			//
			// Set the PCI board to synthetic image mode.
			// Set/clear bit 10 of TIM X:0
			if ( bMode )
			{
				uiReply = command( { TIM_ID, WRM, ( X_MEM | 0 ), ( uiStatus | uiClearBitMask ) } );
			}

			else
			{
				uiReply = command( { TIM_ID, WRM, ( X_MEM | 0 ), ( uiStatus & ~uiClearBitMask ) } );
			}

			if ( uiReply != DON )
			{
				if ( bMode )
				{
					THROW( "Controller not set to synthetic image mode." );
				}
				else
				{
					THROW( "Controller not set to normal image mode." );
				}
			}
		}


		// +----------------------------------------------------------------------------
		// |  setOpenShutter
		// +----------------------------------------------------------------------------
		// |  Sets whether or not the shutter will open when an exposure is started
		// |  ( using SEX ).
		// |
		// |  Throws std::runtime_error on error
		// |
		// |  <IN> -> shouldOpen - Set to 'true' if the shutter should open during
		// |                       expose. Set to 'false' to keep the shutter closed.
		// +----------------------------------------------------------------------------
		void CArcDevice::setOpenShutter( bool bShouldOpen )
		{
			std::uint32_t uiMemValue = 0;
			std::uint32_t uiRetVal   = 0;

			uiMemValue = command( { TIM_ID, RDM, ( X_MEM | 0 ) } );

			if ( bShouldOpen )
			{
				uiRetVal = command( { TIM_ID, WRM, ( X_MEM | 0 ), ( uiMemValue | OPEN_SHUTTER_POSITION ) } );
			}	
			
			else
			{
				uiRetVal = command( { TIM_ID, WRM, ( X_MEM | 0 ), ( uiMemValue & CLOSED_SHUTTER_POSITION ) } );
			}

			if ( uiRetVal != DON )
			{
				THROW( "Shutter position failed to be set! reply: 0x%X", uiRetVal );
			}
		}


		// +----------------------------------------------------------------------------
		// |  expose
		// +----------------------------------------------------------------------------
		// |  Starts an exposure using the specified exposure time and whether or not
		// |  to open the shutter. Callbacks for the elapsed exposure time and image
		// |  data readout can be used.
		// |
		// |  Throws std::runtime_error on error
		// |
		// |  <IN> -> fExpTime - The exposure time ( in seconds ).
		// |  <IN> -> bOpenShutter - Set to 'true' if the shutter should open during the
		// |                         exposure. Set to 'false' to keep the shutter closed.
		// |  <IN> -> uiRows - The image row size ( in pixels ).
		// |  <IN> -> uiCols - The image column size ( in pixels ).
		// |  <IN> -> fExpTime - The exposure time ( in seconds ).
		// |  <IN> -> bAbort - Pointer to boolean value that can cause the readout
		// |                   method to abort/stop either exposing or image readout.
		// |                   NULL by default.
		// |  <IN> -> pExpIFace - Function pointer to CExpIFace class. NULL by default.
		// +----------------------------------------------------------------------------
		void CArcDevice::expose( float fExpTime, std::uint32_t uiRows, std::uint32_t uiCols, const bool& bAbort, arc::gen3::CExpIFace* pExpIFace, bool bOpenShutter )
		{
			float			fElapsedTime		= fExpTime;
			bool			bInReadout			= false;
			std::uint32_t   uiTimeoutCounter	= 0;
			std::uint32_t   uiLastPixelCount	= 0;
			std::uint32_t   uiPixelCount		= 0;
			std::uint32_t   uiExposeCounter		= 0;

			//
			// Check for adequate buffer size
			//
			if ( ( static_cast<std::uint64_t>( uiRows ) * static_cast< std::uint64_t >( uiCols ) * sizeof( std::uint16_t ) ) > commonBufferSize() )
			{
				THROW( "Image dimensions [ %u x %u ] exceed buffer size: %u. Try calling ReMapCommonBuffer().", uiCols, uiRows, commonBufferSize() );
			}

			//
			// Set the shutter position
			//
			setOpenShutter( bOpenShutter );

			//
			// Set the exposure time
			//
			auto uiRetVal = command( { TIM_ID, SET, static_cast<std::uint32_t>( fExpTime * 1000.0 ) } );

			if ( uiRetVal != DON )
			{
				THROW( "Set exposure time failed. Reply: 0x%X", uiRetVal );
			}

			//
			// Start the exposure
			//
			uiRetVal = command( { TIM_ID, SEX } );

			if ( uiRetVal != DON )
			{
				THROW( "Start exposure command failed. Reply: 0x%X", uiRetVal );
			}

			while ( uiPixelCount < ( uiRows * uiCols ) )
			{
				if ( isReadout() )
				{
					bInReadout = true;
				}

				// ----------------------------
				// READ ELAPSED EXPOSURE TIME
				// ----------------------------
				// Checking the elapsed time > 1 sec. is to prevent race conditions with
				// sending RET while the PCI board is going into readout. Added check
				// for exposure_time > 1 sec. to prevent RET error.
				if ( !bInReadout && fElapsedTime > 1.1f && uiExposeCounter >= 5 && fExpTime > 1.0f )
				{
					// Ignore all RET timeouts
					try
					{
						// Read the elapsed exposure time.
						uiRetVal = command( { TIM_ID, RET } );

						if ( uiRetVal != ROUT )
						{
							if ( containsError( uiRetVal ) || containsError( uiRetVal, 0, static_cast<std::int32_t>( fExpTime * 1000.f ) ) )
							{
								stopExposure();

								THROW( "Failed to read elapsed time!" );
							}

							if ( bAbort )
							{
								stopExposure();

								THROW( "Expose Aborted!" );
							}

							uiExposeCounter  = 0;
							fElapsedTime    = fExpTime - static_cast<float>( uiRetVal / 1000 );

							if ( pExpIFace != nullptr )
							{
								pExpIFace->exposeCallback( fElapsedTime );
							}
						}
					}
					catch ( ... ) {}
				}

				uiExposeCounter++;

				// ----------------------------
				// READOUT PIXEL COUNT
				// ----------------------------
				if ( bAbort )
				{
					stopExposure();

					THROW( "Expose aborted!" );
				}

				// Save the last pixel count for use by the timeout counter.
				uiLastPixelCount = uiPixelCount;
				uiPixelCount	 = getPixelCount();

				if ( containsError( uiPixelCount ) )
				{
					stopExposure();

					THROW( "Failed to read pixel count!" );
				}

				if ( bAbort )
				{
					stopExposure();

					THROW( "Expose aborted!" );
				}

				if ( bInReadout && pExpIFace != nullptr )
				{
					pExpIFace->readCallback( uiPixelCount );
				}

				if ( bAbort )
				{
					stopExposure();

					THROW( "Expose aborted!" );
				}

				// If the controller's in READOUT, then increment the timeout
				// counter. Checking for readout prevents timeouts when clearing
				// large and/or slow arrays.
				if ( bInReadout && uiPixelCount == uiLastPixelCount )
				{
					uiTimeoutCounter++;
				}
				else
				{
					uiTimeoutCounter = 0;
				}

				if ( bAbort )
				{
					stopExposure();

					THROW( "Expose aborted!" );
				}

				if ( uiTimeoutCounter >= READ_TIMEOUT )
				{
					stopExposure();

					THROW( "Read timeout!" );
				}

				std::this_thread::sleep_for( std::chrono::milliseconds( 25 ) );
			}
		}


		// +----------------------------------------------------------------------------
		// |  expose -- Caltech
		// +----------------------------------------------------------------------------
		// |  Starts an exposure using the specified exposure time and whether or not
		// |  to open the shutter. Callbacks for the elapsed exposure time and image
		// |  data readout can be used.
		// |
		// |  Throws std::runtime_error on error
		// |
		// |  <IN> -> fExpTime - The exposure time ( in seconds ).
		// |  <IN> -> bOpenShutter - Set to 'true' if the shutter should open during the
		// |                         exposure. Set to 'false' to keep the shutter closed.
		// |  <IN> -> uiRows - The image row size ( in pixels ).
		// |  <IN> -> uiCols - The image column size ( in pixels ).
		// |  <IN> -> uiExpTime - The exposure time ( in milliseconds ).
		// |  <IN> -> bAbort - Pointer to boolean value that can cause the readout
		// |                   method to abort/stop either exposing or image readout.
		// |                   NULL by default.
		// |  <IN> -> pExpIFace - Function pointer to CooExpIFace class. NULL by default.
		// +----------------------------------------------------------------------------
		void CArcDevice::expose( int devnum, const std::uint32_t &uiExpTime, std::uint32_t uiRows, std::uint32_t uiCols, const bool& bAbort, arc::gen3::CooExpIFace* pCooExpIFace, bool bOpenShutter )
		{
			std::uint32_t	uiElapsedTime		= 0;
			std::uint32_t	uiExposureTime		= 0;
			std::uint32_t	uiRetTime		= uiExpTime;
			bool		bInReadout		= false;
			bool		bOnce			= true;
			std::uint32_t	uiTimeoutCounter	= 0;
			std::uint32_t	uiLastPixelCount	= 0;
			std::uint32_t	uiPixelCount		= 0;
			std::uint32_t	uiExposeCounter		= 0;
			std::uint32_t	uiFPBCount		= 0;
			std::uint32_t	uiPCIFrameCount		= 0;
			std::uint32_t uiImageSize         	= uiRows * uiCols * sizeof( std::uint16_t );
			std::uint32_t uiBoundedImageSize  	= getContinuousImageSize( uiImageSize );

			//
			// Check for adequate buffer size
			//
			if ( ( static_cast<std::uint64_t>( uiRows ) * static_cast< std::uint64_t >( uiCols ) * sizeof( std::uint16_t ) ) > commonBufferSize() )
			{
				THROW( "(arc::gen3::CArcDevice::expose) Image [ %u x %u ] exceeds buffer size: %u. Try ReMapCommonBuffer().", uiCols, uiRows, commonBufferSize() );
			}

			//
			// Set the shutter position
			//
			setOpenShutter( bOpenShutter );

			//
			// Start the exposure
			//
			auto uiRetVal = command( { TIM_ID, SEX } );

			if ( uiRetVal != DON )
			{
				THROW( "(arc::gen3::CArcDevice::expose) Start exposure command failed. Reply: 0x%X", uiRetVal );
			}

			while ( uiPixelCount < ( uiRows * uiCols ) )
			{
				if ( isReadout() )
				{
					bInReadout = true;
				}

				// ----------------------------
				// READ ELAPSED EXPOSURE TIME
				// ----------------------------
				// Checking the remaining time > 1 sec. is to prevent race conditions with
				// sending RET while the PCI board is going into readout. Added check
				// for exposure_time > 1 sec. to prevent RET error.
				if ( !bInReadout && uiRetTime > 1000 && uiExposeCounter >= 5 && uiExpTime > 1000 )
				{
					// Ignore all RET timeouts
					try
					{
						// Read the elapsed exposure time.
						uiElapsedTime = command( { TIM_ID, RET } );

						// Read the exposure time (if supported)
						uiExposureTime = command( { TIM_ID, GET } );
						if ( uiExposureTime==ERR ) uiExposureTime=0x1BAD1BAD;

						if ( uiElapsedTime != ROUT )
						{
							if ( containsError( uiElapsedTime ) || containsError( uiElapsedTime, 0, uiExpTime ) )
							{
								stopExposure();
								THROW( "(arc::gen3::CArcDevice::expose) Failed to read elapsed time!" );
							}

							if ( bAbort )
							{
								stopExposure();
								THROW( "(arc::gen3::CArcDevice::expose) aborted" );
							}

							uiExposeCounter  = 0;
							uiRetTime = uiExpTime - uiElapsedTime;

							if ( pCooExpIFace != nullptr )
							{
								pCooExpIFace->exposeCallback( devnum, uiElapsedTime, uiExposureTime );
							}
						}
					}
					catch ( ... ) {}
				}
				else
				if ( bInReadout && bOnce )
				{
					bOnce = false;
					if ( pCooExpIFace != nullptr )
					{
						pCooExpIFace->exposeCallback( devnum, uiExpTime, uiExposureTime );
					}
				}

				uiExposeCounter++;

				// ----------------------------
				// READOUT PIXEL COUNT
				// ----------------------------
				if ( bAbort )
				{
					stopExposure();
					THROW( "(arc::gen3::CArcDevice::expose) aborted" );
				}

				// Save the last pixel count for use by the timeout counter.
				uiLastPixelCount = uiPixelCount;
				uiPixelCount     = getPixelCount();

				if ( containsError( uiPixelCount ) )
				{
					stopExposure();
					THROW( "(arc::gen3::CArcDevice::expose) Failed to read pixel count!" );
				}

				if ( bAbort )
				{
					stopExposure();
					THROW( "(arc::gen3::CArcDevice::expose) aborted" );
				}

				if ( bInReadout && pCooExpIFace != nullptr )
				{
					pCooExpIFace->readCallback( 0, devnum, uiPixelCount, uiRows*uiCols );
				}

				if ( bAbort )
				{
					stopExposure();
					THROW( "(arc::gen3::CArcDevice::expose) aborted" );
				}

				// If the controller's in READOUT, then increment the timeout
				// counter. Checking for readout prevents timeouts when clearing
				// large and/or slow arrays.
				if ( bInReadout && uiPixelCount == uiLastPixelCount )
				{
					uiTimeoutCounter++;
				}
				else
				{
					uiTimeoutCounter = 0;
				}

				if ( bAbort )
				{
					stopExposure();
					THROW( "(arc::gen3::CArcDevice::expose) aborted" );
				}

				if ( uiTimeoutCounter >= READ_TIMEOUT )
				{
					stopExposure();
					THROW( "(arc::gen3::CArcDevice::expose) Read timeout!" );
				}

				std::this_thread::sleep_for( std::chrono::milliseconds( 25 ) );
			}

			uiPCIFrameCount = getFrameCount();

			// Call external deinterlace and fits file functions here
			if ( pCooExpIFace != nullptr )
			{
				pCooExpIFace->frameCallback( 0, devnum, 
							  uiFPBCount,
							  uiPCIFrameCount,
							  uiRows,
							  uiCols,
							  ( commonBufferVA() + static_cast<std::uint64_t>( uiFPBCount ) * static_cast< std::uint64_t >( uiBoundedImageSize ) ) );
			}
		}


		// +----------------------------------------------------------------------------
		// |  readout -- Caltech
		// +----------------------------------------------------------------------------
		// |  Starts the readout waveforms in the controller.
		// |
		// |  Throws std::runtime_error on error
		// |
		// |  <IN> -> expbuf - exposure buffer number
		// |  <IN> -> devnum - device number
		// |  <IN> -> uiRows - The image row size ( in pixels ).
		// |  <IN> -> uiCols - The image column size ( in pixels ).
		// |  <IN> -> bAbort - Pointer to boolean value that can cause the readout
		// |                   method to abort/stop either exposing or image readout.
		// |                   NULL by default.
		// |  <IN> -> pExpIFace - Function pointer to CooExpIFace class. NULL by default.
		// +----------------------------------------------------------------------------
		void CArcDevice::readout( int expbuf, int devnum, std::uint32_t uiRows, std::uint32_t uiCols, const bool &bAbort, arc::gen3::CooExpIFace* pCooExpIFace )
		{
			bool		bInReadout		= false;
			std::uint32_t	uiTimeoutCounter	= 0;
			std::uint32_t	uiLastPixelCount	= 0;
			std::uint32_t	uiPixelCount		= 0;
			std::uint32_t	uiFPBCount		= 0;
			std::uint32_t	uiPCIFrameCount		= 0;
			std::uint32_t uiImageSize         	= uiRows * uiCols * sizeof( std::uint16_t );
			std::uint32_t uiBoundedImageSize  	= getContinuousImageSize( uiImageSize );

			std::cerr << "\n(arc::gen3::CArcDevice::readout) expbuf:" << expbuf << " devnum:" << devnum << "\n";

			//
			// Check for adequate buffer size
			//
			if ( ( static_cast<std::uint64_t>( uiRows ) * static_cast< std::uint64_t >( uiCols ) * sizeof( std::uint16_t ) ) > commonBufferSize() )
			{
				std::cerr << "(arc::gen3::CArcDevice::readout) devnum " << devnum
					<< " Image [" << uiCols << "x" << uiRows << "] exceeds buffer size: " << commonBufferSize() << "\n";
				THROW( "(arc::gen3::CArcDevice::readout) Image [ %u x %u ] exceeds buffer size: %u. Try ReMapCommonBuffer().", uiCols, uiRows, commonBufferSize() );
			}

			if ( isReadout() ) {
				std::cerr << "(arc::gen3::CArcDevice::readout) devnum " << devnum << " already in readout\n";
				THROW( "(arc::gen3::CArcDevice::readout) already in readout" );
			}

			//
			// Start the readout
			//
			auto uiRetVal = command( { TIM_ID, SRE } );

			if ( uiRetVal != DON )
			{
				std::cerr << "(arc::gen3::CArcDevice::readout) devnum " << devnum << " start readout command failed\n";
				THROW( "(arc::gen3::CArcDevice::readout) Start readout command failed. Reply: 0x%X", uiRetVal );
			}

			uiPixelCount     = getPixelCount();

      // reduce the number of PixelCount callbacks by this factor
      //
      const int throttle_pixelcount_callbacks=10;
      int callback_count = throttle_pixelcount_callbacks;

			while ( uiPixelCount < ( uiRows * uiCols ) )
			{
				if ( isReadout() )
				{
					bInReadout = true;
				}

				// ----------------------------
				// READOUT PIXEL COUNT
				// ----------------------------

				// Save the last pixel count for use by the timeout counter.
				uiLastPixelCount = uiPixelCount;
				uiPixelCount     = getPixelCount();

				if ( containsError( uiPixelCount ) )
				{
					stopExposure();
					std::cerr << "(arc::gen3::CArcDevice::readout) devnum " << devnum << " failed to read pixel count\n";
					THROW( "(arc::gen3::CArcDevice::readout) Failed to read pixel count!" );
				}

				if ( bAbort )
				{
					stopExposure();
					std::cerr << "(arc::gen3::CArcDevice::readout) devnum " << devnum << " aborted\n";
					THROW( "(arc::gen3::CArcDevice::readout) aborted" );
				}

				if ( bInReadout && pCooExpIFace != nullptr && --callback_count==0 )
				{
					pCooExpIFace->readCallback( expbuf, devnum, uiPixelCount, uiRows*uiCols );
          callback_count = throttle_pixelcount_callbacks;
				}

				// If the controller's in READOUT, then increment the timeout
				// counter. Checking for readout prevents timeouts when clearing
				// large and/or slow arrays.
				if ( bInReadout && uiPixelCount == uiLastPixelCount )
				{
					uiTimeoutCounter++;
				}
				else
				{
					uiTimeoutCounter = 0;
				}

				if ( uiTimeoutCounter >= READ_TIMEOUT )
				{
					stopExposure();
					std::cerr << "(arc::gen3::CArcDevice::readout) devnum " << devnum << " read timeout\n";
					THROW( "(arc::gen3::CArcDevice::readout) Read timeout!" );
				}

				std::this_thread::sleep_for( std::chrono::milliseconds( 25 ) );
			}

			pCooExpIFace->readCallback( expbuf, devnum, uiPixelCount, uiRows*uiCols );

			uiPCIFrameCount = getFrameCount();

			// Call external deinterlace and fits file functions here
			if ( pCooExpIFace != nullptr )
			{
				pCooExpIFace->frameCallback( expbuf,
							  devnum,
							  uiFPBCount,
							  uiPCIFrameCount,
							  uiRows,
							  uiCols,
							  commonBufferVA() );
			}
			return;
		}


		// +----------------------------------------------------------------------------
		// |  frame_transfer -- Caltech
		// +----------------------------------------------------------------------------
		void CArcDevice::frame_transfer( int expbuf, int devnum, std::uint32_t uiRows, std::uint32_t uiCols, arc::gen3::CooExpIFace* pCooExpIFace )
		{
			std::uint32_t uiRetVal;

			std::cerr << "\n(arc::gen3::CArcDevice::frame_transfer) expbuf:" << expbuf << " devnum:" << devnum << "\n";

			// set Y:IN_FT = 2 for "pending"
			// FRAME_TRANSFER subroutine will set this to 1 on entry, then 0 on exit.
			//
			uiRetVal = command( { TIM_ID, WRM, ( Y_MEM | 0x25 ), 2 } );

			if ( uiRetVal != DON )
			{
				std::cerr << "\n(arc::gen3::CArcDevice::frame_transfer) Failed to set FrameTransferState for dev " << devnum << "\n";
				THROW( "(arc::gen3::CArcDevice::frame_transfer) Failed to set FrameTransferState. reply: 0x%X", uiRetVal );
			}

			// Trigger the FRame Transfer waveforms
			//
			uiRetVal = command( { TIM_ID, FRT } );

			if ( uiRetVal != DON )
			{
				std::cerr << "\n(arc::gen3::CArcDevice::frame_transfer) Frame Transfer command (FRT) failed for dev " << devnum << "\n";
				THROW( "(arc::gen3::CArcDevice::frame_transfer) Frame Transfer command (FRT) failed. Reply: 0x%X", uiRetVal );
			}

			auto FrameTransferState = command( { TIM_ID, RDM, ( Y_MEM | 0x25 ) } );

			int to=0;
			while( FrameTransferState != 0 ) {
				FrameTransferState = command( { TIM_ID, RDM, ( Y_MEM | 0x25 ) } );
				if ( FrameTransferState != 0 ) std::this_thread::sleep_for( std::chrono::microseconds( 1 ) );
				if ( ++to > 10 ) {
					std::cerr << "\n(arc::gen3::CArcDevice::frame_transfer) timeout exceeded waiting for Frame Transfer to end for dev " << devnum << "\n";
					THROW( "(arc::gen3::CArcDevice::frame_transfer) timeout exceeded waiting for Frame Transfer to end" );
				}
			}
			std::cerr << "(arc::gen3::CArcDevice::frame_transfer) FRAME_TRANSFER complete expbuf:" << expbuf << " devnum:" << devnum << "\n";

			// Generate Callback
			//
			if ( pCooExpIFace != nullptr )
			{
				std::cerr << "(arc::gen3::CArcDevice::frame_transfer) calling ftCallback with devnum=" << devnum << "\n\n";

				pCooExpIFace->ftCallback( expbuf, devnum );
			}

			std::cerr << "(arc::gen3::CArcDevice::frame_transfer) dev " << devnum << " returning\n";
			return;
		}


		// +----------------------------------------------------------------------------
		// |  continuous
		// +----------------------------------------------------------------------------
		// |  This method can be called to start continuous readout.  A callback for 
		// |  each frame read out can be used to process the frame.
		// |
		// |  Throws std::runtime_error on error
		// |
		// |  <IN> -> uiRows - The image row size ( in pixels ).
		// |  <IN> -> uiCols - The image column size ( in pixels ).
		// |  <IN> -> uiNumOfFrames - The number of frames to take.
		// |  <IN> -> fExpTime - The exposure time ( in seconds ).
		// |  <IN> -> bAbort - 'true' to cause the readout method to abort/stop either
		// |                    exposing or image readout. Default: false
		// |  <IN> -> pConIFace - Function pointer to callback for frame completion.
		// |                       NULL by default.
		// |  <IN> -> bOpenShutter - 'true' to open the shutter during expose; 'false'
		// |                         otherwise.
		// +----------------------------------------------------------------------------
		void CArcDevice::continuous( std::uint32_t uiRows, std::uint32_t uiCols, std::uint32_t uiNumOfFrames, float fExpTime, const bool& bAbort, arc::gen3::CConIFace* pConIFace, bool bOpenShutter )
		{
			std::uint32_t uiFramesPerBuffer   = 0;
			std::uint32_t uiPCIFrameCount     = 0;
			std::uint32_t uiLastPCIFrameCount = 0;
			std::uint32_t uiFPBCount          = 0;

			std::uint32_t uiImageSize         = uiRows * uiCols * sizeof( std::uint16_t );
			std::uint32_t uiBoundedImageSize  = getContinuousImageSize( uiImageSize );

			//
			// Check for adequate buffer size
			//
			if ( uiImageSize > commonBufferSize() )
			{
				THROW( "Image dimensions [ %u x %u ] exceed buffer size: %u. %Try calling ReMapCommonBuffer().", uiCols, uiRows, commonBufferSize() );
			}

			//
			// Check for valid frame count
			//
			if ( uiNumOfFrames == 0 )
			{
				THROW( "Number of frames must be > 0" );
			}

			if ( bAbort )
			{
				THROW( "Continuous readout aborted by user!" );
			}

			uiFramesPerBuffer = static_cast<std::uint32_t>( floor( static_cast<float>( commonBufferSize() / uiBoundedImageSize ) ) );

			if ( bAbort )
			{
				THROW( "Continuous readout aborted by user!" );
			}

			try
			{
				// Set the frames-per-buffer
				auto uiRetVal = command( { TIM_ID, FPB, uiFramesPerBuffer } );

				if ( uiRetVal != DON )
				{
					THROW( "Failed to set the frames per buffer (FPB). Reply: 0x%X", uiRetVal );
				}

				if ( bAbort )
				{
					THROW( "Continuous readout aborted by user!" );
				}

				// Set the number of frames-to-take
				uiRetVal = command( { TIM_ID, SNF, uiNumOfFrames } );

				if ( uiRetVal != DON )
				{
					THROW( "Failed to set the number of frames (SNF). Reply: 0x%X", uiRetVal );
				}

				if ( bAbort )
				{
					THROW( "Continuous readout aborted by user!" );
				}

				//
				// Set the shutter position
				//
				setOpenShutter( bOpenShutter );

				//
				// Set the exposure time
				//
				std::uint32_t uiExpTime = static_cast<std::uint32_t>( fExpTime * 1000.0 );

				uiRetVal = command( { TIM_ID, SET, uiExpTime } );

				if ( uiRetVal != DON )
				{
					THROW( "Set exposure time failed. Reply: 0x%X", uiRetVal );
				}

				//
				// Start the exposure
				//
				uiRetVal = command( { TIM_ID, SEX } );

				if ( uiRetVal != DON )
				{
					THROW( "Start exposure command failed. Reply: 0x%X", uiRetVal );
				}

				if ( bAbort )
				{
					THROW( "Continuous readout aborted by user!" );
				}

				// Read the images
				while ( uiPCIFrameCount < uiNumOfFrames )
				{
					if ( bAbort )
					{
						THROW( "Continuous readout aborted by user!" );
					}

					uiPCIFrameCount = getFrameCount();

					if ( bAbort )
					{
						THROW( "Continuous readout aborted by user!" );
					}

					if ( uiFPBCount >= uiFramesPerBuffer )
					{
							uiFPBCount = 0;
					}

					if ( uiPCIFrameCount > uiLastPCIFrameCount )
					{
						// Call external deinterlace and fits file functions here
						if ( pConIFace != nullptr )
						{
							pConIFace->frameCallback( uiFPBCount,
													  uiPCIFrameCount,
													  uiRows,
													  uiCols,
													  ( commonBufferVA() + static_cast<std::uint64_t>( uiFPBCount ) * static_cast< std::uint64_t >( uiBoundedImageSize ) ) );
						}

						uiLastPCIFrameCount = uiPCIFrameCount;
	
						uiFPBCount++;
					}
				}

				// Set back to single image mode
				uiRetVal = command( { TIM_ID, SNF, 1 } );

				if ( uiRetVal != DON )
				{
					THROW( "Failed to set number of frames (SNF) to 1. Reply: 0x%X", uiRetVal );
				}
			}
			catch ( ... )
			{
				// Set back to single image mode
				stopContinuous();

				throw std::current_exception;
			}
		}


		// +----------------------------------------------------------------------------
		// |  stopContinuous
		// +----------------------------------------------------------------------------
		// |  Sends abort expose/readout and sets the controller back into single
		// |  read mode.
		// |
		// |  Throws std::runtime_error on error
		// +----------------------------------------------------------------------------
		void CArcDevice::stopContinuous( void )
		{
			stopExposure();

			auto uiRetVal = command( { TIM_ID, SNF, 1 } );

			if ( uiRetVal != DON )
			{
				THROW( "Failed to set number of frames ( SNF ) to 1. Reply: 0x%X", uiRetVal );
			}
		}


		// +----------------------------------------------------------------------------
		// |  Check the specified value for error replies:
		// |  TOUT, ROUT, HERR, ERR, SYR, RST
		// +----------------------------------------------------------------------------
		// |  Returns 'true' if the specified value matches 'TOUT, 'ROUT', 'HERR',
		// |  'ERR', 'SYR' or 'RST'. Returns 'false' otherwise.
		// |
		// |  <IN> -> uiWord - The value to check
		// +----------------------------------------------------------------------------
		bool CArcDevice::containsError( std::uint32_t uiWord )
		{
			bool bError = false; 

			if ( uiWord == TOUT || uiWord == ERR || uiWord == SYR  ||
				 uiWord == CNR  || uiWord == RST || uiWord == ROUT ||
				 uiWord == HERR )
			{
				bError = true;
			}

			return bError;
		}


		// +----------------------------------------------------------------------------
		// |  Check that the specified value falls within the specified range.
		// +----------------------------------------------------------------------------
		// |  Returns 'true' if the specified value falls within the specified range.
		// |  Returns 'false' otherwise.
		// |
		// |  <IN> -> uiWord    - The value to check
		// |  <IN> -> uiWordMin - The minimum range value ( not inclusive )
		// |  <IN> -> uiWordMax - The maximum range value ( not inclusive )
		// +----------------------------------------------------------------------------
		bool CArcDevice::containsError( std::uint32_t uiWord, std::uint32_t uiWordMin, std::uint32_t uiWordMax )
		{
			bool bError = false;

			if ( uiWord < uiWordMin || uiWord > uiWordMax )
			{
				bError = true;
			}

			return bError;
		}


		// +----------------------------------------------------------------------------
		// |  getNextLoggedCmd
		// +----------------------------------------------------------------------------
		// |  Pops the first message from the command logger and returns it. If the
		// |  logger is empty, then NULL is returned.
		// +----------------------------------------------------------------------------
		const std::string CArcDevice::getNextLoggedCmd( void )
		{
			if ( !m_pCLog->empty() )
			{
				return std::string( m_pCLog->getNext() );
			}

			else
			{
				return std::string( "" );
			}
		}


		// +----------------------------------------------------------------------------
		// |  getLoggedCmdCount
		// +----------------------------------------------------------------------------
		// |  Returns the available command message count.
		// +----------------------------------------------------------------------------
		std::int32_t CArcDevice::getLoggedCmdCount( void )
		{
			return m_pCLog->getLogCount();
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
		// |  <IN> -> bOnOff - 'true' to turn loggin on; 'false' otherwise.
		// +----------------------------------------------------------------------------
		void CArcDevice::setLogCmds( bool bOnOff )
		{
			m_bStoreCmds = bOnOff;
		}


		////////////////////////////////////////////////////////////////////////////////
		//	TEMPERATURE
		////////////////////////////////////////////////////////////////////////////////
		// +----------------------------------------------------------------------------
		// |  loadTemperatureCtrlData
		// +----------------------------------------------------------------------------
		// |  Loads temperature control constants from the specified file.  The default
		// |  constants are stored in TempCtrl.h and cannot be permanently overwritten.
		// |
		// |  Throws std::runtime_error on error
		// |
		// |  <IN> -> sFilename  - The filename of the temperature control constants
		// |                       file.
		// +----------------------------------------------------------------------------
		void CArcDevice::loadTemperatureCtrlData( const std::string& sFilename )
		{
			std::ifstream ifs( sFilename.c_str() );

			std::string sBuf;

			if ( ifs.is_open() )
			{
				while ( !ifs.eof() )
				{
					getline( ifs, sBuf );

					if ( sBuf.find( "//" ) != std::string::npos )
					{
						continue;
					}

					if ( sBuf.find( TMPCTRL_DT670_COEFF_1_KEY ) != std::string::npos )
					{
						getline( ifs, sBuf );
						m_gTmpCtrl_DT670Coeff1 = std::atof( sBuf.c_str() );
					}

					else if ( sBuf.find( TMPCTRL_DT670_COEFF_2_KEY ) != std::string::npos )
					{
						getline( ifs, sBuf );
						m_gTmpCtrl_DT670Coeff2 = std::atof( sBuf.c_str() );
					}

					else if ( sBuf.find( TMPCTRL_SDADU_OFFSET_KEY ) != std::string::npos )
					{
						getline( ifs, sBuf );
						m_gTmpCtrl_SDAduOffset = std::atof( sBuf.c_str() );
					}

					else if ( sBuf.find( TMPCTRL_SDADU_PER_VOLT_KEY ) != std::string::npos )
					{
						getline( ifs, sBuf );
						m_gTmpCtrl_SDAduPerVolt = std::atof( sBuf.c_str() );
					}

					else if ( sBuf.find( TMPCTRL_HGADU_OFFSET_KEY ) != std::string::npos )
					{
						getline( ifs, sBuf );
						m_gTmpCtrl_HGAduOffset = std::atof( sBuf.c_str() );
					}

					else if ( sBuf.find( TMPCTRL_HGADU_PER_VOLT_KEY ) != std::string::npos )
					{
						getline( ifs, sBuf );
						m_gTmpCtrl_HGAduPerVolt = std::atof( sBuf.c_str() );
					}

					else if ( sBuf.find( TMPCTRL_SDNUMBER_OF_READS_KEY ) != std::string::npos )
					{
						getline( ifs, sBuf );
						m_gTmpCtrl_SDNumberOfReads = atoi( sBuf.c_str() );
					}

					else if ( sBuf.find( TMPCTRL_SDVOLT_TOLERANCE_TRIALS_KEY ) != std::string::npos )
					{
						getline( ifs, sBuf );
						m_gTmpCtrl_SDVoltToleranceTrials = atoi( sBuf.c_str() );
					}

					else if ( sBuf.find( TMPCTRL_SDVOLT_TOLERANCE_KEY ) != std::string::npos )
					{
						getline( ifs, sBuf );
						m_gTmpCtrl_SDVoltTolerance = std::atof( sBuf.c_str() );
					}

					else if ( sBuf.find( TMPCTRL_SDDEG_TOLERANCE_KEY ) != std::string::npos )
					{
						getline( ifs, sBuf );
						m_gTmpCtrl_SDDegTolerance = std::atof( sBuf.c_str() );
					}

					else if ( sBuf.find( TMPCTRL_SD2_12K_COEFF_KEY ) != std::string::npos )
					{
						getline( ifs, sBuf );
						m_tTmpCtrl_SD_2_12K.vu = std::atof( sBuf.c_str() );

						getline( ifs, sBuf );
						m_tTmpCtrl_SD_2_12K.vl = std::atof( sBuf.c_str() );

						getline( ifs, sBuf );
						m_tTmpCtrl_SD_2_12K.count = atoi( sBuf.c_str() );

						for ( int i=0; i<m_tTmpCtrl_SD_2_12K.count; i++ )
						{
							getline( ifs, sBuf );
							m_tTmpCtrl_SD_2_12K.coeff[ i ] = std::atof( sBuf.c_str() );
						}
					}

					else if ( sBuf.find( TMPCTRL_SD12_24K_COEFF_KEY ) != std::string::npos )
					{
						getline( ifs, sBuf );
						m_tTmpCtrl_SD_12_24K.vu = std::atof( sBuf.c_str() );

						getline( ifs, sBuf );
						m_tTmpCtrl_SD_12_24K.vl = std::atof( sBuf.c_str() );

						getline( ifs, sBuf );
						m_tTmpCtrl_SD_12_24K.count = atoi( sBuf.c_str() );

						for ( int i=0; i<m_tTmpCtrl_SD_12_24K.count; i++ )
						{
							getline( ifs, sBuf );
							m_tTmpCtrl_SD_12_24K.coeff[ i ] = std::atof( sBuf.c_str() );
						}
					}

					else if ( sBuf.find( TMPCTRL_SD24_100K_COEFF_KEY ) != std::string::npos )
					{
						getline( ifs, sBuf );
						m_tTmpCtrl_SD_24_100K.vu = std::atof( sBuf.c_str() );

						getline( ifs, sBuf );
						m_tTmpCtrl_SD_24_100K.vl = std::atof( sBuf.c_str() );

						getline( ifs, sBuf );
						m_tTmpCtrl_SD_24_100K.count = atoi( sBuf.c_str() );

						for ( int i=0; i<m_tTmpCtrl_SD_24_100K.count; i++ )
						{
							getline( ifs, sBuf );
							m_tTmpCtrl_SD_24_100K.coeff[ i ] = std::atof( sBuf.c_str() );
						}
					}

					else if ( sBuf.find( TMPCTRL_SD100_475K_COEFF_KEY ) != std::string::npos )
					{
						getline( ifs, sBuf );
						m_tTmpCtrl_SD_100_475K.vu = std::atof( sBuf.c_str() );

						getline( ifs, sBuf );
						m_tTmpCtrl_SD_100_475K.vl = std::atof( sBuf.c_str() );

						getline( ifs, sBuf );
						m_tTmpCtrl_SD_100_475K.count = atoi( sBuf.c_str() );

						for ( int i=0; i<m_tTmpCtrl_SD_100_475K.count; i++ )
						{
							getline( ifs, sBuf );
							m_tTmpCtrl_SD_100_475K.coeff[ i ] = std::atof( sBuf.c_str() );
						}
					}
				}
			}

			else
			{
				THROW( "Failed to open temperature control file: %s", sFilename.c_str() );
			}

			ifs.close();
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
		// |  <IN> -> sFilename  - The filename of the temperature control constants
		// |                       file to save.
		// +----------------------------------------------------------------------------
		void CArcDevice::saveTemperatureCtrlData( const std::string& sFilename )
		{
			std::ofstream ofs( sFilename.c_str() );

			if ( ofs.is_open() )
			{
				ofs.precision( 10 );

				ofs << "// _____________________________________________________________" << std::endl;
				ofs << "//" << std::endl;
				ofs << "// TEMPERATURE CONTROL FILE" << std::endl;
				ofs << "// _____________________________________________________________" << std::endl;
				ofs << std::endl;

				ofs << "// +-----------------------------------------------------------" << std::endl;
				ofs << "// | Define Temperature Coeffients for DT-670 Sensor (SmallCam)" << std::endl;
				ofs << "// |" << std::endl;
				ofs << "// | [TMPCTRL_DT670_COEFF_1] - DT-670 coefficient #1." << std::endl;
				ofs << "// |" << std::endl;
				ofs << "// | [TMPCTRL_DT670_COEFF_2] - DT-670 coefficient #2." << std::endl;
				ofs << "// +-----------------------------------------------------------" << std::endl;
				ofs << TMPCTRL_DT670_COEFF_1_KEY << std::endl;
				ofs << m_gTmpCtrl_DT670Coeff1 << std::endl << std::endl;

				ofs << TMPCTRL_DT670_COEFF_2_KEY << std::endl;
				ofs << m_gTmpCtrl_DT670Coeff2 << std::endl << std::endl;

				ofs << "// +-----------------------------------------------------------" << std::endl;
				ofs << "// | Define Temperature Coeffients for CY7 Sensor" << std::endl;
				ofs << "// |" << std::endl;
				ofs << "// | [TMPCTRL_SDADU_OFFSET] - The standard silicon diode ADU" << std::endl;
				ofs << "// | offset." << std::endl;
				ofs << "// |" << std::endl;
				ofs << "// | [TMPCTRL_SDADU_PER_VOLT] - The standard silicon diode" << std::endl;
				ofs << "// | ADU / Volt." << std::endl;
				ofs << "// +-----------------------------------------------------------" << std::endl;
				ofs << TMPCTRL_SDADU_OFFSET_KEY << std::endl;
				ofs << m_gTmpCtrl_SDAduOffset << std::endl << std::endl;

				ofs << TMPCTRL_SDADU_PER_VOLT_KEY << std::endl;
				ofs << m_gTmpCtrl_SDAduPerVolt << std::endl << std::endl;

				ofs << "// +-----------------------------------------------------------" << std::endl;
				ofs << "// | Define Temperature Coeffients for High Gain Utility Board" << std::endl;
				ofs << "// |" << std::endl;
				ofs << "// | [TMPCTRL_HGADU_OFFSET] - The high gain utility board ADU" << std::endl;
				ofs << "// | offset." << std::endl;
				ofs << "// |" << std::endl;
				ofs << "// | [TMPCTRL_HGADU_PER_VOLT] - The high gain utility board" << std::endl;
				ofs << "// | ADU / Volt." << std::endl;
				ofs << "// +-----------------------------------------------------------" << std::endl;
				ofs << TMPCTRL_HGADU_OFFSET_KEY << std::endl;
				ofs << m_gTmpCtrl_HGAduOffset << std::endl << std::endl;

				ofs << TMPCTRL_HGADU_PER_VOLT_KEY << std::endl;
				ofs << m_gTmpCtrl_HGAduPerVolt << std::endl << std::endl;

				ofs << "// + ----------------------------------------------------------" << std::endl;
				ofs << "// | Define temperature control variables" << std::endl;
				ofs << "// |" << std::endl;
				ofs << "// | [TMPCTRL_SDNUMBER_OF_READS] - The number of temperature" << std::endl;
				ofs << "// | reads to average for each GetArrayTemperature call." << std::endl;
				ofs << "// |" << std::endl;
				ofs << "// | [TMPCTRL_SDVOLT_TOLERANCE_TRIALS] - The number of times to" << std::endl;
				ofs << "// | calculate the voltage." << std::endl;
				ofs << "// |" << std::endl;
				ofs << "// | [TMPCTRL_SDVOLT_TOLERANCE] - The voltage tolerance in" << std::endl;
				ofs << "// | volts." << std::endl;
				ofs << "// |" << std::endl;
				ofs << "// | [TMPCTRL_SDDEG_TOLERANCE] - The temperature tolerance in" << std::endl;
				ofs << "// | degrees celcius." << std::endl;
				ofs << "// + ----------------------------------------------------------" << std::endl;
				ofs << TMPCTRL_SDNUMBER_OF_READS_KEY << std::endl;
				ofs << m_gTmpCtrl_SDNumberOfReads << std::endl << std::endl;

				ofs << TMPCTRL_SDVOLT_TOLERANCE_TRIALS_KEY << std::endl;
				ofs << m_gTmpCtrl_SDVoltToleranceTrials << std::endl << std::endl;

				ofs << TMPCTRL_SDVOLT_TOLERANCE_KEY << std::endl;
				ofs << m_gTmpCtrl_SDVoltTolerance << std::endl << std::endl;

				ofs << TMPCTRL_SDDEG_TOLERANCE_KEY << std::endl;
				ofs << m_gTmpCtrl_SDDegTolerance << std::endl << std::endl;

				ofs << "// +-----------------------------------------------------------" << std::endl;
				ofs << "// | Define Temperature Coeffients for Non-linear Silicone" << std::endl;
				ofs << "// | Diode ( SD )" << std::endl;
				ofs << "// |" << std::endl;
				ofs << "// | [Coeff Set Name] can be one of the following:" << std::endl;
				ofs << "// | --------------------------------------------" << std::endl;
				ofs << "// | [TMPCTRL_SD2_12K_COEFF]    - Coefficients for 2 - 12K" << std::endl;
				ofs << "// | [TMPCTRL_SD12_24K_COEFF]   - Coefficients for 12 - 24K" << std::endl;
				ofs << "// | [TMPCTRL_SD24_100K_COEFF]  - Coefficients for 24 - 100K" << std::endl;
				ofs << "// | [TMPCTRL_SD100_475K_COEFF] - Coefficients for 100 - 475K" << std::endl;
				ofs << "// |" << std::endl;
				ofs << "// | FORMAT:" << std::endl;
				ofs << "// | ---------------------" << std::endl;
				ofs << "// | [Coeff Set Name]" << std::endl;
				ofs << "// | Voltage Upper Limit" << std::endl;
				ofs << "// | Voltage Lower Limit" << std::endl;
				ofs << "// | Coefficient Count (N)" << std::endl;
				ofs << "// | Coefficient #1" << std::endl;
				ofs << "// | ..." << std::endl;
				ofs << "// | Coefficient #N" << std::endl;
				ofs << "// +-----------------------------------------------------------" << std::endl;
				ofs << TMPCTRL_SD2_12K_COEFF_KEY << std::endl;
				ofs << m_tTmpCtrl_SD_2_12K.vu << std::endl;
				ofs << m_tTmpCtrl_SD_2_12K.vl << std::endl;
				ofs << m_tTmpCtrl_SD_2_12K.count << std::endl;

				for ( auto i = 0; i < m_tTmpCtrl_SD_2_12K.count; i++ )
				{
					ofs << m_tTmpCtrl_SD_2_12K.coeff[ i ] << std::endl;
				}
				ofs << std::endl;

				ofs << TMPCTRL_SD12_24K_COEFF_KEY << std::endl;
				ofs << m_tTmpCtrl_SD_12_24K.vu << std::endl;
				ofs << m_tTmpCtrl_SD_12_24K.vl << std::endl;
				ofs << m_tTmpCtrl_SD_12_24K.count << std::endl;

				for ( auto i = 0; i < m_tTmpCtrl_SD_12_24K.count; i++ )
				{
					ofs << m_tTmpCtrl_SD_12_24K.coeff[ i ] << std::endl;
				}
				ofs << std::endl;

				ofs << TMPCTRL_SD24_100K_COEFF_KEY << std::endl;
				ofs << m_tTmpCtrl_SD_24_100K.vu << std::endl;
				ofs << m_tTmpCtrl_SD_24_100K.vl << std::endl;
				ofs << m_tTmpCtrl_SD_24_100K.count << std::endl;

				for ( auto i = 0; i < m_tTmpCtrl_SD_24_100K.count; i++ )
				{
					ofs << m_tTmpCtrl_SD_24_100K.coeff[ i ] << std::endl;
				}
				ofs << std::endl;

				ofs << TMPCTRL_SD100_475K_COEFF_KEY << std::endl;
				ofs << m_tTmpCtrl_SD_100_475K.vu << std::endl;
				ofs << m_tTmpCtrl_SD_100_475K.vl << std::endl;
				ofs << m_tTmpCtrl_SD_100_475K.count << std::endl;

				for ( auto i = 0; i < m_tTmpCtrl_SD_100_475K.count; i++ )
				{
					ofs << m_tTmpCtrl_SD_100_475K.coeff[ i ] << std::endl;
				}
				ofs << std::endl;
			}
			else
			{
				THROW( "Failed to save temperature control file: %s", sFilename.c_str() );
			}

			ofs.close();
		}


		// +----------------------------------------------------------------------------
		// |  setDefaultTemperatureValues
		// +----------------------------------------------------------------------------
		// |  Sets the default array temperature values. Called by class constructor.
		// +----------------------------------------------------------------------------
		void CArcDevice::setDefaultTemperatureValues( void )
		{
			m_gTmpCtrl_DT670Coeff1			=	TMPCTRL_DT670_COEFF_1;
			m_gTmpCtrl_DT670Coeff2			=	TMPCTRL_DT670_COEFF_2;
			m_gTmpCtrl_SDAduOffset			=	TMPCTRL_SD_ADU_OFFSET;
			m_gTmpCtrl_SDAduPerVolt			=	TMPCTRL_SD_ADU_PER_VOLT;
			m_gTmpCtrl_HGAduOffset			=	TMPCTRL_HG_ADU_OFFSET;
			m_gTmpCtrl_HGAduPerVolt			=	TMPCTRL_HG_ADU_PER_VOLT;
			m_gTmpCtrl_SDNumberOfReads		=	TMPCTRL_SD_NUM_OF_READS;
			m_gTmpCtrl_SDVoltToleranceTrials	=	TMPCTRL_SD_VOLT_TOLERANCE_TRIALS;
			m_gTmpCtrl_SDVoltTolerance		=	TMPCTRL_SD_VOLT_TOLERANCE;
			m_gTmpCtrl_SDDegTolerance			=	TMPCTRL_SD_DEG_TOLERANCE;

			m_tTmpCtrl_SD_2_12K.vu	   = TMPCTRL_SD_2_12K_VU;
			m_tTmpCtrl_SD_2_12K.vl	   = TMPCTRL_SD_2_12K_VL;
			m_tTmpCtrl_SD_2_12K.count = TMPCTRL_SD_2_12K_COUNT;

			for ( auto i = 0; i < TMPCTRL_SD_2_12K_COUNT; i++ )
			{
				m_tTmpCtrl_SD_2_12K.coeff[ i ] = TMPCTRL_SD_2_12K_COEFF[ i ];
			}

			m_tTmpCtrl_SD_12_24K.vu    = TMPCTRL_SD_12_24K_VU;
			m_tTmpCtrl_SD_12_24K.vl    = TMPCTRL_SD_12_24K_VL;
			m_tTmpCtrl_SD_12_24K.count = TMPCTRL_SD_12_24K_COUNT;

			for ( auto i = 0; i < TMPCTRL_SD_12_24K_COUNT; i++ )
			{
				m_tTmpCtrl_SD_12_24K.coeff[ i ] = TMPCTRL_SD_12_24K_COEFF[ i ];
			}

			m_tTmpCtrl_SD_24_100K.vu    = TMPCTRL_SD_24_100K_VU;
			m_tTmpCtrl_SD_24_100K.vl    = TMPCTRL_SD_24_100K_VL;
			m_tTmpCtrl_SD_24_100K.count = TMPCTRL_SD_24_100K_COUNT;

			for ( auto i = 0; i < TMPCTRL_SD_24_100K_COUNT; i++ )
			{
				m_tTmpCtrl_SD_24_100K.coeff[ i ] = TMPCTRL_SD_24_100K_COEFF[ i ];
			}

			m_tTmpCtrl_SD_100_475K.vu    = TMPCTRL_SD_100_475K_VU;
			m_tTmpCtrl_SD_100_475K.vl    = TMPCTRL_SD_100_475K_VL;
			m_tTmpCtrl_SD_100_475K.count = TMPCTRL_SD_100_475K_COUNT;

			for ( auto i = 0; i < TMPCTRL_SD_100_475K_COUNT; i++ )
			{
				m_tTmpCtrl_SD_100_475K.coeff[ i ] = TMPCTRL_SD_100_475K_COEFF[ i ];
			}
		}


		// +----------------------------------------------------------------------------
		// |  setArrayTemperature
		// +----------------------------------------------------------------------------
		// |  Sets the array temperature to regulate around.
		// |
		// |  Throws std::runtime_error on error
		// |
		// |  <IN> -> gTempVal - The temperature value ( in Celcius ) to regulate
		// |                     around.
		// +----------------------------------------------------------------------------
		void CArcDevice::setArrayTemperature( double gTempVal )
		{
			std::uint32_t uiReply = 0;

			if ( isReadout() )
			{
				THROW( "Readout in progress!" );
			}

			if ( isOpen() )
			{
				//
				// Check for SmallCam
				//
				bool bArc12 = IS_ARC12( getControllerId() );

				//
				// Test for High Gain
				//
				bool bHighGain = ( command( { UTIL_ID, THG } ) == 1 ? true : false );

				//
				// Calculate voltage/adu
				//
				double gVoltage = calculateVoltage( gTempVal );

				auto uiADU = static_cast<std::uint32_t>( voltageToADU( gVoltage, bArc12, bHighGain ) );

				if ( bArc12 )
				{
					uiReply = command( { TIM_ID, CDT, uiADU } );
				}

				else
				{
					uiReply = command( { UTIL_ID, WRM, ( Y_MEM | 0x1C ), uiADU } );
				}

				if ( uiReply != DON )
				{
					THROW( "Failed to set array temperature. Command reply: 0x%X", uiReply );
				}
			}

			else
			{
				THROW( "Not connected to any device!" );
			}
		}


		// +----------------------------------------------------------------------------
		// |  getArrayTemperature
		// +----------------------------------------------------------------------------
		// |  Returns the array temperature.
		// |
		// |  Throws std::runtime_error on error
		// |
		// |  Returns the average temperature value ( in Celcius ). The temperature is
		// |  read m_gTmpCtrl_SDNumberOfReads times and averaged. Also, for a read to be
		// |  included in the average the difference between the target temperature and
		// |  the actual temperature must be less than the tolerance specified by
		// |  gTmpCtrl_SDTolerance.
		// +----------------------------------------------------------------------------
		double CArcDevice::getArrayTemperature( void )
		{
			double gRetVal = 0.0;

			if ( isReadout() )
			{
				THROW( "Readout in progress, skipping temperature read!" );
			}

			if ( isOpen() )
			{
				gRetVal = calculateAverageTemperature();
			}
			else
			{
				THROW( "Not connected to any device!" );
			}

			return gRetVal;
		}


		// +----------------------------------------------------------------------------
		// |  getArrayTemperatureDN
		// +----------------------------------------------------------------------------
		// |  Returns the digital number associated with the current array temperature
		// |  value.
		// |
		// |  Throws std::runtime_error on error
		// +----------------------------------------------------------------------------
		double CArcDevice::getArrayTemperatureDN( void )
		{
			double gDn = 0.0;

			if ( isReadout() )
			{
				THROW( "Readout in progress, skipping temperature read!" );
			}

			if ( isOpen() )
			{
				if ( IS_ARC12( getControllerId() ) )
				{
					gDn = static_cast<double>( command( { TIM_ID, RDC } ) );
				}
				else
				{
					gDn = command( { UTIL_ID, RDM, ( Y_MEM | 0xC ) } );
				}
			}

			else
			{
				THROW( "Not connected to any device!" );
			}

			return gDn;
		}


		// +----------------------------------------------------------------------------
		// |  calculateAverageTemperature
		// +----------------------------------------------------------------------------
		// |  Calculates the average array temperature.
		// |
		// |  Throws std::runtime_error on error
		// |
		// |  Returns the average temperature value ( in Celcius ). The temperature is
		// |  read m_gTmpCtrl_SDNumberOfReads times and averaged. Also, for a read to be
		// |  included in the average the difference between the target temperature and
		// |  the actual temperature must be less than the tolerance specified by
		// |  m_gTmpCtrl_SDDegTolerance ( default = 3.0 degrees celcius ).
		// +----------------------------------------------------------------------------
		double CArcDevice::calculateAverageTemperature( void )
		{
			std::shared_ptr<double[]> pgTemperature( new double[ m_gTmpCtrl_SDNumberOfReads ], ArrayDeleter<double>() );

			double gTemperatureSum		= 0.0;
			double gAvgTemperature		= 0.0;
			double gVoltage				= 0.0;

			std::uint32_t uiNumberOfReads		= 0;
			std::uint32_t uiNumberOfValidReads	= 0;
			std::uint32_t uiAdu					= 0;

			std::vector<double> tempCoeffVector;

			if ( isReadout() )
			{
				return gAvgTemperature;
			}

			//
			// Check the system id
			//
			bool bArc12 = IS_ARC12( getControllerId() );

			//
			// Check for RDT implementation
			//
			bool bHasRDT = ( ( command( { UTIL_ID, RDT } ) != ERR ) ? true : false );

			//
			// Test for High Gain
			//
			bool bHighGain = ( command( { UTIL_ID, THG } ) == 1 ? true : false );

			for ( decltype( m_gTmpCtrl_SDNumberOfReads ) i = 0; i < m_gTmpCtrl_SDNumberOfReads; i++ )
			{
				if ( isReadout() )
				{
					break;
				}

				//
				// Read the temperature ADU from the controller
				// ------------------------------------------------
				if ( bArc12 )
				{
					uiAdu = command( { TIM_ID, RDT } );
				}

				else if ( bHasRDT )
				{
					uiAdu = command( { UTIL_ID, RDT } );
				}

				else
				{
					uiAdu = command( { UTIL_ID, RDM, ( Y_MEM | 0xC ) } );
				}

				if ( bHasRDT )
				{
					gVoltage = ADUToVoltage( uiAdu, bHasRDT, bHighGain );
				}

				else
				{
					gVoltage = ADUToVoltage( uiAdu, bArc12, bHighGain );
				}

				if ( !containsError( uiAdu ) )
				{
					//
					// Calculate the temperature from the given adu
					// ------------------------------------------------
					pgTemperature[ i ] = calculateTemperature( gVoltage );

					gTemperatureSum   += pgTemperature[ i ];

					uiNumberOfReads++;

					//
					// Sleep for 2 milliseconds. The controller only
					// updates the temperature ADU value every 3 ms.
					// ------------------------------------------------
					#ifdef _WINDOWS
						Sleep( 2 );
					#endif
				}

				else
				{
					THROW( "Failed to read temperature from controller. Reply: 0x%X", uiAdu );
				}

				//
				// Don't average for SmallCam
				//
				if ( bArc12 )
				{
					break;
				}
			}

			//
			// Calculate the mean of the temperature values and average
			// only those that are within 3 degrees of the mean.
			//
			if ( bArc12 )
			{
				gAvgTemperature = pgTemperature[ 0 ];
			}

			else if ( uiNumberOfReads > 0 )
			{
				double gMeanTemperature = ( gTemperatureSum / ( static_cast<double>( uiNumberOfReads ) ) );

				gTemperatureSum = 0.0;

				for ( decltype( uiNumberOfReads ) i = 0; i < uiNumberOfReads; i++ )
				{
					if ( std::fabs( pgTemperature[ i ] - gMeanTemperature ) < m_gTmpCtrl_SDDegTolerance )
					{
						gTemperatureSum += pgTemperature[ i ];

						uiNumberOfValidReads++;
					}
				}

				if ( uiNumberOfValidReads > 0 )
				{
					gAvgTemperature = ( gTemperatureSum / ( static_cast< double >( uiNumberOfValidReads ) ) );
				}
			}

			return gAvgTemperature;
		}


		// +----------------------------------------------------------------------------
		// |  ADUToVoltage
		// +----------------------------------------------------------------------------
		// |  Calculates the voltage associated with the specified digital number (adu).
		// |  There is a different calculation for SmallCam, which can be specified via
		// |  the parameters.
		// |
		// |  Throws std::runtime_error on error
		// |
		// |  <IN> -> uiADU      - The digital number to use for the voltage calculation.
		// |  <IN> -> bArc12    - 'true' to use the SmallCam calculation. Default=false.
		// |  <IN> -> bHighGain - 'true' if High Gain is used. Default=false.
		// +----------------------------------------------------------------------------
		double CArcDevice::ADUToVoltage( std::uint32_t uiAdu, bool bArc12, bool bHighGain )
		{
			if ( bArc12 )
			{
				return ( m_gTmpCtrl_DT670Coeff1 + m_gTmpCtrl_DT670Coeff2 * static_cast<double>( uiAdu ) );
			}

			double aduOffset  = m_gTmpCtrl_SDAduOffset;
			double aduPerVolt = m_gTmpCtrl_SDAduPerVolt;

			if ( bHighGain )
			{
				aduOffset  = m_gTmpCtrl_HGAduOffset;
				aduPerVolt = m_gTmpCtrl_HGAduPerVolt;
			}

			return ( ( static_cast<double>( uiAdu ) - aduOffset ) / aduPerVolt );
		}


		// +----------------------------------------------------------------------------
		// |  voltageToADU
		// +----------------------------------------------------------------------------
		// |  Calculates the digital number (adu) associated with the specified voltage.
		// |  There is a different calculation for SmallCam, which can be specified via
		// |  the parameters.
		// |
		// |  Throws std::runtime_error on error
		// |
		// |  <IN> -> gVoltage  - The digital number to use for the voltage calculation.
		// |  <IN> -> bArc12    - 'true' to use the SmallCam calculation. Default=false.
		// |  <IN> -> bHighGain - 'true' if High Gain is used. Default=false.
		// +----------------------------------------------------------------------------
		double CArcDevice::voltageToADU( double gVoltage, bool bArc12, bool bHighGain )
		{
			if ( bArc12 )
			{
				return ( ( gVoltage - m_gTmpCtrl_DT670Coeff1 ) / m_gTmpCtrl_DT670Coeff2 );
			}

			double aduOffset  = m_gTmpCtrl_SDAduOffset;
			double aduPerVolt = m_gTmpCtrl_SDAduPerVolt;

			if ( bHighGain )
			{
				aduOffset  = m_gTmpCtrl_HGAduOffset;
				aduPerVolt = m_gTmpCtrl_HGAduPerVolt;
			}

			return ( gVoltage * aduPerVolt + aduOffset );
		}


		// +----------------------------------------------------------------------------
		// |  calculateTemperature
		// +----------------------------------------------------------------------------
		// |  Calculates the silicon diode temperature using a voltage ( converted 
		// |  digital number read from the controller ) and a Chebychev polynomial 
		// |  series.
		// |
		// |  Throws std::runtime_error on error
		// |
		// |  <IN> -> gVoltage  - The voltage to use for the temperature calculation.
		// +----------------------------------------------------------------------------
		double CArcDevice::calculateTemperature( double gVoltage )
		{
			double gTemperature		= -273.15;
			double gX				=     0.0;
			double gTempVU			=     0.0;
			double gTempVL			=     0.0;

			std::unique_ptr<std::vector<double>> pvTC( new std::vector<double>() );

			std::unique_ptr<std::vector<double>> pvTempCoeff( new std::vector<double>() );

			if ( gVoltage <= 0.0 )
			{
				THROW( "Voltage ( %f V ) out of range ( 0.4V - 1.0V )", gVoltage );
			}

			// -------------------------------------
			// TEMP  = 2.0 to 12.0 K
			// VOLTS = 1.294390 to 1.680000
			// -------------------------------------
			if ( gVoltage >= m_tTmpCtrl_SD_2_12K.vl && gVoltage <= m_tTmpCtrl_SD_2_12K.vu )
			{
				gTempVU = m_tTmpCtrl_SD_2_12K.vu;
				gTempVL = m_tTmpCtrl_SD_2_12K.vl;

				for ( auto t = 0; t < m_tTmpCtrl_SD_2_12K.count; t++ )
				{
					pvTempCoeff->push_back( m_tTmpCtrl_SD_2_12K.coeff[ t ]  );
				}
			}

			// -------------------------------------
			// TEMP  = 12.0 to 24.5 K
			// VOLTS = 1.11230 to 1.38373
			// -------------------------------------
			else if ( gVoltage >= m_tTmpCtrl_SD_12_24K.vl && gVoltage <= m_tTmpCtrl_SD_12_24K.vu )
			{
				gTempVU = m_tTmpCtrl_SD_12_24K.vu;
				gTempVL = m_tTmpCtrl_SD_12_24K.vl;

				for ( auto t = 0; t < m_tTmpCtrl_SD_12_24K.count; t++ )
				{
					pvTempCoeff->push_back( m_tTmpCtrl_SD_12_24K.coeff[ t ]  );
				}
			}

			// -------------------------------------
			// TEMP  = 24.5 to 100.0 K
			// VOLTS = 0.909416 to 1.122751
			// -------------------------------------
			else if ( gVoltage >= m_tTmpCtrl_SD_24_100K.vl && gVoltage <= m_tTmpCtrl_SD_24_100K.vu )
			{
				gTempVU = m_tTmpCtrl_SD_24_100K.vu;
				gTempVL = m_tTmpCtrl_SD_24_100K.vl;

				for ( auto t = 0; t < m_tTmpCtrl_SD_24_100K.count; t++ )
				{
					pvTempCoeff->push_back( m_tTmpCtrl_SD_24_100K.coeff[ t ]  );
				}
			}

			// -------------------------------------
			// TEMP  = 100.0 to 475.0 K
			// VOLTS = 0.07000 to 0.99799
			// -------------------------------------
			else if ( gVoltage <= m_tTmpCtrl_SD_100_475K.vu )
			{
				gTempVU = m_tTmpCtrl_SD_100_475K.vu;
				gTempVL = m_tTmpCtrl_SD_100_475K.vl;

				for ( auto t = 0; t < m_tTmpCtrl_SD_100_475K.count; t++ )
				{
					pvTempCoeff->push_back( m_tTmpCtrl_SD_100_475K.coeff[ t ]  );
				}
			}

			// -------------------------------------
			//  ERROR - No Coefficients Exist
			// -------------------------------------
			else
			{
				THROW( "Coefficients for the voltage ( %f V ) don't exist!", gVoltage );
			}

			if ( pvTempCoeff->empty() || ( pvTempCoeff->size() > 3 ) || pvTC->empty() )
			{
				// Calculate dimensionless variable for the Chebychev series.
				gX = ( ( gVoltage - gTempVL ) - ( gTempVU - gVoltage ) );
				gX = ( gX / ( gTempVU - gTempVL ) );

				pvTC->push_back( 1 );
				pvTC->push_back( gX );

				gTemperature += pvTempCoeff->at( 0 ) + ( pvTempCoeff->at( 1 ) * gX );

				for ( auto i = 2; i < static_cast<int>( pvTempCoeff->size() ); i++ )
				{
					pvTC->push_back( 2.0 * gX * pvTC->at( i - 1 ) - pvTC->at( i - 2 ) );
					gTemperature += pvTempCoeff->at( i ) * pvTC->at( i );
				}
			}

			return gTemperature;
		}


		// +----------------------------------------------------------------------------
		// |  calculateVoltage
		// +----------------------------------------------------------------------------
		// |  Calculates the voltage using the specified temperature. The voltage is
		// |  determined using the number of trials specified by 
		// |  m_gTmpCtrl_SDVoltToleranceTrials.  Also, the difference between the target
		// |  temperature and the actual must be less than the tolerance specified by
		// |  m_gTmpCtrl_SDVoltTolerance.
		// |
		// |  Throws std::runtime_error on error
		// |
		// |  <IN> -> gTemperature - The temperature to use for the calculation.
		// +----------------------------------------------------------------------------
		double CArcDevice::calculateVoltage( double gTemperature )
		{
			bool			bTolerance	= true;
			std::uint32_t	uiTrials	= 0;
			double			gVmid		= 0.0;
			double			gTempVU		= 0.0;
			double			gTempVL		= 0.0;
			double			gTargetTemp	= -273.15;

			// -------------------------------------
			// TMP = 0.0 to 12.0 K
			// -------------------------------------
			if ( gTemperature < -261.15 )
			{
				gTempVU = m_tTmpCtrl_SD_2_12K.vu;
				gTempVL = m_tTmpCtrl_SD_2_12K.vl;
			}

			// -------------------------------------
			// TMP = 12.0 to 24.5 K
			// -------------------------------------
			else if ( gTemperature >= -261.15 && gTemperature < -248.65 )
			{
				gTempVU = m_tTmpCtrl_SD_12_24K.vu;
				gTempVL = m_tTmpCtrl_SD_12_24K.vl;
			}

			// -------------------------------------
			// TMP = 24.5 to 100.0 K
			// -------------------------------------
			else if ( gTemperature >= -248.65 && gTemperature < -173.15 )
			{
				gTempVU = m_tTmpCtrl_SD_24_100K.vu;
				gTempVL = m_tTmpCtrl_SD_24_100K.vl;
			}

			// -------------------------------------
			// TMP = 100.0 to 475.0 K
			// -------------------------------------
			else if ( gTemperature >= -173.15 )
			{
				gTempVU = m_tTmpCtrl_SD_100_475K.vu;
				gTempVL = m_tTmpCtrl_SD_100_475K.vl;
			}

			gVmid = ( gTempVL + gTempVU ) * 0.5;

			while ( bTolerance && ( uiTrials < m_gTmpCtrl_SDVoltToleranceTrials ) )
			{
				gTargetTemp = calculateTemperature( gVmid );

				if ( fabs( gTargetTemp - gTemperature ) <= m_gTmpCtrl_SDVoltTolerance )
				{
					bTolerance = false;
				}

				if ( bTolerance )
				{
					if ( gTargetTemp < gTemperature )
					{
						gTempVU	= gVmid;
					}

					else if ( gTargetTemp > gTemperature )
					{
						gTempVL	= gVmid;
					}

					gVmid	= ( gTempVL + gTempVU ) * 0.5;
				}
				uiTrials++;
			}

			return gVmid;
		}

	}	//end namespace gen3
}	// end namespace arc
