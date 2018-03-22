/************************************************************************************

Filename    :   VrIntegration_Config.h
Content     :   VrIntegration preprocessor settings
Created     :   Nov 28, 2017
Authors     :   Jian Zhang
Language    :   C99

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#ifndef OVR_VrIntegration_Config_h
#define OVR_VrIntegration_Config_h

/*

OVR_VRINTEGRATION_EXPORT

*/

#if defined( _MSC_VER ) || defined( __ICL )

#if defined( OVR_VRINTEGRATION_ENABLE_EXPORT )
    #define OVR_VRINTEGRATION_EXPORT  __declspec( dllexport )
#else
    #define OVR_VRINTEGRATION_EXPORT
#endif

#else

#if defined( OVR_VRINTEGRATION_ENABLE_EXPORT )
    #define OVR_VRINTEGRATION_EXPORT __attribute__( ( __visibility__( "default" ) ) )
#else
    #define OVR_VRINTEGRATION_EXPORT
#endif

#endif

#if defined( __x86_64__ ) || defined( __aarch64__ ) || defined( _WIN64 )
	#define OVR_VRINTEGRATION_64_BIT
#else
	#define OVR_VRINTEGRATION_32_BIT
#endif

/*

OVR_VRINTEGRATION_STATIC_ASSERT( exp )						// static assert
OVR_VRINTEGRATION_PADDING( bytes )							// insert bytes of padding
OVR_VRINTEGRATION_PADDING_32_BIT( bytes )					// insert bytes of padding only when using a 32-bit compiler
OVR_VRINTEGRATION_PADDING_64_BIT( bytes )					// insert bytes of padding only when using a 64-bit compiler
OVR_VRINTEGRATION_ASSERT_TYPE_SIZE( type, bytes )			// assert the size of a type
OVR_VRINTEGRATION_ASSERT_TYPE_SIZE_32_BIT( type, bytes )	// assert the size of a type only when using a 32-bit compiler
OVR_VRINTEGRATION_ASSERT_TYPE_SIZE_64_BIT( type, bytes )	// assert the size of a type only when using a 64-bit compiler

*/

#if defined( __cplusplus ) && __cplusplus >= 201103L
	#define OVR_VRINTEGRATION_STATIC_ASSERT( exp )					static_assert( exp, #exp )
#endif

#if !defined( OVR_VRINTEGRATION_STATIC_ASSERT ) && defined( __clang__ )
	#if __has_feature( cxx_static_assert ) || __has_extension( cxx_static_assert )
		#define OVR_VRINTEGRATION_STATIC_ASSERT( exp )				static_assert( exp )
	#endif
#endif

#if !defined( OVR_VRINTEGRATION_STATIC_ASSERT )
	#if defined( __COUNTER__ )
		#define OVR_VRINTEGRATION_STATIC_ASSERT( exp )				OVR_VRAPI_STATIC_ASSERT_ID( exp, __COUNTER__ )
	#else
		#define OVR_VRINTEGRATION_STATIC_ASSERT( exp )				OVR_VRAPI_STATIC_ASSERT_ID( exp, __LINE__ )
	#endif
	#define OVR_VRINTEGRATION_STATIC_ASSERT_ID( exp, id )			OVR_VRAPI_STATIC_ASSERT_ID_EXPANDED( exp, id )
	#define OVR_VRINTEGRATION_STATIC_ASSERT_ID_EXPANDED( exp, id )	typedef char assert_failed_##id[(exp) ? 1 : -1]
#endif

#if defined( __COUNTER__ )
	#define OVR_VRINTEGRATION_PADDING( bytes )						OVR_VRINTEGRATION_PADDING_ID( bytes, __COUNTER__ )
#else
	#define OVR_VRINTEGRATION_PADDING( bytes )						OVR_VRINTEGRATION_PADDING_ID( bytes, __LINE__ )
#endif
#define OVR_VRINTEGRATION_PADDING_ID( bytes, id )					OVR_VRINTEGRATION_PADDING_ID_EXPANDED( bytes, id )
#define OVR_VRINTEGRATION_PADDING_ID_EXPANDED( bytes, id )			unsigned char dead##id[(bytes)]

#define OVR_VRINTEGRATION_ASSERT_TYPE_SIZE( type, bytes	)			OVR_VRINTEGRATION_STATIC_ASSERT( sizeof( type ) == (bytes) )

#if defined( OVR_VRINTEGRATION_64_BIT )
	#define OVR_VRINTEGRATION_PADDING_32_BIT( bytes )
	#if defined( __COUNTER__ )
		#define OVR_VRINTEGRATION_PADDING_64_BIT( bytes )				OVR_VRINTEGRATION_PADDING_ID( bytes, __COUNTER__ )
	#else
		#define OVR_VRINTEGRATION_PADDING_64_BIT( bytes )				OVR_VRINTEGRATION_PADDING_ID( bytes, __LINE__ )
	#endif
	#define OVR_VRINTEGRATION_ASSERT_TYPE_SIZE_32_BIT( type, bytes	)
	#define OVR_VRINTEGRATION_ASSERT_TYPE_SIZE_64_BIT( type, bytes	)	OVR_VRINTEGRATION_STATIC_ASSERT( sizeof( type ) == (bytes) )
#else
	#define OVR_VRINTEGRATION_ASSERT_TYPE_SIZE( type, bytes )			OVR_VRINTEGRATION_STATIC_ASSERT( sizeof( type ) == (bytes) )
	#if defined( __COUNTER__ )
		#define OVR_VRINTEGRATION_PADDING_32_BIT( bytes )				OVR_VRINTEGRATION_PADDING_ID( bytes, __COUNTER__ )
	#else
		#define OVR_VRINTEGRATION_PADDING_32_BIT( bytes )				OVR_VRINTEGRATION_PADDING_ID( bytes, __LINE__ )
	#endif
	#define OVR_VRINTEGRATION_PADDING_64_BIT( bytes )
	#define OVR_VRINTEGRATION_ASSERT_TYPE_SIZE_32_BIT( type, bytes	)	OVR_VRINTEGRATION_STATIC_ASSERT( sizeof( type ) == (bytes) )
	#define OVR_VRINTEGRATION_ASSERT_TYPE_SIZE_64_BIT( type, bytes	)
#endif

#endif	// !OVR_VrIntegration_Config_h
