// +----------------------------------------------------------------------+
// | Log.h : Defines a std::string message logging class                       |
// +----------------------------------------------------------------------+

#ifndef _ARC_CLOG_H_
#define _ARC_CLOG_H_

#ifdef _WINDOWS
#pragma warning( disable: 4251 )
#endif

#include <queue>
#include <string>
#include <sstream>
#include <cstdarg>
#include <memory>

#include <CArcDeviceDllMain.h>


namespace arc
{
	namespace gen3
	{

		class GEN3_CARCDEVICE_API CArcLog
		{
			public:

				CArcLog( void );

				~CArcLog( void ) = default;

				void  setMaxSize( std::uint32_t ulSize );

				void  put( const char* szFmt, ... );

				const std::string getNext( void );

				const std::string getLast( void );

				std::uint32_t getLogCount( void );

				bool empty( void );

				void selfTest( void );

			private:

				static std::queue<std::string>::size_type Q_MAX;

				std::unique_ptr<std::queue<std::string>> m_sQueue;
		};

	}	// end gen3 namespace
}	// end arc namespace


#endif
