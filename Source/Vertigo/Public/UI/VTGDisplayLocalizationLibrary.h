#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VTGDisplayLocalizationLibrary.generated.h"
class UDataTable;
UCLASS()
class VERTIGO_API UVTGDisplayLocalizationLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Legacy sender names remain identifiers; only the returned FText is translated. */
    UFUNCTION(BlueprintPure,Category="Localization")
    static FText GetMailSenderText(const UDataTable* Table,FName SenderID);
};
