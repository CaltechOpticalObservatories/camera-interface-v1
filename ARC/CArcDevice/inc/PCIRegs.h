#ifndef __PCI_REGS_H_
#define __PCI_REGS_H_

// +--------------------------------------------------------------------------+
// |   PCIRegs.h                                                              |
// +--------------------------------------------------------------------------+
// |   This file defines the generic PCI Configuration Registers and access   |
// |   macros.                                                                |
// +--------------------------------------------------------------------------+


namespace arc
{
	namespace gen3
	{

		//
		// PCI Register Header Count
		//
		#define CFG_PCI_REG_COUNT		16


		//
		// PCI offsets for Type 0 Header
		//
		#define CFG_VENDOR_ID           0x000
		#define CFG_DEVICE_ID			0x002
		#define CFG_COMMAND             0x004
		#define CFG_STATUS				0x006
		#define CFG_REV_ID              0x008
		#define CFG_CACHE_SIZE          0x00C
		#define CFG_BAR0                0x010
		#define CFG_BAR1                0x014
		#define CFG_BAR2                0x018
		#define CFG_BAR3                0x01C
		#define CFG_BAR4                0x020
		#define CFG_BAR5                0x024
		#define CFG_CIS_PTR             0x028
		#define CFG_SUB_VENDOR_ID       0x02C
		#define CFG_EXP_ROM_BASE        0x030
		#define CFG_CAP_PTR             0x034
		#define CFG_RESERVED1           0x038
		#define CFG_INT_LINE            0x03C


		//
		// PCI Config Device/Vendor Register macros ( 0x2/0x0 )
		//
		#define PCI_GET_VEN( x )                       ( x & 0x0000FFFF )
		#define PCI_GET_DEV( x )                       ( ( x & 0xFFFF0000 ) >> 16 )


		//
		// PCI Class Code/Revision ID Register macros ( 0x9/0x8 )
		//
		#define PCI_GET_BASECLASS( x )                 ( ( x & 0xFF000000 ) >> 24 )
		#define PCI_GET_SUBCLASS( x )                  ( ( x & 0x00FF0000 ) >> 16 )
		#define PCI_GET_INTERFACE( x )                 ( ( x & 0x0000FF00 ) >> 8  )
		#define PCI_GET_REVID( x )                     ( x & 0x000000FF )

		#define PCI_GET_GET_BASECLASS_STRING( x )      ( ( ( PCI_GET_BASECLASS( x ) == 0x00 ) ? "Old Device" :                                    \
														( ( ( PCI_GET_BASECLASS( x ) == 0x01 ) ? "Mass Storage Controller" :                       \
														( ( ( PCI_GET_BASECLASS( x ) == 0x02 ) ? "Network Controller" :                            \
														( ( ( PCI_GET_BASECLASS( x ) == 0x03 ) ? "Display Controller" :                            \
														( ( ( PCI_GET_BASECLASS( x ) == 0x04 ) ? "Multimedia Device" :                             \
														( ( ( PCI_GET_BASECLASS( x ) == 0x05 ) ? "Memory Controller" :                             \
														( ( ( PCI_GET_BASECLASS( x ) == 0x06 ) ? "Bridge Device" :                                 \
														( ( ( PCI_GET_BASECLASS( x ) == 0x07 ) ? "Simple Communication Controller" :               \
														( ( ( PCI_GET_BASECLASS( x ) == 0x08 ) ? "Base System Peripherals" :                       \
														( ( ( PCI_GET_BASECLASS( x ) == 0x09 ) ? "Input Device" :                                  \
														( ( ( PCI_GET_BASECLASS( x ) == 0x0A ) ? "Docking Stations" :                              \
														( ( ( PCI_GET_BASECLASS( x ) == 0x0B ) ? "Processors" :                                    \
														( ( ( PCI_GET_BASECLASS( x ) == 0x0C ) ? "Serial Bus Controller" :                         \
														( ( ( PCI_GET_BASECLASS( x ) >= 0x0D && PCI_GET_BASECLASS( x ) >= 0xFE ) ? "Reserved" :   \
														"unknown" ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) )

		//
		// PCI Config command Register macros ( 0x4 )
		//
		#define PCI_GET_CMD( x )                           ( x & 0x0000FFFF )
		#define PCI_GET_CMD_IO_ACCESS_ENABLED( x )         ( x & 0x0001 )
		#define PCI_GET_CMD_MEMORY_ACCESS_ENABLED( x )     ( ( x & 0x0002 ) >> 1 )
		#define PCI_GET_CMD_ENABLE_MASTERING( x )          ( ( x & 0x0004 ) >> 2 )
		#define PCI_GET_CMD_SPECIAL_CYCLE_MONITORING( x )  ( ( x & 0x0008 ) >> 3 )
		#define PCI_GET_CMD_MEM_WRITE_INVAL_ENABLE( x )    ( ( x & 0x0010 ) >> 4 )
		#define PCI_GET_CMD_PALETTE_SNOOP_ENABLE( x )      ( ( x & 0x0020 ) >> 5 )
		#define PCI_GET_CMD_PARITY_ERROR_RESPONSE( x )     ( ( x & 0x0040 ) >> 6 )
		#define PCI_GET_CMD_WAIT_CYCLE_CONTROL( x )        ( ( x & 0x0080 ) >> 7 )
		#define PCI_GET_CMD_SERR_ENABLE( x )               ( ( x & 0x0100 ) >> 8 )
		#define PCI_GET_CMD_FAST_BACK2BACK_ENABLE( x )     ( ( x & 0x0200 ) >> 9 )


		//
		// PCI Config Status Register macros ( 0x6 )
		//
		#define PCI_GET_STATUS( x )						( ( x & 0xFFFF0000 ) >> 16 )
		#define PCI_GET_STATUS_66MHZ_CAPABLE( x )			( ( x & 0x00200000 ) >> 21 )
		#define PCI_GET_STATUS_UDF_SUPPORTED( x )			( ( x & 0x00400000 ) >> 22 )
		#define PCI_GET_STATUS_FAST_BACK2BACK_CAPABLE( x )	( ( x & 0x00800000 ) >> 23 )
		#define PCI_GET_STATUS_DATA_PARITY_REPORTED( x )	( ( x & 0x01000000 ) >> 24 )
		#define PCI_GET_STATUS_DEVSEL_TIMING( x )			( ( x & 0x06000000 ) >> 25 )
		#define PCI_GET_STATUS_SIGNALLED_TARGET_ABORT( x )	( ( x & 0x08000000 ) >> 27 )
		#define PCI_GET_STATUS_RECEIVED_TARGET_ABORT( x )	( ( x & 0x10000000 ) >> 28 )
		#define PCI_GET_STATUS_RECEIVED_MASTER_ABORT( x )	( ( x & 0x20000000 ) >> 29 )
		#define PCI_GET_STATUS_SIGNALLED_SYSTEM_ERROR( x )	( ( x & 0x40000000 ) >> 30 )
		#define PCI_GET_STATUS_DETECTED_PARITY_ERROR( x )	( ( x & 0x80000000 ) >> 31 )

		#define PCI_GET_STATUS_GET_DEVSEL_STRING( x )	( ( ( ( x & 0x0600 ) == 0x0000 ) ? "fast" :     \
														( ( ( ( x & 0x0600 ) == 0x0100 ) ? "medium" :   \
														( ( ( ( x & 0x0600 ) == 0x0200 ) ? "slow" :     \
														( ( ( ( x & 0x0600 ) == 0x0500 ) ? "reserved" : \
														"unknown" ) ) ) ) ) ) ) )


		//
		// PCI BIST/Header Type/Latency Timer/Cache Line Size Register macros ( 0xF/0xE/0xD/0xC )
		//
		#define PCI_GET_BIST( x )                              ( ( x & 0xFF000000 ) >> 24 )
		#define PCI_GET_HEADER_TYPE( x )                       ( ( x & 0x00FF0000 ) >> 16 )
		#define PCI_GET_LATENCY_TIMER( x )                     ( ( x & 0x0000FF00 ) >> 8 )
		#define PCI_GET_CACHE_LINE_SIZE( x )                   ( x & 0x000000FF )

		#define PCI_GET_BIST_COMPLETE_CODE( x )                ( PCI_GET_BIST( x ) & 0x0F )
		#define PCI_GET_BIST_INVOKED( x )                      ( ( PCI_GET_BIST( x ) & 0x40 ) >> 6 )
		#define PCI_GET_BIST_CAPABLE( x )                      ( ( PCI_GET_BIST( x ) & 0x80 ) >> 7 )


		//
		// PCI Base Address Register macros ( 0x24 - 0x10 )
		//
		#define PCI_GET_BASE_ADDR( x )                         ( ( x & 0xFFFFFFF0 ) >> 4 )
		#define PCI_GET_BASE_ADDR_TYPE( x )                    ( x & 0x00000001 )
		#define PCI_GET_BASE_ADDR_MEM_TYPE( x )                ( ( x & 0x00000006 ) >> 1 )
		#define PCI_GET_BASE_ADDR_MEM_PREFETCHABLE( x )        ( ( x & 0x00000008 ) >> 3 )

		#define PCI_GET_BASE_ADDR_MEM_TYPE_STRING( x )	( ( ( PCI_GET_BASE_ADDR_MEM_TYPE( x ) == 0x0 ) ? "locate anywhere 32-bit addr space" :     \
														( ( ( PCI_GET_BASE_ADDR_MEM_TYPE( x ) == 0x1 ) ? "locate below 1 Meg" :                    \
														( ( ( PCI_GET_BASE_ADDR_MEM_TYPE( x ) == 0x2 ) ? "locate anywhere 64-bit addr space" :     \
														"reserved" ) ) ) ) ) )

		//
		// PCI Max_Lat/Min_Grant/Interrupt Pin/Interrupt Line Register macros ( 0x3C )
		//
		#define PCI_GET_MAX_LAT( x )                         ( ( x & 0xFF000000 ) >> 24 )
		#define PCI_GET_MIN_GRANT( x )                       ( ( x & 0x00FF0000 ) >> 16 )
		#define PCI_GET_INTR_PIN( x )                        ( ( x & 0x0000FF00 ) >> 8 )
		#define PCI_GET_INTR_LINE( x )                       ( x & 0x000000FF )

	}	// end gen3 namespace
}	// end arc namespace


#endif
