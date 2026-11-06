#include "Audio/VTGPlayerAudioComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/AutomationTest.h"
#include "Sound/SoundWave.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVTGFootstepSurfaceTest, "Vertigo.Audio.FootstepSurfaces",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVTGFootstepSurfaceTest::RunTest(const FString &)
{
    UWorld *World = UWorld::CreateWorld(EWorldType::Editor, false);
    ACharacter *Owner = World->SpawnActor<ACharacter>();
    auto *Audio = NewObject<UVTGPlayerAudioComponent>(Owner);
    auto *Floor = NewObject<UStaticMeshComponent>(Owner);
    Floor->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
    auto *Sound = NewObject<USoundWave>();
    Audio->DefaultSteps.Add(Sound);
    Audio->MetalSteps.Add(Sound);
    Audio->ConcreteSteps.Add(Sound);
    Audio->BarefootSteps.Add(Sound);
    FHitResult Hit;
    Hit.Component = Floor;
    const TCHAR *Paths[] = {
        TEXT("/Game/EnvArt/ModularSciFi/Materials/Grid/MI_Grid2.MI_Grid2"),
        TEXT("/Game/EnvArt/CyberpunkRestaurant/Materials/MI_Ground_Tiles_01.MI_Ground_Tiles_01"),
        TEXT("/Game/EnvArt/ModernCity/Materials/MI_FireEscape_a.MI_FireEscape_a"),
        TEXT("/Game/EnvArt/ModernCity/Materials/MI_L2Street.MI_L2Street"),
        TEXT("/Game/Cyberpunk_Gigapack/Materials/Material_Instance/MI_CoffeeShop_ConcreteRoof.MI_CoffeeShop_ConcreteRoof"),
        TEXT("/Game/Cyberpunk_Gigapack/Materials/Material_Instance/MI_BarFloorTiles6.MI_BarFloorTiles6")
    };
    for (int32 I = 0; I < UE_ARRAY_COUNT(Paths); ++I)
    {
        auto *Material = LoadObject<UMaterialInterface>(nullptr, Paths[I]);
        TestNotNull(Paths[I], Material);
        if (!Material) continue;
        Floor->SetMaterial(0, Material);
        const auto *Expected = I < 3 ? &Audio->MetalSteps : &Audio->ConcreteSteps;
        TestTrue(FString::Printf(TEXT("Explicit mapping %s"), Paths[I]), &Audio->ResolveFootstepPool(Hit) == Expected);
        Floor->SetMaterial(0, UMaterialInstanceDynamic::Create(Material, Floor));
        TestTrue(TEXT("Unnamed child inherits parent classification"), &Audio->ResolveFootstepPool(Hit) == Expected);
    }
    Floor->SetMaterial(0, nullptr);
    TestTrue(TEXT("Unknown ground retains boots fallback"), &Audio->ResolveFootstepPool(Hit) == &Audio->DefaultSteps);
    Floor->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Game/EnvArt/ModernCity/Meshes/Props/SM_FireEscape_Landing2.SM_FireEscape_Landing2")));
    TestNotNull(TEXT("Fire escape landing resource exists"),Floor->GetStaticMesh().Get());
    TestTrue(TEXT("Fire escape landing uses metal footsteps"), &Audio->ResolveFootstepPool(Hit)==&Audio->MetalSteps);
    for(const TCHAR* Path:{TEXT("/Game/EnvArt/PortCity/Meshes/SM_Slums_ArchitecturalKit/SM_Stairs_Metal.SM_Stairs_Metal"),TEXT("/Game/EnvArt/PortCity/Meshes/SM_Slums_ArchitecturalKit/SM_Stairs_Small_Metal.SM_Stairs_Small_Metal")})
    {
        Floor->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,Path));TestNotNull(Path,Floor->GetStaticMesh().Get());
        TestTrue(TEXT("PortCity stairs explicitly select metal footsteps"),&Audio->ResolveFootstepPool(Hit)==&Audio->MetalSteps);
    }
    Audio->BarefootMesh = NewObject<USkeletalMesh>();
    Owner->GetMesh()->SetSkeletalMeshAsset(Audio->BarefootMesh);
    Floor->ComponentTags.Add(TEXT("AudioSurface.Metal"));
    TestTrue(TEXT("Bare feet override metal"), &Audio->ResolveFootstepPool(Hit) == &Audio->BarefootSteps);
    Audio->BarefootSteps.Empty();
    TestTrue(TEXT("Empty barefoot pool never plays boots"), Audio->ResolveFootstepPool(Hit).IsEmpty());
    Owner->GetMesh()->SetSkeletalMeshAsset(nullptr);
    TestTrue(TEXT("Changing outfit restores ground routing"), &Audio->ResolveFootstepPool(Hit) == &Audio->MetalSteps);
    World->DestroyWorld(false);
    return true;
}
