#ifndef _ARC_COOEXPIFACE_H_
#define _ARC_COOEXPIFACE_H_

#include <cstdint>

#include <CArcDeviceDllMain.h>


namespace arc
{
	namespace gen3
	{

		class GEN3_CARCDEVICE_API CooExpIFace   // COO Expose Interface Class
		{
			public:

				virtual ~CooExpIFace( void ) = default;

				virtual void exposeCallback( int devnum, std::uint32_t uiElapsedTime, std::uint32_t ExposureTime ) = 0;

				virtual void readCallback( int expbuf, int devnum, std::uint32_t uiPixelCount, std::uint32_t uiImageSize ) = 0;

                                virtual void frameCallback( int expbuf,                         // exposure buffer number
                                                            int devnum,                         // device number
                                                            std::uint32_t   uiFramesPerBuffer,  // Frames-per-buffer count
                                                            std::uint32_t   uiFrameCount,       // PCI frame count
                                                            std::uint32_t   uiRows,             // # of rows in frame
                                                            std::uint32_t   uiCols,             // # of cols in frame
                                                            void* pBuffer ) = 0;                // Pointer to frame start in buffer

                                virtual void ftCallback( int expbuf, int devnum ) = 0;

			protected:

				CooExpIFace( void ) = default;
		};

	}	// end gen3 namespace
}	// end arc namespace


#endif	// _ARC_COOEXPIFACE_H_
