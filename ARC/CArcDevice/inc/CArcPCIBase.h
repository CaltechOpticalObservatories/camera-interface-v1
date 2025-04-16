#ifndef _CARC_PCIBASE_H_
#define _CARC_PCIBASE_H_

#ifdef _WINDOWS
	#pragma warning( disable: 4251 )
#endif

#include <vector>
#include <string>
#include <memory>

#include <CArcDeviceDllMain.h>
#include <CArcStringList.h>
#include <CArcDevice.h>



namespace arc
{
	namespace gen3
	{

		// +-------------------------------------------------------------------+
		// |  PCI configuration space register data value                      |
		// +-------------------------------------------------------------------+
		using CStrListPtr = std::unique_ptr<arc::gen3::CArcStringList>;

		typedef struct PCI_REG_DATA
		{
			CStrListPtr		pBitList;
			std::string		sName;
			std::uint32_t	uiValue;
			std::uint32_t	uiAddr;
		} PCIRegData;

		using PCIRegDataPtr = std::shared_ptr<PCIRegData>;
		using PCIRegList	= std::vector<PCIRegDataPtr>;


		// +-------------------------------------------------------------------+
		// |  PCI configuration space register bar item ( used to name and     |
		// |  contain a set of local gen3 configuration registers; if they     |
		// |  exist )                                                          |
		// +-------------------------------------------------------------------+
		using PCIRegListPtr = std::unique_ptr<PCIRegList>;

		typedef struct PCI_BAR_DATA
		{
			std::string		sName;
			PCIRegListPtr	pList;
		} PCIBarData;

		using PCIBarDataPtr = std::shared_ptr<PCIBarData>;
		using PCIBarList	= std::vector<PCIBarDataPtr>;


		// +-------------------------------------------------------------------+
		// |  PCI Base Class exported in CArcDevice.dll                        |
		// +-------------------------------------------------------------------+
		class GEN3_CARCDEVICE_API CArcPCIBase : public CArcDevice
		{
			public:

				CArcPCIBase( void ) = default;

				virtual ~CArcPCIBase( void );

				virtual std::uint32_t	getCfgSpByte( std::uint32_t uiOffset )  = 0;
				virtual std::uint32_t	getCfgSpWord( std::uint32_t uiOffset )  = 0;
				virtual std::uint32_t	getCfgSpDWord( std::uint32_t uiOffset ) = 0;

				virtual void			setCfgSpByte( std::uint32_t uiOffset, std::uint32_t uiValue )  = 0;
				virtual void			setCfgSpWord( std::uint32_t uiOffset, std::uint32_t uiValue )  = 0;
				virtual void			setCfgSpDWord( std::uint32_t uiOffset, std::uint32_t uiValue ) = 0;

				virtual void			getCfgSp( void );
				virtual void			getBarSp( void );

				std::uint32_t			getCfgSpCount( void );
				std::uint32_t			getCfgSpAddr( std::uint32_t uiIndex );
				std::uint32_t			getCfgSpValue( std::uint32_t uiIndex );
				const std::string		getCfgSpName( std::uint32_t uiIndex );
				const std::string*		getCfgSpBitList( std::uint32_t uiIndex, std::uint32_t& pCount );

				std::uint32_t			getBarCount( void );
				const std::string		getBarName( std::uint32_t dIndex );

				std::uint32_t			getBarRegCount( std::uint32_t dIndex );
				std::uint32_t			getBarRegAddr( std::uint32_t dIndex, std::uint32_t dRegIndex );
				std::uint32_t			getBarRegValue( std::uint32_t dIndex, std::uint32_t dRegIndex );
				const std::string		getBarRegName( std::uint32_t dIndex, std::uint32_t dRegIndex );

				std::uint32_t			getBarRegBitListCount( std::uint32_t dIndex, std::uint32_t dRegIndex );
				const std::string		getBarRegBitListDef( std::uint32_t dIndex, std::uint32_t dRegIndex, std::uint32_t dBitListIndex );

//				const std::string*		getBarRegBitList( std::uint32_t dIndex, std::uint32_t dRegIndex, std::uint32_t& pCount );

				void					printCfgSp( void );
				void					printBars( void );

			protected:

				void addRegItem( arc::gen3::PCIRegList* pvDataList, std::uint32_t uiAddr, const std::string& sName, std::uint32_t uiValue, arc::gen3::CArcStringList* pBitList = nullptr );

				void addBarItem( const std::string& sName, PCIRegList* pList );

				std::unique_ptr<arc::gen3::CArcStringList> getDevVenBitList( std::uint32_t uiData, bool bDrawSeparator = false );
				std::unique_ptr<arc::gen3::CArcStringList> getCommandBitList( std::uint32_t uiData, bool bDrawSeparator = false );
				std::unique_ptr<arc::gen3::CArcStringList> getStatusBitList( std::uint32_t uiData, bool bDrawSeparator = false );
				std::unique_ptr<arc::gen3::CArcStringList> getClassRevBitList( std::uint32_t uiData, bool bDrawSeparator = false );
				std::unique_ptr<arc::gen3::CArcStringList> getBistHeaderLatencyCache( std::uint32_t uiData, bool bDrawSeparator = false );
				std::unique_ptr<arc::gen3::CArcStringList> getBaseAddressBitList( std::uint32_t uiData, bool bDrawSeparator = false );
				std::unique_ptr<arc::gen3::CArcStringList> getSubSysBitList( std::uint32_t uiData, bool bDrawSeparator = false );
				std::unique_ptr<arc::gen3::CArcStringList> getMaxLatGntIntBitList( std::uint32_t uiData, bool bDrawSeparator = false );

				//  Smart pointer array deleter
				// +--------------------------------------------------------------------+
				template<typename T> static void ArrayDeleter( T* p );

				struct VectorDeleter
				{
					void operator()( PCIRegList* p ) const
					{
						if ( p != nullptr )
						{
							p->clear();

							delete p;
						}
					}
				};

//				std::unique_ptr<PCIRegList, CArcPCIBase::VectorDeleter> m_pCfgSpList;
				std::shared_ptr<PCIRegList> m_pCfgSpList;
				std::shared_ptr<PCIBarList> m_pBarList;

				std::shared_ptr<std::string> m_pTmpCfgBitList;
				std::shared_ptr<std::string> m_pTmpBarBitList;
		};

	}	// end gen3 namespace
}	// end arc namespace


#endif		// _CARC_PCIBASE_H_
