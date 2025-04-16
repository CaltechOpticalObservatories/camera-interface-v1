#ifndef _CARC_DEVICE_H_
#define _CARC_DEVICE_H_


#include <CArcDeviceDllMain.h>
#include <ArcOSDefs.h>
#include <CExpIFace.h>
#include <CooExpIFace.h>
#include <CConIFace.h>
#include <TempCtrl.h>
#include <CArcLog.h>

#if defined( linux ) || defined( __linux )
	#include <sys/types.h>
#endif


namespace arc
{
	namespace gen3
	{
		namespace device
		{

			// +------------------------------------------------+
			// | Image buffer info                              |
			// +------------------------------------------------+
			typedef struct ARC_DEVICE_BUFFER
			{
				std::uint16_t*	pUserAddr;
				std::uint64_t	ulPhysicalAddr;
				std::uint64_t	ulSize;
			} ImgBuf_t;


			// +------------------------------------------------+
			// | Device info                                    |
			// +------------------------------------------------+
			typedef struct ARC_DEVICE_INFO
			{
				std::string			sName;

				#ifdef __APPLE__
					io_service_t		tService;
				#endif
			} ArcDev_t;

		}	// end device namespace

		// +------------------------------------------------+
		// | CArcDevice class definition                    |
		// +------------------------------------------------+
		class GEN3_CARCDEVICE_API CArcDevice
		{
			public:

				CArcDevice( void );

				virtual ~CArcDevice( void ) = default;

				virtual const std::string toString( void ) = 0;

				//  Device access
				// +-------------------------------------------------+
				virtual bool isOpen( void );

				virtual void open( std::uint32_t uiDeviceNumber = 0 ) = 0;

				virtual void open( std::uint32_t uiDeviceNumber, std::uint32_t dBytes ) = 0;

				virtual void open( std::uint32_t uiDeviceNumber, std::uint32_t dRows, std::uint32_t dCols ) = 0;

				virtual void close( void ) = 0;

				virtual void reset( void ) = 0;

				virtual void mapCommonBuffer( std::uint32_t uiBytes = 0 ) = 0;

				virtual void unMapCommonBuffer( void ) = 0;

				virtual void reMapCommonBuffer( std::uint32_t uiBytes = 0 );

				virtual void fillCommonBuffer( std::uint16_t uwValue = 0 );

				virtual std::uint8_t* commonBufferVA( void );

				virtual std::uint64_t commonBufferPA( void );

				virtual std::uint64_t commonBufferSize( void );

				virtual std::uint32_t getId( void ) = 0;

				virtual std::uint32_t getStatus( void ) = 0;

				virtual void clearStatus( void ) = 0;

				virtual void set2xFOTransmitter( bool bOnOff ) = 0;

				virtual void loadDeviceFile( const std::string& sFile ) = 0;

				//  Setup & General commands
				// +-------------------------------------------------+
	//			virtual std::uint32_t command( std::uint32_t uiBoardId, std::uint32_t uiCommand, std::uint32_t uiArg1 = NOPARAM, std::uint32_t uiArg2 = NOPARAM, std::uint32_t uiArg3 = NOPARAM, std::uint32_t uiArg4 = NOPARAM ) = 0;
				virtual std::uint32_t command( const std::initializer_list<std::uint32_t>& tCmdList ) = 0;

				virtual std::uint32_t getControllerId( void ) = 0;

				virtual void resetController( void ) = 0;

				virtual bool isControllerConnected( void ) = 0;

				virtual void setupController( bool bReset, bool bTdl, bool bPower, std::uint32_t uiRows, std::uint32_t uiCols, const std::string& sTimFile,
											  const std::string& sUtilFile = "", const std::string& sPciFile = "", const bool& bAbort = false );

                		virtual void selectOutputSource( std::uint32_t arg );

				virtual void loadControllerFile( const std::string& sFilename, bool bValidate = true, const bool& bAbort = false );

				virtual void setImageSize( std::uint32_t uiRows, std::uint32_t uiCols );

				virtual std::uint32_t  getImageRows( void );

				virtual std::uint32_t  getImageCols( void );

				virtual std::uint32_t  getCCParams( void );

				virtual bool isCCParamSupported( std::uint32_t uiParameter );

				virtual bool isCCD( void );

				virtual bool isBinningSet( void );

				virtual void setBinning( std::uint32_t uiRows, std::uint32_t uiCols, std::uint32_t uiRowFactor, std::uint32_t uiColFactor, std::uint32_t* pBinRows = nullptr, std::uint32_t* pBinCols = nullptr );

				virtual void unSetBinning( std::uint32_t uiRows, std::uint32_t uiCols );

				virtual void setSubArray( std::uint32_t& uiOldRows, std::uint32_t& uiOldCols, std::uint32_t uiRow, std::uint32_t uiCol, std::uint32_t uiSubRows, std::uint32_t uiSubCols, std::uint32_t uiBiasOffset, std::uint32_t uiBiasWidth );

				virtual void unSetSubArray( std::uint32_t uiRows, std::uint32_t uiCols );

				virtual bool isSyntheticImageMode( void );

				virtual void setSyntheticImageMode( bool bMode );


				//  expose commands
				// +-------------------------------------------------+
				virtual void setOpenShutter( bool bShouldOpen );

				virtual void expose( float fExpTime, std::uint32_t uiRows, std::uint32_t uiCols, const bool& bAbort = false, arc::gen3::CExpIFace* pExpIFace = nullptr, bool bOpenShutter = true );
				virtual void expose( int devnum, const std::uint32_t &uiExpTime, std::uint32_t uiRows, std::uint32_t uiCols, const bool& bAbort = false, arc::gen3::CooExpIFace* pCooExpIFace = nullptr, bool bOpenShutter = true );
				virtual void readout( int expbuf, int devnum, std::uint32_t uiRows, std::uint32_t uiCols, const bool &bAbort = false, arc::gen3::CooExpIFace* pCooExpIFace = nullptr );
				virtual void frame_transfer( int expbuf, int devnum, std::uint32_t uiRows, std::uint32_t uiCols, arc::gen3::CooExpIFace* pCooExpIFace );

				virtual void stopExposure( void ) = 0;

				virtual void continuous( std::uint32_t uiRows, std::uint32_t uiCols, std::uint32_t uiNumOfFrames, float fExpTime, const bool& bAbort = false, arc::gen3::CConIFace* pConIFace = nullptr, bool bOpenShutter = true );

				virtual void stopContinuous( void );

				virtual bool isReadout( void ) = 0;

				virtual std::uint32_t getPixelCount( void ) = 0;
				
				virtual std::uint32_t getCRPixelCount( void ) = 0;

				virtual std::uint32_t getFrameCount( void ) = 0;


				//  Error & Degug message access
				// +-------------------------------------------------+
				virtual bool containsError( std::uint32_t uiWord );

				virtual bool containsError( std::uint32_t uiWord, std::uint32_t uiWordMin, std::uint32_t uiWordMax );

				virtual const std::string getNextLoggedCmd( void );

				virtual std::int32_t getLoggedCmdCount( void );

				virtual void setLogCmds( bool bOnOff );


				//  Temperature control
				// +-------------------------------------------------+
				virtual double getArrayTemperature( void );

				virtual double getArrayTemperatureDN( void );

				virtual void setArrayTemperature( double gTempVal );

				virtual void loadTemperatureCtrlData( const std::string& sFilename );

				virtual void saveTemperatureCtrlData( const std::string& sFilename );


				//  Maximum number of command parameters the controller will accept 
				// +------------------------------------------------------------------+
				static const std::uint32_t CTLR_CMD_MAX = 6;


				//  Timeout loop count for image readout                             
				// +------------------------------------------------------------------+
				static const std::uint32_t READ_TIMEOUT = 200;


				//  Invalid parameter value                           
				// +------------------------------------------------------------------+
				static const std::uint32_t NOPARAM = 0xFF000000;


				//  No file value
				// +------------------------------------------------------------------+
				static const std::string NO_FILE;

			protected:

				virtual bool getCommonBufferProperties( void ) = 0;

				virtual void setDefaultTemperatureValues( void );
				virtual double ADUToVoltage( std::uint32_t uiAdu, bool bArc12 = false, bool bHighGain = false );
				virtual double voltageToADU( double gVoltage, bool bArc12 = false, bool bHighGain = false );
				virtual double calculateAverageTemperature( void );
				virtual double calculateVoltage( double gTemperature );
				virtual double calculateTemperature( double gVoltage );

				virtual std::uint32_t getContinuousImageSize( std::uint32_t uiImageSize ) = 0;

				virtual std::uint32_t smallCamDLoad( std::uint32_t uiBoardId, std::vector<std::uint32_t>* pvData ) = 0;

				virtual void loadSmallCamControllerFile( const std::string& sFilename, bool bValidate, const bool& bAbort = false );

				virtual void loadGen23ControllerFile( const std::string& sFilename, bool bValidate, const bool& bAbort = false ) = 0;

				virtual void setByteSwapping( void ) = 0;

				virtual const std::string formatDLoadString( std::uint32_t uiReply, std::uint32_t uiBoardId, std::vector<std::uint32_t>* pvData );

				//  Temperature control variables
				// +-------------------------------------------------------------+
				double								m_gTmpCtrl_DT670Coeff1;
				double								m_gTmpCtrl_DT670Coeff2;
				double								m_gTmpCtrl_SDAduOffset;
				double								m_gTmpCtrl_SDAduPerVolt;
				double								m_gTmpCtrl_HGAduOffset;
				double								m_gTmpCtrl_HGAduPerVolt;
				double								m_gTmpCtrl_SDVoltTolerance;
				double								m_gTmpCtrl_SDDegTolerance;
				std::uint32_t						m_gTmpCtrl_SDNumberOfReads;
				std::uint32_t						m_gTmpCtrl_SDVoltToleranceTrials;

				TmpCtrlCoeff_t						m_tTmpCtrl_SD_2_12K;
				TmpCtrlCoeff_t						m_tTmpCtrl_SD_12_24K;
				TmpCtrlCoeff_t						m_tTmpCtrl_SD_24_100K;
				TmpCtrlCoeff_t						m_tTmpCtrl_SD_100_475K;

				Arc_DevHandle						m_hDevice;		// Driver file descriptor
				std::unique_ptr<arc::gen3::CArcLog>	m_pCLog;
				arc::gen3::device::ImgBuf_t			m_tImgBuffer;
				std::uint32_t						m_uiCCParam;
				bool	 							m_bStoreCmds;	// 'true' stores cmd strings in queue
		};

	}	// end gen3 namespace
}	// end arc namespace


#endif	// _CARC_DEVICE_H_
