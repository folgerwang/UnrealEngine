

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 8.01.0618 */
/* @@MIDL_FILE_HEADING(  ) */



/* verify that the <rpcndr.h> version is high enough to compile this file*/
#ifndef __REQUIRED_RPCNDR_H_VERSION__
#define __REQUIRED_RPCNDR_H_VERSION__ 500
#endif

/* verify that the <rpcsal.h> version is high enough to compile this file*/
#ifndef __REQUIRED_RPCSAL_H_VERSION__
#define __REQUIRED_RPCSAL_H_VERSION__ 100
#endif

#include "rpc.h"
#include "rpcndr.h"

#ifndef __RPCNDR_H_VERSION__
#error this stub requires an updated version of <rpcndr.h>
#endif /* __RPCNDR_H_VERSION__ */

#ifndef COM_NO_WINDOWS_H
#include "windows.h"
#include "ole2.h"
#endif /*COM_NO_WINDOWS_H*/

#ifndef __simulationstream_h__
#define __simulationstream_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

/* Forward Declarations */ 

#ifndef __ISimulationStreamSink_FWD_DEFINED__
#define __ISimulationStreamSink_FWD_DEFINED__
typedef interface ISimulationStreamSink ISimulationStreamSink;

#endif 	/* __ISimulationStreamSink_FWD_DEFINED__ */


#ifndef __ISimulationStreamSinkFactory_FWD_DEFINED__
#define __ISimulationStreamSinkFactory_FWD_DEFINED__
typedef interface ISimulationStreamSinkFactory ISimulationStreamSinkFactory;

#endif 	/* __ISimulationStreamSinkFactory_FWD_DEFINED__ */


/* header files for imported files */
#include "oaidl.h"

#ifdef __cplusplus
extern "C"{
#endif 


/* interface __MIDL_itf_simulationstream_0000_0000 */
/* [local] */ 

#define S_DATA_SHADOWED MAKE_HRESULT(SEVERITY_SUCCESS, FACILITY_ITF, 0xFFFF)
#define E_DATA_DROPPED MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0xFFFE)
typedef unsigned __int64 SIMULATION_CALLBACK_TOKEN;

typedef /* [v1_enum] */ 
enum STREAM_DATA_TYPE
    {
        STREAM_DATA_TYPE_NONE	= 0,
        STREAM_DATA_TYPE_HEAD	= 0x1,
        STREAM_DATA_TYPE_HANDS	= 0x2,
        STREAM_DATA_TYPE_SPATIAL_MAPPING	= 0x8,
        STREAM_DATA_TYPE_CALIBRATION	= 0x10,
        STREAM_DATA_TYPE_ENVIRONMENT	= 0x20,
        STREAM_DATA_TYPE_ALL	= ( ( ( ( STREAM_DATA_TYPE_HEAD | STREAM_DATA_TYPE_HANDS )  | STREAM_DATA_TYPE_SPATIAL_MAPPING )  | STREAM_DATA_TYPE_CALIBRATION )  | STREAM_DATA_TYPE_ENVIRONMENT ) 
    } 	STREAM_DATA_TYPE;

typedef /* [helpstring] */ struct SIMULATION_PACKET_HEADER
    {
    /* [helpstring] */ STREAM_DATA_TYPE Type;
    /* [helpstring] */ unsigned __int32 Version;
    } 	SIMULATION_PACKET_HEADER;



extern RPC_IF_HANDLE __MIDL_itf_simulationstream_0000_0000_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_simulationstream_0000_0000_v0_0_s_ifspec;

#ifndef __ISimulationStreamSink_INTERFACE_DEFINED__
#define __ISimulationStreamSink_INTERFACE_DEFINED__

/* interface ISimulationStreamSink */
/* [helpstring][uuid][object] */ 


EXTERN_C const IID IID_ISimulationStreamSink;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("8A2D5134-6C59-4E08-A0E0-34E5222F86D7")
    ISimulationStreamSink : public IUnknown
    {
    public:
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE OnPacketReceived( 
            /* [in] */ unsigned int length,
            /* [size_is][unique][in] */ __RPC__in_ecount_full_opt(length) byte *packet) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct ISimulationStreamSinkVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            __RPC__in ISimulationStreamSink * This,
            /* [in] */ __RPC__in REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            __RPC__in ISimulationStreamSink * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            __RPC__in ISimulationStreamSink * This);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *OnPacketReceived )( 
            __RPC__in ISimulationStreamSink * This,
            /* [in] */ unsigned int length,
            /* [size_is][unique][in] */ __RPC__in_ecount_full_opt(length) byte *packet);
        
        END_INTERFACE
    } ISimulationStreamSinkVtbl;

    interface ISimulationStreamSink
    {
        CONST_VTBL struct ISimulationStreamSinkVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define ISimulationStreamSink_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define ISimulationStreamSink_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define ISimulationStreamSink_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define ISimulationStreamSink_OnPacketReceived(This,length,packet)	\
    ( (This)->lpVtbl -> OnPacketReceived(This,length,packet) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __ISimulationStreamSink_INTERFACE_DEFINED__ */


#ifndef __ISimulationStreamSinkFactory_INTERFACE_DEFINED__
#define __ISimulationStreamSinkFactory_INTERFACE_DEFINED__

/* interface ISimulationStreamSinkFactory */
/* [helpstring][uuid][object] */ 


EXTERN_C const IID IID_ISimulationStreamSinkFactory;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("D7767D93-57E9-47DB-B098-BB45F3F42843")
    ISimulationStreamSinkFactory : public IUnknown
    {
    public:
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE CreateSimulationStreamSink( 
            /* [retval][out] */ __RPC__deref_out_opt ISimulationStreamSink **ppSink) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct ISimulationStreamSinkFactoryVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            __RPC__in ISimulationStreamSinkFactory * This,
            /* [in] */ __RPC__in REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            __RPC__in ISimulationStreamSinkFactory * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            __RPC__in ISimulationStreamSinkFactory * This);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *CreateSimulationStreamSink )( 
            __RPC__in ISimulationStreamSinkFactory * This,
            /* [retval][out] */ __RPC__deref_out_opt ISimulationStreamSink **ppSink);
        
        END_INTERFACE
    } ISimulationStreamSinkFactoryVtbl;

    interface ISimulationStreamSinkFactory
    {
        CONST_VTBL struct ISimulationStreamSinkFactoryVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define ISimulationStreamSinkFactory_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define ISimulationStreamSinkFactory_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define ISimulationStreamSinkFactory_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define ISimulationStreamSinkFactory_CreateSimulationStreamSink(This,ppSink)	\
    ( (This)->lpVtbl -> CreateSimulationStreamSink(This,ppSink) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __ISimulationStreamSinkFactory_INTERFACE_DEFINED__ */


/* Additional Prototypes for ALL interfaces */

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif


