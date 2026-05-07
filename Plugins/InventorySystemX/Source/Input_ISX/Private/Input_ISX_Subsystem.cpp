/*
* Inventory System X
*
* Copyright (C) 2023-2024 Mykhailo Oliynik <m19tes@gmail.com> All Rights Reserved.
*/

#include "../Public/Input_ISX_Subsystem.h"

#include "PlatformSettings_ISX.h"
#include "Engine/Engine.h"
#include "Stats/Stats.h"
#include "Engine/LocalPlayer.h"
#include "Engine/PlatformSettings.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateUser.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/ConfigCacheIni.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "HAL/IConsoleManager.h"
#include "Containers/Ticker.h"
#include "GenericPlatform/GenericPlatformTime.h"
#include "Stats/Stats.h"


class FISX_InputPreprocessor : public IInputProcessor
{
public:
	FISX_InputPreprocessor(UInput_ISX_Subsystem& InInputSubsystem_ISX)
		: InputSubsystem(InInputSubsystem_ISX)
	{
	}

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override
	{
	}


	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
	{
		const EInputType_ISX InputType = GetInputType(InKeyEvent.GetKey());
		if (IsRelevantInput(SlateApp, InKeyEvent, InputType))
		{
			RefreshCurrentInputMethod(InputType);
		}
		return false;
	}


	virtual bool HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override
	{
		const EInputType_ISX InputType = GetInputType(MouseEvent);

		if (IsRelevantInput(SlateApp, MouseEvent, InputType))
		{
			if (!MouseEvent.GetCursorDelta().IsNearlyZero(0.1))
			{
				RefreshCurrentInputMethod(InputType);
			}
		}
		return false;
	}

	bool IsRelevantInput(const FSlateApplication& SlateApp, const FInputEvent& InputEvent,
	                     const EInputType_ISX DesiredInputType) const
	{
		if (SlateApp.IsActive()
			|| SlateApp.GetHandleDeviceInputWhenApplicationNotActive()
			|| (DesiredInputType == EInputType_ISX::Gamepad))
		{
			const ULocalPlayer& LocalPlayer = *InputSubsystem.GetLocalPlayerChecked();
			int32 ControllerId = LocalPlayer.GetControllerId();

#if WITH_EDITOR
			// PIE is a very special flower, especially when running two clients - we have two LocalPlayers with ControllerId 0
			// The editor has existing shenanigans for handling this scenario, so we're using those for now
			// Ultimately this would ideally be something the editor resolves at the SlateApplication level with a custom ISlateInputMapping or something
			GEngine->RemapGamepadControllerIdForPIE(LocalPlayer.ViewportClient, ControllerId);
#endif
			return ControllerId == InputEvent.GetUserIndex();
		}
		return false;
	}

	void RefreshCurrentInputMethod(const EInputType_ISX InputType) const
	{
		if (IsValid(&InputSubsystem))
			InputSubsystem.SetCurrentInputType(InputType);
	}

	static EInputType_ISX GetInputType(const FPointerEvent& PointerEvent)
	{
		if (PointerEvent.IsTouchEvent())
		{
			return EInputType_ISX::Touch;
		}
		return EInputType_ISX::MouseAndKeyboard;
	}

	
	
	static EInputType_ISX GetInputType(const FKey& Key)
	{
		if (Key.IsGamepadKey())
		{
			if (UInput_ISX_Subsystem::IsMobileGamepadKey(Key))
			{
				return EInputType_ISX::Touch;
			}
			return EInputType_ISX::Gamepad;
		}
		return EInputType_ISX::MouseAndKeyboard;
	}



private:
	UInput_ISX_Subsystem& InputSubsystem;
};


UInput_ISX_Subsystem* UInput_ISX_Subsystem::Get(const ULocalPlayer* LocalPlayer)
{
	return LocalPlayer ? LocalPlayer->GetSubsystem<UInput_ISX_Subsystem>() : nullptr;
}

void UInput_ISX_Subsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UPlatformSettings_ISX* Settings = UPlatformSettingsManager::Get().GetSettingsForPlatform<UPlatformSettings_ISX>();
	CurrentInputType = Settings->GetDefaultInputType();
	
	InputPreprocessor = MakeShared<FISX_InputPreprocessor>(*this);
	FSlateApplication::Get().RegisterInputPreProcessor(InputPreprocessor, 0);
}

void UInput_ISX_Subsystem::Deinitialize()
{
	Super::Deinitialize();

	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(InputPreprocessor);
	}
	InputPreprocessor.Reset();
}



void UInput_ISX_Subsystem::BroadcastInputMethodChanged() const
{
	if (UWorld* World = GetWorld())
	{
		if (!World->bIsTearingDown)
		{
			OnInputMethodChanged.Broadcast(CurrentInputType);
		}
	}
}

 bool UInput_ISX_Subsystem::IsMobileGamepadKey(const FKey& InKey)
{
	// Mobile keys that can be physically present on the device
	static TArray<FKey> PhysicalMobileKeys = {
		EKeys::Android_Back,
		EKeys::Android_Menu,
		EKeys::Android_Volume_Down,
		EKeys::Android_Volume_Up
	};

	return PhysicalMobileKeys.Contains(InKey);
}

void UInput_ISX_Subsystem::SetCurrentInputType(EInputType_ISX NewInputType)
{
	if (NewInputType != CurrentInputType)
	{
		CurrentInputType = NewInputType;
		/*FSlateApplication& SlateApplication = FSlateApplication::Get();
		ULocalPlayer* LocalPlayer = GetLocalPlayerChecked();
		bool bCursorUser = LocalPlayer && LocalPlayer->GetSlateUser() == SlateApplication.GetCursorUser();

		switch (CurrentInputType)
		{
		case EInputType_ISX::Gamepad:
			if (bCursorUser)
			{
				SlateApplication.UsePlatformCursorForCursorUser(false);
			}
			break;
		case EInputType_ISX::Touch:
			break;
		case EInputType_ISX::MouseAndKeyboard:
		default:
			if (bCursorUser)
			{
				SlateApplication.UsePlatformCursorForCursorUser(true);
			}
			break;
		}*/
		BroadcastInputMethodChanged();
	}
}
