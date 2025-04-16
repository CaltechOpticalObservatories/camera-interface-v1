#include <iostream>
#include <sstream>
#include <cstdarg>
#include <iomanip>

#ifdef _WINDOWS
	#include <windows.h>
#else
	#include <errno.h>
#endif

#include <CArcBase.h>
#include <CArcPCIBase.h>
#include <CArcStringList.h>
#include <PCIRegs.h>



namespace arc
{
	namespace gen3
	{

		// +---------------------------------------------------------------------------+
		// | ArrayDeleter                                                              |
		// +---------------------------------------------------------------------------+
		// | Called by std::shared_ptr to delete the temporary image buffer.           |
		// | This method should NEVER be called directly by the user.                  |
		// +---------------------------------------------------------------------------+
		template<typename T> void CArcPCIBase::ArrayDeleter( T* p )
		{
			if ( p != nullptr )
			{
				delete [] p;
			}
		}


		// +----------------------------------------------------------------------------
		// |  Destructor
		// +----------------------------------------------------------------------------
		CArcPCIBase::~CArcPCIBase( void )
		{
			m_pCfgSpList.reset();

			m_pBarList.reset();
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
		void CArcPCIBase::getCfgSp( void )
		{
			if ( m_pCfgSpList != nullptr )
			{
				m_pCfgSpList->clear();
			}

			m_pCfgSpList.reset( new PCIRegList );

			if ( m_pCfgSpList == nullptr )
			{
					THROW( "Failed to allocate CfgSp list pointer!" );
			}

			//  Get the standard PCI register values
			// +------------------------------------------------------------+
			std::uint32_t uiRegAddr  = 0;
			std::uint32_t uiRegValue = 0;

			uiRegAddr  = CFG_VENDOR_ID;
			uiRegValue = getCfgSpDWord( uiRegAddr );
			addRegItem( m_pCfgSpList.get(),
						uiRegAddr,
						"Device ID / Vendor ID",
						uiRegValue,
						getDevVenBitList( uiRegValue ).release() );

			uiRegAddr  = CFG_COMMAND;
			uiRegValue = getCfgSpDWord( uiRegAddr );
			auto cRegBitList = getCommandBitList( uiRegValue, false );
			*cRegBitList += *getStatusBitList( uiRegValue, true );
			addRegItem( m_pCfgSpList.get(),
						   uiRegAddr,
						   "Status / Command",
						   uiRegValue,
						   cRegBitList.release() );

			uiRegAddr  = CFG_REV_ID;
			uiRegValue = getCfgSpDWord( uiRegAddr );
			addRegItem( m_pCfgSpList.get(),
						   uiRegAddr,
						   "Base Class / Sub Class / Interface / Revision ID",
						   uiRegValue,
						   getClassRevBitList( uiRegValue ).release() );

			uiRegAddr  = CFG_CACHE_SIZE;
			uiRegValue = getCfgSpDWord( uiRegAddr );
			addRegItem( m_pCfgSpList.get(),
						   uiRegAddr,
						   "BIST / Header Type / Latency Timer / Cache Line Size",
						   uiRegValue,
						   getBistHeaderLatencyCache( uiRegValue, true ).release() );

			uiRegAddr  = CFG_BAR0;
			uiRegValue = getCfgSpDWord( uiRegAddr );
			addRegItem( m_pCfgSpList.get(),
						   uiRegAddr,
						   "PCI Base Address 0",
						   uiRegValue,
						   getBaseAddressBitList( uiRegValue, false ).release() );

			uiRegAddr  = CFG_BAR1;
			uiRegValue = getCfgSpDWord( uiRegAddr );
			addRegItem( m_pCfgSpList.get(),
						   uiRegAddr,
						   "PCI Base Address 1",
						   uiRegValue,
						   getBaseAddressBitList( uiRegValue, false ).release() );

			uiRegAddr  = CFG_BAR2;
			uiRegValue = getCfgSpDWord( uiRegAddr );
			addRegItem( m_pCfgSpList.get(),
						   uiRegAddr,
						   "PCI Base Address 2",
						   uiRegValue,
						   getBaseAddressBitList( uiRegValue, false ).release() );

			uiRegAddr  = CFG_BAR3;
			uiRegValue = getCfgSpDWord( uiRegAddr );
			addRegItem( m_pCfgSpList.get(),
						   uiRegAddr,
						   "PCI Base Address 3",
						   uiRegValue,
						   getBaseAddressBitList( uiRegValue, false ).release() );

			uiRegAddr  = CFG_BAR4;
			uiRegValue = getCfgSpDWord( uiRegAddr );
			addRegItem( m_pCfgSpList.get(),
						   uiRegAddr,
						   "PCI Base Address 4",
						   uiRegValue,
						   getBaseAddressBitList( uiRegValue, false ).release() );

			uiRegAddr  = CFG_BAR5;
			uiRegValue = getCfgSpDWord( uiRegAddr );
			addRegItem( m_pCfgSpList.get(),
						   uiRegAddr,
						   "PCI Base Address 5",
						   uiRegValue,
						   getBaseAddressBitList( uiRegValue, false ).release() );

			uiRegAddr  = CFG_CIS_PTR;
			uiRegValue = getCfgSpDWord( uiRegAddr );
			addRegItem( m_pCfgSpList.get(),
						   uiRegAddr,
						   "Cardbus CIS Pointer",
						   uiRegValue );

			uiRegAddr  = CFG_SUB_VENDOR_ID;
			uiRegValue = getCfgSpDWord( uiRegAddr );
			addRegItem( m_pCfgSpList.get(),
						   uiRegAddr,
						   "Subsystem Device ID / Subsystem Vendor ID",
						   uiRegValue,
						   getSubSysBitList( uiRegValue ).release() );

			uiRegAddr  = CFG_EXP_ROM_BASE;
			uiRegValue = getCfgSpDWord( uiRegAddr );
			addRegItem( m_pCfgSpList.get(),
						   uiRegAddr,
						   "PCI Base Address-to-Local Expansion ROM",
						   uiRegValue );

			uiRegAddr  = CFG_CAP_PTR;
			uiRegValue = getCfgSpDWord( uiRegAddr );
			addRegItem( m_pCfgSpList.get(),
						   uiRegAddr,
						   "Next Capability Pointer",
						   uiRegValue );

			uiRegAddr  = CFG_RESERVED1;
			uiRegValue = getCfgSpDWord( uiRegAddr );
			addRegItem( m_pCfgSpList.get(),
						   uiRegAddr,
						   "Reserved",
						   uiRegValue );

			uiRegAddr  = CFG_INT_LINE;
			uiRegValue = getCfgSpDWord( uiRegAddr );
			addRegItem( m_pCfgSpList.get(),
						   uiRegAddr,
						   "Max_Lat / Min_Grant / Interrupt Pin / Interrupt Line",
						   uiRegValue,
						   getMaxLatGntIntBitList( uiRegValue ).release() );
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
		void CArcPCIBase::getBarSp( void )
		{
			if ( m_pBarList != nullptr )
			{
				m_pBarList->clear();
			}

			m_pBarList.reset( new PCIBarList );

			if ( m_pBarList == nullptr )
			{
				THROW( "Failed to allocate BAR list pointer!" );
			}
		}


		// +----------------------------------------------------------------------------
		// |  getCfgSpCount
		// +----------------------------------------------------------------------------
		// |  Returns the number of elements in the configuration space member list.
		// |
		// |  Throws std::runtime_error if getCfgSp hasn't been called first!
		// +----------------------------------------------------------------------------
		std::uint32_t CArcPCIBase::getCfgSpCount( void )
		{
			if ( m_pCfgSpList == nullptr )
			{
				THROW( "Empty register list, call GetCfgSp() first!" );
			}

			return static_cast<int>( m_pCfgSpList->size() );
		}


		// +----------------------------------------------------------------------------
		// |  getCfgSpAddr
		// +----------------------------------------------------------------------------
		// |  Returns the address for the specified configuration space member list
		// |  register.
		// |
		// |  <IN> -> uiIndex - Index into the cfg sp member list. Use getCfgSpCount().
		// |
		// |  Throws std::runtime_error if getCfgSp hasn't been called first!
		// +----------------------------------------------------------------------------
		std::uint32_t CArcPCIBase::getCfgSpAddr( std::uint32_t uiIndex )
		{
			if ( m_pCfgSpList == nullptr )
			{
				THROW( "Empty register list, call GetCfgSp() first!" );
			}

			if ( uiIndex >= m_pCfgSpList->size() )
			{
				THROW_OUT_OF_RANGE( uiIndex, std::make_pair( static_cast< std::uint32_t >( 0 ), static_cast<std::uint32_t>( m_pCfgSpList->size() ) ) );
			}

			return m_pCfgSpList->at( uiIndex )->uiAddr;
		}


		// +----------------------------------------------------------------------------
		// |  getCfgSpValue
		// +----------------------------------------------------------------------------
		// |  Returns the value for the specified configuration space member list
		// |  register.
		// |
		// |  <IN> -> uiIndex - Index into the cfg sp member list. Use getCfgSpCount().
		// |
		// |  Throws std::runtime_error if getCfgSp hasn't been called first!
		// +----------------------------------------------------------------------------
		std::uint32_t CArcPCIBase::getCfgSpValue( std::uint32_t uiIndex )
		{
			if ( m_pCfgSpList == nullptr )
			{
				THROW( "Empty register list, call GetCfgSp() first!" );
			}

			if ( uiIndex >= m_pCfgSpList->size() )
			{
				THROW_OUT_OF_RANGE( uiIndex, std::make_pair( static_cast< std::uint32_t >( 0 ), static_cast< std::uint32_t >( m_pCfgSpList->size() ) ) );
			}

			return m_pCfgSpList->at( uiIndex )->uiValue;
		}


		// +----------------------------------------------------------------------------
		// |  getCfgSpName
		// +----------------------------------------------------------------------------
		// |  Returns the name for the specified configuration space member list
		// |  register.
		// |
		// |  <IN> -> uiIndex - Index into the cfg sp member list. Use getCfgSpCount().
		// |
		// |  Throws std::runtime_error if getCfgSp hasn't been called first!
		// +----------------------------------------------------------------------------
		const std::string CArcPCIBase::getCfgSpName( std::uint32_t uiIndex )
		{
			if ( m_pCfgSpList == nullptr )
			{
				THROW( "Empty register list, call GetCfgSp() first!" );
			}

			if ( uiIndex >= m_pCfgSpList->size() )
			{
				THROW_OUT_OF_RANGE( uiIndex, std::make_pair( static_cast< std::uint32_t >( 0 ), static_cast< std::uint32_t >( m_pCfgSpList->size() ) ) );
			}

			return m_pCfgSpList->at( uiIndex )->sName;
		}


		// +----------------------------------------------------------------------------
		// |  getCfgSpBitList
		// +----------------------------------------------------------------------------
		// |  Returns a pointer to the bit list for the specified configuration space
		// |  member list register.
		// |
		// |  <IN> -> uiIndex - Index into the cfg sp member list. Use getCfgSpCount().
		// |  <IN> -> pCount - Reference to local variable that will receive list count.
		// |
		// |  Throws std::runtime_error if getCfgSp hasn't been called first!
		// +----------------------------------------------------------------------------
		const std::string* CArcPCIBase::getCfgSpBitList( std::uint32_t uiIndex, std::uint32_t& pCount )
		{
			if ( m_pCfgSpList == nullptr )
			{
				THROW( "Empty register list, call GetCfgSp() first!" );
			}

			if ( uiIndex >= static_cast<std::uint32_t>( m_pCfgSpList->size() ) )
			{
				THROW( "Configuration space index [ %u ] out of range [ 0 - %u ]", uiIndex, m_pCfgSpList->size() );
			}

			arc::gen3::CArcStringList* pBitList = m_pCfgSpList->at( uiIndex )->pBitList.get();

			if ( pBitList != nullptr )
			{
				m_pTmpCfgBitList.reset( new std::string[ pBitList->length() ], &CArcPCIBase::ArrayDeleter<std::string> );

				if ( m_pTmpCfgBitList != nullptr )
				{
					for ( decltype( pBitList->length() ) i = 0; i < pBitList->length(); i++ )
					{
						( m_pTmpCfgBitList.get() )[ i ].assign( pBitList->at( i ) );
					}

					pCount = pBitList->length();
				}

				pCount = pBitList->length();
			}
			else
			{
				pCount = 0;
			}

			return m_pTmpCfgBitList.get();
		}


		// +----------------------------------------------------------------------------
		// |  getBarCount
		// +----------------------------------------------------------------------------
		// |  Returns the number of elements in the base address register member list.
		// |
		// |  Throws std::runtime_error if getBarSp hasn't been called first!
		// +----------------------------------------------------------------------------
		std::uint32_t CArcPCIBase::getBarCount( void )
		{
			if ( m_pBarList == nullptr )
			{
				THROW( "Empty register list, call GetBarSp() first!" );
			}

			return static_cast< std::uint32_t >( m_pBarList->size() );
		}


		// +----------------------------------------------------------------------------
		// |  getBarRegCount
		// +----------------------------------------------------------------------------
		// |  Returns the number of register elements for the specified base address
		// |  member list index.
		// |
		// |  <IN> -> uiIndex - Index into the BAR member list. Use getBarCount().
		// |
		// |  Throws std::runtime_error if getBarSp hasn't been called first!
		// +----------------------------------------------------------------------------
		std::uint32_t CArcPCIBase::getBarRegCount( std::uint32_t uiIndex )
		{
			if ( m_pBarList == nullptr )
			{
				THROW( "Empty register list, call GetBarSp() first!" );
			}

			if ( uiIndex >= m_pBarList->size() )
			{
				THROW_OUT_OF_RANGE( uiIndex, std::make_pair( static_cast< std::uint32_t >( 0 ), static_cast< std::uint32_t >( m_pBarList->size() ) ) );
			}

			if ( m_pBarList->at( uiIndex )->pList == nullptr )
			{
				THROW( "Empty register list, call GetBarSp() first!" );
			}

			return static_cast< std::uint32_t >( m_pBarList->at( uiIndex )->pList->size() );
		}


		// +----------------------------------------------------------------------------
		// |  getBarRegAddr
		// +----------------------------------------------------------------------------
		// |  Returns the address of the specified base address member list register.
		// |
		// |  <IN> -> uiIndex    - Index into the BAR member list. Use getBarCount().
		// |  <IN> -> uiRegIndex - Index into the BAR member list register list. Use
		// |                      getBarRegCount().
		// |
		// |  Throws std::runtime_error if getBarSp hasn't been called first!
		// +----------------------------------------------------------------------------
		std::uint32_t CArcPCIBase::getBarRegAddr( std::uint32_t uiIndex, std::uint32_t uiRegIndex )
		{
			if ( m_pBarList == nullptr )
			{
				THROW( "Empty register list, call GetBarSp() first!" );
			}

			if ( uiIndex >= m_pBarList->size() )
			{
				THROW_OUT_OF_RANGE( uiIndex, std::make_pair( static_cast< std::uint32_t >( 0 ), static_cast< std::uint32_t >( m_pBarList->size() ) ) );
			}

			if ( m_pBarList->at( uiIndex )->pList == nullptr )
			{
				THROW( "Empty register list, call GetBarSp() first!" );
			}

			if ( uiRegIndex >= m_pBarList->at( uiIndex )->pList->size() )
			{
				THROW_OUT_OF_RANGE( uiIndex, std::make_pair( static_cast< std::uint32_t >( 0 ), static_cast< std::uint32_t >( m_pBarList->at( uiIndex )->pList->size() ) ) );
			}

			return m_pBarList->at( uiIndex )->pList->at( uiRegIndex )->uiAddr;
		}


		// +----------------------------------------------------------------------------
		// |  getBarRegValue
		// +----------------------------------------------------------------------------
		// |  Returns the value of the specified base address member list register.
		// |
		// |  <IN> -> uiIndex    - Index into the BAR member list. Use getBarCount().
		// |  <IN> -> uiRegIndex - Index into the BAR member list register list. Use
		// |                      getBarRegCount().
		// |
		// |  Throws std::runtime_error if getBarSp hasn't been called first!
		// +----------------------------------------------------------------------------
		std::uint32_t CArcPCIBase::getBarRegValue( std::uint32_t uiIndex, std::uint32_t uiRegIndex )
		{
			if ( m_pBarList == nullptr )
			{
				THROW( "Empty register list, call GetBarSp() first!" );
			}

			if ( uiIndex >= m_pBarList->size() )
			{
				THROW_OUT_OF_RANGE( uiIndex, std::make_pair( static_cast< std::uint32_t >( 0 ), static_cast< std::uint32_t >( m_pBarList->size() ) ) );
			}

			if ( m_pBarList->at( uiIndex )->pList == nullptr )
			{
				THROW( "Empty register list, call GetBarSp() first!" );
			}

			if ( uiRegIndex >= m_pBarList->at( uiIndex )->pList->size() )
			{
				THROW_OUT_OF_RANGE( uiIndex, std::make_pair( static_cast< std::uint32_t >( 0 ), static_cast< std::uint32_t >( m_pBarList->at( uiIndex )->pList->size() ) ) );
			}

			return m_pBarList->at( uiIndex )->pList->at( uiRegIndex )->uiValue;	
		}


		// +----------------------------------------------------------------------------
		// |  getBarName
		// +----------------------------------------------------------------------------
		// |  Returns the name of the specified base address.
		// |
		// |  <IN> -> uiIndex - Index into the BAR member list. Use getBarCount().
		// |
		// |  Throws std::runtime_error if getBarSp hasn't been called first!
		// +----------------------------------------------------------------------------
		const std::string CArcPCIBase::getBarName( std::uint32_t uiIndex )
		{
			if ( m_pBarList == nullptr )
			{
				THROW( "Empty register list, call GetBarSp() first!" );
			}

			if ( uiIndex >= m_pBarList->size() )
			{
				THROW_OUT_OF_RANGE( uiIndex, std::make_pair( static_cast< std::uint32_t >( 0 ), static_cast< std::uint32_t >( m_pBarList->size() ) ) );
			}

			return m_pBarList->at( uiIndex )->sName;
		}


		// +----------------------------------------------------------------------------
		// |  getBarRegName
		// +----------------------------------------------------------------------------
		// |  Returns the name of the specified base address member list register.
		// |
		// |  <IN> -> uiIndex    - Index into the BAR member list. Use getBarCount().
		// |  <IN> -> uiRegIndex - Index into the BAR member list register list. Use
		// |                      getBarRegCount().
		// |
		// |  Throws std::runtime_error if getBarSp hasn't been called first!
		// +----------------------------------------------------------------------------
		const std::string CArcPCIBase::getBarRegName( std::uint32_t uiIndex, std::uint32_t uiRegIndex )
		{
			if ( m_pBarList == nullptr )
			{
				THROW( "Empty register list, call GetBarSp() first!" );
			}

			if ( uiIndex >= m_pBarList->size() )
			{
				THROW_OUT_OF_RANGE( uiIndex, std::make_pair( static_cast< std::uint32_t >( 0 ), static_cast< std::uint32_t >( m_pBarList->size() ) ) );
			}

			if ( m_pBarList->at( uiIndex )->pList == nullptr )
			{
				THROW( "Empty register list, call GetBarSp() first!" );
			}

			if ( uiRegIndex >= m_pBarList->at( uiIndex )->pList->size() )
			{
				THROW_OUT_OF_RANGE( uiIndex, std::make_pair( static_cast< std::uint32_t >( 0 ), static_cast< std::uint32_t >( m_pBarList->at( uiIndex )->pList->size() ) ) );
			}

			return m_pBarList->at( uiIndex )->pList->at( uiRegIndex )->sName;
		}


		// +----------------------------------------------------------------------------
		// |  getBarRegBitListCount
		// +----------------------------------------------------------------------------
		// |  Returns the number of elements of the specified base address member list
		// |  register bit list.
		// |
		// |  <IN> -> uiIndex    - Index into the BAR member list. Use getBarCount().
		// |  <IN> -> uiRegIndex - Index into the BAR member list register list. Use
		// |                      getBarRegCount().
		// |
		// |  Throws std::runtime_error if getBarSp hasn't been called first!
		// +----------------------------------------------------------------------------
		std::uint32_t CArcPCIBase::getBarRegBitListCount( std::uint32_t uiIndex, std::uint32_t uiRegIndex )
		{
			if ( m_pBarList == nullptr )
			{
				THROW( "Empty register list, call GetBarSp() first!" );
			}

			if ( uiIndex >= m_pBarList->size() )
			{
				THROW_OUT_OF_RANGE( uiIndex, std::make_pair( static_cast< std::uint32_t >( 0 ), static_cast< std::uint32_t >( m_pBarList->size() ) ) );
			}

			if ( m_pBarList->at( uiIndex )->pList == nullptr )
			{
				THROW( "Empty register list, call GetBarSp() first!" );
			}

			if ( uiRegIndex >= m_pBarList->at( uiIndex )->pList->size() )
			{
				THROW_OUT_OF_RANGE( uiIndex, std::make_pair( static_cast< std::uint32_t >( 0 ), static_cast< std::uint32_t >( m_pBarList->at( uiIndex )->pList->size() ) ) );
			}

			if ( m_pBarList->at( uiIndex )->pList->at( uiRegIndex )->pBitList == nullptr )
			{
				return 0;
			}

			return m_pBarList->at( uiIndex )->pList->at( uiRegIndex )->pBitList->length();
		}


		// +----------------------------------------------------------------------------
		// |  getBarRegBitListDef
		// +----------------------------------------------------------------------------
		// |  Returns the bit definition std::string of the specified base address member
		// |  list register bit list index.
		// |
		// |  <IN> -> uiIndex        - Index into the BAR member list. Use getBarCount().
		// |  <IN> -> uiRegIndex     - Index into the BAR member list register list. Use
		// |                          getBarRegCount().
		// |  <IN> -> uiBitListIndex - Index into the BAR member list register list bit
		// |                          list. Use getBarRegBitListCount().
		// |
		// |  Throws std::runtime_error if getBarSp hasn't been called first!
		// +----------------------------------------------------------------------------
		const std::string CArcPCIBase::getBarRegBitListDef( std::uint32_t uiIndex, std::uint32_t uiRegIndex, std::uint32_t uiBitListIndex )
		{
			if ( m_pBarList == nullptr )
			{
				THROW( "Empty register list, call GetBarSp() first!" );
			}

			if ( uiIndex >= m_pBarList->size() )
			{
				THROW_OUT_OF_RANGE( uiIndex, std::make_pair( static_cast< std::uint32_t >( 0 ), static_cast< std::uint32_t >( m_pBarList->size() ) ) );
			}

			if ( m_pBarList->at( uiIndex )->pList == nullptr )
			{
				THROW( "Empty register list, call GetBarSp() first!" );
			}

			if ( uiRegIndex >= m_pBarList->at( uiIndex )->pList->size() )
			{
				THROW_OUT_OF_RANGE( uiIndex, std::make_pair( static_cast< std::uint32_t >( 0 ), static_cast< std::uint32_t >( m_pBarList->at( uiIndex )->pList->size() ) ) );
			}

			if ( m_pBarList->at( uiIndex )->pList->at( uiRegIndex )->pBitList == nullptr )
			{
				return std::string( "" );
			}

			return m_pBarList->at( uiIndex )->pList->at( uiRegIndex )->pBitList->at( uiBitListIndex );
		}


		//// +----------------------------------------------------------------------------
		//// |  getBarRegBitList
		//// +----------------------------------------------------------------------------
		//// |  Returns a pointer to the bit list for the specified base address member
		//// |  list register.
		//// |
		//// |  <IN> -> uiIndex    - Index into the BAR member list. Use getBarCount().
		//// |  <IN> -> uiRegIndex - Index into the BAR member list register list. Use
		//// |                      getBarRegCount().
		//// |  <IN> -> pCount    - Reference to local variable that will receive list
		//// |                      count.
		//// |
		//// |  Throws std::runtime_error if getBarSp hasn't been called first!
		//// +----------------------------------------------------------------------------
		//const std::string* CArcPCIBase::getBarRegBitList( std::uint32_t uiIndex, std::uint32_t uiRegIndex, std::uint32_t& pCount )
		//{
		//	if ( m_pBarList == nullptr )
		//	{
		//		THROW( "Empty register list, call GetBarSp() first!" );
		//	}

		//	if ( uiIndex >= static_cast<std::uint32_t>( m_pBarList->size() ) )
		//	{
		//		THROW( "Base address register index [ %u ] out of range [ 0 - %u ]", uiIndex, m_pBarList->size() );
		//	}

		//	if ( m_pBarList->at( uiIndex )->pList == nullptr )
		//	{
		//		THROW( "Empty register list, call GetBarSp() first!" );
		//	}

		//	if ( uiRegIndex >= m_pBarList->at( uiIndex )->pList->size() )
		//	{
		//		THROW_OUT_OF_RANGE( uiIndex, std::make_pair( static_cast< std::uint32_t >( 0 ), static_cast< std::uint32_t >( m_pBarList->at( uiIndex )->pList->size() ) ) );
		//	}

		//	arc::gen3::CArcStringList* pBitList = m_pBarList->at( uiIndex )->pList->at( uiRegIndex )->pBitList.release();

		//	if ( pBitList != nullptr )
		//	{
		//		m_pTmpBarBitList.reset( new std::string[ pBitList->length() ], &CArcPCIBase::ArrayDeleter<std::string> );

		//		if ( m_pTmpBarBitList != nullptr )
		//		{
		//			for ( decltype( pBitList->length() ) i = 0; i < pBitList->length(); i++ )
		//			{
		//				( m_pTmpBarBitList.get() )[ i ].assign( pBitList->at( i ) );
		//			}

		//			pCount = pBitList->length();
		//		}

		//		else
		//		{
		//			pCount = 0;
		//		}
		//	}

		//	return m_pTmpBarBitList.get();
		//}


		// +----------------------------------------------------------------------------
		// |  printCfgSp
		// +----------------------------------------------------------------------------
		// |  Prints the member configuration list to std out.
		// |
		// |  Throws std::runtime_error if getCfgSp hasn't been called first!
		// +----------------------------------------------------------------------------
		void CArcPCIBase::printCfgSp( void )
		{
			std::cout << std::endl;
			std::cout << "_________________________Configuration Space_________________________"
					  << std::endl << std::endl;

			for ( std::uint32_t i = 0; i  <getCfgSpCount(); i++ )
			{
				std::cout << "\tAddr: 0x" << std::hex << getCfgSpAddr( i ) << std::dec << std::endl
						  << "\tValue: 0x" << std::hex << getCfgSpValue( i ) << std::dec << std::endl
						  << "\tName: " << getCfgSpName( i ) << std::endl;

				std::uint32_t uiCount = 0;

				const std::string* psBitList = getCfgSpBitList( i, uiCount );

				for ( std::uint32_t j = 0; j < uiCount; j++ )
				{
					std::cout << "\tBit List[ " << j << " ]: " << psBitList[ j ] << std::endl;
				}

				std::cout << std::endl;
			}

			std::cout << std::endl;
		}


		// +----------------------------------------------------------------------------
		// |  printBars
		// +----------------------------------------------------------------------------
		// |  Prints the member base address list to std out.
		// |
		// |  Throws std::runtime_error if getBarSp hasn't been called first!
		// +----------------------------------------------------------------------------
		void CArcPCIBase::printBars( void )
		{
			std::cout << std::endl;
			std::cout << "_______________________Configuration Space BARS_______________________"
					  << std::endl << std::endl;

			for ( std::uint32_t i = 0; i < getBarCount(); i++ )
			{
				std::cout << std::endl
						  << "___________________" << getBarName( i ) << "___________________"
						  << std::endl << std::endl;

				for ( std::uint32_t j = 0; j < getBarRegCount( i ); j++ )
				{
					std::cout << "\tReg Addr:  0x" << std::hex << getBarRegAddr( i, j ) << std::dec << std::endl
							  << "\tReg Value: 0x" << std::hex << getBarRegValue( i, j ) << std::dec << std::endl
							  << "\tReg Name: " << getBarRegName( i, j ) << std::endl;

					for ( std::uint32_t k = 0; k < getBarRegBitListCount( i, j ); k++ )
					{
						std::cout << "\tBit List: " << getBarRegBitListDef( i, j, k ) << std::endl;
					}

					std::cout << std::endl;
				}
			}

			std::cout << std::endl;
		}


		// +----------------------------------------------------------------------------
		// |  addRegItem
		// +----------------------------------------------------------------------------
		// |  Adds the specified parameters to the specified register data list.
		// |  Convenience method.
		// |
		// |  <IN> -> pDataList - PCI register list that will contain the remaining
		// |                      parameters.
		// |  <IN> -> uiAddr     - Register address
		// |  <IN> -> sName		- Register name
		// |  <IN> -> uiValue    - Register value
		// |  <IN> -> pBitList  - String list of register bit definitions.
		// |
		// |  Throws std::runtime_error if data list pointer is invalid.
		// +----------------------------------------------------------------------------
		void CArcPCIBase::addRegItem( arc::gen3::PCIRegList* pvDataList, std::uint32_t uiAddr, const std::string& sName, std::uint32_t uiValue, arc::gen3::CArcStringList* pBitList )
		{
			if ( pvDataList != nullptr )
			{
				std::shared_ptr<PCIRegData> pConfigData( new PCIRegData() );

				if ( pConfigData.get() == nullptr )
				{
					THROW( "Failed to allocate PCIRegData structure!" );
				}

				pConfigData->uiAddr     = uiAddr;
				pConfigData->sName      = sName;
				pConfigData->uiValue    = uiValue;

				pConfigData->pBitList.reset( pBitList );

				pvDataList->push_back( std::move( pConfigData ) );
			}

			else
			{
				THROW( "Invalid config data list pointer ( nullptr )!" );
			}
		}


		// +----------------------------------------------------------------------------
		// |  addBarItem
		// +----------------------------------------------------------------------------
		// |  Adds an element with the specified parameters to the base address
		// |  register list.
		// |
		// |  <IN> -> sName  - BAR name
		// |  <IN> -> pList  - BAR register list
		// |
		// |  Throws std::runtime_error on error
		// +----------------------------------------------------------------------------
		void CArcPCIBase::addBarItem( const std::string& sName, PCIRegList* pList )
		{
			if ( m_pBarList.get() != nullptr )
			{
				std::shared_ptr<PCIBarData> pLocalItem( new PCIBarData() );

				pLocalItem->sName = sName;

				pLocalItem->pList.reset( pList );

				m_pBarList->push_back( std::move( pLocalItem ) );
			}

			else
			{
				THROW( "Invalid config data list pointer ( nullptr )!" );
			}
		}


		// +----------------------------------------------------------------------------
		// |  getDevVenBitList
		// +----------------------------------------------------------------------------
		// |  Sets the bit list strings for the DEVICE ID and VENDOR ID ( 0x0 )PCI Cfg
		// |  Sp register.
		// |
		// |  <IN> -> uiData  - The PCI cfg sp DEVICE and VENDOR ID register value.
		// |  <IN> -> bDrawSeparator - 'true' to include a line separator within the
		// |                            bit list strings.
		// |
		// |  Throws std::runtime_error on error
		// +----------------------------------------------------------------------------
		std::unique_ptr<arc::gen3::CArcStringList> CArcPCIBase::getDevVenBitList( std::uint32_t uiData, bool bDrawSeparator )
		{
			std::unique_ptr<arc::gen3::CArcStringList> pBitList( new arc::gen3::CArcStringList() );

			if ( !pBitList->empty() )
			{
				pBitList->clear();
			}

			if ( bDrawSeparator )
			{
				*pBitList << "____________________________________________________";
			}

			*pBitList << CArcBase::formatString( "Device ID: 0x%X", PCI_GET_DEV( uiData ) )
					  << CArcBase::formatString( "Vendor ID: 0x%X", PCI_GET_VEN( uiData ) );

			return std::move( pBitList );
		}


		// +----------------------------------------------------------------------------
		// |  getCommandBitList
		// +----------------------------------------------------------------------------
		// |  Sets the bit list strings for the COMMAND ( 0x4 ) PCI Cfg Sp register.
		// |
		// |  <IN> -> uiData  - The PCI cfg sp COMMAND register value.
		// |  <IN> -> bDrawSeparator - 'true' to include a line separator within the
		// |                            bit list strings.
		// |
		// |  Throws std::runtime_error on error
		// +----------------------------------------------------------------------------
		std::unique_ptr<arc::gen3::CArcStringList> CArcPCIBase::getCommandBitList( std::uint32_t uiData, bool bDrawSeparator )
		{
			std::unique_ptr<arc::gen3::CArcStringList> pBitList( new arc::gen3::CArcStringList() );

			if ( bDrawSeparator )
			{
				*pBitList << "____________________________________________________";
			}

			*pBitList << CArcBase::formatString( "PCI COMMAND BIT DEFINITIONS ( 0x%X )",
												   PCI_GET_CMD( uiData ) )

					  << CArcBase::formatString( "Bit  0 : I/O Access Enable : %d",
												   PCI_GET_CMD_IO_ACCESS_ENABLED( uiData ) )

					  << CArcBase::formatString( "Bit  1 : Memory Space Enable : %d",
												   PCI_GET_CMD_MEMORY_ACCESS_ENABLED( uiData ) )

					  << CArcBase::formatString( "Bit  2 : Bus Master Enable : %d",
												   PCI_GET_CMD_ENABLE_MASTERING( uiData ) )

					  << CArcBase::formatString( "Bit  3 : Special Cycle Enable : %d",
												   PCI_GET_CMD_SPECIAL_CYCLE_MONITORING( uiData ) )

					  << CArcBase::formatString( "Bit  4 : Memory Write and Invalidate : %d",
												   PCI_GET_CMD_MEM_WRITE_INVAL_ENABLE( uiData ) )

					  << CArcBase::formatString( "Bit  5 : VGA Palette Snoop : %d",
												   PCI_GET_CMD_PALETTE_SNOOP_ENABLE( uiData ) )

					  << CArcBase::formatString( "Bit  6 : Parity Error Response Enable : %d",
												   PCI_GET_CMD_PARITY_ERROR_RESPONSE( uiData ) )

					  << CArcBase::formatString( "Bit  7 : Address Stepping Enable : %d",
												   PCI_GET_CMD_WAIT_CYCLE_CONTROL( uiData ) )

					  << CArcBase::formatString( "Bit  8 : Internal SERR# Enable : %d",
												   PCI_GET_CMD_SERR_ENABLE( uiData ) )

					  << CArcBase::formatString( "Bit  9 : Fast Back-to-Back Enable : %d",
												   PCI_GET_CMD_FAST_BACK2BACK_ENABLE( uiData ) )

					  << std::string( "Bit 10-15 : Reserved" );

			return std::move( pBitList );
		}


		// +----------------------------------------------------------------------------
		// |  getStatusBitList
		// +----------------------------------------------------------------------------
		// |  Sets the bit list strings for the STATUS ( 0x6 ) PCI Cfg Sp register.
		// |
		// |  <IN> -> uiData  - The PCI cfg sp STATUS register value.
		// |  <IN> -> bDrawSeparator - 'true' to include a line separator within the
		// |                            bit list strings.
		// |
		// |  Throws std::runtime_error on error
		// +----------------------------------------------------------------------------
		std::unique_ptr<arc::gen3::CArcStringList> CArcPCIBase::getStatusBitList( std::uint32_t uiData, bool bDrawSeparator )
		{
			std::unique_ptr<arc::gen3::CArcStringList> pBitList( new arc::gen3::CArcStringList() );

			if ( bDrawSeparator )
			{
				*pBitList << "____________________________________________________";
			}

			*pBitList << CArcBase::formatString( "PCI STATUS BIT DEFINITIONS ( 0x%X )",
												   PCI_GET_STATUS( uiData ) )

					  << std::string( "Bit 0-4 : Reserved" )

					  << CArcBase::formatString( "Bit 5 : 66-MHz Capable (Internal Clock Frequency) : %d",
												   PCI_GET_STATUS_66MHZ_CAPABLE( uiData ) )

					  << std::string( "Bit 6 : Reserved" )

					  << CArcBase::formatString( "Bit 7 : Fast Back-to-Back Transactions Capable : %d",
												   PCI_GET_STATUS_FAST_BACK2BACK_CAPABLE( uiData ) )

					  << CArcBase::formatString( "Bit 8 : Master Data Parity Error : %d",
												   PCI_GET_STATUS_DATA_PARITY_REPORTED( uiData ) )

					  << CArcBase::formatString( "Bit 9-10 : DEVSEL Timing : %d [ %s ]",
												   PCI_GET_STATUS_DEVSEL_TIMING( uiData ),
												   PCI_GET_STATUS_GET_DEVSEL_STRING( uiData ) )

					  << CArcBase::formatString( "Bit 11 : Signaled Target Abort : %d",
												   PCI_GET_STATUS_SIGNALLED_TARGET_ABORT( uiData ) )

					  << CArcBase::formatString( "Bit 12 : Received Target Abort : %d",
												   PCI_GET_STATUS_RECEIVED_TARGET_ABORT( uiData ) )

					  << CArcBase::formatString( "Bit 13 : Received Master Abort : %d",
												   PCI_GET_STATUS_RECEIVED_MASTER_ABORT( uiData ) )

					  << CArcBase::formatString( "Bit 14 : Signaled System Error : %d",
												   PCI_GET_STATUS_SIGNALLED_SYSTEM_ERROR( uiData ) )

					  << CArcBase::formatString( "Bit 15 : Detected Parity Error : %d",
												   PCI_GET_STATUS_DETECTED_PARITY_ERROR( uiData ) );

			return std::move( pBitList );
		}


		// +----------------------------------------------------------------------------
		// |  getClassRevBitList
		// +----------------------------------------------------------------------------
		// |  Sets the bit list strings for the CLASS CODE ( 0x8 ) and REVISION ID
		// |  ( 0x9 ) PCI Cfg Sp register.
		// |
		// |  <IN> -> uiData  - The PCI cfg sp CLASS CODE and REV ID register value.
		// |  <IN> -> bDrawSeparator - 'true' to include a line separator within the
		// |                            bit list strings.
		// |
		// |  Throws std::runtime_error on error
		// +----------------------------------------------------------------------------
		std::unique_ptr<arc::gen3::CArcStringList> CArcPCIBase::getClassRevBitList( std::uint32_t uiData, bool bDrawSeparator )
		{
			std::unique_ptr<arc::gen3::CArcStringList> pBitList( new arc::gen3::CArcStringList() );

			if ( bDrawSeparator )
			{
				*pBitList << "____________________________________________________";
			}

			*pBitList << CArcBase::formatString( "Base Class Code: 0x%X [ %s ]",
												   PCI_GET_BASECLASS( uiData ),
												   PCI_GET_GET_BASECLASS_STRING( uiData ) )

					  << CArcBase::formatString( "Sub Class Code: 0x%X", PCI_GET_SUBCLASS( uiData ) )
					  << CArcBase::formatString( "Interface: 0x%X", PCI_GET_INTERFACE( uiData ) )
					  << CArcBase::formatString( "Revision ID: 0x%X", PCI_GET_REVID( uiData ) );

			return std::move( pBitList );
		}


		// +----------------------------------------------------------------------------
		// |  getBistHeaderLatencyCache
		// +----------------------------------------------------------------------------
		// |  Sets the bit list strings for the BIST ( 0xF ), HEADER TYPE ( 0xE ),
		// |  LATENCY TIMER ( 0xD ) and CACHE LINE SIZE ( 0xC ) PCI Cfg Sp registers.
		// |
		// |  <IN> -> uiData  - The PCI cfg sp BIST, HEADER TYPE, LATENCY TIMER and
		// |                   CACHE LINE SIZE register value.
		// |  <IN> -> bDrawSeparator - 'true' to include a line separator within the
		// |                            bit list strings.
		// |
		// |  Throws std::runtime_error on error
		// +----------------------------------------------------------------------------
		std::unique_ptr<arc::gen3::CArcStringList> CArcPCIBase::getBistHeaderLatencyCache( std::uint32_t uiData, bool bDrawSeparator )
		{
			std::unique_ptr<arc::gen3::CArcStringList> pBitList( new arc::gen3::CArcStringList() );

			*pBitList << CArcBase::formatString( "BIST BIT DEFINITIONS ( 0x%X )",
												   PCI_GET_BIST( uiData ) )

					  << CArcBase::formatString( "Bit 0-3 : BIST Completion Code : 0x%X",
												   PCI_GET_BIST_COMPLETE_CODE( uiData ) )

					  << std::string( "Bit 4-5 : Reserved" )

					  << CArcBase::formatString( "Bit 6 : BIST Invoked : %d",
												   PCI_GET_BIST_INVOKED( uiData ) )

					  << CArcBase::formatString( "Bit 7 : Device BIST Capable : %d",
												   PCI_GET_BIST_CAPABLE( uiData ) );

			if ( bDrawSeparator )
			{
				*pBitList << "____________________________________________________";
			}

			*pBitList << CArcBase::formatString( "Header Type: 0x%X", PCI_GET_HEADER_TYPE( uiData ) )
					  << CArcBase::formatString( "Latency Timer: 0x%X", PCI_GET_LATENCY_TIMER( uiData ) )
					  << CArcBase::formatString( "Cache Line Size: 0x%X", PCI_GET_CACHE_LINE_SIZE( uiData ) );

			return std::move( pBitList );
		}


		// +----------------------------------------------------------------------------
		// |  getBaseAddressBitList
		// +----------------------------------------------------------------------------
		// |  Sets the bit list strings for the BASE ADDRESS REGISTERS ( 0x10 to 0x24 )
		// |  PCI Cfg Sp registers.
		// |
		// |  <IN> -> uiData  - The PCI cfg sp BASE ADDRESS REGISTERS register value.
		// |  <IN> -> bDrawSeparator - 'true' to include a line separator within the
		// |                            bit list strings.
		// |
		// |  Throws std::runtime_error on error
		// +----------------------------------------------------------------------------
		std::unique_ptr<arc::gen3::CArcStringList> CArcPCIBase::getBaseAddressBitList( std::uint32_t uiData, bool bDrawSeparator )
		{
			std::unique_ptr<arc::gen3::CArcStringList> pBitList( new arc::gen3::CArcStringList() );

			if ( bDrawSeparator )
			{
				*pBitList << "____________________________________________________";
			}

			*pBitList << CArcBase::formatString( "BASE ADDRESS BIT DEFINITIONS ( 0x%X )", uiData );

			if ( PCI_GET_BASE_ADDR_TYPE( uiData ) == 0 )
			{
				*pBitList << CArcBase::formatString( "Bit 0 : Memory Space Indicator : %d [ Memory Space ]",
													   PCI_GET_BASE_ADDR_TYPE( uiData ) )

						  << CArcBase::formatString( "Bit 1-2 : Type: %d [ %s ]",
													   PCI_GET_BASE_ADDR_MEM_TYPE( uiData ),
													   PCI_GET_BASE_ADDR_MEM_TYPE_STRING( uiData ) )

						  << CArcBase::formatString( "Bit 3 : Prefetchable : %d",
													   PCI_GET_BASE_ADDR_MEM_PREFETCHABLE( uiData ) )

						  << CArcBase::formatString( "Bit 4-31 : Base Address : 0x%X",
													   PCI_GET_BASE_ADDR( uiData ) );
			}

			else
			{
				*pBitList << CArcBase::formatString( "Bit 0 : Memory Space Indicator : %d [ I/O Space ]",
													   PCI_GET_BASE_ADDR_TYPE( uiData ) )

						  << std::string( "Bit 1 : Reserved" )

						  << CArcBase::formatString( "Bit 2-31 : Base Address : 0x%X",
													   PCI_GET_BASE_ADDR( uiData ) );
			}

			return std::move( pBitList );
		}


		// +----------------------------------------------------------------------------
		// |  getSubSysBitList
		// +----------------------------------------------------------------------------
		// |  Sets the bit list strings for the SUBSYSTEM ID's ( 0x2C ) PCI Cfg Sp
		// |  register.
		// |
		// |  <IN> -> uiData  - The PCI cfg sp SUBSYSTEM ID's register value.
		// |  <IN> -> bDrawSeparator - 'true' to include a line separator within the
		// |                            bit list strings.
		// |
		// |  Throws std::runtime_error on error
		// +----------------------------------------------------------------------------
		std::unique_ptr<arc::gen3::CArcStringList> CArcPCIBase::getSubSysBitList( std::uint32_t uiData, bool bDrawSeparator )
		{
			std::unique_ptr<arc::gen3::CArcStringList> pBitList( new arc::gen3::CArcStringList() );

			if ( bDrawSeparator )
			{
				*pBitList << "____________________________________________________";
			}

			*pBitList << CArcBase::formatString( "Subsystem ID: 0x%X", PCI_GET_DEV( uiData ) )
					  << CArcBase::formatString( "Subsystem Vendor ID: 0x%X", PCI_GET_VEN( uiData ) );

			return std::move( pBitList );
		}


		// +----------------------------------------------------------------------------
		// |  getMaxLatGntIntBitList
		// +----------------------------------------------------------------------------
		// |  Sets the bit list strings for the MAX LATENCY ( 0x3F ), MIN GNT ( 0x3E ),
		// |  INTERRUPT PIN ( 0x3D ) and INTERRUPT LINE ( 0x3C ) PCI Cfg Sp register.
		// |
		// |  <IN> -> uiData  - The PCI cfg sp BASE ADDRESS REGISTERS register value.
		// |  <IN> -> bDrawSeparator - 'true' to include a line separator within the
		// |                            bit list strings.
		// |
		// |  Throws std::runtime_error on error
		// +----------------------------------------------------------------------------
		std::unique_ptr<arc::gen3::CArcStringList> CArcPCIBase::getMaxLatGntIntBitList( std::uint32_t uiData, bool bDrawSeparator )
		{
			std::unique_ptr<arc::gen3::CArcStringList> pBitList( new arc::gen3::CArcStringList() );

			if ( bDrawSeparator )
			{
				*pBitList << "____________________________________________________";
			}

			*pBitList << CArcBase::formatString( "Max_Lat: 0x%X", PCI_GET_MAX_LAT( uiData ) )
					  << CArcBase::formatString( "Min_Grant: 0x%X", PCI_GET_MIN_GRANT( uiData ) )
					  << CArcBase::formatString( "Interrupt Pin: 0x%X", PCI_GET_INTR_PIN( uiData ) )

					  << CArcBase::formatString( "Interrupt Line: 0x%X [ %d ]",
												   PCI_GET_INTR_LINE( uiData ),
												   PCI_GET_INTR_LINE( uiData ) );

			return std::move( pBitList );
		}

	}	// end gen3 namespace
}	// end arc namespace