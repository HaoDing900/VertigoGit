#include "UI/VTGDeathScreen.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Engine/Texture2D.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"

int32 VTGDeathBackground(const FString& Params)
{
    auto* BP=LoadObject<UWidgetBlueprint>(nullptr,TEXT("/Game/Blueprints/Widget/WBP_Death"));
    if(!BP || !BP->WidgetTree || !BP->ParentClass->IsChildOf(UVTGDeathScreen::StaticClass())) return 60;
    auto* Root=Cast<UOverlay>(BP->WidgetTree->RootWidget);
    if(!Root) return 61;
    auto* Image=Cast<UImage>(BP->WidgetTree->FindWidget(TEXT("DeathBackgroundImage")));
    const bool Verify=FParse::Param(*Params,TEXT("Verify"));
    if(!Verify)
    {
        if(!Image)
        {
            Image=BP->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(),TEXT("DeathBackgroundImage"));
            Image->bIsVariable=true;
            Image->SetVisibility(ESlateVisibility::HitTestInvisible);
            FSlateBrush Brush; Brush.DrawAs=ESlateBrushDrawType::NoDrawType;
            Image->SetBrush(Brush); // No texture configured yet: retain the original blur.
            auto* Slot=Cast<UOverlaySlot>(Root->InsertChildAt(1,Image));
            if(!Slot) return 62;
            Slot->SetHorizontalAlignment(HAlign_Fill); Slot->SetVerticalAlignment(VAlign_Fill);
        }
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
        FCompilerResultsLog Log; FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::None,&Log);
        if(Log.NumErrors) return 63;
        FSavePackageArgs Args; Args.TopLevelFlags=RF_Public|RF_Standalone;
        const auto Filename=FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
        if(!UPackage::SavePackage(BP->GetOutermost(),BP,*Filename,Args)) return 64;
    }
    if(!Image || Root->GetChildIndex(Image)!=1 || !BP->GeneratedClass) return 65;
    auto* Default=CastChecked<UVTGDeathScreen>(BP->GeneratedClass->GetDefaultObject());
    if(Default->bUseOtherDeathImage) return 66;
    for(const TCHAR* Name:{TEXT("bUseOtherDeathImage"),TEXT("OtherDeathImage")})
    {
        auto* Property=FindFProperty<FProperty>(BP->GeneratedClass,Name);
        if(!Property || !Property->HasAnyPropertyFlags(CPF_ExposeOnSpawn)) return 67;
    }
    auto* Widget=NewObject<UVTGDeathScreen>(GetTransientPackage(),BP->GeneratedClass);
    Widget->Initialize();
    auto* ActualImage=Cast<UImage>(Widget->WidgetTree->FindWidget(TEXT("DeathBackgroundImage")));
    if(!ActualImage || !Widget->WidgetTree->FindWidget(TEXT("RetryButton")) || !Widget->WidgetTree->FindWidget(TEXT("MainMenuButton"))) return 68;
    auto* A=NewObject<UTexture2D>(); auto* B=NewObject<UTexture2D>();
    Widget->DefaultDeathImage=A;
    Widget->SetDeathBackground(false,B);
    if(ActualImage->GetBrush().GetResourceObject()!=A) return 69;
    Widget->SetDeathBackground(true,B);
    if(ActualImage->GetBrush().GetResourceObject()!=B) return 70;
    Widget->SetDeathBackground(true,nullptr);
    if(ActualImage->GetBrush().GetResourceObject()!=A) return 71;
    Widget->SetDeathBackground(false,nullptr);
    if(ActualImage->GetBrush().GetResourceObject()!=A) return 72;
    Widget->DefaultDeathImage=nullptr;
    Widget->SetDeathBackground(false,nullptr);
    if(ActualImage->GetBrush().DrawAs!=ESlateBrushDrawType::NoDrawType) return 73;
    UE_LOG(LogTemp,Display,TEXT("DEATH BACKGROUND PASS: default-off, spawn pins, override, empty fallback, reused widget reset, original blur fallback and button preservation"));
    return 0;
}
