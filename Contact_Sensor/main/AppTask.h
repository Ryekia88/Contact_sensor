/*
 *    Copyright (c) 2020 Project CHIP Authors
 *    Copyright (c) 2019 Google LLC.
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
 
 #ifndef APP_TASK_H
 #define APP_TASK_H
 
 #include <stdbool.h>
 #include <stdint.h>
 
 #include "AppEvent.h"
 #include "ContactSensorManager.h"
 
 #include "CHIPProjectConfig.h"
 
 #include "FreeRTOS.h"
 #include "semphr.h"
 #include "task.h"
 
 #include <ti/drivers/apps/Button.h>
 
#include <app/clusters/identify-server/identify-server.h>
#include "timers.h"


class AppTask
{
	public:
		int StartAppTask();
		static void AppTaskMain(void * pvParameter);
		
		void PostContactActionRequest(ContactSensorManager::Action aAction);
		void PostEvent(const AppEvent * event);
		
		void UpdateClusterState(void);
		void UpdateDeviceState(void);
		
		static void OnIdentifyStart(Identify * identify);
		static void OnIdentifyStop(Identify * identify);
		
	private:
		friend AppTask & GetAppTask(void);
		
		int Init();
		
		static void OnStateChanged(ContactSensorManager::State aState);
		
		void CancelTimer(void);
		
		void DispatchEvent(AppEvent * event);
		
		static void ButtonLeftEventHandler(Button_Handle handle, Button_EventMask events);
    	static void ButtonRightEventHandler(Button_Handle handle, Button_EventMask events);
 
 		static void FunctionTimerEventHandler(void * aGenericEvent);
 
		static void ContactActionEventHandler(void * aGenericEvent);
		static void ResetActionEventHandler(void * aGenericEvent);
		static void InstallEventHandler(void * aGenericEvent);
 
   	    static void MatterEventHandler(const chip::DeviceLayer::ChipDeviceEvent * event, intptr_t arg);
    	void StartTimer(uint32_t aTimeoutInMs);
 
		static void UpdateClusterStateInternal(intptr_t arg);
		static void UpdateDeviceStateInternal(intptr_t arg);
		static void InitServer(intptr_t arg);
		static void PrintOnboardingInfo();
 
		enum class Function : uint8_t
		{
			kNoneSelected = 0,
			kFactoryReset,
			Kcontact,
			kIdentify,
			kInvalid,
		};
		
		Function mFunction				= Function::kNoneSelected;
		bool mResetTimerActive			= false;
		bool mSyncClusterToButtonAction = false;
		
		static AppTask sAppTask;
};

inline AppTask & GetAppTask(void)
{
	return AppTask::sAppTask;
}

#endif // APP_TASK_H
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
