#include "VTGPlaneSlideshowCommandlet.h"
#include "UI/VTGPlaneSlideshow.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"

namespace PlaneSlideshow
{
const TCHAR* Folder = TEXT("/Game/Movie/TVshow/PlaneSlideshow/");
bool Save(UObject* Asset)
{
    FSavePackageArgs Args;
    Args.TopLevelFlags = RF_Public | RF_Standalone;
    Args.SaveFlags = SAVE_NoError;
    const FString Filename = FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
    return UPackage::SavePackage(Asset->GetOutermost(), Asset, *Filename, Args);
}
template<class T> T* Expression(UMaterial* Material, int32 X, int32 Y)
{
    T* Node = NewObject<T>(Material);
    Node->MaterialExpressionEditorX = X;
    Node->MaterialExpressionEditorY = Y;
    Material->GetExpressionCollection().AddExpression(Node);
    return Node;
}
bool Verify(UBlueprint* BP)
{
    if (!BP || BP->ParentClass != AVTGPlaneSlideshow::StaticClass()) return false;
    FCompilerResultsLog Log;
    FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::None, &Log);
    if (Log.NumErrors || !BP->GeneratedClass) return false;
    const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
        .CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false, TEXT("PlaneSlideshowTest"), nullptr, true, ERHIFeatureLevel::Num, &Init);
    auto* A = World->SpawnActor<AVTGPlaneSlideshow>(BP->GeneratedClass);
    auto* B = World->SpawnActor<AVTGPlaneSlideshow>(BP->GeneratedClass);
    UTexture2D* One = LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture"));
    UTexture2D* Two = UTexture2D::CreateTransient(2, 2);
    bool Valid = A && B && One && Two;
    if (Valid)
    {
        A->Images = { One, nullptr, Two };
        A->RerunConstructionScripts();
        auto* MID = Cast<UMaterialInstanceDynamic>(A->Screen->GetMaterial(0));
        auto* OtherMID = Cast<UMaterialInstanceDynamic>(B->Screen->GetMaterial(0));
        Valid &= MID && OtherMID && MID != OtherMID && A->CurrentIndex == 0;
        Valid &= !A->IsActorTickEnabled() && A->Screen->GetCollisionEnabled() == ECollisionEnabled::NoCollision;
        A->Play(); A->NextImage();
        Valid &= A->CurrentIndex == 2 && A->IsActorTickEnabled();
        A->Tick(.25f);
        Valid &= MID && FMath::IsNearlyEqual(MID->K2_GetScalarParameterValue(TEXT("Blend")), .5f);
        A->Pause(); Valid &= !A->IsActorTickEnabled();
        A->Play(); Valid &= A->IsActorTickEnabled();
        A->Tick(.25f); Valid &= !A->IsActorTickEnabled();
        A->NextImage(); A->Tick(.5f); Valid &= A->CurrentIndex == 0;
        A->bLoop = false; A->ShowImage(2); A->NextImage(); Valid &= A->CurrentIndex == 2;
        A->ShowImage(1); Valid &= A->CurrentIndex == 2;
        A->bLoop = true; A->FadeSeconds = 0; A->NextImage();
        Valid &= A->CurrentIndex == 0 && !A->IsActorTickEnabled();
        A->Images = { nullptr, Two }; A->Restart(); Valid &= A->CurrentIndex == 1 && !A->IsActorTickEnabled();
        A->Images.Empty(); A->Restart(); Valid &= A->CurrentIndex == INDEX_NONE && !A->Screen->IsVisible();
        Valid &= B->CurrentIndex == INDEX_NONE;
    }
    World->DestroyWorld(false);
    UE_LOG(LogTemp, Display, TEXT("PLANE SLIDESHOW: compile, crossfade, pause/resume, looping, null/empty/single images, instance isolation: %s"), Valid ? TEXT("PASS") : TEXT("FAIL"));
    return Valid;
}
}

UVTGPlaneSlideshowCommandlet::UVTGPlaneSlideshowCommandlet()
{ IsClient = false; IsServer = false; IsEditor = true; LogToConsole = true; }

int32 UVTGPlaneSlideshowCommandlet::Main(const FString& Params)
{
    using namespace PlaneSlideshow;
    const FString MaterialPackage = FString(Folder) + TEXT("M_PlaneSlideshow");
    const FString BPPackage = FString(Folder) + TEXT("BP_PlaneSlideshow");
    if (Params.Contains(TEXT("Verify")))
        return Verify(LoadObject<UBlueprint>(nullptr, *(BPPackage + TEXT(".BP_PlaneSlideshow")))) ? 0 : 1;
    // Never overwrite existing user assets on a repeated install.
    if (FPackageName::DoesPackageExist(MaterialPackage) || FPackageName::DoesPackageExist(BPPackage)) return 2;
    UMaterial* Material = NewObject<UMaterial>(CreatePackage(*MaterialPackage), TEXT("M_PlaneSlideshow"), RF_Public | RF_Standalone);
    Material->SetShadingModel(MSM_Unlit);
    Material->TwoSided = true;
    auto* White = LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture"));
    if (!White) return 3;
    auto* A = Expression<UMaterialExpressionTextureSampleParameter2D>(Material, -700, -150);
    A->ParameterName = TEXT("ImageA"); A->Texture = White;
    auto* B = Expression<UMaterialExpressionTextureSampleParameter2D>(Material, -700, 150);
    B->ParameterName = TEXT("ImageB"); B->Texture = White;
    auto* Blend = Expression<UMaterialExpressionScalarParameter>(Material, -700, 450);
    Blend->ParameterName = TEXT("Blend"); Blend->DefaultValue = 0;
    auto* Lerp = Expression<UMaterialExpressionLinearInterpolate>(Material, -350, 0);
    Lerp->A.Connect(0, A); Lerp->B.Connect(0, B); Lerp->Alpha.Connect(0, Blend);
    auto* Brightness = Expression<UMaterialExpressionScalarParameter>(Material, -350, 300);
    Brightness->ParameterName = TEXT("Brightness"); Brightness->DefaultValue = 1;
    auto* Multiply = Expression<UMaterialExpressionMultiply>(Material, -100, 0);
    Multiply->A.Connect(0, Lerp); Multiply->B.Connect(0, Brightness);
    Material->GetEditorOnlyData()->EmissiveColor.Connect(0, Multiply);
    Material->PostEditChange();
    UBlueprint* BP = FKismetEditorUtilities::CreateBlueprint(AVTGPlaneSlideshow::StaticClass(), CreatePackage(*BPPackage), TEXT("BP_PlaneSlideshow"), BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
    if (!BP) return 4;
    auto* Defaults = Cast<AVTGPlaneSlideshow>(BP->GeneratedClass->GetDefaultObject());
    Defaults->ScreenMaterial = Material;
    if (!Verify(BP)) return 5;
    if (!Save(Material) || !Save(BP)) return 6;
    UE_LOG(LogTemp, Display, TEXT("PLANE SLIDESHOW installed: %s"), *BPPackage);
    return 0;
}
