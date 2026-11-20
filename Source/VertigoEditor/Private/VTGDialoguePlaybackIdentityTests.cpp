#include "Misc/AutomationTest.h"
#include "Dialogue.h"
#include "DialogueSM.h"
#include "DialogueBlueprintGeneratedClass.h"
#include "UObject/Package.h"
#include "UObject/UObjectHash.h"
#include "Internationalization/TextLocalizationManager.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVTGPlaybackIdentityTest,"Vertigo.Localization.PlaybackMapIdentity",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVTGPlaybackIdentityTest::RunTest(const FString&)
{
 auto* Class=LoadObject<UDialogueBlueprintGeneratedClass>(nullptr,TEXT("/Game/Narrative/Dialogues/Levels/L2BarNStreetOutside/L2_Bar_NPC.L2_Bar_NPC_C"));
 if(!TestNotNull(TEXT("Real bar dialogue class"),Class))return false;
 // A map outer is essential: a transient-package clone does not reproduce stable-key reauthoring.
 auto* Package=CreatePackage(*(TEXT("/Game/VTG_Test_Playback_")+FGuid::NewGuid().ToString(EGuidFormats::Digits)));
 Package->SetPackageFlags(PKG_PlayInEditor);
 const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
 auto* W=UWorld::CreateWorld(EWorldType::PIE,false,TEXT("PlaybackIdentity"),Package,true,ERHIFeatureLevel::Num,&Init);
 auto* Actor=W->SpawnActor<AActor>();auto* Runtime=NewObject<UDialogue>(Actor,Class);Class->InitializeDialogue(Runtime);
 TMap<FName,UDialogueNode*> Originals;for(auto* N:Class->GetDialogueTemplate()->GetNodes())if(N)Originals.Add(N->GetFName(),N);
 int32 Checked=0;
 for(auto* N:Runtime->GetNodes())if(N)if(auto** Source=Originals.Find(N->GetFName()))
 {
  auto Same=[&](const FText& A,const FText& B){return FTextInspector::GetNamespace(A)==FTextInspector::GetNamespace(B) && FTextInspector::GetKey(A)==FTextInspector::GetKey(B);};
  TestTrue(TEXT("Playback node retains authored localization identity inside PIE map"),Same(N->Line.Text,(*Source)->Line.Text));
  for(int32 I=0;I<N->AlternativeLines.Num();++I)TestTrue(TEXT("Alternative line identity retained"),Same(N->AlternativeLines[I].Text,(*Source)->AlternativeLines[I].Text));
  ++Checked;
 }
 TestTrue(TEXT("Real graph nodes checked"),Checked>10);
 W->DestroyWorld(false);Package->SetDirtyFlag(false);
 return true;
}
#endif
