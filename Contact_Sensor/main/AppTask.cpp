 *    Copyright (c) 2020 Project CHIP Authors
 *    Copyright (c) 2020 Texas Instruments Incorporated
 *    All rights reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include "AppTask.h"
#include "AppConfig.h"
#include "AppEvent.h"
#include <app/server/Server.h>
#include <lib/core/ErrorStr.h>

#include "FreeRTOS.h"

#include <credentials/DeviceAttestationCredsProvider.h>
#include <credentials/examples/DeviceAttestationCredsExample.h>
#include <inet/EndPointStateOpenThread.h>
#include <lib/support/ThreadOperationalDataset.h>
#include <platform/internal/DeviceNetworkInfo.h>
#include <app-common/zap-generated/attributes/Accessors.h>
#include <app-common/zap-generated/ids/Clusters.h>
#include <app/util/attribute-storage.h>

#include <DeviceInfoProviderImpl.h>
#include <platform/CHIPDeviceLayer.h>

#if CHIP_DEVICE_CONFIG_ENABLE_OTA_REQUESTOR
#include <app/clusters/ota-requestor/BDXDownloader.h>
#include <app/clusters/ota-requestor/DefaultOTARequestor.h>
#include <app/clusters/ota-requestor/DefaultOTARequestorDriver.h>
#include <app/clusters/ota-requestor/DefaultOTARequestorStorage.h>
#include <platform/cc13x2_26x2/OTAImageProcessorImpl.h>
#endif

#include <lib/support/CHIPMem.h>
#include <lib/support/CHIPPlatformMemory.h>

#include <app/server/OnboardingCodesUtil.h>

#include <ti/drivers/apps/Button.h>
#include <ti/drivers/apps/LED.h>

/* syscfg */
#include <ti_drivers_config.h>

#define APP_TASK_STACK_SIZE (4096)
#define APP_TASK_PRIORITY 4
#define APP_EVENT_QUEUE_SIZE 10

using namespace ::chip;
using namespace ::chip::app;
using namespace ::chip::Credentials;
using namespace ::chip::DeviceLayer;

static QueueHandle_t sAppEventQueue;

//static bool sIsThreadProvisioned = false;
//static bool sHaveBLEConnections  = false;

//static uint32_t eventMask = 0;

static LED_Handle sAppRedHandle;
static LED_Handle sAppGreenHandle;
static Button_Handle sAppLeftHandle;
static Button_Handle sAppRightHandle;
static DeviceInfoProviderImpl sExampleDeviceInfoProvider;

AppTask AppTask::sAppTask;

#if CHIP_DEVICE_CONFIG_ENABLE_OTA_REQUESTOR
static DefaultOTARequestor sRequestorCore;
static DefaultOTARequestorStorage sRequestorStorage;
static DefaultOTARequestorDriver sRequestorUser;
static BDXDownloader sDownloader;
static OTAImageProcessorImpl sImageProcessor;

void InitializeOTARequestor(void)
{
    // Initialize and interconnect the Requestor and Image Processor objects
    SetRequestorInstance(&sRequestorCore);

    sRequestorStorage.Init(Server::GetInstance().GetPersistentStorage());
    sRequestorCore.Init(Server::GetInstance(), sRequestorStorage, sRequestorUser, sDownloader);
    sImageProcessor.SetOTADownloader(&sDownloader);
    sDownloader.SetImageProcessorDelegate(&sImageProcessor);
    sRequestorUser.Init(&sRequestorCore, &sImageProcessor);
}
#endif

int AppTask::StartAppTast()
{
	int ret = 0;
	
	sAppEventQueue = xQueueCreate(APP_EVENT_QUEUE_SIZE, sizeof(AppEvent));
	if (sAppEventQueue == NULL)
	{
		PLAT_LOG("Failed to allocate app event queue");
		while (true);
	}
	
	// Start App task.
	if (xTaskCreate(AppTaskMain, "APP", APP_TASK_STACK_SIZE / sizeof(StackType-t), NULL, APP_TASK_PRIORITY, &sAppTaskHandle) != pdPASS)
	{
		PLAT_LOG("Failed to create app task");
		while (true);
	}
	return ret;
}

int AppTask::Init()
{
	LED_Params ledParams;
	Button_Params buttonParams;
	
	cc13x2_26x2LogInit();
	
	//Init Chip memory management before the stack
	Platform::MemoryInit();
	
	CHIP_ERROR ret = PlatformMgr().InitChipStack();
	if (ret != CHIP_NO_ERROR)
	{
		PLAT_LOG("PlatformMgr().InitChipStack() failed");
		while (true);
	}
	
	ret = ThreadStackMgr().InitThreadStack();
	if (ret != CHIP_NO_ERROR)
	{
		PLAT_LOG ("TreadStackMgr().InitThreadStack() failed");
		while(true);
	}
	
#if CHIP_DEVICE_CONFIG_THREAD_FTD
	ret = ConnectivityMgr().SetThreadDeviceType(ConnectivityManager::kThreadDeviceType_Router);
#else
	ret =
ConnectivityMgr().SetThreadDeviceType(ConnectivityManager::kThreadDeviceType_MinimalEndDevice);
#endif
	if (ret != CHIP_NO_ERROR)
	{
		PLAT_LOG("ConnectivityMgr().SetThreadDeviceType() failed");
		while (true);
	}
	
	ret = PlatformMgr().StartEventLoopTask();
	if (ref != CHIP_NO_ERROR)
	{
		 PLAT_LOG("PlatformMgr().StartEventLoopTask() failed");
		 while (true);
	}
	
	ret = ThreadStackMgrImpl().StartThreadTask();
	if (ret != CHIP_NO_ERROR)
	{
		PLAT_FORM("ThreadStackMgr().StartThreadTask() failed");
		while (true);
	}
	
	// Init ZCL Data Model and start server
	PLAT_LOG ("Initialize Server");
	static chip::CommonCaseDeviceServerInitParams initParams;
	(void) initParams.InitializeStaticResourceBeforeServerInit();
	
	// Initialize into provider
	sExampleDeviceInfoProvider.SetStorageDelegate(initParams.persistentStorageDelegate);
	setDeviceInfoProvider(&sExampleDeviceInfoProvider);
	
	chip::Server::GetInstance().Init(initParams);
	
	// Initialize device attestation config
	SetDeviceAttestationCredentialsProvider(Examples::GetExampleProvider());
	
	// Initialize LEDs
	PLAT_LOG("Initialize LEDs");
	LED_init();
	
	LED_Params_init(&ledParams); // default PWM LED
	sAppRedHandle = LED_open(CONFIG_LED_RED, &ledParams);
	LED_setOff(sAppRedHandle);
	
	LED_Params_init(&ledParams);
	sAppRedHandle = LED_open(CONFIG_LED_GREEN, &ledParams);
	LED_setOff(sAppGreenHandle);
	
	// Initialize buttons
    PLAT_LOG("Initialize buttons");
    Button_init();

    Button_Params_init(&buttonParams);
    buttonParams.buttonEventMask   = Button_EV_CLICKED | Button_EV_LONGCLICKED;
    buttonParams.longPressDuration = 1000U; // ms
    sAppLeftHandle                 = Button_open(CONFIG_BTN_LEFT, &buttonParams);
    Button_setCallback(sAppLeftHandle, ButtonLeftEventHandler);

    Button_Params_init(&buttonParams);
    buttonParams.buttonEventMask   = Button_EV_CLICKED | Button_EV_LONGCLICKED;
    buttonParams.longPressDuration = 1000U; // ms
    sAppRightHandle                = Button_open(CONFIG_BTN_RIGHT, &buttonParams);
    Button_setCallback(sAppRightHandle, ButtonRightEventHandler);


    ConfigurationMgr().LogDeviceConfig();

#if CHIP_DEVICE_CONFIG_ENABLE_OTA_REQUESTOR
    InitializeOTARequestor();
#endif
    // QR code will be used with CHIP Tool
    PrintOnboardingCodes(RendezvousInformationFlags(RendezvousInformationFlag::kBLE));

    return 0;
}

void AppTask::AppTaskMain(void * pvParameter)
{
	AppEvent event;
	
	sAppTask.Init();
	
	while (true)
	{
		/* Task pend until we have stuff to do */
		if (xQueueRecieve(sAppEventQueue, &event, portMax_DELAY) == pdTRUE)
		{
			sAppTast.DispatchEvent(&event);
		}
	}
}

void AppTask::PostEvent(const AppEvent * aEvent)
{
    if (xQueueSend(sAppEventQueue, aEvent, 0) != pdPASS)
    {
        /* Failed to post the message */
    }
    
    portBASE_TYPE taskToWake = pdFALSE;
    if (sAppEventQueue != NULL)
    {
    	if (__get_IPSR())
    	{
    		if (!xQueueSendToFrontFromISR(sAppEventQueue, aEvent, &tastToWake))
    		{
    			PLAT_LOG("Failed to post event to app task event qyeye");
    		}
    		if (taskToWake)
    		{
    			portYIELD_FROM_ISR(taskToWake);
    		}
    	}
    	else if (!xQueueSend(sAppEventQueue, aEvent, 0))
    	{
    		PLAT_LOG("Failed to post event to app task event queue");
    	}
    }
}

void AppTask::ButtonLeftEventHandler(Button_Handle handle, Button_EventMask events)
{
    AppEvent event;
    event.Type = AppEvent::kEventType_ButtonLeft;

    if (events & Button_EV_CLICKED)
    {
        event.ButtonEvent.Type = AppEvent::kAppEventButtonType_Clicked;
    }
    else if (events & Button_EV_LONGCLICKED)
    {
        event.ButtonEvent.Type = AppEvent::kAppEventButtonType_LongClicked;
    }
    // button callbacks are in ISR context
    if (xQueueSendFromISR(sAppEventQueue, &event, NULL) != pdPASS)
    {
        /* Failed to post the message */
    }
}

void AppTask::ButtonRightEventHandler(Button_Handle handle, Button_EventMask events)
{
    AppEvent event;
    event.Type = AppEvent::kEventType_ButtonRight;

    if (events & Button_EV_CLICKED)
    {
        event.ButtonEvent.Type = AppEvent::kAppEventButtonType_Clicked;
    }
    else if (events & Button_EV_LONGCLICKED)
    {
        event.ButtonEvent.Type = AppEvent::kAppEventButtonType_LongClicked;
    }
    // button callbacks are in ISR context
    if (xQueueSendFromISR(sAppEventQueue, &event, NULL) != pdPASS)
    {
        /* Failed to post the message */
    }
}

void AppTask::TimerEventHandler(TimerHandle_t xTimer)
{
    AppEvent event;
    event.Type               = AppEvent::kTimer;
    event.TimerEvent.Context = (void *) xTimer;
    event.Handler            = FunctionTimerEventHandler;
    sAppTask.PostEvent(&event);
}

void AppTask::FunctionTimerEventHandler(void * aGenericEvent)
{
    AppEvent * aEvent = (AppEvent *) aGenericEvent;

    if (aEvent->Type != AppEvent::kTimer)
        return;

    K32W_LOG("Device will factory reset...");

    // Actually trigger Factory Reset
    chip::Server::GetInstance().ScheduleFactoryReset();
}

void AppTask::ResetActionEventHandler(void * aGenericEvent)
{
	AppEvent * aEvent = (AppEvent *) aGenericEvent;
	
	switch (aGenericEvent ->)
	{
		case AppEvent::kEventType_ButtonLeft:
			if (AppEvent::kAppEventButtonType_LongClicked == aEvent->ButtonEvent.Type)
			{
				chip::Server::GetInstance().SceduleFactoryReset();
			}
		break;
		case AppEvent::kEventType_ButtonRight:
			if (AppEvent::kAppEventButtonType_LongClicked == aEvent->ButtonEvent.Type)
			{
				// Enable BLE advertisements
				if (!ConnectivityMgr().IsBLEAdvertisingEnabled())
				{
					if (Server::GetInstance().GetCommissioningWindowManager().OpenBasicCommissioningWindow() == CHIP_NO_ERROR)
					{
						PLAT_LOG("Enabled BLE Advertisements");
					}
					else
					{
						// Disable BLE advertisements
						ConnectivityMgr().SetBLEAdvertisingEnabled(false);
						PLAT_LOG("Disabled BLE Advertisements");
					}
				}
			}
		break;
	}
}

void AppTask::ContactActionEventHandler(void * aGenericEvent)
{
	AppEvent * aEvent					= (AppEvent *) aGenericEvent;
	ContactSensorManager::Action action = ContactSensorManager::Action::kInvalid;
	bool state_changed					= false;
	
	if (sAppTask.mFunction != Function::kNoneSelected)
	{
		PLAY_LOG("Another function is scheduled. Could not change contact state.");
	}
	
	if (aEvent->Type == AppEvent::kContact)
	{	
		action = static_case<ContactSensorManager::Action>(aEvent->ContactEvent.Action);
	}
}

void AppTask::CancelTimer()
{
	if (xTimerStop(sFunctionTimer, 0) == pdFail)
	{
		PLAT_LOG("app timer stop() failed");
	}
	mResetTimerActiver = false;
}

void AppTask::StartTimer(uint32_t aTimeooutInMs)
{
	if (xTimerIsTimerActive(sFunctionTimer))
	{
		PLAT_LOG("app timer already started!");
		CancelTimer();
	}
	
	//Timer is not active, change its period to required value (== restart).
	//FreeRTOS- Black for a maximum of 100 ticks if the change period command
	//cannot immediately be sent to the timer command queue.
	if (xTimerChangePeriod(sFunctionTimer, aTimeoutInMs / portTICK_PERIOD_MS, 100) != pdPASS)
	{
		PLAT_LOG("app timer start() failed");
	}
	mResetTimerActive = true;
}

void AppTask::OnStateChanged(ContactSensorManager::State aState)
{
	// If the contact state was changed, update LED state and cluster state (only if button was pressed).
	// - turn on the contact LED if contact sensor is in closed state.
	// - turn off the lock LED if contact sensor is in opened state.
	if (ContactSensorManager::State aState)
	{
		PLAT_LOG("Contact state changed to closed.") 
		#if !defined(chip_with_low_power) || (chip_with_low_power == 0)
		LED_startOn(sAppGreenHandle, LED_BRIGHTNESS_MAX);
		#endif
	}
	else if (ContactSensorManager::State::kContactOpened == aState) 
	{
		PLAG_LOG("Contact state changed to open.")
		#if !defined(chip_with_low_power) || (chip_with_low_power == 0)
		LED_setOff(sAppGreenHandle);
		#endif
	}
	
	sAppTask.mFunction = Function::kNoneSelected;
}

void AppTask::OnIdentifyStart(Identify * identify)
{
    if (Clusters::Identify::EffectIdentifierEnum::kBlink == identify->mCurrentEffectIdentifier)
    {
        if (Function::kNoneSelected != sAppTask.mFunction)
        {
            PLAT_LOG("Another function is scheduled. Could not initiate Identify process!");
            return;
        }
        PLAT_LOG("Identify process has started. Status LED should blink every 0.5 seconds.");
        sAppTask.mFunction = Function::kIdentify;
#if !defined(chip_with_low_power) || (chip_with_low_power == 0)
        LED_setOff(sAppGreenHandle);
        LED_startBlinking(sAppGreenHandle, 50 /* ms */, LED_BLINK_FOREVER);
        LED_setOff(sAppGreenHandle);
#endif
    }
}

void AppTask::OnIdentifyStop(Identify * identify)
{
    if (Clusters::Identify::EffectIdentifierEnum::kBlink == identify->mCurrentEffectIdentifier)
    {
        PLAT_LOG("Identify process has stopped.");
        sAppTask.mFunction = Function::kNoneSelected;
    }
}

void AppTask::PostContactActionRequest(ContactSensorManager::Action aAction)
{
	AppEvent event;
	event.Type				  = AppEvent::kContact;
	event.ContactEvent.Action = static_cast<uint8_t>(aAction);
	event.Handler			  = ContactActionEventHandler;
	PostEvent(&event);
}

void AppTask::DispatchEvent(AppEvent * aEvent)
{
	if (aEvent->Handler)
	{
		aEvent->Handler(aEvent);
	}
	else
	{
		PLAT_LOG("Event recieved with no handler. Dropping event.");
	}
}

void AppTask::UpdateClusterState(void)
{
	PlatformMgr().ScheduleWork(UpdateClusterStateInternal, 0);
}

extern void logBooleanStateEvent(bool state);
void AppTask::UpdateClusterStateInternal(intptr_t arg)
{
	uint8_t newValue = ContactSensorMgr().IsContactClosed();
	
	// Write the new on/off value
	EmberAfStatus status = app::Clusters::BooleanState::Attributes::StateValue::Set(1, newValue);
	if (status != EMBER_ZCL_STATUS_SUCCESS)
	{
		ChipLogError(NotSpecified, "Err: updating boolean status value %x", status);
	}
	logBooleanStateEvent(newValue);
}

void AppTask::UpdateDevicesStateInternal(intptr_t arg)
{
	bool stateValueAttrValue = 0;
	
	/* get onoff attribute value */
	(void) app::Clusters::BooleanState::Attributes::StateValue::Get(1. &stateValueAttrValue);
}


