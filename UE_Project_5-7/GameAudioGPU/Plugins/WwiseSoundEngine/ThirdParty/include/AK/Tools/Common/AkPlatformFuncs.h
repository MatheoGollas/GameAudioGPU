/*******************************************************************************
The content of this file includes portions of the AUDIOKINETIC Wwise Technology
released in source code form as part of the SDK installer package.

Commercial License Usage

Licensees holding valid commercial licenses to the AUDIOKINETIC Wwise Technology
may use this file in accordance with the end user license agreement provided 
with the software or, alternatively, in accordance with the terms contained in a
written agreement between you and Audiokinetic Inc.

Apache License Usage

Alternatively, this file may be used under the Apache License, Version 2.0 (the 
"Apache License"); you may not use this file except in compliance with the 
Apache License. You may obtain a copy of the Apache License at 
http://www.apache.org/licenses/LICENSE-2.0.

Unless required by applicable law or agreed to in writing, software distributed
under the Apache License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES
OR CONDITIONS OF ANY KIND, either express or implied. See the Apache License for
the specific language governing permissions and limitations under the License.

  Copyright (c) 2026 Audiokinetic Inc.
*******************************************************************************/

// AkPlatformFuncs.h

/// \file 
/// Platform-dependent functions definition.

#ifndef _AK_TOOLS_COMMON_AKPLATFORMFUNCS_H
#define _AK_TOOLS_COMMON_AKPLATFORMFUNCS_H

#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/SoundEngine/Common/AkAtomicTypes.h>

namespace AK
{
    class IAkPluginMemAlloc;
}

// Uncomment the following to enable built-in platform profiler markers in the sound engine
//#define AK_ENABLE_INSTRUMENT

#if defined(AK_NULL_PLATFORM)
// null platform has no funcs
struct AkThreadProperties {};
#elif defined(AK_WIN)
#include <AK/Tools/Win32/AkPlatformFuncs.h>

#elif defined (AK_XBOX)
#include <AK/Tools/XboxGC/AkPlatformFuncs.h>

#elif defined (AK_APPLE)
#include <AK/Tools/Mac/AkPlatformFuncs.h>
#include <AK/Tools/POSIX/AkPlatformFuncs.h>

#elif defined( AK_ANDROID ) || defined ( AK_LINUX_AOSP ) 
#include <AK/Tools/Android/AkPlatformFuncs.h>

#elif defined ( AK_HARMONY ) 
#include <AK/Tools/OpenHarmony/AkPlatformFuncs.h>

#elif defined (AK_PS4)
#include <AK/Tools/PS4/AkPlatformFuncs.h>

#elif defined (AK_PS5)
#include <AK/Tools/PS5/AkPlatformFuncs.h>

#elif defined (AK_EMSCRIPTEN)
#include <AK/Tools/Emscripten/AkPlatformFuncs.h>

#elif defined (AK_LINUX)
#include <AK/Tools/Linux/AkPlatformFuncs.h>
#include <AK/Tools/POSIX/AkPlatformFuncs.h>

#elif defined (AK_QNX)
#include <AK/Tools/QNX/AkPlatformFuncs.h>
#include <AK/Tools/POSIX/AkPlatformFuncs.h>

#elif defined (AK_NX)
#include <AK/Tools/NX/AkPlatformFuncs.h>

#elif defined (AK_OUNCE)
#include <AK/Tools/Ounce/AkPlatformFuncs.h>

#else
#error AkPlatformFuncs.h: Undefined platform
#endif

#ifndef AkPrefetchZero
#define AkPrefetchZero(___Dest, ___Size)
#endif

#ifndef AkPrefetchZeroAligned
#define AkPrefetchZeroAligned(___Dest, ___Size)
#endif

#ifndef AkZeroMemAligned
#define AkZeroMemAligned(___Dest, ___Size) AKPLATFORM::AkMemSet(___Dest, 0, ___Size);
#endif
#ifndef AkZeroMemLarge
#define AkZeroMemLarge(___Dest, ___Size) AKPLATFORM::AkMemSet(___Dest, 0, ___Size);
#endif
#ifndef AkZeroMemSmall
#define AkZeroMemSmall(___Dest, ___Size) AKPLATFORM::AkMemSet(___Dest, 0, ___Size);
#endif

#ifndef AK_THREAD_INIT_CODE
#define AK_THREAD_INIT_CODE(_threadProperties)
#endif

#ifndef AK_PLATFORM_MEMCPY
namespace AKPLATFORM
{
	/// Platform Independent Helper for memcpy/memmove/memset
	AkForceInline void AkMemCpy(void* pDest, const void* pSrc, AkUInt32 uSize)
	{
		memcpy(pDest, pSrc, uSize);
	}

	AkForceInline void AkMemMove(void* pDest, const void* pSrc, AkUInt32 uSize)
	{
		memmove(pDest, pSrc, uSize);
	}

	AkForceInline void AkMemSet(void* pDest, AkInt32 iVal, AkUInt32 uSize)
	{
		memset(pDest, iVal, uSize);
	}
}
#endif // AK_PLATFORM_MEMCPY

#if !defined(AK_NULL_PLATFORM)
/// Platform-dependent helpers
namespace AKPLATFORM
{
	// Threads
	// ------------------------------------------------------------------

	// Returns true if in_pThread is in a valid state and executing
	bool AkIsValidThread(AkThread* in_pThread);

	// Destroys the internal thread object
	//  (set the memalloc interface to nullptr to use the soundengine's built-in memalloc systems)
	void AkCloseThread(AkThread* in_pThread, AK::IAkPluginMemAlloc* in_pMemAlloc);
	// Resets the AkThread to an invalid/null-thread state
	void AkClearThread(AkThread* in_pThread);

	// Initializes a thread 
	AKRESULT AkCreateThread(
		AkThreadRoutine pStartRoutine,                 // Thread routine.
		void* pParams,                                 // Routine params.
		const AkThreadProperties& in_threadProperties, // Properties. NULL for default.
		AkThread* out_pThread,                         // Returned thread handle.
		const char* in_szThreadName,                   // thread name.
		AK::IAkPluginMemAlloc* in_pMemAlloc            // Interface to memory allocator for thread creation (set to nullptr to use the soundengine's built-in memalloc systems)
		);
	
	// Forces the current thread to yield for at least the specified # of milliseconds.
	// Actual time that the thread is yielded for may be significantly higher that value specified on some platforms and operating environments
	void AkSleep(AkUInt32 in_ulMilliseconds);

	// Blocks execution until the provided thread has returned and completed execution
	void AkWaitForSingleThread(AkThread* in_pThread);
	
	// Returns a platform-specific threadId for the current thread
	AkThreadID CurrentThread();

	// Sets the values in out_threadProperties to the normal soundengine defaults. Defaults may vary across platforms
	void AkGetDefaultThreadProperties(AkThreadProperties& out_threadProperties);
	// Sets the values in out_threadProperties to the normal soundengine defaults, except for priority which will be "High"
	void AkGetDefaultHighPriorityThreadProperties(AkThreadProperties& out_threadProperties);
}
#endif

namespace AKPLATFORM
{
	// to be used to unequivocally cause a crash for scenarios that are critical failures that we cannot hope to recover from
	AkForceInline void AkForceCrash()
	{
		// We don't want to simply abort() the program; we want a real SIGSEGV happening on *this* thread
		// This convoluted way of crashing is necessary to avoid compilers optimizing out invalid code
		// By calling PerformanceCounter, which is inherently non-deterministic, the compiler must generate code as-is
		AkInt64 one, two;
		PerformanceCounter(&one);
		PerformanceCounter(&two);
		AkUIntPtr ptr = (AkUIntPtr)two - (AkUIntPtr)one;
		((void(*)())ptr)();
	}
}

#ifndef AK_FORCE_CRASH
#define AK_FORCE_CRASH AKPLATFORM::AkForceCrash()
#endif

// on platforms that support it, this will invoke a breakpoint when a debugger is attached.
// Otherwise, forcefully crashes the process
#ifndef AK_DEBUG_BREAK
#define AK_DEBUG_BREAK AK_FORCE_CRASH
#endif

#endif // _AK_TOOLS_COMMON_AKPLATFORMFUNCS_H
