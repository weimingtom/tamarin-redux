/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is [Open Source Virtual Machine.].
 *
 * The Initial Developer of the Original Code is
 * Adobe System Incorporated.
 * Portions created by the Initial Developer are Copyright (C) 2004-2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adobe AS3 Team
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef __avmbuild__
#define __avmbuild__

#ifdef AVMPLUS_DISABLE_NJ
#  error "AVMPLUS_DISABLE_NJ is no longer supported"
#endif

// all x64, and all MacTel machines, always have sse2
#if defined(AVMPLUS_AMD64) || (defined(AVMPLUS_MAC) && defined(AVMPLUS_IA32))
	#define AVMPLUS_SSE2_ALWAYS
#endif

#ifdef FEATURE_NANOJIT
// enable the jitmax global variables and -jitmax switch, for bisecting nanojit bugs
//#define AVMPLUS_JITMAX
#endif

#ifdef _DEBUG
#  ifndef DEBUG
#    define DEBUG 1 // don't use _DEBUG
#  endif
#endif

#if defined(VTUNE) || defined(DEBUG) || defined(DEBUGGER)
#  define AVMPLUS_VERBOSE
#endif

#ifndef VMCFG_METHOD_NAMES
	#if defined AVMPLUS_VERBOSE || defined DEBUGGER
		#define VMCFG_METHOD_NAMES 1
	#else
		#define VMCFG_METHOD_NAMES 0
	#endif
#endif

// #undef verify, a Mac thing
#undef verify

#ifndef AVMPLUS_UNALIGNED_ACCESS
    #if defined(AVMPLUS_IA32) || defined(AVMPLUS_AMD64)
        #define AVMPLUS_UNALIGNED_ACCESS
    #else
        // leave undefined, assume unaligned access is a bad thing
    #endif
#endif

// Enable interfacing Java ; so you can access java methods/properties like native AS; e.g.
// var hello = JObject.create("java.lang.String", " hello world ");  print(hello.indexOf('o')); 
//#define AVMPLUS_WITH_JNI

#define PCRE_STATIC

// performance metrics for NJ 
//#define PERFM

#ifdef PERFM
# define PERFM_NVPROF(n,v) _nvprof(n,v)
# define PERFM_NTPROF(n) _ntprof(n)
# define PERFM_TPROF_END() _tprof_end()
#else
# define PERFM_NVPROF(n,v)
# define PERFM_NTPROF(n)
# define PERFM_TPROF_END() 
#endif

#ifdef AVMPLUS_BIG_ENDIAN
	// define in case any old code relies on this
	#define AVM10_BIG_ENDIAN
#endif

// FASTCALL 
#ifdef AVMPLUS_IA32
	#if _MSC_VER
		#define FASTCALL __fastcall
	#elif __GNUC__
		#define FASTCALL __attribute__((fastcall))
	#else
		#define FASTCALL
	#endif
#else
	#define FASTCALL
#endif

// temporary impedance-matching define for code that needs to build with different versions of tamarin...
// will be removed soon
#define AVMPLUS_REDUX_API 1

// The use of this switch is described in comments at the head of utils/superwordprof.c
//
// The limit is optional and describes a cutoff for sampling; the program continues to
// run after sampling ends but data are no longer gathered or stored.  A limit of 250e6
// produces 1GB of sample data.  There is one sample per VM instruction executed.
//#define SUPERWORD_PROFILING
//#define SUPERWORD_LIMIT 250000000

#ifdef SUPERWORD_PROFILING
#  ifndef VMCFG_WORDCODE
#    error "You must have word code enabled to perform superword profiling"
#  endif
#  ifdef AVMPLUS_DIRECT_THREADED
#    error "You must disable direct threading to perform superword profiling"
#  endif
#endif

// this can be useful in tracking down memory usage for Traits and Traits-related caches,
// but is very invasive and should only be used in special engineering builds. It should be
// be left in place (but disabled) for now, as it's still in use...
// it will go away at some point in the not-too-distant future, however.
//#define AVMPLUS_TRAITS_MEMTRACK

// define this to 1 to keep a shadow copy of implGPR in MethodEnv (vs MethodInfo only).
// more speed, but more memory used... not clear if the tradeoff is worthwhile yet.
#ifndef VMCFG_METHODENV_IMPL32
#  define VMCFG_METHODENV_IMPL32 1
#endif

// If you need to build on a system that forbids static initialization of global constant function pointers,
// define this -- it will change the way native-method-table initialization is done to be compatible (at the
// expense of slightly more code size). 
//#define AVMPLUS_NO_STATIC_POINTERS

#if defined(AVMPLUS_PORTING_API)
	// The portapi_avmbuild.h file is used to override
	// definitions in this file. E.g. turning off
	// features, etc.
	#include "portapi_avmbuild.h"
#endif

#endif /* __avmbuild__ */
