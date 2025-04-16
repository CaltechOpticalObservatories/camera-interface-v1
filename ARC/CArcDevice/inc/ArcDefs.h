#ifndef _ARC_DEFS_H_
#define _ARC_DEFS_H_

#ifdef __cplusplus
namespace arc
{
#endif


	// +----------------------------------------------------------------------------------------
	// | Define board id's
	// +----------------------------------------------------------------------------------------
	#define PCI_ID				1
	#define TIM_ID				2
	#define UTIL_ID				3
	#define SMALLCAM_DLOAD_ID	3


	// +----------------------------------------------------------------------------------------
	// | Memory Location Id Constants
	// | R_MEM	(Bit 20)  ROM
	// | P_MEM	(Bit 21)  DSP program memory space
	// | X_MEM	(Bit 22)  DSP X memory space
	// | Y_MEM	(Bit 23)  DSP Y memory space
	// +----------------------------------------------------------------------------------------
	#define P_MEM		0x100000
	#define X_MEM 		0x200000
	#define Y_MEM		0x400000
	#define R_MEM		0x800000


	// +----------------------------------------------------------------------------------------
	// | Define controller/PCI replies
	// +----------------------------------------------------------------------------------------
	#define TOUT		0x544F5554	// Timeout
	#define ROUT		0x524F5554	// Readout
	#define HERR		0x48455252	// Header Error
	#define DON			0x00444F4E	// Done
	#define ERR			0x00455252	// Error
	#define SYR			0x00535952	// System reset
	#define RST			0x00525354	// reset
	#define	CNR			0x00434E52	// Controller Not Ready


	// +----------------------------------------------------------------------------------------
	// | Define controller/manual commands
	// +----------------------------------------------------------------------------------------
	#define DBC			0x00444243	// Download Boot Code
	#define TDL			0x0054444C	// Test Data Link
	#define RDM			0x0052444D	// ReaD Memory
	#define WRM			0x0057524D	// WRite Memory
	#define SEX			0x00534558	// Start EXposure
	#define SRE			0x00535245	// Start REadout
	#define FRT			0x00465254	// FRame Transfer
	#define SET			0x00534554	// Set Exposure Time
	#define PEX			0x00504558	// Pause EXposure
	#define REX			0x00524558	// Resume EXposure
	#define GET			0x00474554	// Get Exposure Time
	#define RET			0x00524554	// Read Elapsed Time
	#define PON			0x00504F4E	// Power ON
	#define POF			0x00504F46	// Power OFf
	#define RDI			0x00524449	// ReaD Image
	#define SOS			0x00534F53	// Select Output Source
	#define MPP			0x004D5050	// Multi-Pinned Phase mode
	#define DCA			0x00444341	// Download CoAdder
	#define SNF			0x00534E46	// Set Number of Frames
	#define FPB			0x00465042	// Set the Frames-Per-Buffer for coadds
	#define VID			0x00564944	// mnemonic that means VIDeo board
	#define SBN			0x0053424E	// Set Bias Number
	#define SBV			0x00534256	// Set Bias Voltage
	#define SGN			0x0053474E	// Set GaiN
	#define SMX			0x00534D58	// Select MultipleXer
	#define CLK			0x00434C4B	// mnemonic that means CLocK driver board
	#define SSS			0x00535353	// Set Subarray Sizes
	#define SSP			0x00535350	// Set Subarray Positions
	#define LGN			0x004C474E	// Set Low Gain
	#define HGN			0x0048474E	// Set High Gain
	#define SRM			0x0053524D	// Set Readout Mode - either CDS or single
	#define CDS			0x00434453	// Correlated Double Sampling
	#define SFS			0x00534653	// Send Fowler Sample
	#define SPT			0x00535054	// Set Pass Through mode
	#define LDA			0x004C4441	// LoaD Application
	#define RCC			0x00524343	// Read Controller Configuration
	#define CLR			0x00434C52	// CleaR Array
	#define IDL			0x0049444C	// IDLe
	#define STP			0x00535450	// SToP idle
	#define CSH			0x00435348	// close SHutter
	#define OSH			0x004F5348	// open SHutter
	#define SUR			0x00535552	// Set Up the Ramp mode
	#define MH1			0x004D4831	// Move NIRIM Filter Wheel 1 Home
	#define MM1			0x004D4D31	// Move NIRIM Filter Wheel 1
	#define MH2			0x004D4832	// Move NIRIM Filter Wheel 2 Home
	#define MM2			0x004D4D32	// Move NIRIM Filter Wheel 2
	#define SBS			0x00534253	// Set Hardware Byte Swapping
	#define TBS			0x00544253	// Test for Hardware Byte Swapping
	#define RNC			0x00524E43	// Read Number of Channels for Hawaii RG array
	#define THG			0x00544847	// Test High Gain - Utility Board Temperature
	#define SID			0x00534944	// System ID - FastCam
	#define JDL			0x004A444C	// Jump to DownLoad for C based ARC-22 systems
	#define XMT			0x00584D54	// Set/Clear fiber optic 2x transmitter mode
	#define ABR			0x00414252	// ABort Readout & Exposure
	#define STM			0x0053544D	// Set Trigger Mode

	#define CDT			0x00434454	// SmallCam control detector temperature
	#define RDT			0x00524454	// SmallCam read detector temperature
	#define RHV			0x00524856	// SmallCam read heater voltage
	#define RDC			0x00524443	// SmallCam read detector current ( actually, DN )

	#define RSC			0x00525343	// FastCam Only - ReSet Controller


	// +----------------------------------------------------------------------------------------
	// | Controller Configuration Bit Definitions
	// +----------------------------------------------------------------------------------------
	// |
	// | BIT #'s		FUNCTION
	// | 2,1,0		Video Processor
	// | 			000	ARC-41 Dual Readout CCD
	// | 			001	CCD Gen I
	// | 			010	ARC-42 Dual Readout IR
	// | 			011	ARC-44 Four Readout IR Coadder ( obsolete )
	// | 			100	ARC-45 Dual Readout CCD
	// | 			101	ARC-46 8-Channel IR
	// |			110 ARC-48 8-Channel CCD
	// |			111 ARC-47 4-Channel CCD
	// | 
	// | 4,3		Timing Board
	// | 			00	Rev. 4, Gen II
	// | 			01	Gen I
	// | 			10	Rev. 5, Gen III, 250 MHz
	// | 
	// | 6,5		Utility Board
	// | 			00	No utility board
	// | 			01	Utility Rev. 3
	// | 
	// | 7		Shutter
	// | 			0	No shutter support
	// | 			1	Yes shutter support
	// | 
	// | 9,8		Temperature readout
	// | 			00	No temperature readout
	// | 			01	Polynomial Diode calibration
	// | 			10	Linear temperature sensor calibration
	// | 
	// | 10		Subarray readout
	// | 			0	Not supported
	// | 			1	Yes supported
	// | 
	// | 11		Binning
	// | 			0	Not supported
	// | 			1	Yes supported
	// | 
	// | 12		Split-Serial readout
	// | 			0	Not supported
	// | 			1	Yes supported
	// | 
	// | 13		Split-Parallel readout
	// | 			0	Not supported
	// | 			1	Yes supported
	// | 
	// | 14		MPP = Inverted parallel clocks
	// | 			0	Not supported
	// | 			1	Yes supported
	// | 
	// | 16,15		Clock Driver Board
	// | 			00	Rev. 3
	// | 			11	No clock driver board (Gen I)
	// | 
	// | 19,18,17		Special implementations
	// | 			000 	Somewhere else
	// | 			001	Mount Laguna Observatory
	// | 			010	NGST Aladdin
	// |            011 2x FO Transmitter
	// | 			xxx	Other	
	// +----------------------------------------------------------------------------------------
	#define CCDVIDREV3B		0x000000		// CCD Video Processor Rev. 3
	#define ARC41			0x000000	
	#define VIDGENI			0x000001		// CCD Video Processor Gen I
	#define IRREV4			0x000002		// IR Video Processor Rev. 4
	#define ARC42			0x000002	
	#define COADDER			0x000003		// IR Coadder
	#define ARC44			0x000003	
	#define CCDVIDREV5		0x000004		// Differential input CCD video Rev. 5
	#define ARC45			0x000004
	#define IR8X			0x000005		// 8x IR
	#define ARC46			0x000005		// 8-channel IR video board
	#define ARC48			0x000006		// 8-channel CCD video board
	#define ARC47			0x000007		// 4-channel CCD video board
	#define TIMREV4			0x000000		// Timing Revision 4 = 50 MHz
	#define ARC20			0x000000	
	#define TIMGENI			0x000008		// Timing Gen I = 40 MHz
	#define TIMREV5			0x000010		// Timing Revision 5 = 250 MHz
	#define ARC22			0x000010
	#define UTILREV3		0x000020		// Utility Rev. 3 supported
	#define ARC50			0x000020
	#define SHUTTER_CC		0x000080		// Shutter supported
	#define TEMP_SIDIODE	0x000100		// Polynomial calibration
	#define TEMP_LINEAR		0x000200		// Linear calibration
	#define SUBARRAY		0x000400		// Subarray readout supported
	#define BINNING			0x000800		// Binning supported
	#define SPLIT_SERIAL	0x001000		// Split serial supported
	#define SPLIT_PARALLEL	0x002000		// Split parallel supported
	#define MPP_CC			0x004000		// Inverted clocks supported
	#define ARC32			0x008000		// CCD & IR clock driver board
	#define CLKDRVGENI		0x018000		// No clock driver board - Gen I
	#define MLO				0x020000		// Set if Mount Laguna Observatory
	#define NGST			0x040000		// NGST Aladdin implementation
	#define FO_2X_TRANSMITR	0x060000		// 2x FO transmitters
	#define CONT_RD			0x100000		// continuous readout implemented
	#define SEL_READ_SPEED	0x200000		// Selectable readout speeds

	#define	ALL_READOUTS	( SPLIT_SERIAL | SPLIT_PARALLEL )

	// +----------------------------------------------------------------------------------------
	// | 'SOS' Array Amplifier Parameters
	// +----------------------------------------------------------------------------------------
	#define AMP_0			0x5F5F43	//  Ascii __C amp 0
	#define AMP_1			0x5F5F44	//  Ascii __D amp 1
	#define AMP_2			0x5F5F42	//  Ascii __B amp 2
	#define AMP_3			0x5F5F41	//  Ascii __A amp 3
	#define AMP_L			0x5F5F4C	//  Ascii __L left amp
	#define AMP_R			0x5F5F52	//  Ascii __R left amp
	#define AMP_LR			0x5F4C52	//  Ascii _LR right two amps
	#define AMP_ALL			0x414C4C	//  Ascii ALL four amps (quad)


	// +-----------------------------------------------------------
	// | Define continuous readout modes
	// +-----------------------------------------------------------
	#define CR_WRITE		0
	#define CR_COADD		1
	#define CR_DEBUG		2
 

	// +----------------------------------------------------------------------------------------
	// A valid start address must be less than 0x4000 for
	// the load DSP file in timming or utility boards.
	// +----------------------------------------------------------------------------------------
	#define MAX_DSP_START_LOAD_ADDR		0x4000


	// +-----------------------------------------------------------
	// | Shutter Position Constants
	// +-----------------------------------------------------------
	#define OPEN_SHUTTER_POSITION		 ( 1 << 11 )
	#define CLOSED_SHUTTER_POSITION		~( 1 << 11 )


	// +----------------------------------------------------------------------------
	// |  Macro that returns 'true' if the parameter indicates the ARC-12
	// |  ( SmallCam ) controller; returns 'false' otherwise.
	// +----------------------------------------------------------------------------
	#define IS_ARC12( id ) \
				( ( ( ( id & 0xFF0000 ) >> 16 ) == 'S' &&	\
				( ( id & 0x00FF00 ) >> 8 ) == 'C' )			\
				? true : false )


	// +-----------------------------------------------------------
	// | SmallCam Synthetic Image Commands & Arguments
	// +-----------------------------------------------------------
	#define SIM				0x0053494D

	enum {
		SYN_IMG_DISABLE = 0,
		SYN_IMG_FIXED,
		SYN_IMG_RAMP,
		SYN_IMG_RESET
	};

#ifdef __cplusplus
}	// end namespace
#endif

#endif		// _ARC_DEFS_H_
