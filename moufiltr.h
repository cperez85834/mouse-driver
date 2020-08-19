/*++
Copyright (c) 2008  Microsoft Corporation

Module Name:

    moufiltr.h

Abstract:

    This module contains the common private declarations for the mouse
    packet filter

Environment:

    kernel mode only

Notes:


Revision History:


--*/

#ifndef MOUFILTER_H
#define MOUFILTER_H

#include <ntddk.h>
#include <kernelspecs.h>
#include <kbdmou.h>
#include <ntddmou.h>
#include <ntddkbd.h>
#include <ntdd8042.h>
#include <wdf.h>
#include <wdfrequest.h>

#define NTSTRSAFE_LIB
#include <ntstrsafe.h>

#include <initguid.h>
#include <devguid.h>

#if DBG

#define TRAP()                      DbgBreakPoint()

#define DebugPrint(_x_) DbgPrint _x_

#else   // DBG

#define TRAP()

#define DebugPrint(_x_)

#endif

 
typedef struct _DEVICE_EXTENSION
{
 
	WDFDEVICE WdfDevice;
     //
    // Previous hook routine and context
    //                               
    PVOID UpperContext;
     
    PI8042_MOUSE_ISR UpperIsrHook;

	WDFQUEUE rawPdoQueue;

	LONG EnableCount;

    //
    // Write to the mouse in the context of MouFilter_IsrHook
    //
    IN PI8042_ISR_WRITE_PORT IsrWritePort;

    //
    // Context for IsrWritePort, QueueMousePacket
    //
    IN PVOID CallContext;

    //
    // Queue the current packet (ie the one passed into MouFilter_IsrHook)
    // to be reported to the class driver
    //
    IN PI8042_QUEUE_PACKET QueueMousePacket;

    //
    // The real connect data that this driver reports to
    //
    CONNECT_DATA UpperConnectData;

  
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_EXTENSION,
                                        FilterGetData)
 
typedef struct _WORKER_ITEM_CONTEXT {

	WDFREQUEST  Request;
	WDFIOTARGET IoTarget;

} WORKER_ITEM_CONTEXT, *PWORKER_ITEM_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(WORKER_ITEM_CONTEXT, GetWorkItemContext)

//
// Prototypes
//
DRIVER_INITIALIZE DriverEntry;

EVT_WDF_DRIVER_DEVICE_ADD MouFilter_EvtDeviceAdd;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL MouFilter_EvtIoDeviceControlForRawPdo;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL MouFilter_EvtIoDeviceControlFromRawPdo;
EVT_WDF_IO_QUEUE_IO_INTERNAL_DEVICE_CONTROL MouFilter_EvtIoInternalDeviceControl;
 
typedef struct _RPDO_DEVICE_DATA
{

	ULONG InstanceNo;

	//
	// Queue of the parent device we will forward requests to
	//
	WDFQUEUE ParentQueue;

} RPDO_DEVICE_DATA, *PRPDO_DEVICE_DATA;

VOID
MouFilter_DispatchPassThrough(
     _In_ WDFREQUEST Request,
    _In_ WDFIOTARGET Target
    );

BOOLEAN
MouFilter_IsrHook (
    PVOID         DeviceExtension,
    PMOUSE_INPUT_DATA       CurrentInput, 
    POUTPUT_PACKET          CurrentOutput,
    UCHAR                   StatusByte,
    PUCHAR                  DataByte,
    PBOOLEAN                ContinueProcessing,
    PMOUSE_STATE            MouseState,
    PMOUSE_RESET_SUBSTATE   ResetSubState
);

VOID
MouFilter_ServiceCallback(
    IN PDEVICE_OBJECT DeviceObject,
    IN PMOUSE_INPUT_DATA InputDataStart,
    IN PMOUSE_INPUT_DATA InputDataEnd,
    IN OUT PULONG InputDataConsumed
    );

EVT_WDF_REQUEST_COMPLETION_ROUTINE
KbFilterRequestCompletionRoutine;

DEFINE_GUID(GUID_BUS_MOUFILTER,  /* 78639d26-4500-4006-826d-fd81bf347923 */
	0x78639d26, 0x4500, 0x4006, 0x82, 0x6d, 0xfd, 0x81, 0xbf, 0x34, 0x79, 0x23);


DEFINE_GUID(GUID_DEVINTERFACE_MOUFILTER, /* bfa25f02-9388-4831-bac0-99b58132877d */
	0xbfa25f02, 0x9388, 0x4831, 0xba, 0xc0, 0x99, 0xb5, 0x81, 0x32, 0x87, 0x7d);



#define  MOUFILTR_DEVICE_ID L"{78639d26-4500-4006-826d-fd81bf347923}\\MouseFilter\0"


WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(RPDO_DEVICE_DATA, PdoGetData)

NTSTATUS
MouFiltr_CreateRawPdo(
	WDFDEVICE       Device,
	ULONG           InstanceNo
);


#endif  // MOUFILTER_H


