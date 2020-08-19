/*--         
Copyright (c) 2008  Microsoft Corporation

Module Name:

    moufiltr.c

Abstract:

Environment:

    Kernel mode only- Framework Version 

Notes:


--*/

#include "moufiltr.h"
#include "public.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
#pragma alloc_text (PAGE, MouFilter_EvtDeviceAdd)
#pragma alloc_text (PAGE, MouFilter_EvtIoInternalDeviceControl)
#endif

#pragma warning(push)
#pragma warning(disable:4055) // type case from PVOID to PSERVICE_CALLBACK_ROUTINE
#pragma warning(disable:4152) // function/data pointer conversion in expression

ULONG InstanceNo = 0;

NTSTATUS
DriverEntry (
    IN  PDRIVER_OBJECT  DriverObject,
    IN  PUNICODE_STRING RegistryPath
    )
/*++
Routine Description:

     Installable driver initialization entry point.
    This entry point is called directly by the I/O system.

--*/
{
    WDF_DRIVER_CONFIG               config;
    NTSTATUS                                status;

    DebugPrint(("Mouse Filter Driver Sample - Driver Framework Edition.\n"));
    DebugPrint(("Built %s %s\n", __DATE__, __TIME__));
    
    // Initiialize driver config to control the attributes that
    // are global to the driver. Note that framework by default
    // provides a driver unload routine. If you create any resources
    // in the DriverEntry and want to be cleaned in driver unload,
    // you can override that by manually setting the EvtDriverUnload in the
    // config structure. In general xxx_CONFIG_INIT macros are provided to
    // initialize most commonly used members.

    WDF_DRIVER_CONFIG_INIT(
        &config,
        MouFilter_EvtDeviceAdd
    );

    //
    // Create a framework driver object to represent our driver.
    //
    status = WdfDriverCreate(DriverObject,
                            RegistryPath,
                            WDF_NO_OBJECT_ATTRIBUTES,
                            &config,
                            WDF_NO_HANDLE); // hDriver optional
    if (!NT_SUCCESS(status)) {
        DebugPrint( ("WdfDriverCreate failed with status 0x%x\n", status));
    }

    return status; 
}

NTSTATUS
MouFilter_EvtDeviceAdd(
    IN WDFDRIVER        Driver,
    IN PWDFDEVICE_INIT  DeviceInit
    )
/*++
Routine Description:

    EvtDeviceAdd is called by the framework in response to AddDevice
    call from the PnP manager. Here you can query the device properties
    using WdfFdoInitWdmGetPhysicalDevice/IoGetDeviceProperty and based
    on that, decide to create a filter device object and attach to the
    function stack.

    If you are not interested in filtering this particular instance of the
    device, you can just return STATUS_SUCCESS without creating a framework
    device.

Arguments:

    Driver - Handle to a framework driver object created in DriverEntry

    DeviceInit - Pointer to a framework-allocated WDFDEVICE_INIT structure.

Return Value:

    NTSTATUS

--*/   
{
	WDF_OBJECT_ATTRIBUTES   deviceAttributes;
	NTSTATUS                status;
	WDFDEVICE               hDevice;
	WDFQUEUE                hQueue;
	PDEVICE_EXTENSION       filterExt;
	WDF_IO_QUEUE_CONFIG     ioQueueConfig;
    
    UNREFERENCED_PARAMETER(Driver);

    PAGED_CODE();

    DebugPrint(("Enter FilterEvtDeviceAdd \n"));

    //
    // Tell the framework that you are filter driver. Framework
    // takes care of inherting all the device flags & characterstics
    // from the lower device you are attaching to.
    //
    WdfFdoInitSetFilter(DeviceInit);

    WdfDeviceInitSetDeviceType(DeviceInit, FILE_DEVICE_MOUSE);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes,
        DEVICE_EXTENSION);

    
    //
    // Create a framework device object.  This call will in turn create
    // a WDM deviceobject, attach to the lower stack and set the
    // appropriate flags and attributes.
    //
    status = WdfDeviceCreate(&DeviceInit, &deviceAttributes, &hDevice);
    if (!NT_SUCCESS(status)) {
        DebugPrint(("WdfDeviceCreate failed with status code 0x%x\n", status));
        return status;
    }

	filterExt = FilterGetData(hDevice);

    //
    // Configure the default queue to be Parallel. Do not use sequential queue
    // if this driver is going to be filtering PS2 ports because it can lead to
    // deadlock. The PS2 port driver sends a request to the top of the stack when it
    // receives an ioctl request and waits for it to be completed. If you use a
    // a sequential queue, this request will be stuck in the queue because of the 
    // outstanding ioctl request sent earlier to the port driver.
    //
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&ioQueueConfig,
                             WdfIoQueueDispatchParallel);

    //
    // Framework by default creates non-power managed queues for
    // filter drivers.
    //
    ioQueueConfig.EvtIoInternalDeviceControl = MouFilter_EvtIoInternalDeviceControl;

    status = WdfIoQueueCreate(hDevice,
                            &ioQueueConfig,
                            WDF_NO_OBJECT_ATTRIBUTES,
                            WDF_NO_HANDLE
                            );
    if (!NT_SUCCESS(status)) {
        DebugPrint( ("WdfIoQueueCreate failed 0x%x\n", status));
        return status;
    }

	//
	// Create a new queue to handle IOCTLs that will be forwarded to us from
	// the rawPDO. 
	//
	WDF_IO_QUEUE_CONFIG_INIT(&ioQueueConfig,
		WdfIoQueueDispatchParallel);

	//
	// Framework by default creates non-power managed queues for
	// filter drivers.
	//
	ioQueueConfig.EvtIoDeviceControl = MouFilter_EvtIoDeviceControlFromRawPdo;

	status = WdfIoQueueCreate(hDevice,
		&ioQueueConfig,
		WDF_NO_OBJECT_ATTRIBUTES,
		&hQueue
		);
	if (!NT_SUCCESS(status)) {
		DebugPrint(("WdfIoQueueCreate failed 0x%x\n", status));
		return status;
	}

	filterExt->rawPdoQueue = hQueue;

	//
	// Create a RAW pdo so we can provide a sideband communication with
	// the application. Please note that not filter drivers desire to
	// produce such a communication and not all of them are contrained
	// by other filter above which prevent communication thru the device
	// interface exposed by the main stack. So use this only if absolutely
	// needed. Also look at the toaster filter driver sample for an alternate
	// approach to providing sideband communication.
	//
	status = MouFiltr_CreateRawPdo(hDevice, ++InstanceNo);

	return status;
}

VOID
MouFilter_EvtIoDeviceControlFromRawPdo(
IN WDFQUEUE      Queue,
IN WDFREQUEST    Request,
IN size_t        OutputBufferLength,
IN size_t        InputBufferLength,
IN ULONG         IoControlCode
)
/*++

Routine Description:

This routine is the dispatch routine for device control requests.

Arguments:

Queue - Handle to the framework queue object that is associated
with the I/O request.
Request - Handle to a framework request object.

OutputBufferLength - length of the request's output buffer,
if an output buffer is available.
InputBufferLength - length of the request's input buffer,
if an input buffer is available.

IoControlCode - the driver-defined or system-defined I/O control code
(IOCTL) that is associated with the request.

Return Value:

VOID

--*/

{
	NTSTATUS status = STATUS_SUCCESS;
	WDFDEVICE hDevice;
	PDEVICE_EXTENSION devExt;
	size_t bytesTransferred = 0;
	SHORT UnitID = 2;
	OutputBufferLength;
	InputBufferLength;
	//UNREFERENCED_PARAMETER(InputBufferLength);

	DebugPrint(("Entered KbFilter_EvtIoInternalDeviceControl\n"));
	DebugPrint(("HEN\n"));
	hDevice = WdfIoQueueGetDevice(Queue);
	devExt = FilterGetData(hDevice);
	DebugPrint(("LO\n"));
	//
	// Process the ioctl and complete it when you are done.
	//
	DebugPrint(("IoControlCode: %d\n", IoControlCode));
	switch (IoControlCode) {
	case IOCTL_MOUFILTR_LEFT_CLICK:
	{
		MOUSE_INPUT_DATA MouseData;
		PCHAR location = NULL;
		ULONG huh = 0;
		PULONG what = &huh;

		status = WdfRequestRetrieveInputBuffer(Request, 1, &location, NULL);

		if (!NT_SUCCESS(status)) {
			DebugPrint(("Error! Code:  %x\n", status));
			break;
		}


		PMOUSE_INPUT_DATA PMouseData = &MouseData;

		MouseData.UnitId = UnitID;
		MouseData.Flags = MOUSE_MOVE_RELATIVE;
		MouseData.ButtonFlags = MOUSE_LEFT_BUTTON_DOWN;
		MouseData.ButtonData = 0;
		MouseData.Buttons = MouseData.ButtonFlags;
		MouseData.LastX = 0;
		MouseData.LastY = 0;
		MouseData.ExtraInformation = 0;

		PMouseData = &MouseData;
		DebugPrint(("Data Sent\n"));
		MouFilter_ServiceCallback(WdfDeviceWdmGetDeviceObject(hDevice), PMouseData, PMouseData + 1, what);

		MouseData.UnitId = UnitID;
		MouseData.Flags = MOUSE_MOVE_RELATIVE;
		MouseData.ButtonFlags = MOUSE_LEFT_BUTTON_UP;
		MouseData.ButtonData = 0;
		MouseData.Buttons = MouseData.ButtonFlags;
		MouseData.LastX = 0;
		MouseData.LastY = 0;
		MouseData.ExtraInformation = 0;

		PMouseData = &MouseData;

		MouFilter_ServiceCallback(WdfDeviceWdmGetDeviceObject(hDevice), PMouseData, PMouseData + 1, what);
	}


		break;

	case IOCTL_MOUFILTR_RIGHT_CLICK:
	{
		MOUSE_INPUT_DATA MouseData;
		PCHAR location = NULL;
		ULONG huh = 0;
		PULONG what = &huh;

		status = WdfRequestRetrieveInputBuffer(Request, 1, &location, NULL);

		if (!NT_SUCCESS(status)) {
			DebugPrint(("Error! Code:  %x\n", status));
			break;
		}


		PMOUSE_INPUT_DATA PMouseData = &MouseData;

		MouseData.UnitId = UnitID;
		MouseData.Flags = MOUSE_MOVE_RELATIVE;
		MouseData.ButtonFlags = MOUSE_RIGHT_BUTTON_DOWN;
		MouseData.ButtonData = 0;
		MouseData.Buttons = MouseData.ButtonFlags;
		MouseData.LastX = 0;
		MouseData.LastY = 0;
		MouseData.ExtraInformation = 0;

		PMouseData = &MouseData;
		DebugPrint(("Data Sent\n"));
		MouFilter_ServiceCallback(WdfDeviceWdmGetDeviceObject(hDevice), PMouseData, PMouseData + 1, what);

		MouseData.UnitId = UnitID;
		MouseData.Flags = MOUSE_MOVE_RELATIVE;
		MouseData.ButtonFlags = MOUSE_RIGHT_BUTTON_UP;
		MouseData.ButtonData = 0;
		MouseData.Buttons = MouseData.ButtonFlags;
		MouseData.LastX = 0;
		MouseData.LastY = 0;
		MouseData.ExtraInformation = 0;

		PMouseData = &MouseData;

		MouFilter_ServiceCallback(WdfDeviceWdmGetDeviceObject(hDevice), PMouseData, PMouseData + 1, what);
	}


	break;

	case IOCTL_MOUFILTR_MOVE_ABSOLUTE:
	{
		MOUSE_INPUT_DATA MouseData;
		PSHORT location = NULL;
		ULONG huh = 0;
		PULONG what = &huh;
		NTSTATUS FloatStatus;
		XSTATE_SAVE SaveState;
		int x = 0, y = 0;

		//
		// UpperConnectData must be called at DISPATCH
		//

		status = WdfRequestRetrieveInputBuffer(Request, 8, &location, NULL);

		if (!NT_SUCCESS(status)) {
			DebugPrint(("Error! Code:  %x\n", status));
			break;
		}
		int CX = *(location + 4), CY = *(location + 6);
		FloatStatus = KeSaveExtendedProcessorState(XSTATE_MASK_LEGACY, &SaveState);
		if (NT_SUCCESS(FloatStatus)){
			double dx = 65535.0 / CX, dy = 65535.0 / CY; // edit the dimensions
			x = (int)(*(location) * dx);
			y = (int)(*(location + 2) * dy);
			DebugPrint(("x: %d, y: %d\n", x, y));

			KeRestoreExtendedProcessorState(&SaveState);
		}

		PMOUSE_INPUT_DATA PMouseData = &MouseData;

		MouseData.UnitId = UnitID - 1;
		MouseData.Flags = MOUSE_MOVE_ABSOLUTE;
		MouseData.ButtonFlags = 0;
		MouseData.ButtonData = 0;
		MouseData.Buttons = MouseData.ButtonFlags;
		MouseData.LastX = x;
		MouseData.LastY = y;
		MouseData.ExtraInformation = 0;

		PMouseData = &MouseData;
		MouFilter_ServiceCallback(WdfDeviceWdmGetDeviceObject(hDevice), PMouseData, PMouseData + 1, what);
	}


	break;

	case IOCTL_MOUFILTR_MOVE_RELATIVE:
	{
		MOUSE_INPUT_DATA MouseData;
		PSHORT location = NULL;
		ULONG huh = 0;
		PULONG what = &huh;
		NTSTATUS FloatStatus;
		XSTATE_SAVE SaveState;
		int x = 0, y = 0;

		//
		// UpperConnectData must be called at DISPATCH
		//

		status = WdfRequestRetrieveInputBuffer(Request, 8, &location, NULL);

		if (!NT_SUCCESS(status)) {
			DebugPrint(("Error! Code:  %x\n", status));
			break;
		}
		int CX = *(location + 4), CY = *(location + 6);
		FloatStatus = KeSaveExtendedProcessorState(XSTATE_MASK_LEGACY, &SaveState);
		if (NT_SUCCESS(FloatStatus)){
			double dx = 65535.0 / CX, dy = 65535.0 / CY; // edit the dimensions
			x = (int)(*(location)* dx);
			y = (int)(*(location + 2) * dy);

			KeRestoreExtendedProcessorState(&SaveState);
		}

		PMOUSE_INPUT_DATA PMouseData = &MouseData;

		MouseData.UnitId = UnitID - 1;
		MouseData.Flags = MOUSE_MOVE_RELATIVE;
		MouseData.ButtonFlags = 0;
		MouseData.ButtonData = 0;
		MouseData.Buttons = MouseData.ButtonFlags;
		MouseData.LastX = (*(location));
		MouseData.LastY = (*(location + 2));
		MouseData.ExtraInformation = 0;

		PMouseData = &MouseData;
		MouFilter_ServiceCallback(WdfDeviceWdmGetDeviceObject(hDevice), PMouseData, PMouseData + 1, what);
	}


	break;

	case IOCTL_MOUFILTR_MOVE_X_RELATIVE:
	{
		MOUSE_INPUT_DATA MouseData;
		PSHORT location = NULL;
		ULONG huh = 0;
		PULONG what = &huh;
		NTSTATUS FloatStatus;
		XSTATE_SAVE SaveState;
		int x = 0;

		//
		// UpperConnectData must be called at DISPATCH
		//

		status = WdfRequestRetrieveInputBuffer(Request, 8, &location, NULL);

		if (!NT_SUCCESS(status)) {
			DebugPrint(("Error! Code:  %x\n", status));
			break;
		}
		int CX = *(location + 2);
		FloatStatus = KeSaveExtendedProcessorState(XSTATE_MASK_LEGACY, &SaveState);
		if (NT_SUCCESS(FloatStatus)){
			double dx = 65535.0 / CX; // edit the dimensions
			x = (int)(*(location)* dx);

			KeRestoreExtendedProcessorState(&SaveState);
		}

		PMOUSE_INPUT_DATA PMouseData = &MouseData;

		MouseData.UnitId = UnitID - 1; //Remember to change this lol
		MouseData.Flags = MOUSE_MOVE_RELATIVE;
		MouseData.ButtonFlags = 0;
		MouseData.ButtonData = 0;
		MouseData.Buttons = MouseData.ButtonFlags;
		MouseData.LastX = x;
		MouseData.LastY = 0;
		MouseData.ExtraInformation = 0;

		PMouseData = &MouseData;
		MouFilter_ServiceCallback(WdfDeviceWdmGetDeviceObject(hDevice), PMouseData, PMouseData + 1, what);
	}


	break;

	case IOCTL_MOUFILTR_MOVE_Y_RELATIVE:
	{
		MOUSE_INPUT_DATA MouseData;
		PSHORT location = NULL;
		ULONG huh = 0;
		PULONG what = &huh;
		NTSTATUS FloatStatus;
		XSTATE_SAVE SaveState;
		int y = 0;

		//
		// UpperConnectData must be called at DISPATCH
		//

		status = WdfRequestRetrieveInputBuffer(Request, 8, &location, NULL);

		if (!NT_SUCCESS(status)) {
			DebugPrint(("Error! Code:  %x\n", status));
			break;
		}
		int CY = *(location + 2);
		FloatStatus = KeSaveExtendedProcessorState(XSTATE_MASK_LEGACY, &SaveState);
		if (NT_SUCCESS(FloatStatus)){
			double dy = 65535.0 / CY; // edit the dimensions
			y = (int)(*(location + 2) * dy);

			KeRestoreExtendedProcessorState(&SaveState);
		}

		PMOUSE_INPUT_DATA PMouseData = &MouseData;

		MouseData.UnitId = UnitID - 1;
		MouseData.Flags = MOUSE_MOVE_RELATIVE;
		MouseData.ButtonFlags = 0;
		MouseData.ButtonData = 0;
		MouseData.Buttons = MouseData.ButtonFlags;
		MouseData.LastX = 0;
		MouseData.LastY = y;
		MouseData.ExtraInformation = 0;

		PMouseData = &MouseData;
		MouFilter_ServiceCallback(WdfDeviceWdmGetDeviceObject(hDevice), PMouseData, PMouseData + 1, what);
	}


	break;

	case IOCTL_MOUFILTR_HOLD_LEFT:
	{
		MOUSE_INPUT_DATA MouseData;
		PSHORT location = NULL;
		ULONG huh = 0;
		PULONG what = &huh;

		//
		// UpperConnectData must be called at DISPATCH
		//

		status = WdfRequestRetrieveInputBuffer(Request, 1, &location, NULL);

		if (!NT_SUCCESS(status)) {
			DebugPrint(("Error! Code:  %x\n", status));
			break;
		}

		PMOUSE_INPUT_DATA PMouseData = &MouseData;

		MouseData.UnitId = UnitID;
		MouseData.Flags = MOUSE_MOVE_RELATIVE;
		MouseData.ButtonFlags = MOUSE_LEFT_BUTTON_DOWN;
		MouseData.ButtonData = 0;
		MouseData.Buttons = MouseData.ButtonFlags;
		MouseData.LastX = 0;
		MouseData.LastY = 0;
		MouseData.ExtraInformation = 0;
		PMouseData = &MouseData;
		MouFilter_ServiceCallback(WdfDeviceWdmGetDeviceObject(hDevice), PMouseData, PMouseData + 1, what);
	}


	break;

	case IOCTL_MOUFILTR_HOLD_RIGHT:
	{
		MOUSE_INPUT_DATA MouseData;
		PSHORT location = NULL;
		ULONG huh = 0;
		PULONG what = &huh;

		//
		// UpperConnectData must be called at DISPATCH
		//

		status = WdfRequestRetrieveInputBuffer(Request, 1, &location, NULL);

		if (!NT_SUCCESS(status)) {
			DebugPrint(("Error! Code:  %x\n", status));
			break;
		}

		PMOUSE_INPUT_DATA PMouseData = &MouseData;

		MouseData.UnitId = UnitID;
		MouseData.Flags = MOUSE_MOVE_RELATIVE;
		MouseData.ButtonFlags = MOUSE_RIGHT_BUTTON_DOWN;
		MouseData.ButtonData = 0;
		MouseData.Buttons = MouseData.ButtonFlags;
		MouseData.LastX = 0;
		MouseData.LastY = 0;
		MouseData.ExtraInformation = 0;

		PMouseData = &MouseData;
		MouFilter_ServiceCallback(WdfDeviceWdmGetDeviceObject(hDevice), PMouseData, PMouseData + 1, what);
	}


	break;

	case IOCTL_MOUFILTR_RELEASE_LEFT:
	{
		MOUSE_INPUT_DATA MouseData;
		PSHORT location = NULL;
		ULONG huh = 0;
		PULONG what = &huh;

		//
		// UpperConnectData must be called at DISPATCH
		//

		status = WdfRequestRetrieveInputBuffer(Request, 1, &location, NULL);

		if (!NT_SUCCESS(status)) {
			DebugPrint(("Error! Code:  %x\n", status));
			break;
		}

		PMOUSE_INPUT_DATA PMouseData = &MouseData;

		MouseData.UnitId = UnitID;
		MouseData.Flags = MOUSE_MOVE_RELATIVE;
		MouseData.ButtonFlags = MOUSE_LEFT_BUTTON_UP;
		MouseData.ButtonData = 0;
		MouseData.Buttons = MouseData.ButtonFlags;
		MouseData.LastX = 0;
		MouseData.LastY = 0;
		MouseData.ExtraInformation = 0;

		PMouseData = &MouseData;
		MouFilter_ServiceCallback(WdfDeviceWdmGetDeviceObject(hDevice), PMouseData, PMouseData + 1, what);
	}


	break;

	case IOCTL_MOUFILTR_RELEASE_RIGHT:
	{
		MOUSE_INPUT_DATA MouseData;
		PSHORT location = NULL;
		ULONG huh = 0;
		PULONG what = &huh;

		//
		// UpperConnectData must be called at DISPATCH
		//

		status = WdfRequestRetrieveInputBuffer(Request, 1, &location, NULL);

		if (!NT_SUCCESS(status)) {
			DebugPrint(("Error! Code:  %x\n", status));
			break;
		}


		PMOUSE_INPUT_DATA PMouseData = &MouseData;

		MouseData.UnitId = UnitID;
		MouseData.Flags = MOUSE_MOVE_RELATIVE;
		MouseData.ButtonFlags = MOUSE_RIGHT_BUTTON_UP;
		MouseData.ButtonData = 0;
		MouseData.Buttons = MouseData.ButtonFlags;
		MouseData.LastX = 0;
		MouseData.LastY = 0;
		MouseData.ExtraInformation = 0;

		PMouseData = &MouseData;
		MouFilter_ServiceCallback(WdfDeviceWdmGetDeviceObject(hDevice), PMouseData, PMouseData + 1, what);
	}


	break;

	case IOCTL_MOUFILTR_SCROLLUP:
	{
		MOUSE_INPUT_DATA MouseData;
		PSHORT location = NULL;
		ULONG huh = 0;
		PULONG what = &huh;

		//
		// UpperConnectData must be called at DISPATCH
		//

		status = WdfRequestRetrieveInputBuffer(Request, 1, &location, NULL);

		if (!NT_SUCCESS(status)) {
			DebugPrint(("Error! Code:  %x\n", status));
			break;
		}


		PMOUSE_INPUT_DATA PMouseData = &MouseData;

		MouseData.UnitId = UnitID;
		MouseData.Flags = MOUSE_MOVE_RELATIVE;
		MouseData.ButtonFlags = MOUSE_WHEEL;
		MouseData.ButtonData = 120;
		MouseData.Buttons = MouseData.ButtonFlags;
		MouseData.LastX = 0;
		MouseData.LastY = 0;
		MouseData.ExtraInformation = 0;

		PMouseData = &MouseData;
		MouFilter_ServiceCallback(WdfDeviceWdmGetDeviceObject(hDevice), PMouseData, PMouseData + 1, what);
	}


	break;

	case IOCTL_MOUFILTR_SCROLLDOWN:
	{
		MOUSE_INPUT_DATA MouseData;
		PSHORT location = NULL;
		ULONG huh = 0;
		PULONG what = &huh;

		//
		// UpperConnectData must be called at DISPATCH
		//

		status = WdfRequestRetrieveInputBuffer(Request, 1, &location, NULL);

		if (!NT_SUCCESS(status)) {
			DebugPrint(("Error! Code:  %x\n", status));
			break;
		}

		DebugPrint(("gucci lmao\n"));
		PMOUSE_INPUT_DATA PMouseData = &MouseData;

		MouseData.UnitId = UnitID;
		MouseData.Flags = MOUSE_MOVE_RELATIVE;
		MouseData.Buttons = MouseData.ButtonFlags;
		MouseData.ButtonFlags = MOUSE_WHEEL;
		MouseData.ButtonData = 65416;
		MouseData.LastX = 0;
		MouseData.LastY = 0;
		MouseData.ExtraInformation = 0;

		PMouseData = &MouseData;
		MouFilter_ServiceCallback(WdfDeviceWdmGetDeviceObject(hDevice), PMouseData, PMouseData + 1, what);
	}


	break;

	default:
		status = STATUS_NOT_IMPLEMENTED;
		break;
	}

	WdfRequestCompleteWithInformation(Request, status, bytesTransferred);

	return;
}

VOID
MouFilter_DispatchPassThrough(
    _In_ WDFREQUEST Request,
    _In_ WDFIOTARGET Target
    )
/*++
Routine Description:

    Passes a request on to the lower driver.


--*/
{
    //
    // Pass the IRP to the target
    //
 
    WDF_REQUEST_SEND_OPTIONS options;
    BOOLEAN ret;
    NTSTATUS status = STATUS_SUCCESS;

    //
    // We are not interested in post processing the IRP so 
    // fire and forget.
    //
    WDF_REQUEST_SEND_OPTIONS_INIT(&options,
                                  WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET);

    ret = WdfRequestSend(Request, Target, &options);

    if (ret == FALSE) {
        status = WdfRequestGetStatus (Request);
        DebugPrint( ("WdfRequestSend failed: 0x%x\n", status));
        WdfRequestComplete(Request, status);
    }

    return;
}           

VOID
MouFilter_EvtIoInternalDeviceControl(
    IN WDFQUEUE      Queue,
    IN WDFREQUEST    Request,
    IN size_t        OutputBufferLength,
    IN size_t        InputBufferLength,
    IN ULONG         IoControlCode
    )
/*++

Routine Description:

    This routine is the dispatch routine for internal device control requests.
    There are two specific control codes that are of interest:
    
    IOCTL_INTERNAL_MOUSE_CONNECT:
        Store the old context and function pointer and replace it with our own.
        This makes life much simpler than intercepting IRPs sent by the RIT and
        modifying them on the way back up.
                                      
    IOCTL_INTERNAL_I8042_HOOK_MOUSE:
        Add in the necessary function pointers and context values so that we can
        alter how the ps/2 mouse is initialized.
                                            
    NOTE:  Handling IOCTL_INTERNAL_I8042_HOOK_MOUSE is *NOT* necessary if 
           all you want to do is filter MOUSE_INPUT_DATAs.  You can remove
           the handling code and all related device extension fields and 
           functions to conserve space.
                                         

--*/
{
    
    PDEVICE_EXTENSION           devExt;
    PCONNECT_DATA               connectData;
    PINTERNAL_I8042_HOOK_MOUSE  hookMouse;
    NTSTATUS                   status = STATUS_SUCCESS;
    WDFDEVICE                 hDevice;
    size_t                           length; 

    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);

    PAGED_CODE();

    hDevice = WdfIoQueueGetDevice(Queue);
    devExt = FilterGetData(hDevice);

    switch (IoControlCode) {

    //
    // Connect a mouse class device driver to the port driver.
    //
    case IOCTL_INTERNAL_MOUSE_CONNECT:
        //
        // Only allow one connection.
        //
        if (devExt->UpperConnectData.ClassService != NULL) {
            status = STATUS_SHARING_VIOLATION;
            break;
        }
        
        //
        // Copy the connection parameters to the device extension.
        //
         status = WdfRequestRetrieveInputBuffer(Request,
                            sizeof(CONNECT_DATA),
                            &connectData,
                            &length);
        if(!NT_SUCCESS(status)){
            DebugPrint(("WdfRequestRetrieveInputBuffer failed %x\n", status));
            break;
        }

        
        devExt->UpperConnectData = *connectData;

        //
        // Hook into the report chain.  Everytime a mouse packet is reported to
        // the system, MouFilter_ServiceCallback will be called
        //
        connectData->ClassDeviceObject = WdfDeviceWdmGetDeviceObject(hDevice);
        connectData->ClassService = MouFilter_ServiceCallback;

        break;

    //
    // Disconnect a mouse class device driver from the port driver.
    //
    case IOCTL_INTERNAL_MOUSE_DISCONNECT:

        //
        // Clear the connection parameters in the device extension.
        //
        // devExt->UpperConnectData.ClassDeviceObject = NULL;
        // devExt->UpperConnectData.ClassService = NULL;

        status = STATUS_NOT_IMPLEMENTED;
        break;

    //
    // Attach this driver to the initialization and byte processing of the 
    // i8042 (ie PS/2) mouse.  This is only necessary if you want to do PS/2
    // specific functions, otherwise hooking the CONNECT_DATA is sufficient
    //
    case IOCTL_INTERNAL_I8042_HOOK_MOUSE:   

          DebugPrint(("hook mouse received!\n"));
        
        // Get the input buffer from the request
        // (Parameters.DeviceIoControl.Type3InputBuffer)
        //
        status = WdfRequestRetrieveInputBuffer(Request,
                            sizeof(INTERNAL_I8042_HOOK_MOUSE),
                            &hookMouse,
                            &length);
        if(!NT_SUCCESS(status)){
            DebugPrint(("WdfRequestRetrieveInputBuffer failed %x\n", status));
            break;
        }
      
        //
        // Set isr routine and context and record any values from above this driver
        //
        devExt->UpperContext = hookMouse->Context;
        hookMouse->Context = (PVOID) devExt;

        if (hookMouse->IsrRoutine) {
            devExt->UpperIsrHook = hookMouse->IsrRoutine;
        }
        hookMouse->IsrRoutine = (PI8042_MOUSE_ISR) MouFilter_IsrHook;

        //
        // Store all of the other functions we might need in the future
        //
        devExt->IsrWritePort = hookMouse->IsrWritePort;
        devExt->CallContext = hookMouse->CallContext;
        devExt->QueueMousePacket = hookMouse->QueueMousePacket;

        status = STATUS_SUCCESS;
        break;

    //
    // Might want to capture this in the future.  For now, then pass it down
    // the stack.  These queries must be successful for the RIT to communicate
    // with the mouse.
    //
    case IOCTL_MOUSE_QUERY_ATTRIBUTES:
    default:
        break;
    }

    if (!NT_SUCCESS(status)) {
        WdfRequestComplete(Request, status);
        return ;
    }

    MouFilter_DispatchPassThrough(Request,WdfDeviceGetIoTarget(hDevice));
}


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
)
/*++

Remarks:
    i8042prt specific code, if you are writing a packet only filter driver, you
    can remove this function

Arguments:

    DeviceExtension - Our context passed during IOCTL_INTERNAL_I8042_HOOK_MOUSE
    
    CurrentInput - Current input packet being formulated by processing all the
                    interrupts

    CurrentOutput - Current list of bytes being written to the mouse or the
                    i8042 port.
                    
    StatusByte    - Byte read from I/O port 60 when the interrupt occurred                                            
    
    DataByte      - Byte read from I/O port 64 when the interrupt occurred. 
                    This value can be modified and i8042prt will use this value
                    if ContinueProcessing is TRUE

    ContinueProcessing - If TRUE, i8042prt will proceed with normal processing of
                         the interrupt.  If FALSE, i8042prt will return from the
                         interrupt after this function returns.  Also, if FALSE,
                         it is this functions responsibilityt to report the input
                         packet via the function provided in the hook IOCTL or via
                         queueing a DPC within this driver and calling the
                         service callback function acquired from the connect IOCTL
                                             
Return Value:

    Status is returned.

  --+*/
{
    PDEVICE_EXTENSION   devExt;
    BOOLEAN             retVal = TRUE;

    devExt = DeviceExtension;
    if (devExt->UpperIsrHook) {
        retVal = (*devExt->UpperIsrHook) (devExt->UpperContext,
                            CurrentInput,
                            CurrentOutput,
                            StatusByte,
                            DataByte,
                            ContinueProcessing,
                            MouseState,
                            ResetSubState
            );

        if (!retVal || !(*ContinueProcessing)) {
            return retVal;
        }
    }

    *ContinueProcessing = TRUE;
    return retVal;
}

    

VOID
MouFilter_ServiceCallback(
    IN PDEVICE_OBJECT DeviceObject,
    IN PMOUSE_INPUT_DATA InputDataStart,
    IN PMOUSE_INPUT_DATA InputDataEnd,
    IN OUT PULONG InputDataConsumed
    )
/*++

Routine Description:

    Called when there are mouse packets to report to the RIT.  You can do 
    anything you like to the packets.  For instance:
    
    o Drop a packet altogether
    o Mutate the contents of a packet 
    o Insert packets into the stream 
                    
Arguments:

    DeviceObject - Context passed during the connect IOCTL
    
    InputDataStart - First packet to be reported
    
    InputDataEnd - One past the last packet to be reported.  Total number of
                   packets is equal to InputDataEnd - InputDataStart
    
    InputDataConsumed - Set to the total number of packets consumed by the RIT
                        (via the function pointer we replaced in the connect
                        IOCTL)

Return Value:

    Status is returned.

--*/
{
    PDEVICE_EXTENSION   devExt;
    WDFDEVICE   hDevice;
	NTSTATUS FloatStatus;
	XSTATE_SAVE SaveState;
    hDevice = WdfWdmDeviceGetWdfDeviceHandle(DeviceObject);
    devExt = FilterGetData(hDevice);
    //
    // UpperConnectData must be called at DISPATCH
    //
	DbgPrint("hey");
	FloatStatus = KeSaveExtendedProcessorState(XSTATE_MASK_LEGACY, &SaveState);
	if (NT_SUCCESS(FloatStatus)){
		//double dx = 65535.0 / 1024.0; //dy = 65535.0 / 768.0;
		for (ULONG i = 0; i < (ULONG)(InputDataEnd - InputDataStart); i++){
			//double x = ((InputDataStart)->LastX / dx);// y = ((InputDataStart)->LastY / dy);
			//InputDataStart->Buttons;
			
		}
		KeRestoreExtendedProcessorState(&SaveState);
	}
	DbgPrint("ButtonData: %d\n", InputDataStart->ButtonData);
	/*for (ULONG i = 0; i < (ULONG)(InputDataEnd - InputDataStart); i++){
		KdPrint(("0 %d 1 %d",
			i, (InputDataStart + i)->LastY));

	}*/
	
    (*(PSERVICE_CALLBACK_ROUTINE) devExt->UpperConnectData.ClassService)(
        devExt->UpperConnectData.ClassDeviceObject,
        InputDataStart,
        InputDataEnd,
        InputDataConsumed
        );
} 

#pragma warning(pop)
