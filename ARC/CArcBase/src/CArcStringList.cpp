// +------------------------------------------------------------------------------------------------------------------+
// |  FILE:  CArcStringList.cpp                                                                                       |
// +------------------------------------------------------------------------------------------------------------------+
// |  PURPOSE: This file implements a string list class.                                                              |
// |                                                                                                                  |
// |  AUTHOR:  Scott Streit			DATE: July 16, 2013                                                               |
// |                                                                                                                  |
// |  Copyright 2013 Astronomical Research Cameras, Inc. All rights reserved.                                         |
// +------------------------------------------------------------------------------------------------------------------+

#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <regex>

#include <CArcStringList.h>


namespace arc
{
	namespace gen3
	{

		// +----------------------------------------------------------------------------------------------------------+
		// | Constructor                                                                                              |
		// +----------------------------------------------------------------------------------------------------------+
		// | Initializes the class instance.                                                                          |
		// +----------------------------------------------------------------------------------------------------------+
		CArcStringList::CArcStringList()
		{
			m_pvList.reset( new std::vector<std::string>() );
		}


		// +----------------------------------------------------------------------------------------------------------+
		// | List constructor                                                                                         |
		// +----------------------------------------------------------------------------------------------------------+
		// | <IN> tList - A list of strings to initialize the list with. The list must be contained within curly      |
		// | brackets: {}.                                                                                            |
		// |                                                                                                          |
		// | Ex: command( { "txr", "abc" } ).                                                                         |
		// +----------------------------------------------------------------------------------------------------------+
		CArcStringList::CArcStringList( const std::initializer_list<std::string>& tList )
		{
			m_pvList.reset( new std::vector<std::string>() );

			for ( auto s : tList )
			{
				m_pvList->push_back( s );
			}
		}


		// +----------------------------------------------------------------------------------------------------------+
		// | add                                                                                                      |
		// +----------------------------------------------------------------------------------------------------------+
		// | Adds the specified string to the back of the list.                                                       |
		// |                                                                                                          |
		// | <IN> sElem - The string to add to the end of the list.                                                   |
		// +----------------------------------------------------------------------------------------------------------+
		void CArcStringList::add( std::string const &sElem )
		{
			m_pvList->push_back( sElem );
		}


		// +----------------------------------------------------------------------------------------------------------+
		// | clear                                                                                                    |
		// +----------------------------------------------------------------------------------------------------------+
		// | Removes all strings from the list. The list will be empty when done.                                     |
		// +----------------------------------------------------------------------------------------------------------+
		void CArcStringList::clear()
		{
			m_pvList->clear();
		}


		// +----------------------------------------------------------------------------------------------------------+
		// | empty                                                                                                    |
		// +----------------------------------------------------------------------------------------------------------+
		// | Returns 'true' if the list is empty; 'false' otherwise.                                                  |
		// +----------------------------------------------------------------------------------------------------------+
		bool CArcStringList::empty()
		{
			return m_pvList->empty();
		}


		// +----------------------------------------------------------------------------------------------------------+
		// | toString                                                                                                 |
		// +----------------------------------------------------------------------------------------------------------+
		// | Returns the entire list as a single string. Each list element is seperated by a newline character        |
		// | ( '\n' ).                                                                                                |
		// +----------------------------------------------------------------------------------------------------------+
		const std::string CArcStringList::toString()
		{
			std::string sTemp;

			std::for_each( m_pvList->begin(),
						   m_pvList->end(),
						   [ &sTemp ]( const std::string& s ) { sTemp += s + std::string( "\n" ); } );

			return sTemp;
		}


		// +----------------------------------------------------------------------------------------------------------+
		// | at                                                                                                       |
		// +----------------------------------------------------------------------------------------------------------+
		// | Returns the string at the specified list index.                                                          |
		// |                                                                                                          |
		// | <IN> uiIndex - The index of the element to be returned.                                                   |
		// |                                                                                                          |
		// | Throws std::runtime_error if index is out of range.                                                      |
		// +----------------------------------------------------------------------------------------------------------+
		const std::string& CArcStringList::at( std::uint32_t uiIndex )
		{
			if ( uiIndex >= static_cast< std::uint32_t >( m_pvList->size() ) )
			{
				std::ostringstream oss;

				oss << "( CArcStringList::at() ): The index [ "
					<< uiIndex << " ] is out of range [ 0 - "
					<< m_pvList->size() << " ]!" << std::ends;

				throw std::runtime_error( oss.str() );
			}

			return m_pvList->at( uiIndex );
		}


		// +----------------------------------------------------------------------------------------------------------+
		// | find                                                                                                     |
		// +----------------------------------------------------------------------------------------------------------+
		// | Searches for the specified string within the list. Returns 'true' if the string is found; 'false'        |
		// | otherwise.                                                                                               |
		// |                                                                                                          |
		// | <IN> sSearchString - The string to search for.                                                            |
		// |                                                                                                          |
		// | Throws std::runtime_error if index is out of range.                                                      |
		// +----------------------------------------------------------------------------------------------------------+
		bool CArcStringList::find( const std::string& sSearchString )
		{
			bool bSuccess = false;

			for ( auto it = m_pvList->begin(); it != m_pvList->end(); it++ )
			{
				if ( ( *it ).find( sSearchString ) != std::string::npos )
				{
					bSuccess = true;

					break;
				}
			}

			return bSuccess;
		}


		// +----------------------------------------------------------------------------------------------------------+
		// | length                                                                                                   |
		// +----------------------------------------------------------------------------------------------------------+
		// | Returns the number of elements in the list. An empty list return 0.                                      |
		// +----------------------------------------------------------------------------------------------------------+
		std::uint32_t CArcStringList::length()
		{
			return static_cast<std::uint32_t>( m_pvList->size() );
		}


		// +----------------------------------------------------------------------------------------------------------+
		// | operator <<                                                                                              |
		// +----------------------------------------------------------------------------------------------------------+
		// | Operator to add an element to the back of the list.                                                      |
		// |                                                                                                          |
		// | <IN> sElem - The element to be added to the back of the list.                                            |
		// +----------------------------------------------------------------------------------------------------------+
		CArcStringList& CArcStringList::operator<<( std::string const& sElem )
		{
			m_pvList->push_back( sElem );

			return *this;
		}


		// +----------------------------------------------------------------------------------------------------------+
		// | operator +=                                                                                              |
		// +----------------------------------------------------------------------------------------------------------+
		// | Operator to add two lists together.                                                                      |
		// |                                                                                                          |
		// | <IN> cAnotherList - The list to add to the end of this list.                                             |
		// +----------------------------------------------------------------------------------------------------------+
		CArcStringList& CArcStringList::operator+=( CArcStringList& cAnotherList )
		{
			for ( std::uint32_t i = 0; i < cAnotherList.length(); i++ )
			{
				m_pvList->push_back( cAnotherList.at( i ) );
			}

			return *this;
		}


		// +----------------------------------------------------------------------------------------------------------+
		// | begin                                                                                                    |
		// +----------------------------------------------------------------------------------------------------------+
		// | Returns an iterator to the first element of the container.  If the container is empty, the returned      |
		// | iterator will be equal to end().                                                                         |
		// +----------------------------------------------------------------------------------------------------------+
		CArcStringList::iterator CArcStringList::begin( void )
		{
			return m_pvList->begin();
		}


		// +----------------------------------------------------------------------------------------------------------+
		// | end                                                                                                      |
		// +----------------------------------------------------------------------------------------------------------+
		// | Returns an iterator to the element following the last element of the container. This element acts as a   |
		// | placeholder; attempting to access it results in undefined behavior.                                      |
		// +----------------------------------------------------------------------------------------------------------+
		CArcStringList::iterator CArcStringList::end( void )
		{
			return m_pvList->end();
		}


		// +----------------------------------------------------------------------------------------------------------+
		// | sortAscending                                                                                            |
		// +----------------------------------------------------------------------------------------------------------+
		// | Sorts the list elements into ascending order.                                                            |
		// +----------------------------------------------------------------------------------------------------------+
		void CArcStringList::sortAscending( void )
		{
			std::sort( m_pvList->begin(), m_pvList->end(), []( const std::string& sLHS, const std::string& sRHS )
			{
				const std::regex regEx( ".*\\| ([[:upper:]]\\w+[[:punct:]]*).*" );

				std::smatch LHSMatch;
				std::smatch RHSMatch;

				auto bLHSOkay = std::regex_search( sLHS.begin(), sLHS.end(), LHSMatch, regEx );
				auto bRHSOkay = std::regex_search( sRHS.begin(), sRHS.end(), RHSMatch, regEx );

				if ( bLHSOkay and bRHSOkay )
				{
					if ( LHSMatch[ 1 ] < RHSMatch[ 1 ] )
					{
						return true;
					}
				}

				return false;
			} );
		}


		// +----------------------------------------------------------------------------------------------------------+
		// | sortDescending                                                                                           |
		// +----------------------------------------------------------------------------------------------------------+
		// | Sorts the list elements into descending order.                                                           |
		// +----------------------------------------------------------------------------------------------------------+
		void CArcStringList::sortDescending( void )
		{
			std::sort( m_pvList->begin(), m_pvList->end(), []( const std::string& sLHS, const std::string& sRHS )
			{
				const std::regex regEx( ".*\\| ([[:upper:]]\\w+[[:punct:]]*).*" );

				std::smatch LHSMatch;
				std::smatch RHSMatch;

				auto bLHSOkay = std::regex_search( sLHS.begin(), sLHS.end(), LHSMatch, regEx );
				auto bRHSOkay = std::regex_search( sRHS.begin(), sRHS.end(), RHSMatch, regEx );

				if ( bLHSOkay and bRHSOkay )
				{
					if ( LHSMatch[ 1 ] > RHSMatch[ 1 ] )
					{
						return true;
					}
				}

				return false;
			} );
		}


	}	// end gen3 namespace
}		// end arc namespace



// +------------------------------------------------------------------------------------------------+
// |  default_delete Definition                                                                     |
// +------------------------------------------------------------------------------------------------+
// |  Creates a modified version of the std::default_delete class for use by all                    |
// |  std::unique_ptr's that wrap a CArcStringList object.                                          |
// +------------------------------------------------------------------------------------------------+
namespace std
{

	void default_delete<arc::gen3::CArcStringList>::operator()( arc::gen3::CArcStringList* pObj )
	{
		if ( pObj != nullptr )
		{
			delete pObj;
		}
	}

}
