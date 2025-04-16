#ifndef _CCONIFACE_H_
#define _CCONIFACE_H_

#include <CArcDeviceDllMain.h>


namespace arc
{
	namespace gen3
	{

		class GEN3_CARCDEVICE_API CConIFace   // continuous Readout Interface Class
		{
			public:

				virtual ~CConIFace( void ) = default;

				virtual void frameCallback( std::uint32_t   uiFramesPerBuffer,	// Frames-per-buffer count
											std::uint32_t   uiFrameCount,		// PCI frame count
											std::uint32_t   uiRows,				// # of rows in frame
											std::uint32_t   uiCols,				// # of cols in frame
											void* pBuffer ) = 0;				// Pointer to frame start in buffer

			protected:

				CConIFace( void ) = default;

		};

	}	// end gen3 namespace
}	// end arc namespace


#endif	// _CCONIFACE_H_
