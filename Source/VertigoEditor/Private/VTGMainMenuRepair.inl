#include "UI/VTGMainMenu.h"
int32 RepairMainMenu(bool VerifyOnly)
{
 using namespace AlleyFix;
 auto* BP=LoadObject<UWidgetBlueprint>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/UI/MainMenu/WBP_MainMenu"));check(BP);
 auto* Button=CastChecked<UButton>(BP->WidgetTree->FindWidget(TEXT("Button")));
 auto* Label=CastChecked<UTextBlock>(Button->GetContent());
 if(!VerifyOnly)
 {
  const FString File=FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
  const FString Backup=FPaths::ProjectSavedDir()/TEXT("QA/MainMenuBefore/WBP_MainMenu.uasset");
  IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup),true);
  if(!IFileManager::Get().FileExists(*Backup))check(IFileManager::Get().Copy(*Backup,*File)==COPY_OK);
  check(BP->ParentClass==UUserWidget::StaticClass() || BP->ParentClass==UVTGMainMenu::StaticClass());
  BP->ParentClass=UVTGMainMenu::StaticClass();
  Label->SetText(NSLOCTEXT("VTGMainMenu","Continue", "继续游戏"));
  if(!Save(BP))return 1;
 }
 check(BP->GeneratedClass->IsChildOf(UVTGMainMenu::StaticClass()));
 check(Label->GetText().ToString()==TEXT("继续游戏"));
 check(BP->WidgetTree->FindWidget(TEXT("Button_86")) && BP->WidgetTree->FindWidget(TEXT("Button_2")));
 // Read-only slot selection smoke test; never alter the player's save files.
 auto* GI=NewObject<UVTGGameInstanceBase>();
 auto* Coordinator=NewObject<UVTGSaveCoordinator>(GI);
 auto* SlotCount=FindFProperty<FIntProperty>(Coordinator->GetClass(),TEXT("MaxSlots"));check(SlotCount);
 const int32 Count=SlotCount->GetPropertyValue_InContainer(Coordinator);
 SlotCount->SetPropertyValue_InContainer(Coordinator,0);
 check(Coordinator->GetLatestValidSlot()==INDEX_NONE && !Coordinator->ContinueLatestSave());
 SlotCount->SetPropertyValue_InContainer(Coordinator,Count);
 const int32 Latest=Coordinator->GetLatestValidSlot();
 if(Latest!=INDEX_NONE)
 {
  auto* Selected=CastChecked<UVTGSaveGame>(UGameplayStatics::LoadGameFromSlot(FString::Printf(TEXT("VTG_Slot_%d"),Latest),0));
  check(Selected->Meta.bIsValid);
  UE_LOG(LogTemp,Display,TEXT("CONTINUE SAVE PASS: latest readable slot=%d map=%s time=%s; no-save returns false without travel."),Latest,*Selected->Meta.LevelName,*Selected->Meta.SaveTimeUtc.ToIso8601());
 }
 FCompilerResultsLog Log;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::None,&Log);
 UE_LOG(LogTemp,Display,TEXT("MAIN MENU PASS: original continue button, layout and new-game preserved; errors=%d"),Log.NumErrors);
 return Log.NumErrors?2:0;
}
