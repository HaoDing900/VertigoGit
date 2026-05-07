/*
* Inventory System X
*
* Copyright (C) 2023-2024 Mykhailo Oliynik <m19tes@gmail.com> All Rights Reserved.
*/

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Containers/Ticker.h"
#include "InputCoreTypes.h"
#include "Misc/DataDrivenPlatformInfoRegistry.h"
#include "Input_ISX_Subsystem.generated.h"








class FISX_InputPreprocessor;

/**
 * 
 */

UENUM(BlueprintType)
enum class EInputType_ISX : uint8
{
	MouseAndKeyboard,
	Gamepad,
	Touch,
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInputMethodChanged, const EInputType_ISX, InputMethod);

UCLASS()
class INPUT_ISX_API UInput_ISX_Subsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	static UInput_ISX_Subsystem* Get(const ULocalPlayer* LocalPlayer);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	
	UPROPERTY(BlueprintAssignable, Category="Input")
	FOnInputMethodChanged OnInputMethodChanged;

	static bool IsMobileGamepadKey(const FKey& InKey);

	void SetCurrentInputType(EInputType_ISX NewInputType);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Input")
	FORCEINLINE EInputType_ISX GetCurrentInputType() const { return CurrentInputType; }

private:
	void BroadcastInputMethodChanged() const;

	UPROPERTY(Transient)
	EInputType_ISX CurrentInputType;

	TSharedPtr<class FISX_InputPreprocessor> InputPreprocessor;
};
