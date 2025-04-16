// +---------------------------------------------------------------------------------------------------------+
// |  FILE:  CArcStringList.h                                                                                |
// +---------------------------------------------------------------------------------------------------------+
// |  PURPOSE: This file defines a string list class.                                                        |
// |                                                                                                         |
// |  AUTHOR:  Scott Streit			DATE: March 25, 2020                                                     |
// |                                                                                                         |
// |  Copyright 2013 Astronomical Research Cameras, Inc. All rights reserved.                                |
// +---------------------------------------------------------------------------------------------------------+
#ifndef _ARC_CARCSTRINGLIST_H_
#define _ARC_CARCSTRINGLIST_H_

#ifdef _WINDOWS
	#pragma warning( disable: 4251 )
#endif

#include <initializer_list>
#include <functional>
#include <cstdint>
#include <memory>
#include <vector>
#include <string>

#include <CArcBaseDllMain.h>



namespace arc
{
	namespace gen3
	{
		// +-----------------------------------------------------------------------------------------------------+
		// |  CArcStringList Class                                                                               |
		// +-----------------------------------------------------------------------------------------------------+

		/** @class CArcStringList
		*  This class provides a string list.
		*/
		class GEN3_CARCBASE_API CArcStringList
		{

			public:

				typedef std::vector<std::string>::iterator			iterator;
				typedef std::vector<std::string>::const_iterator	const_iterator;


				/** Constructor
				 */
				CArcStringList( void );

				/** List constructor
				 *  @param rList - A list of strings to initialize the list with. The list must be 
				 *                 contained within curly brackets: {}. Ex: command( { "txr", "abc" } ).
				 */
				CArcStringList( const std::initializer_list<std::string>& rList );

				/** Append a string to the end of the list.
				 *   @param sElem - The string to add to the list.
				 */
				void add( std::string const& sElem );

				/** Removes all elements from the list. The list will be empty when complete.
				 */
				void clear( void );

				/** Returns whether or not the list is empty.
				 *   @return <code>true</code> if the list is empty; <code>false</code> otherwise.
				 */
				bool empty( void );

				/** Returns a string representation of the list contents.
				 *  @return A string representation of the list contents.
				 */
				const std::string toString( void );

				/** Returns the string located at the specified index. Each element is separated
				 *  by a newline character ( '\n' ).
				 *  @param uIndex - The index. Range 0 to CArcStringList::maxLength().
				 *  @return The string located at the specified index.
				 */
				const std::string& at( std::uint32_t uIndex );

				/** Searches for the specified string within the list.
				 *  @param searchString - The string to search for.
				 *  @return <i>true</i> if the string is found; <i>false</i> otherwise.
				 */
				bool find( const std::string& searchString );

				/** Returns the number of elements in the list. An empty list returns a value of 0.
				 *  @return The number of elements in the list.
				 */
				std::uint32_t length( void );

				/** Append operator. Appends a string to the end of the list.
				 *   @param sElem - The string to add to the list.
				 *   @return A reference to the new list.
				 */
				CArcStringList& operator<<( std::string const& sElem );

				/** Append operator. Appends a CArcStringList to the end of this one.
				 *   @param anotherList - The CArcStringList to append to this list.
				 *   @return A reference to the new list.
				 */
				CArcStringList& operator+=( CArcStringList& anotherList );

				/** Returns an iterator to the first element of the container. If the container is empty,
				 *  the returned iterator will be equal to end().
				 */
				iterator begin( void );

				/** Returns an iterator to the element following the last element of the container. This 
				 *  element acts as a placeholder; attempting to access it results in undefined behavior.
				 */
				iterator end( void );

				/** Sort the elements into ascending order.
				 */
				void sortAscending( void );

				/** Sort the elements into descending order.
				 */
				void sortDescending( void );

			private:

				/** The list */
				std::unique_ptr<std::vector<std::string>> m_pvList;
		};


	}	// end gen3 namespace
}		// end arc namespace



namespace std
{
	/**
	 *  Creates a modified version of the std::default_delete class for use by
	 *  all std::unique_ptr's returned from a CArcStringList object.
	 */
	template<>
	class GEN3_CARCBASE_API default_delete< arc::gen3::CArcStringList >
	{
		public:

			/** Deletes the specified string list object
			 *  @param pObj - The string list object to be deleted/destroyed.
			 */
			void operator()( arc::gen3::CArcStringList* pObj );
	};
}


#endif		// _ARC_CARCSTRINGLIST_H_
