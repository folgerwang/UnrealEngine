

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

#ifndef __microsoft2Eperception2Esimulation_h__
#define __microsoft2Eperception2Esimulation_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

/* Forward Declarations */ 

#ifndef __IPerceptionSimulationFrame_FWD_DEFINED__
#define __IPerceptionSimulationFrame_FWD_DEFINED__
typedef interface IPerceptionSimulationFrame IPerceptionSimulationFrame;

#endif 	/* __IPerceptionSimulationFrame_FWD_DEFINED__ */


#ifndef __IPerceptionSimulationFrameGeneratedCallback_FWD_DEFINED__
#define __IPerceptionSimulationFrameGeneratedCallback_FWD_DEFINED__
typedef interface IPerceptionSimulationFrameGeneratedCallback IPerceptionSimulationFrameGeneratedCallback;

#endif 	/* __IPerceptionSimulationFrameGeneratedCallback_FWD_DEFINED__ */


#ifndef __IPerceptionSimulationControl_FWD_DEFINED__
#define __IPerceptionSimulationControl_FWD_DEFINED__
typedef interface IPerceptionSimulationControl IPerceptionSimulationControl;

#endif 	/* __IPerceptionSimulationControl_FWD_DEFINED__ */


/* header files for imported files */
#include "oaidl.h"
#include "d3d11.h"
#include "SimulationStream.h"
#include "Inspectable.h"

#ifdef __cplusplus
extern "C"{
#endif 


/* interface __MIDL_itf_microsoft2Eperception2Esimulation_0000_0000 */
/* [local] */ 

typedef /* [v1_enum] */ 
enum PerceptionSimulationControlFlags
    {
        PerceptionSimulationControlFlags_None	= 0,
        PerceptionSimulationControlFlags_WaitForCalibration	= 1
    } 	PerceptionSimulationControlFlags;

typedef struct FocusPoint
    {
    float Position[ 3 ];
    float Normal[ 3 ];
    float Velocity[ 3 ];
    boolean IsValid;
    } 	FocusPoint;



extern RPC_IF_HANDLE __MIDL_itf_microsoft2Eperception2Esimulation_0000_0000_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_microsoft2Eperception2Esimulation_0000_0000_v0_0_s_ifspec;

#ifndef __IPerceptionSimulationFrame_INTERFACE_DEFINED__
#define __IPerceptionSimulationFrame_INTERFACE_DEFINED__

/* interface IPerceptionSimulationFrame */
/* [uuid][object] */ 


EXTERN_C const IID IID_IPerceptionSimulationFrame;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("09893EA8-E55F-40DE-AEE9-8BAD66C5890C")
    IPerceptionSimulationFrame : public IUnknown
    {
    public:
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_PredictionTargetTime( 
            /* [retval][out] */ __RPC__out INT64 *value) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_Frame( 
            /* [retval][out] */ __RPC__deref_out_opt ID3D11Texture2D **frame) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_FocusPointInLeftViewSpace( 
            /* [out] */ __RPC__out FocusPoint *value) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IPerceptionSimulationFrameVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            __RPC__in IPerceptionSimulationFrame * This,
            /* [in] */ __RPC__in REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            __RPC__in IPerceptionSimulationFrame * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            __RPC__in IPerceptionSimulationFrame * This);
        
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_PredictionTargetTime )( 
            __RPC__in IPerceptionSimulationFrame * This,
            /* [retval][out] */ __RPC__out INT64 *value);
        
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_Frame )( 
            __RPC__in IPerceptionSimulationFrame * This,
            /* [retval][out] */ __RPC__deref_out_opt ID3D11Texture2D **frame);
        
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_FocusPointInLeftViewSpace )( 
            __RPC__in IPerceptionSimulationFrame * This,
            /* [out] */ __RPC__out FocusPoint *value);
        
        END_INTERFACE
    } IPerceptionSimulationFrameVtbl;

    interface IPerceptionSimulationFrame
    {
        CONST_VTBL struct IPerceptionSimulationFrameVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IPerceptionSimulationFrame_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IPerceptionSimulationFrame_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IPerceptionSimulationFrame_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IPerceptionSimulationFrame_get_PredictionTargetTime(This,value)	\
    ( (This)->lpVtbl -> get_PredictionTargetTime(This,value) ) 

#define IPerceptionSimulationFrame_get_Frame(This,frame)	\
    ( (This)->lpVtbl -> get_Frame(This,frame) ) 

#define IPerceptionSimulationFrame_get_FocusPointInLeftViewSpace(This,value)	\
    ( (This)->lpVtbl -> get_FocusPointInLeftViewSpace(This,value) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IPerceptionSimulationFrame_INTERFACE_DEFINED__ */


#ifndef __IPerceptionSimulationFrameGeneratedCallback_INTERFACE_DEFINED__
#define __IPerceptionSimulationFrameGeneratedCallback_INTERFACE_DEFINED__

/* interface IPerceptionSimulationFrameGeneratedCallback */
/* [uuid][object] */ 


EXTERN_C const IID IID_IPerceptionSimulationFrameGeneratedCallback;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("71D800E0-BFA5-4D75-9A78-1CD7D4A7E852")
    IPerceptionSimulationFrameGeneratedCallback : public IUnknown
    {
    public:
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE FrameGenerated( 
            /* [in] */ __RPC__in_opt IPerceptionSimulationFrame *frame) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IPerceptionSimulationFrameGeneratedCallbackVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            __RPC__in IPerceptionSimulationFrameGeneratedCallback * This,
            /* [in] */ __RPC__in REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            __RPC__in IPerceptionSimulationFrameGeneratedCallback * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            __RPC__in IPerceptionSimulationFrameGeneratedCallback * This);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *FrameGenerated )( 
            __RPC__in IPerceptionSimulationFrameGeneratedCallback * This,
            /* [in] */ __RPC__in_opt IPerceptionSimulationFrame *frame);
        
        END_INTERFACE
    } IPerceptionSimulationFrameGeneratedCallbackVtbl;

    interface IPerceptionSimulationFrameGeneratedCallback
    {
        CONST_VTBL struct IPerceptionSimulationFrameGeneratedCallbackVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IPerceptionSimulationFrameGeneratedCallback_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IPerceptionSimulationFrameGeneratedCallback_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IPerceptionSimulationFrameGeneratedCallback_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IPerceptionSimulationFrameGeneratedCallback_FrameGenerated(This,frame)	\
    ( (This)->lpVtbl -> FrameGenerated(This,frame) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IPerceptionSimulationFrameGeneratedCallback_INTERFACE_DEFINED__ */


#ifndef __IPerceptionSimulationControl_INTERFACE_DEFINED__
#define __IPerceptionSimulationControl_INTERFACE_DEFINED__

/* interface IPerceptionSimulationControl */
/* [uuid][object] */ 


EXTERN_C const IID IID_IPerceptionSimulationControl;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("AB2FAC5E-DC24-4C0D-A763-43EA141F0960")
    IPerceptionSimulationControl : public IUnknown
    {
    public:
        virtual /* [propget][helpstring] */ HRESULT STDMETHODCALLTYPE get_HolographicSpace( 
            /* [retval][out] */ __RPC__deref_out_opt IUnknown **value) = 0;
        
        virtual /* [propget][helpstring] */ HRESULT STDMETHODCALLTYPE get_ControlStream( 
            /* [retval][out] */ __RPC__deref_out_opt ISimulationStreamSink **sink) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE SetFrameGeneratedCallback( 
            /* [in] */ __RPC__in_opt IPerceptionSimulationFrameGeneratedCallback *callback) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IPerceptionSimulationControlVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            __RPC__in IPerceptionSimulationControl * This,
            /* [in] */ __RPC__in REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            __RPC__in IPerceptionSimulationControl * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            __RPC__in IPerceptionSimulationControl * This);
        
        /* [propget][helpstring] */ HRESULT ( STDMETHODCALLTYPE *get_HolographicSpace )( 
            __RPC__in IPerceptionSimulationControl * This,
            /* [retval][out] */ __RPC__deref_out_opt IUnknown **value);
        
        /* [propget][helpstring] */ HRESULT ( STDMETHODCALLTYPE *get_ControlStream )( 
            __RPC__in IPerceptionSimulationControl * This,
            /* [retval][out] */ __RPC__deref_out_opt ISimulationStreamSink **sink);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *SetFrameGeneratedCallback )( 
            __RPC__in IPerceptionSimulationControl * This,
            /* [in] */ __RPC__in_opt IPerceptionSimulationFrameGeneratedCallback *callback);
        
        END_INTERFACE
    } IPerceptionSimulationControlVtbl;

    interface IPerceptionSimulationControl
    {
        CONST_VTBL struct IPerceptionSimulationControlVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IPerceptionSimulationControl_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IPerceptionSimulationControl_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IPerceptionSimulationControl_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IPerceptionSimulationControl_get_HolographicSpace(This,value)	\
    ( (This)->lpVtbl -> get_HolographicSpace(This,value) ) 

#define IPerceptionSimulationControl_get_ControlStream(This,sink)	\
    ( (This)->lpVtbl -> get_ControlStream(This,sink) ) 

#define IPerceptionSimulationControl_SetFrameGeneratedCallback(This,callback)	\
    ( (This)->lpVtbl -> SetFrameGeneratedCallback(This,callback) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IPerceptionSimulationControl_INTERFACE_DEFINED__ */


/* interface __MIDL_itf_microsoft2Eperception2Esimulation_0000_0003 */
/* [local] */ 

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus
HRESULT __stdcall InitializePerceptionSimulation(
    _In_ PerceptionSimulationControlFlags flags,
    _In_ REFIID riid,
    _Outptr_ void** ppv);
HANDLE __stdcall CreateSpatialSurfacesInterestEvent();
HANDLE __stdcall CreateSpatialAnchorsInUseChangedEvent();
HRESULT __stdcall GetSpatialAnchorsInUse(_COM_Outptr_ IInspectable **ppSpatialAnchors);
#ifdef __cplusplus
}
#endif // __cplusplus


extern RPC_IF_HANDLE __MIDL_itf_microsoft2Eperception2Esimulation_0000_0003_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_microsoft2Eperception2Esimulation_0000_0003_v0_0_s_ifspec;

/* Additional Prototypes for ALL interfaces */

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif


