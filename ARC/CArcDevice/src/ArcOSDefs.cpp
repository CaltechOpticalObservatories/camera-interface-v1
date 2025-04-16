/*
 *  ArcOSDefs.cpp
 *  CArcDevice
 *
 *  Created by Scott Streit on 2/8/11.
 *  Copyright 2011 __MyCompanyName__. All rights reserved.
 *
 */

#include "ArcOSDefs.h"

#ifdef _WINDOWS
	#include "CArcPCIe.h"
#endif


namespace arc
{

	// +==================================================================+
	// |  WINDOWS DEFINITIONS
	// +==================================================================+
	#ifdef _WINDOWS

		void* arc::Arc_MMap( Arc_DevHandle hDev, int dMapCmd, int dSize )
		{
			ULONG64 u64VirtAddr = 0;

			Arc_IOCtl( hDev, dMapCmd, &u64VirtAddr, sizeof( u64VirtAddr ) );

			return PVOID( u64VirtAddr );
		}


		void arc::Arc_MUnMap( Arc_DevHandle hDev, int dMapCmd, void* pAddr, int dSize )
		{
			Arc_IOCtl( hDev, dMapCmd, PULONG64( pAddr ), sizeof( ULONG64 ) );
		}


	// +==================================================================+
	// |  MAC DEFINITIONS
	// +==================================================================+
	#elif defined( __APPLE__ )

		int arc::Arc_OpenHandle( Arc_DevHandle& hDev, void* pService )
		{
			//
			// This call will cause the user client to be instantiated. It
			// returns an io_connect_t handle that is used for all subsequent
			// calls to the user client.
			//
			kern_return_t kernResult = IOServiceOpen( *( ( io_service_t* )pService ),
				mach_task_self(),
				0,
				&hDev );

			if ( kernResult == KERN_SUCCESS )
			{
				kernResult = IOConnectCallScalarMethod( hDev,
					kArcOpenUserClient,
					NULL,
					0,
					NULL,
					NULL );
			}

			return ( kernResult == KERN_SUCCESS ? 1 : 0 );
		}

		int arc::Arc_CloseHandle( Arc_DevHandle hDev )
		{
			kern_return_t kernResult = IOConnectCallScalarMethod( hDev,
				kArcCloseUserClient,
				NULL,
				0,
				NULL,
				NULL );

			kernResult = IOServiceClose( hDev );

			return ( kernResult == KERN_SUCCESS ? 1 : 0 );
		}


		void* arc::Arc_MMap( Arc_DevHandle hDev, int dMapCmd, int dSize )
		{
	#if __LP64__
			mach_vm_address_t   addr;
			mach_vm_size_t      size;
	#else
			vm_address_t        addr;
			vm_size_t           size;
	#endif

			kern_return_t kernResult = IOConnectMapMemory( hDev,
				kIODefaultMemoryType,
				mach_task_self(),
				&addr,
				&size,
				( kIOMapAnywhere ) ); //| kIOMapDefaultCache ) );

			if ( kernResult != kIOReturnSuccess )
			{
				addr = NULL;
			}

			return ( void* )addr;
		}


		void arc::Arc_MUnMap( Arc_DevHandle hDev, int dMapCmd, void* pAddr, int dSize )
		{
	#if __LP64__
			mach_vm_address_t   addr = ( mach_vm_address_t )pAddr;
	#else
			vm_address_t        addr = ( vm_address_t )pAddr;
	#endif

			IOConnectUnmapMemory( hDev,
				kIODefaultMemoryType,
				mach_task_self(),
				addr );
		}


		int arc::Arc_IOCtl( Arc_DevHandle hDev, int dCmd, ushort* pArg, int dArgSize )
		{
			kern_return_t kernResult = kBadArgsErr;
			uint32_t      u32ArgCount = ( dArgSize / sizeof( ushort ) );

			uint64_t      u64Data[ MAX_IOCTL_IN_COUNT ];
			uint32_t      u32DataCount = MAX_IOCTL_OUT_COUNT;

			//
			// Zero in/out data buffer
			//
			Arc_ZeroMemory( u64Data, MAX_IOCTL_IN_COUNT * sizeof( uint64_t ) );

			//
			// Initialize data with command
			//
			u64Data[ 0 ] = MKCMD( dCmd );

			if ( pArg != NULL && u32ArgCount <= ( MAX_IOCTL_IN_COUNT - 1 ) )
			{
				//
				// Initialize argument(s)
				//
				if ( u32ArgCount == 1 )
				{
					u64Data[ 1 ] = *pArg;
				}
				else
				{
					for ( int i = 0; i < u32ArgCount; i++ )
					{
						u64Data[ i + 1 ] = pArg[ i ];
					}
				}

				kernResult = IOConnectCallScalarMethod( hDev,
					kArcIOCtlUserClient,
					u64Data,
					MAX_IOCTL_IN_COUNT,
					u64Data,
					&u32DataCount );

				if ( kernResult == KERN_SUCCESS )
				{
					//
					// Set the return value(s)
					//
					if ( u32ArgCount == 1 )
					{
						*pArg = ( ushort )u64Data[ 0 ];
					}
					else
					{
						for ( int i = 0; i < u32ArgCount; i++ )
						{
							pArg[ i ] = ( ushort )u64Data[ i ];
						}
					}
				}
				else
				{
					*pArg = 0;
				}

			}

			return ( kernResult == KERN_SUCCESS ? 1 : 0 );
		}

		int arc::Arc_IOCtl( Arc_DevHandle hDev, int dCmd, ulong* pArg, int dArgSize )
		{
			kern_return_t kernResult = kBadArgsErr;
			uint32_t      u32ArgCount = ( dArgSize / sizeof( ulong ) );

			uint64_t      u64Data[ MAX_IOCTL_IN_COUNT ];
			uint32_t      u32DataCount = MAX_IOCTL_OUT_COUNT;

			//
			// Zero in/out data buffer
			//
			Arc_ZeroMemory( u64Data, MAX_IOCTL_IN_COUNT * sizeof( uint64_t ) );

			//
			// Initialize data with command
			//
			u64Data[ 0 ] = MKCMD( dCmd );

			if ( pArg != NULL && u32ArgCount <= ( MAX_IOCTL_IN_COUNT - 1 ) )
			{
				//
				// Initialize argument(s)
				//
				if ( u32ArgCount == 1 )
				{
					u64Data[ 1 ] = *pArg;
				}
				else
				{
					for ( int i = 0; i < u32ArgCount; i++ )
					{
						u64Data[ i + 1 ] = pArg[ i ];
					}
				}

				kernResult = IOConnectCallScalarMethod( hDev,
					kArcIOCtlUserClient,
					u64Data,
					MAX_IOCTL_IN_COUNT,
					u64Data,
					&u32DataCount );

				if ( kernResult == KERN_SUCCESS )
				{
					//
					// Set return value(s)
					//
					if ( u32ArgCount == 1 )
					{
						*pArg = ( ulong )u64Data[ 0 ];
					}
					else
					{
						for ( int i = 0; i < u32ArgCount; i++ )
						{
							pArg[ i ] = ( ulong )u64Data[ i ];
						}
					}
				}
				else
				{
					*pArg = 0;
				}
			}

			return ( kernResult == KERN_SUCCESS ? 1 : 0 );
		}

		int arc::Arc_IOCtl( Arc_DevHandle hDev, int dCmd, ubyte* pArg, int dArgSize )
		{
			kern_return_t kernResult = kBadArgsErr;
			uint32_t      u32ArgCount = ( dArgSize / sizeof( ubyte ) );

			uint64_t      u64Data[ MAX_IOCTL_IN_COUNT ];
			uint32_t      u32DataCount = MAX_IOCTL_OUT_COUNT;

			//
			// Zero in/out data buffer
			//
			Arc_ZeroMemory( u64Data, MAX_IOCTL_IN_COUNT * sizeof( uint64_t ) );

			//
			// Initialize data with command
			//
			u64Data[ 0 ] = MKCMD( dCmd );

			if ( pArg != NULL && u32ArgCount <= ( MAX_IOCTL_IN_COUNT - 1 ) )
			{
				//
				// Initialize argument(s)
				if ( u32ArgCount == 1 )
				{
					u64Data[ 1 ] = *pArg;
				}
				else
				{
					for ( int i = 0; i < u32ArgCount; i++ )
					{
						u64Data[ i + 1 ] = pArg[ i ];
					}
				}

				kernResult = IOConnectCallScalarMethod( hDev,
					kArcIOCtlUserClient,
					u64Data,
					MAX_IOCTL_IN_COUNT,
					u64Data,
					&u32DataCount );

				if ( kernResult == KERN_SUCCESS )
				{
					//
					// Set return value(s)
					//
					if ( u32ArgCount == 1 )
					{
						*pArg = ( ubyte )u64Data[ 0 ];
					}
					else
					{
						for ( int i = 0; i < u32ArgCount; i++ )
						{
							pArg[ i ] = ( ubyte )u64Data[ i ];
						}
					}
				}
				else
				{
					*pArg = 0;
				}
			}

			return ( kernResult == KERN_SUCCESS ? 1 : 0 );
		}

		int arc::Arc_IOCtl( Arc_DevHandle hDev, int dCmd, uint* pArg, int dArgSize )
		{
			kern_return_t kernResult = kBadArgsErr;
			uint32_t      u32ArgCount = ( dArgSize / sizeof( uint ) );

			uint64_t      u64Data[ MAX_IOCTL_IN_COUNT ];
			uint32_t      u32DataCount = MAX_IOCTL_OUT_COUNT;

			//
			// Zero in/out data buffer
			//
			Arc_ZeroMemory( u64Data, MAX_IOCTL_IN_COUNT * sizeof( uint64_t ) );

			//
			// Initialize data with command
			//
			u64Data[ 0 ] = MKCMD( dCmd );

			if ( pArg != NULL && u32ArgCount <= ( MAX_IOCTL_IN_COUNT - 1 ) )
			{
				//
				// Initialize argument(s)
				//
				if ( u32ArgCount == 1 )
				{
					u64Data[ 1 ] = *pArg;
				}
				else
				{
					for ( int i = 0; i < u32ArgCount; i++ )
					{
						u64Data[ i + 1 ] = pArg[ i ];
					}
				}

				kernResult = IOConnectCallScalarMethod( hDev,
					kArcIOCtlUserClient,
					u64Data,
					MAX_IOCTL_IN_COUNT,
					u64Data,
					&u32DataCount );

				if ( kernResult == KERN_SUCCESS )
				{
					//
					// Set return value(s)
					//
					if ( u32ArgCount == 1 )
					{
						*pArg = ( uint )u64Data[ 0 ];
					}
					else
					{
						for ( int i = 0; i < u32ArgCount; i++ )
						{
							pArg[ i ] = ( uint )u64Data[ i ];
						}
					}
				}
				else
				{
					*pArg = 0;
				}
			}

			return ( kernResult == KERN_SUCCESS ? 1 : 0 );
		}

		int arc::Arc_IOCtl( Arc_DevHandle hDev, int dCmd, int* pArg, int dArgSize )
		{
			kern_return_t kernResult = kBadArgsErr;
			uint32_t      u32ArgCount = ( dArgSize / sizeof( int ) );

			uint64_t      u64Data[ MAX_IOCTL_IN_COUNT ];
			uint32_t      u32DataCount = MAX_IOCTL_OUT_COUNT;

			//
			// Zero in/out data buffer
			//
			Arc_ZeroMemory( u64Data, MAX_IOCTL_IN_COUNT * sizeof( uint64_t ) );

			//
			// Initialize data with command
			//
			u64Data[ 0 ] = MKCMD( dCmd );

			if ( pArg != NULL && u32ArgCount <= ( MAX_IOCTL_IN_COUNT - 1 ) )
			{
				//
				// Initialize argument(s)
				//
				if ( u32ArgCount == 1 )
				{
					u64Data[ 1 ] = *pArg;
				}
				else
				{
					for ( int i = 0; i < u32ArgCount; i++ )
					{
						u64Data[ i + 1 ] = pArg[ i ];
					}
				}

				kernResult = IOConnectCallScalarMethod( hDev,
					kArcIOCtlUserClient,
					u64Data,
					MAX_IOCTL_IN_COUNT,
					u64Data,
					&u32DataCount );

				if ( kernResult == KERN_SUCCESS )
				{
					//
					// Set the return value(s)
					//
					if ( u32ArgCount == 1 )
					{
						*pArg = ( int )u64Data[ 0 ];
					}
					else
					{
						for ( int i = 0; i < u32ArgCount; i++ )
						{
							pArg[ i ] = ( int )u64Data[ i ];
						}
					}
				}
				else
				{
					*pArg = 0;
				}
			}

			return ( kernResult == KERN_SUCCESS ? 1 : 0 );
		}

	#endif

}	// end arc namespace
