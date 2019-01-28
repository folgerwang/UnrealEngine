

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

#ifndef __perceptionsimulationmanager_h__
#define __perceptionsimulationmanager_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

/* Forward Declarations */ 

#ifndef __IPerceptionSimulationManager_FWD_DEFINED__
#define __IPerceptionSimulationManager_FWD_DEFINED__
typedef interface IPerceptionSimulationManager IPerceptionSimulationManager;

#endif 	/* __IPerceptionSimulationManager_FWD_DEFINED__ */


#ifndef __ISimulatedNode_FWD_DEFINED__
#define __ISimulatedNode_FWD_DEFINED__
typedef interface ISimulatedNode ISimulatedNode;

#endif 	/* __ISimulatedNode_FWD_DEFINED__ */


#ifndef __ISimulatedDevice_FWD_DEFINED__
#define __ISimulatedDevice_FWD_DEFINED__
typedef interface ISimulatedDevice ISimulatedDevice;

#endif 	/* __ISimulatedDevice_FWD_DEFINED__ */


#ifndef __ISimulatedHeadTracker_FWD_DEFINED__
#define __ISimulatedHeadTracker_FWD_DEFINED__
typedef interface ISimulatedHeadTracker ISimulatedHeadTracker;

#endif 	/* __ISimulatedHeadTracker_FWD_DEFINED__ */


#ifndef __ISimulatedHandTracker_FWD_DEFINED__
#define __ISimulatedHandTracker_FWD_DEFINED__
typedef interface ISimulatedHandTracker ISimulatedHandTracker;

#endif 	/* __ISimulatedHandTracker_FWD_DEFINED__ */


#ifndef __ISimulatedHuman_FWD_DEFINED__
#define __ISimulatedHuman_FWD_DEFINED__
typedef interface ISimulatedHuman ISimulatedHuman;

#endif 	/* __ISimulatedHuman_FWD_DEFINED__ */


#ifndef __ISimulatedHand_FWD_DEFINED__
#define __ISimulatedHand_FWD_DEFINED__
typedef interface ISimulatedHand ISimulatedHand;

#endif 	/* __ISimulatedHand_FWD_DEFINED__ */


#ifndef __ISimulatedHead_FWD_DEFINED__
#define __ISimulatedHead_FWD_DEFINED__
typedef interface ISimulatedHead ISimulatedHead;

#endif 	/* __ISimulatedHead_FWD_DEFINED__ */


#ifndef __ISimulationRecording_FWD_DEFINED__
#define __ISimulationRecording_FWD_DEFINED__
typedef interface ISimulationRecording ISimulationRecording;

#endif 	/* __ISimulationRecording_FWD_DEFINED__ */


#ifndef __ISimulationRecordingCallback_FWD_DEFINED__
#define __ISimulationRecordingCallback_FWD_DEFINED__
typedef interface ISimulationRecordingCallback ISimulationRecordingCallback;

#endif 	/* __ISimulationRecordingCallback_FWD_DEFINED__ */


/* header files for imported files */
#include "oaidl.h"
#include "SimulationStream.h"

#ifdef __cplusplus
extern "C"{
#endif 


/* interface __MIDL_itf_perceptionsimulationmanager_0000_0000 */
/* [local] */ 







typedef struct VECTOR3
    {
    float X;
    float Y;
    float Z;
    } 	VECTOR3;

typedef struct ROTATION3
    {
    /* [helpstring] */ float Pitch;
    /* [helpstring] */ float Yaw;
    /* [helpstring] */ float Roll;
    } 	ROTATION3;

typedef struct FRUSTUM
    {
    /* [helpstring] */ float Near;
    /* [helpstring] */ float Far;
    /* [helpstring] */ float FOV;
    /* [helpstring] */ float AspectRatio;
    } 	FRUSTUM;

typedef /* [v1_enum] */ 
enum SIMULATED_DEVICE_TYPE
    {
        SIMULATED_DEVICE_TYPE_REFERENCE	= 0
    } 	SIMULATED_DEVICE_TYPE;

typedef /* [v1_enum] */ 
enum HEAD_TRACKER_MODE
    {
        HEAD_TRACKER_MODE_DEFAULT	= 0,
        HEAD_TRACKER_MODE_ORIENTATION	= 1,
        HEAD_TRACKER_MODE_POSITION	= 2
    } 	HEAD_TRACKER_MODE;

typedef /* [v1_enum] */ 
enum SIMULATED_GESTURE
    {
        SIMULATED_GESTURE_NONE	= 0,
        SIMULATED_GESTURE_FINGERPRESSED	= 1,
        SIMULATED_GESTURE_FINGERRELEASED	= 2,
        SIMULATED_GESTURE_HOME	= 4,
        SIMULATED_GESTURE_MAX	= SIMULATED_GESTURE_HOME
    } 	SIMULATED_GESTURE;

typedef /* [v1_enum] */ 
enum PLAYBACK_STATE
    {
        PLAYBACK_STATE_STOPPED	= 0,
        PLAYBACK_STATE_PLAYING	= 1,
        PLAYBACK_STATE_PAUSED	= 2,
        PLAYBACK_STATE_END	= 3,
        PLAYBACK_STATE_ERROR	= 4
    } 	PLAYBACK_STATE;



extern RPC_IF_HANDLE __MIDL_itf_perceptionsimulationmanager_0000_0000_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_perceptionsimulationmanager_0000_0000_v0_0_s_ifspec;

#ifndef __IPerceptionSimulationManager_INTERFACE_DEFINED__
#define __IPerceptionSimulationManager_INTERFACE_DEFINED__

/* interface IPerceptionSimulationManager */
/* [helpstring][local][uuid][object] */ 


EXTERN_C const IID IID_IPerceptionSimulationManager;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("A648E128-7D10-439C-9551-403222F45AA0")
    IPerceptionSimulationManager : public IUnknown
    {
    public:
        virtual /* [propget][helpstring] */ HRESULT STDMETHODCALLTYPE get_Device( 
            /* [retval][out] */ ISimulatedDevice **ppDevice) = 0;
        
        virtual /* [propget][helpstring] */ HRESULT STDMETHODCALLTYPE get_Human( 
            /* [retval][out] */ ISimulatedHuman **ppHuman) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE Reset( void) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IPerceptionSimulationManagerVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IPerceptionSimulationManager * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IPerceptionSimulationManager * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IPerceptionSimulationManager * This);
        
        /* [propget][helpstring] */ HRESULT ( STDMETHODCALLTYPE *get_Device )( 
            IPerceptionSimulationManager * This,
            /* [retval][out] */ ISimulatedDevice **ppDevice);
        
        /* [propget][helpstring] */ HRESULT ( STDMETHODCALLTYPE *get_Human )( 
            IPerceptionSimulationManager * This,
            /* [retval][out] */ ISimulatedHuman **ppHuman);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *Reset )( 
            IPerceptionSimulationManager * This);
        
        END_INTERFACE
    } IPerceptionSimulationManagerVtbl;

    interface IPerceptionSimulationManager
    {
        CONST_VTBL struct IPerceptionSimulationManagerVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IPerceptionSimulationManager_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IPerceptionSimulationManager_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IPerceptionSimulationManager_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IPerceptionSimulationManager_get_Device(This,ppDevice)	\
    ( (This)->lpVtbl -> get_Device(This,ppDevice) ) 

#define IPerceptionSimulationManager_get_Human(This,ppHuman)	\
    ( (This)->lpVtbl -> get_Human(This,ppHuman) ) 

#define IPerceptionSimulationManager_Reset(This)	\
    ( (This)->lpVtbl -> Reset(This) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IPerceptionSimulationManager_INTERFACE_DEFINED__ */


#ifndef __ISimulatedNode_INTERFACE_DEFINED__
#define __ISimulatedNode_INTERFACE_DEFINED__

/* interface ISimulatedNode */
/* [helpstring][local][uuid][object] */ 


EXTERN_C const IID IID_ISimulatedNode;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("A34DBD84-2B7B-457C-BE89-EC97DA8FDCC1")
    ISimulatedNode : public IUnknown
    {
    public:
        virtual /* [propget][helpstring] */ HRESULT STDMETHODCALLTYPE get_WorldPosition( 
            /* [retval][out] */ VECTOR3 *pos) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct ISimulatedNodeVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            ISimulatedNode * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            ISimulatedNode * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            ISimulatedNode * This);
        
        /* [propget][helpstring] */ HRESULT ( STDMETHODCALLTYPE *get_WorldPosition )( 
            ISimulatedNode * This,
            /* [retval][out] */ VECTOR3 *pos);
        
        END_INTERFACE
    } ISimulatedNodeVtbl;

    interface ISimulatedNode
    {
        CONST_VTBL struct ISimulatedNodeVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define ISimulatedNode_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define ISimulatedNode_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define ISimulatedNode_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define ISimulatedNode_get_WorldPosition(This,pos)	\
    ( (This)->lpVtbl -> get_WorldPosition(This,pos) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __ISimulatedNode_INTERFACE_DEFINED__ */


#ifndef __ISimulatedDevice_INTERFACE_DEFINED__
#define __ISimulatedDevice_INTERFACE_DEFINED__

/* interface ISimulatedDevice */
/* [helpstring][local][uuid][object] */ 


EXTERN_C const IID IID_ISimulatedDevice;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("EB61574E-0857-48F6-B3A6-ED01E675B79E")
    ISimulatedDevice : public IUnknown
    {
    public:
        virtual /* [propget][helpstring] */ HRESULT STDMETHODCALLTYPE get_HeadTracker( 
            /* [retval][out] */ ISimulatedHeadTracker **ppHeadTracker) = 0;
        
        virtual /* [propget][helpstring] */ HRESULT STDMETHODCALLTYPE get_HandTracker( 
            /* [retval][out] */ ISimulatedHandTracker **ppHandTracker) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE SetSimulatedDeviceType( 
            /* [in] */ SIMULATED_DEVICE_TYPE type) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct ISimulatedDeviceVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            ISimulatedDevice * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            ISimulatedDevice * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            ISimulatedDevice * This);
        
        /* [propget][helpstring] */ HRESULT ( STDMETHODCALLTYPE *get_HeadTracker )( 
            ISimulatedDevice * This,
            /* [retval][out] */ ISimulatedHeadTracker **ppHeadTracker);
        
        /* [propget][helpstring] */ HRESULT ( STDMETHODCALLTYPE *get_HandTracker )( 
            ISimulatedDevice * This,
            /* [retval][out] */ ISimulatedHandTracker **ppHandTracker);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *SetSimulatedDeviceType )( 
            ISimulatedDevice * This,
            /* [in] */ SIMULATED_DEVICE_TYPE type);
        
        END_INTERFACE
    } ISimulatedDeviceVtbl;

    interface ISimulatedDevice
    {
        CONST_VTBL struct ISimulatedDeviceVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define ISimulatedDevice_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define ISimulatedDevice_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define ISimulatedDevice_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define ISimulatedDevice_get_HeadTracker(This,ppHeadTracker)	\
    ( (This)->lpVtbl -> get_HeadTracker(This,ppHeadTracker) ) 

#define ISimulatedDevice_get_HandTracker(This,ppHandTracker)	\
    ( (This)->lpVtbl -> get_HandTracker(This,ppHandTracker) ) 

#define ISimulatedDevice_SetSimulatedDeviceType(This,type)	\
    ( (This)->lpVtbl -> SetSimulatedDeviceType(This,type) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __ISimulatedDevice_INTERFACE_DEFINED__ */


#ifndef __ISimulatedHeadTracker_INTERFACE_DEFINED__
#define __ISimulatedHeadTracker_INTERFACE_DEFINED__

/* interface ISimulatedHeadTracker */
/* [helpstring][local][uuid][object] */ 


EXTERN_C const IID IID_ISimulatedHeadTracker;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("A2811735-96BA-4BA7-AE15-7D2163F8113A")
    ISimulatedHeadTracker : public IUnknown
    {
    public:
        virtual /* [propget][helpstring] */ HRESULT STDMETHODCALLTYPE get_HeadTrackerMode( 
            /* [retval][out] */ HEAD_TRACKER_MODE *mode) = 0;
        
        virtual /* [propput][helpstring] */ HRESULT STDMETHODCALLTYPE put_HeadTrackerMode( 
            /* [in] */ HEAD_TRACKER_MODE mode) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct ISimulatedHeadTrackerVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            ISimulatedHeadTracker * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            ISimulatedHeadTracker * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            ISimulatedHeadTracker * This);
        
        /* [propget][helpstring] */ HRESULT ( STDMETHODCALLTYPE *get_HeadTrackerMode )( 
            ISimulatedHeadTracker * This,
            /* [retval][out] */ HEAD_TRACKER_MODE *mode);
        
        /* [propput][helpstring] */ HRESULT ( STDMETHODCALLTYPE *put_HeadTrackerMode )( 
            ISimulatedHeadTracker * This,
            /* [in] */ HEAD_TRACKER_MODE mode);
        
        END_INTERFACE
    } ISimulatedHeadTrackerVtbl;

    interface ISimulatedHeadTracker
    {
        CONST_VTBL struct ISimulatedHeadTrackerVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define ISimulatedHeadTracker_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define ISimulatedHeadTracker_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define ISimulatedHeadTracker_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define ISimulatedHeadTracker_get_HeadTrackerMode(This,mode)	\
    ( (This)->lpVtbl -> get_HeadTrackerMode(This,mode) ) 

#define ISimulatedHeadTracker_put_HeadTrackerMode(This,mode)	\
    ( (This)->lpVtbl -> put_HeadTrackerMode(This,mode) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __ISimulatedHeadTracker_INTERFACE_DEFINED__ */


#ifndef __ISimulatedHandTracker_INTERFACE_DEFINED__
#define __ISimulatedHandTracker_INTERFACE_DEFINED__

/* interface ISimulatedHandTracker */
/* [helpstring][local][uuid][object] */ 


EXTERN_C const IID IID_ISimulatedHandTracker;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("C49CE729-2181-4B61-AC47-B03225D70802")
    ISimulatedHandTracker : public ISimulatedNode
    {
    public:
        virtual /* [propget][helpstring] */ HRESULT STDMETHODCALLTYPE get_Position( 
            /* [retval][out] */ VECTOR3 *position) = 0;
        
        virtual /* [propput][helpstring] */ HRESULT STDMETHODCALLTYPE put_Position( 
            /* [in] */ VECTOR3 position) = 0;
        
        virtual /* [propget][helpstring] */ HRESULT STDMETHODCALLTYPE get_Pitch( 
            /* [retval][out] */ float *radians) = 0;
        
        virtual /* [propput][helpstring] */ HRESULT STDMETHODCALLTYPE put_Pitch( 
            /* [in] */ float radians) = 0;
        
        virtual /* [propget][helpstring] */ HRESULT STDMETHODCALLTYPE get_FrustumIgnored( 
            /* [retval][out] */ BOOL *ignored) = 0;
        
        virtual /* [propput][helpstring] */ HRESULT STDMETHODCALLTYPE put_FrustumIgnored( 
            /* [in] */ BOOL ignored) = 0;
        
        virtual /* [propget][helpstring] */ HRESULT STDMETHODCALLTYPE get_Frustum( 
            /* [retval][out] */ FRUSTUM *frustum) = 0;
        
        virtual /* [propput][helpstring] */ HRESULT STDMETHODCALLTYPE put_Frustum( 
            /* [in] */ FRUSTUM frustum) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct ISimulatedHandTrackerVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            ISimulatedHandTracker * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            ISimulatedHandTracker * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            ISimulatedHandTracker * This);
        
        /* [propget][helpstring] */ HRESULT ( STDMETHODCALLTYPE *get_WorldPosition )( 
            ISimulatedHandTracker * This,
            /* [retval][out] */ VECTOR3 *pos);
        
        /* [propget][helpstring] */ HRESULT ( STDMETHODCALLTYPE *get_Position )( 
            ISimulatedHandTracker * This,
            /* [retval][out] */ VECTOR3 *position);
        
        /* [propput][helpstring] */ HRESULT ( STDMETHODCALLTYPE *put_Position )( 
            ISimulatedHandTracker * This,
            /* [in] */ VECTOR3 position);
        
        /* [propget][helpstring] */ HRESULT ( STDMETHODCALLTYPE *get_Pitch )( 
            ISimulatedHandTracker * This,
            /* [retval][out] */ float *radians);
        
        /* [propput][helpstring] */ HRESULT ( STDMETHODCALLTYPE *put_Pitch )( 
            ISimulatedHandTracker * This,
            /* [in] */ float radians);
        
        /* [propget][helpstring] */ HRESULT ( STDMETHODCALLTYPE *get_FrustumIgnored )( 
            ISimulatedHandTracker * This,
            /* [retval][out] */ BOOL *ignored);
        
        /* [propput][helpstring] */ HRESULT ( STDMETHODCALLTYPE *put_FrustumIgnored )( 
            ISimulatedHandTracker * This,
            /* [in] */ BOOL ignored);
        
        /* [propget][helpstring] */ HRESULT ( STDMETHODCALLTYPE *get_Frustum )( 
            ISimulatedHandTracker * This,
            /* [retval][out] */ FRUSTUM *frustum);
        
        /* [propput][helpstring] */ HRESULT ( STDMETHODCALLTYPE *put_Frustum )( 
            ISimulatedHandTracker * This,
            /* [in] */ FRUSTUM frustum);
        
        END_INTERFACE
    } ISimulatedHandTrackerVtbl;

    interface ISimulatedHandTracker
    {
        CONST_VTBL struct ISimulatedHandTrackerVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define ISimulatedHandTracker_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define ISimulatedHandTracker_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define ISimulatedHandTracker_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define ISimulatedHandTracker_get_WorldPosition(This,pos)	\
    ( (This)->lpVtbl -> get_WorldPosition(This,pos) ) 


#define ISimulatedHandTracker_get_Position(This,position)	\
    ( (This)->lpVtbl -> get_Position(This,position) ) 

#define ISimulatedHandTracker_put_Position(This,position)	\
    ( (This)->lpVtbl -> put_Position(This,position) ) 

#define ISimulatedHandTracker_get_Pitch(This,radians)	\
    ( (This)->lpVtbl -> get_Pitch(This,radians) ) 

#define ISimulatedHandTracker_put_Pitch(This,radians)	\
    ( (This)->lpVtbl -> put_Pitch(This,radians) ) 

#define ISimulatedHandTracker_get_FrustumIgnored(This,ignored)	\
    ( (This)->lpVtbl -> get_FrustumIgnored(This,ignored) ) 

#define ISimulatedHandTracker_put_FrustumIgnored(This,ignored)	\
    ( (This)->lpVtbl -> put_FrustumIgnored(This,ignored) ) 

#define ISimulatedHandTracker_get_Frustum(This,frustum)	\
    ( (This)->lpVtbl -> get_Frustum(This,frustum) ) 

#define ISimulatedHandTracker_put_Frustum(This,frustum)	\
    ( (This)->lpVtbl -> put_Frustum(This,frustum) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __ISimulatedHandTracker_INTERFACE_DEFINED__ */


#ifndef __ISimulatedHuman_INTERFACE_DEFINED__
#define __ISimulatedHuman_INTERFACE_DEFINED__

/* interface ISimulatedHuman */
/* [helpstring][local][uuid][object] */ 


EXTERN_C const IID IID_ISimulatedHuman;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("ACEED7C2-26A8-4AB3-832E-8784D132B16E")
    ISimulatedHuman : public ISimulatedNode
    {
    public:
        virtual /* [propput][helpstring] */ HRESULT STDMETHODCALLTYPE put_WorldPosition( 
            /* [in] */ VECTOR3 pos) = 0;
        
        virtual /* [propget][helpstring] */ HRESULT STDMETHODCALLTYPE get_Direction( 
            /* [retval][out] */ float *radians) = 0;
        
        virtual /* [propput][helpstring] */ HRESULT STDMETHODCALLTYPE put_Direction( 
            /* [in] */ float radians) = 0;
        
        virtual /* [propget][helpstring] */ HRESULT STDMETHODCALLTYPE get_Height( 
            /* [retval][out] */ float *meters) = 0;
        
        virtual /* [propput][helpstring] */ HRESULT STDMETHODCALLTYPE put_Height( 
            float meters) = 0;
        
        virtual /* [propget][helpstring] */ HRESULT STDMETHODCALLTYPE get_LeftHand( 
            /* [retval][out] */ ISimulatedHand **hand) = 0;
        
        virtual /* [propget][helpstring] */ HRESULT STDMETHODCALLTYPE get_RightHand( 
            /* [retval][out] */ ISimulatedHand **hand) = 0;
        
        virtual /* [propget][helpstring] */ HRESULT STDMETHODCALLTYPE get_Head( 
            /* [retval][out] */ ISimulatedHead **head) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE Move( 
            /* [in] */ VECTOR3 translation) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE Rotate( 
            float radians) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct ISimulatedHumanVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            ISimulatedHuman * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            ISimulatedHuman * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            ISimulatedHuman * This);
        
        /* [propget][helpstring] */ HRESULT ( STDMETHODCALLTYPE *get_WorldPosition )( 
            ISimulatedHuman * This,
            /* [retval][out] */ VECTOR3 *pos);
        
        /* [propput][helpstring] */ HRESULT ( STDMETHODCALLTYPE *put_WorldPosition )( 
            ISimulatedHuman * This,
            /* [in] */ VECTOR3 pos);
        
        /* [propget][helpstring] */ HRESULT ( STDMETHODCALLTYPE *get_Direction )( 
            ISimulatedHuman * This,
            /* [retval][out] */ float *radians);
        
        /* [propput][helpstring] */ HRESULT ( STDMETHODCALLTYPE *put_Direction )( 
            ISimulatedHuman * This,
            /* [in] */ float radians);
        
        /* [propget][helpstring] */ HRESULT ( STDMETHODCALLTYPE *get_Height )( 
            ISimulatedHuman * This,
            /* [retval][out] */ float *meters);
        
        /* [propput][helpstring] */ HRESULT ( STDMETHODCALLTYPE *put_Height )( 
            ISimulatedHuman * This,
            float meters);
        
        /* [propget][helpstring] */ HRESULT ( STDMETHODCALLTYPE *get_LeftHand )( 
            ISimulatedHuman * This,
            /* [retval][out] */ ISimulatedHand **hand);
        
        /* [propget][helpstring] */ HRESULT ( STDMETHODCALLTYPE *get_RightHand )( 
            ISimulatedHuman * This,
            /* [retval][out] */ ISimulatedHand **hand);
        
        /* [propget][helpstring] */ HRESULT ( STDMETHODCALLTYPE *get_Head )( 
            ISimulatedHuman * This,
            /* [retval][out] */ ISimulatedHead **head);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *Move )( 
            ISimulatedHuman * This,
            /* [in] */ VECTOR3 translation);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *Rotate )( 
            ISimulatedHuman * This,
            float radians);
        
        END_INTERFACE
    } ISimulatedHumanVtbl;

    interface ISimulatedHuman
    {
        CONST_VTBL struct ISimulatedHumanVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define ISimulatedHuman_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define ISimulatedHuman_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define ISimulatedHuman_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define ISimulatedHuman_get_WorldPosition(This,pos)	\
    ( (This)->lpVtbl -> get_WorldPosition(This,pos) ) 


#define ISimulatedHuman_put_WorldPosition(This,pos)	\
    ( (This)->lpVtbl -> put_WorldPosition(This,pos) ) 

#define ISimulatedHuman_get_Direction(This,radians)	\
    ( (This)->lpVtbl -> get_Direction(This,radians) ) 

#define ISimulatedHuman_put_Direction(This,radians)	\
    ( (This)->lpVtbl -> put_Direction(This,radians) ) 

#define ISimulatedHuman_get_Height(This,meters)	\
    ( (This)->lpVtbl -> get_Height(This,meters) ) 

#define ISimulatedHuman_put_Height(This,meters)	\
    ( (This)->lpVtbl -> put_Height(This,meters) ) 

#define ISimulatedHuman_get_LeftHand(This,hand)	\
    ( (This)->lpVtbl -> get_LeftHand(This,hand) ) 

#define ISimulatedHuman_get_RightHand(This,hand)	\
    ( (This)->lpVtbl -> get_RightHand(This,hand) ) 

#define ISimulatedHuman_get_Head(This,head)	\
    ( (This)->lpVtbl -> get_Head(This,head) ) 

#define ISimulatedHuman_Move(This,translation)	\
    ( (This)->lpVtbl -> Move(This,translation) ) 

#define ISimulatedHuman_Rotate(This,radians)	\
    ( (This)->lpVtbl -> Rotate(This,radians) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __ISimulatedHuman_INTERFACE_DEFINED__ */


#ifndef __ISimulatedHand_INTERFACE_DEFINED__
#define __ISimulatedHand_INTERFACE_DEFINED__

/* interface ISimulatedHand */
/* [helpstring][local][uuid][object] */ 


EXTERN_C const IID IID_ISimulatedHand;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("64B1B92D-8680-4DF6-BB36-14B8CFBBD1E2")
    ISimulatedHand : public ISimulatedNode
    {
    public:
        virtual /* [propget][helpstring] */ HRESULT STDMETHODCALLTYPE get_Position( 
            /* [retval][out] */ VECTOR3 *pos) = 0;
        
        virtual /* [propput][helpstring] */ HRESULT STDMETHODCALLTYPE put_Position( 
            /* [in] */ VECTOR3 pos) = 0;
        
        virtual /* [propget][helpstring] */ HRESULT STDMETHODCALLTYPE get_Activated( 
            /* [retval][out] */ BOOL *activated) = 0;
        
        virtual /* [propput][helpstring] */ HRESULT STDMETHODCALLTYPE put_Activated( 
            /* [in] */ BOOL activated) = 0;
        
        virtual /* [propget][helpstring] */ HRESULT STDMETHODCALLTYPE get_Visible( 
            /* [retval][out] */ BOOL *visible) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE EnsureVisible( void) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE Move( 
            /* [in] */ VECTOR3 translation) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE PerformGesture( 
            /* [in] */ SIMULATED_GESTURE gesture) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct ISimulatedHandVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            ISimulatedHand * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            ISimulatedHand * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            ISimulatedHand * This);
        
        /* [propget][helpstring] */ HRESULT ( STDMETHODCALLTYPE *get_WorldPosition )( 
            ISimulatedHand * This,
            /* [retval][out] */ VECTOR3 *pos);
        
        /* [propget][helpstring] */ HRESULT ( STDMETHODCALLTYPE *get_Position )( 
            ISimulatedHand * This,
            /* [retval][out] */ VECTOR3 *pos);
        
        /* [propput][helpstring] */ HRESULT ( STDMETHODCALLTYPE *put_Position )( 
            ISimulatedHand * This,
            /* [in] */ VECTOR3 pos);
        
        /* [propget][helpstring] */ HRESULT ( STDMETHODCALLTYPE *get_Activated )( 
            ISimulatedHand * This,
            /* [retval][out] */ BOOL *activated);
        
        /* [propput][helpstring] */ HRESULT ( STDMETHODCALLTYPE *put_Activated )( 
            ISimulatedHand * This,
            /* [in] */ BOOL activated);
        
        /* [propget][helpstring] */ HRESULT ( STDMETHODCALLTYPE *get_Visible )( 
            ISimulatedHand * This,
            /* [retval][out] */ BOOL *visible);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *EnsureVisible )( 
            ISimulatedHand * This);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *Move )( 
            ISimulatedHand * This,
            /* [in] */ VECTOR3 translation);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *PerformGesture )( 
            ISimulatedHand * This,
            /* [in] */ SIMULATED_GESTURE gesture);
        
        END_INTERFACE
    } ISimulatedHandVtbl;

    interface ISimulatedHand
    {
        CONST_VTBL struct ISimulatedHandVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define ISimulatedHand_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define ISimulatedHand_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define ISimulatedHand_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define ISimulatedHand_get_WorldPosition(This,pos)	\
    ( (This)->lpVtbl -> get_WorldPosition(This,pos) ) 


#define ISimulatedHand_get_Position(This,pos)	\
    ( (This)->lpVtbl -> get_Position(This,pos) ) 

#define ISimulatedHand_put_Position(This,pos)	\
    ( (This)->lpVtbl -> put_Position(This,pos) ) 

#define ISimulatedHand_get_Activated(This,activated)	\
    ( (This)->lpVtbl -> get_Activated(This,activated) ) 

#define ISimulatedHand_put_Activated(This,activated)	\
    ( (This)->lpVtbl -> put_Activated(This,activated) ) 

#define ISimulatedHand_get_Visible(This,visible)	\
    ( (This)->lpVtbl -> get_Visible(This,visible) ) 

#define ISimulatedHand_EnsureVisible(This)	\
    ( (This)->lpVtbl -> EnsureVisible(This) ) 

#define ISimulatedHand_Move(This,translation)	\
    ( (This)->lpVtbl -> Move(This,translation) ) 

#define ISimulatedHand_PerformGesture(This,gesture)	\
    ( (This)->lpVtbl -> PerformGesture(This,gesture) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __ISimulatedHand_INTERFACE_DEFINED__ */


#ifndef __ISimulatedHead_INTERFACE_DEFINED__
#define __ISimulatedHead_INTERFACE_DEFINED__

/* interface ISimulatedHead */
/* [helpstring][local][uuid][object] */ 


EXTERN_C const IID IID_ISimulatedHead;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("E1E3E1DF-8E5E-4C0E-936F-C3E4A49490A3")
    ISimulatedHead : public ISimulatedNode
    {
    public:
        virtual /* [propget][helpstring] */ HRESULT STDMETHODCALLTYPE get_Rotation( 
            /* [retval][out] */ ROTATION3 *rotation) = 0;
        
        virtual /* [propput][helpstring] */ HRESULT STDMETHODCALLTYPE put_Rotation( 
            /* [in] */ ROTATION3 rotation) = 0;
        
        virtual /* [propget][helpstring] */ HRESULT STDMETHODCALLTYPE get_Diameter( 
            /* [retval][out] */ float *meters) = 0;
        
        virtual /* [propput][helpstring] */ HRESULT STDMETHODCALLTYPE put_Diameter( 
            float meters) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE Rotate( 
            /* [in] */ ROTATION3 rotation) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct ISimulatedHeadVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            ISimulatedHead * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            ISimulatedHead * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            ISimulatedHead * This);
        
        /* [propget][helpstring] */ HRESULT ( STDMETHODCALLTYPE *get_WorldPosition )( 
            ISimulatedHead * This,
            /* [retval][out] */ VECTOR3 *pos);
        
        /* [propget][helpstring] */ HRESULT ( STDMETHODCALLTYPE *get_Rotation )( 
            ISimulatedHead * This,
            /* [retval][out] */ ROTATION3 *rotation);
        
        /* [propput][helpstring] */ HRESULT ( STDMETHODCALLTYPE *put_Rotation )( 
            ISimulatedHead * This,
            /* [in] */ ROTATION3 rotation);
        
        /* [propget][helpstring] */ HRESULT ( STDMETHODCALLTYPE *get_Diameter )( 
            ISimulatedHead * This,
            /* [retval][out] */ float *meters);
        
        /* [propput][helpstring] */ HRESULT ( STDMETHODCALLTYPE *put_Diameter )( 
            ISimulatedHead * This,
            float meters);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *Rotate )( 
            ISimulatedHead * This,
            /* [in] */ ROTATION3 rotation);
        
        END_INTERFACE
    } ISimulatedHeadVtbl;

    interface ISimulatedHead
    {
        CONST_VTBL struct ISimulatedHeadVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define ISimulatedHead_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define ISimulatedHead_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define ISimulatedHead_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define ISimulatedHead_get_WorldPosition(This,pos)	\
    ( (This)->lpVtbl -> get_WorldPosition(This,pos) ) 


#define ISimulatedHead_get_Rotation(This,rotation)	\
    ( (This)->lpVtbl -> get_Rotation(This,rotation) ) 

#define ISimulatedHead_put_Rotation(This,rotation)	\
    ( (This)->lpVtbl -> put_Rotation(This,rotation) ) 

#define ISimulatedHead_get_Diameter(This,meters)	\
    ( (This)->lpVtbl -> get_Diameter(This,meters) ) 

#define ISimulatedHead_put_Diameter(This,meters)	\
    ( (This)->lpVtbl -> put_Diameter(This,meters) ) 

#define ISimulatedHead_Rotate(This,rotation)	\
    ( (This)->lpVtbl -> Rotate(This,rotation) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __ISimulatedHead_INTERFACE_DEFINED__ */


#ifndef __ISimulationRecording_INTERFACE_DEFINED__
#define __ISimulationRecording_INTERFACE_DEFINED__

/* interface ISimulationRecording */
/* [helpstring][uuid][object] */ 


EXTERN_C const IID IID_ISimulationRecording;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("B9F055EF-3418-4E27-885E-C6DFCF3FB126")
    ISimulationRecording : public IUnknown
    {
    public:
        virtual /* [propget][helpstring] */ HRESULT STDMETHODCALLTYPE get_DataTypes( 
            /* [retval][out] */ __RPC__out STREAM_DATA_TYPE *type) = 0;
        
        virtual /* [propget][helpstring] */ HRESULT STDMETHODCALLTYPE get_State( 
            /* [retval][out] */ __RPC__out PLAYBACK_STATE *pState) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE Play( void) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE Pause( void) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE Seek( 
            /* [in] */ UINT64 ticks) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE Stop( void) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct ISimulationRecordingVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            __RPC__in ISimulationRecording * This,
            /* [in] */ __RPC__in REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            __RPC__in ISimulationRecording * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            __RPC__in ISimulationRecording * This);
        
        /* [propget][helpstring] */ HRESULT ( STDMETHODCALLTYPE *get_DataTypes )( 
            __RPC__in ISimulationRecording * This,
            /* [retval][out] */ __RPC__out STREAM_DATA_TYPE *type);
        
        /* [propget][helpstring] */ HRESULT ( STDMETHODCALLTYPE *get_State )( 
            __RPC__in ISimulationRecording * This,
            /* [retval][out] */ __RPC__out PLAYBACK_STATE *pState);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *Play )( 
            __RPC__in ISimulationRecording * This);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *Pause )( 
            __RPC__in ISimulationRecording * This);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *Seek )( 
            __RPC__in ISimulationRecording * This,
            /* [in] */ UINT64 ticks);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *Stop )( 
            __RPC__in ISimulationRecording * This);
        
        END_INTERFACE
    } ISimulationRecordingVtbl;

    interface ISimulationRecording
    {
        CONST_VTBL struct ISimulationRecordingVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define ISimulationRecording_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define ISimulationRecording_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define ISimulationRecording_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define ISimulationRecording_get_DataTypes(This,type)	\
    ( (This)->lpVtbl -> get_DataTypes(This,type) ) 

#define ISimulationRecording_get_State(This,pState)	\
    ( (This)->lpVtbl -> get_State(This,pState) ) 

#define ISimulationRecording_Play(This)	\
    ( (This)->lpVtbl -> Play(This) ) 

#define ISimulationRecording_Pause(This)	\
    ( (This)->lpVtbl -> Pause(This) ) 

#define ISimulationRecording_Seek(This,ticks)	\
    ( (This)->lpVtbl -> Seek(This,ticks) ) 

#define ISimulationRecording_Stop(This)	\
    ( (This)->lpVtbl -> Stop(This) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __ISimulationRecording_INTERFACE_DEFINED__ */


#ifndef __ISimulationRecordingCallback_INTERFACE_DEFINED__
#define __ISimulationRecordingCallback_INTERFACE_DEFINED__

/* interface ISimulationRecordingCallback */
/* [helpstring][uuid][object] */ 


EXTERN_C const IID IID_ISimulationRecordingCallback;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("DAFBDA26-8292-449E-A708-BF70E2B46ACF")
    ISimulationRecordingCallback : public IUnknown
    {
    public:
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE PlaybackStateChanged( 
            /* [in] */ PLAYBACK_STATE newState) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct ISimulationRecordingCallbackVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            __RPC__in ISimulationRecordingCallback * This,
            /* [in] */ __RPC__in REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            __RPC__in ISimulationRecordingCallback * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            __RPC__in ISimulationRecordingCallback * This);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *PlaybackStateChanged )( 
            __RPC__in ISimulationRecordingCallback * This,
            /* [in] */ PLAYBACK_STATE newState);
        
        END_INTERFACE
    } ISimulationRecordingCallbackVtbl;

    interface ISimulationRecordingCallback
    {
        CONST_VTBL struct ISimulationRecordingCallbackVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define ISimulationRecordingCallback_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define ISimulationRecordingCallback_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define ISimulationRecordingCallback_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define ISimulationRecordingCallback_PlaybackStateChanged(This,newState)	\
    ( (This)->lpVtbl -> PlaybackStateChanged(This,newState) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __ISimulationRecordingCallback_INTERFACE_DEFINED__ */


/* interface __MIDL_itf_perceptionsimulationmanager_0000_0010 */
/* [local] */ 

/* [helpstring] */ HRESULT __stdcall CreatePerceptionSimulationManager( 
    /* [annotation][in] */ 
    _In_  ISimulationStreamSink *pSink,
    /* [annotation][retval][out] */ 
    _COM_Outptr_  IPerceptionSimulationManager **ppManager);

/* [helpstring] */ HRESULT __stdcall CreatePerceptionSimulationRecording( 
    /* [annotation][in] */ 
    _In_  BSTR path,
    /* [annotation][retval][out] */ 
    _COM_Outptr_  ISimulationStreamSink **ppRecording);

/* [helpstring] */ HRESULT __stdcall LoadPerceptionSimulationRecording( 
    /* [annotation][in] */ 
    _In_  BSTR path,
    /* [annotation][in] */ 
    _In_  ISimulationStreamSinkFactory *pFactory,
    /* [annotation][in] */ 
    _In_  ISimulationRecordingCallback *pCallback,
    /* [annotation][retval][out] */ 
    _COM_Outptr_  ISimulationRecording **ppRecording);



extern RPC_IF_HANDLE __MIDL_itf_perceptionsimulationmanager_0000_0010_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_perceptionsimulationmanager_0000_0010_v0_0_s_ifspec;

/* Additional Prototypes for ALL interfaces */

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif


