//
// Log.cpp : Defines a std::string message logging class
//
#ifdef _WINDOWS
	#include <windows.h>
#else
	#include <iostream>
	#include <cstring>
#endif

#include <fstream>

#include <CArcBase.h>
#include <CArcLog.h>



namespace arc
{
	namespace gen3
	{

		std::queue<std::string>::size_type arc::gen3::CArcLog::Q_MAX = 256;	// Maximum number of messages to log


		// +----------------------------------------------------------------------------------------------------+
		// |  Constructor                                                                                       |
		// +----------------------------------------------------------------------------------------------------+
		CArcLog::CArcLog( void )
		{
			m_sQueue.reset( new std::queue<std::string>() );
		}


		// +----------------------------------------------------------------------------
		// |  setMaxSize
		// +----------------------------------------------------------------------------
		// |  Sets the maximum number of messages that the Q can hold.
		// |
		// |  <IN> -> ulSize - The maximum number of message the Q can hold. Must be > 0.
		// +----------------------------------------------------------------------------
		void CArcLog::setMaxSize( std::uint32_t ulSize )
		{
			Q_MAX = std::queue<std::string>::size_type( ulSize );
		}


		// +----------------------------------------------------------------------------
		// |  put
		// +----------------------------------------------------------------------------
		// |  Inserts a message into the log queue. It dumps the oldest message if
		// |  the queue size is greater than or equal to Q_MAX.
		// |
		// |  <IN> -> fmt - C-printf style format (sort of):
		// |				%d   -> Integer
		// |				%f   -> Double
		// |				%s   -> Char *, std::string not accepted
		// |				%x,X -> Integer as hex, produces uppercase only
		// |				%e   -> Special case that produces a std::string message from
		// |				        one of the system functions ::GetLastError or
		// |				        strerror, depending on the OS.
		// +----------------------------------------------------------------------------
		void CArcLog::put( const char* szFmt, ... )
		{
			va_list ap;
			va_start( ap, szFmt );

			std::string sTemp = CArcBase::formatString( szFmt, ap );

			va_end( ap );

			//
			// Check the Q size. Dump the oldest if
			// the Q is too big.
			//
			if ( m_sQueue->size() >= Q_MAX )
			{
				m_sQueue->pop();
			}

			if ( !sTemp.empty() )
			{
				m_sQueue->push( sTemp );
			}
		}


		// +----------------------------------------------------------------------------
		// |  getNext
		// +----------------------------------------------------------------------------
		// |  Returns the oldest std::string ( as char * ) from the front of the queue. Also
		// |  pops the message from the queue. Applications should call empty() to check
		// |  if more messages are available.
		// |
		// |  <OUT> -> The oldest message in the queue
		// +----------------------------------------------------------------------------
		const std::string CArcLog::getNext()
		{
			std::string sTemp( "" );

			if ( !m_sQueue->empty() )
			{
				sTemp = std::string( m_sQueue->front() );

				m_sQueue->pop();
			}

			return sTemp;
		}


		// +----------------------------------------------------------------------------
		// |  getLast
		// +----------------------------------------------------------------------------
		// |  Returns the newest std::string ( as char * ) from the back of the queue. Also
		// |  pops the message from the queue.
		// |
		// |  <OUT> -> The newest message in the queue
		// +----------------------------------------------------------------------------
		const std::string CArcLog::getLast( void )
		{
			std::string sTemp( "" );

			if ( !m_sQueue->empty() )
			{
				sTemp = std::string( m_sQueue->back() );

				while ( !m_sQueue->empty() )
				{
					m_sQueue->pop();
				}
			}

			return sTemp;
		}


		// +----------------------------------------------------------------------------
		// |  getLogCount
		// +----------------------------------------------------------------------------
		// |  <OUT> -> Returns the number of elements in the Q.
		// +----------------------------------------------------------------------------
		std::uint32_t CArcLog::getLogCount( void )
		{
			return static_cast<std::uint32_t>( m_sQueue->size() );
		}


		// +----------------------------------------------------------------------------
		// |  empty
		// +----------------------------------------------------------------------------
		// |  Checks if the Q is empty. i.e. if there are any messages in the Q.
		// |
		// |  <OUT> -> Returns 'true' if the queue is empty; 'false' otherwise.
		// +----------------------------------------------------------------------------
		bool CArcLog::empty( void )
		{
			return m_sQueue->empty();
		}


		// +----------------------------------------------------------------------------
		// |  selfTest
		// +----------------------------------------------------------------------------
		// |  Performs a self-test. Output goes to a message box on windows and to the
		// |  terminal on linux systems.
		// +----------------------------------------------------------------------------
		void CArcLog::selfTest( void )
		{
			std::ostringstream oss;

			oss << "Putting 3 controller commands to Q ... ";
			put( CArcBase::cmdToString( 0x444F4E, { 0x2, 0x54444C, 0x112233 } ).c_str() );
			put( CArcBase::cmdToString( 0x455252, { 0x2, 0x111111, 0x1, 0x2, 0x3, 0x4 } ).c_str() );
			put( CArcBase::cmdToString( 0x444F4E, { 0x2, 0x535450 } ).c_str() );
			oss << "done" << std::endl;

			oss << "Reading back Q: " << std::endl;
			while ( !empty() )
			{
				oss << "\t" << getNext() << std::endl;
			}
			oss << "Done reading Q!" << std::endl;

#ifdef _WINDOWS
			::MessageBoxA( NULL, oss.str().c_str(), "CArcLog::SelfTest()", MB_OK );
#else
			std::cout << std::endl
				<< "+--------------------------------------------------------------+" << std::endl
				<< "|  CArcLog::selfTest()                                         |" << std::endl
				<< "+--------------------------------------------------------------+" << std::endl
				<< oss.str()
				<< std::endl;
#endif
		}

	}	// end gen3 namespace
}	// end arc namespace