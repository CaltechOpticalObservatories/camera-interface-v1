#ifndef _ARC_CEXPIFACE_H_
#define _ARC_CEXPIFACE_H_

#include <cstdint>

#include <CArcDeviceDllMain.h>


namespace arc
{
	namespace gen3
	{

		class GEN3_CARCDEVICE_API CExpIFace   // expose Interface Class
		{
			public:

				virtual ~CExpIFace( void ) = default;

				virtual void exposeCallback( float fElapsedTime ) = 0;

				virtual void readCallback( std::uint32_t uiPixelCount ) = 0;

			protected:

				CExpIFace( void ) = default;
		};

	}	// end gen3 namespace
}	// end arc namespace


#endif	// _CEXPIFACE_H_
