#ifndef _ARC_TEMP_CTRL_H_
#define _ARC_TEMP_CTRL_H_

namespace arc
{
	namespace gen3
	{

		// + ----------------------------------------------------------
		// | Define temperature conversions
		// + ----------------------------------------------------------
		#define C2K( c )	( ( double )( c + 273.15 ) )						// Celcius To Kelvin
		#define K2C( k )	( ( double )( k - 273.15 ) )						// Kelvin To Celcius
		#define F2K( f )	( ( double )( f * ( 9.0 / 5.0 ) - 459.67 ) )		// Fahrenheit To Kelvin
		#define K2F( k )	( ( double )( ( k + 459.67 ) * ( 5.0 / 9.0 ) ) )	// Kelvin To Fahrenheit

		// + ----------------------------------------------------------
		// | Define temperature constants
		// + ----------------------------------------------------------
		#define TMPCTRL_SD_NUM_OF_READS				30
		#define TMPCTRL_SD_VOLT_TOLERANCE_TRIALS	30
		#define TMPCTRL_SD_VOLT_TOLERANCE			0.005
		#define TMPCTRL_SD_DEG_TOLERANCE			3.0

		// +-----------------------------------------------------------
		// | Define Temperature Coeffients for DT-670 Sensor
		// +-----------------------------------------------------------
		#define TMPCTRL_DT670_COEFF_1				0.03
		#define TMPCTRL_DT670_COEFF_2				0.0000251


		// +-----------------------------------------------------------
		// | Define Temperature Coeffients for CY7 Sensor
		// +-----------------------------------------------------------
		#define TMPCTRL_SD_ADU_OFFSET				2045.0
		#define TMPCTRL_SD_ADU_PER_VOLT				1366.98


		// +-----------------------------------------------------------
		// | Define Temperature Coeffients for High Gain Utility Board
		// +-----------------------------------------------------------
		#define TMPCTRL_HG_ADU_OFFSET			   -3108.0
		#define TMPCTRL_HG_ADU_PER_VOLT				7321.0


		// + ----------------------------------------------------------
		// | Define temperature coefficient data structure
		// + ----------------------------------------------------------
		typedef struct TmpCtrlData
		{
			double vu;
			double vl;
			int    count;
			double coeff[ 12 ];
		} TmpCtrlCoeff_t;


		//// +-----------------------------------------------------------
		//// | Define Temperature Coeffients for Non-linear Silicone
		//// | Diode ( SD )
		//// +-----------------------------------------------------------
		#define TMPCTRL_SD_2_12K_VU			 1.680000
		#define TMPCTRL_SD_2_12K_VL			 1.294390
		#define TMPCTRL_SD_2_12K_COUNT		 10
		extern double TMPCTRL_SD_2_12K_COEFF[ TMPCTRL_SD_2_12K_COUNT ];


		#define TMPCTRL_SD_12_24K_VU		 1.38373
		#define TMPCTRL_SD_12_24K_VL		 1.11230
		#define TMPCTRL_SD_12_24K_COUNT		 11
		extern double TMPCTRL_SD_12_24K_COEFF[ TMPCTRL_SD_12_24K_COUNT ];


		#define TMPCTRL_SD_24_100K_VU		 1.122751
		#define TMPCTRL_SD_24_100K_VL		 0.909416
		#define TMPCTRL_SD_24_100K_COUNT	 12
		extern double TMPCTRL_SD_24_100K_COEFF[ TMPCTRL_SD_24_100K_COUNT ];


		#define TMPCTRL_SD_100_475K_VU		  0.999614
		#define TMPCTRL_SD_100_475K_VL		  0.079767
		#define TMPCTRL_SD_100_475K_COUNT	  11
		extern double TMPCTRL_SD_100_475K_COEFF[ TMPCTRL_SD_100_475K_COUNT ];


		// +-----------------------------------------------------------
		// | Define temperature control keys for input ini files
		// +-----------------------------------------------------------
		#define TMPCTRL_DT670_COEFF_1_KEY			"[TMPCTRL_DT670_COEFF_1]"
		#define TMPCTRL_DT670_COEFF_2_KEY			"[TMPCTRL_DT670_COEFF_2]"
		#define TMPCTRL_SDADU_OFFSET_KEY			"[TMPCTRL_SDADU_OFFSET]"
		#define TMPCTRL_SDADU_PER_VOLT_KEY			"[TMPCTRL_SDADU_PER_VOLT]"
		#define TMPCTRL_HGADU_OFFSET_KEY			"[TMPCTRL_HGADU_OFFSET]"
		#define TMPCTRL_HGADU_PER_VOLT_KEY			"[TMPCTRL_HGADU_PER_VOLT]"
		#define TMPCTRL_SDNUMBER_OF_READS_KEY		"[TMPCTRL_SDNUMBER_OF_READS]"
		#define TMPCTRL_SDVOLT_TOLERANCE_TRIALS_KEY	"[TMPCTRL_SDVOLT_TOLERANCE_TRIALS]"
		#define TMPCTRL_SDVOLT_TOLERANCE_KEY		"[TMPCTRL_SDVOLT_TOLERANCE]"
		#define TMPCTRL_SDDEG_TOLERANCE_KEY			"[TMPCTRL_SDDEG_TOLERANCE]"
		#define TMPCTRL_SD2_12K_COEFF_KEY			"[TMPCTRL_SD2_12K_COEFF]"
		#define TMPCTRL_SD12_24K_COEFF_KEY			"[TMPCTRL_SD12_24K_COEFF]"
		#define TMPCTRL_SD24_100K_COEFF_KEY			"[TMPCTRL_SD24_100K_COEFF]"
		#define TMPCTRL_SD100_475K_COEFF_KEY		"[TMPCTRL_SD100_475K_COEFF]"

	}	// end gen3 namespace
}	// end arc namespace

#endif
