#include "UI/VTGTerminalSaveLibrary.h"
#include "UI/VTGTerminalSavePage.h"
#include "Components/WidgetSwitcher.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "UObject/UnrealType.h"
#include "UI/VTGTerminalWidget.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

namespace TerminalSave
{
FProperty* Field(UStruct* Struct, const TCHAR* Prefix)
{
 for(TFieldIterator<FProperty> P(Struct);P;++P)
  if(P->GetName().StartsWith(Prefix)) return *P;
 return nullptr;
}
}

bool UVTGTerminalSaveLibrary::TryAcceptTerminalInput(APawn* Player)
{
 if(!IsValid(Player))return false;
 auto* Gate=Player->FindComponentByClass<UVTGTerminalInputGate>();
 if(!Gate){Gate=NewObject<UVTGTerminalInputGate>(Player);Player->AddInstanceComponent(Gate);Gate->RegisterComponent();}
 return Gate->TryAccept();
}

uint8 UVTGTerminalSaveLibrary::ResolveTerminalTabIndex(uint8 Index)
{
 // Keep the authored Document page and its index intact, but unplug access to it.
 return Index==0?1:Index;
}

void UVTGTerminalSaveLibrary::SynchronizeTerminalInput(APawn* Player)
{
 if(!IsValid(Player))return;
 TArray<UActorComponent*> Candidates;
 if(auto* Property=FindFProperty<FObjectPropertyBase>(Player->GetClass(),TEXT("AC_Inventory_HUD_Component")))
  if(auto* HUD=Cast<UActorComponent>(Property->GetObjectPropertyValue_InContainer(Player)))Candidates.Add(HUD);
 if(Candidates.IsEmpty())for(auto* Component:Player->GetComponents())Candidates.Add(Component);
 for(auto* Component:Candidates)
  if(auto* Property=FindFProperty<FObjectPropertyBase>(Component->GetClass(),TEXT("TabSwitchWidget")))
   if(auto* Terminal=Cast<UVTGTerminalWidget>(Property->GetObjectPropertyValue_InContainer(Component)))
   {
    UE_LOG(LogTemp,Display,TEXT("TERMINAL_INPUT_SYNC hud=%s terminal=%s visible=%d"),*Component->GetPathName(),*Terminal->GetPathName(),Terminal->IsVisible());
    if(auto* Reference=FindFProperty<FObjectPropertyBase>(Terminal->GetClass(),TEXT("HUD_InventoryComponent_Reference")))Reference->SetObjectPropertyValue_InContainer(Terminal,Component);
    Terminal->SynchronizeInput(Cast<APlayerController>(Player->GetController()));
    return;
   }
}

void UVTGTerminalSaveLibrary::PrepareTerminalTabs(UUserWidget* Menu)
{
 if(!Menu)return;
 auto* Property=FindFProperty<FArrayProperty>(Menu->GetClass(),TEXT("Tabs"));
 auto* Entry=Property?CastField<FStructProperty>(Property->Inner):nullptr;
 if(!Entry)return;
 auto* Inventory=CastField<FBoolProperty>(TerminalSave::Field(Entry->Struct,TEXT("ShowInventory_")));
 auto* Index=CastField<FByteProperty>(TerminalSave::Field(Entry->Struct,TEXT("WidgetIndex_")));
 auto* Text=CastField<FTextProperty>(TerminalSave::Field(Entry->Struct,TEXT("Text_")));
 auto* Image=CastField<FSoftObjectProperty>(TerminalSave::Field(Entry->Struct,TEXT("Image_")));
 if(!Inventory||!Index||!Text||!Image)return;
 FScriptArrayHelper Tabs(Property,Property->ContainerPtrToValuePtr<void>(Menu));
 FSoftObjectPtr DocumentIcon(FSoftObjectPath(TEXT("/Game/Inventory/InventoryXBased/UMG/Textures/Tabs/T_Note.T_Note")));
 for(int32 I=0;I<Tabs.Num();++I)
  if(!Inventory->GetPropertyValue_InContainer(Tabs.GetRawPtr(I)) && Index->GetPropertyValue_InContainer(Tabs.GetRawPtr(I))==0)
  {const auto AuthoredIcon=Image->GetPropertyValue_InContainer(Tabs.GetRawPtr(I));if(!AuthoredIcon.IsNull())DocumentIcon=AuthoredIcon;break;}
 // Runtime only: preserve authored Document data and page, exclude its button
 // from rendering, focus and Q/E navigation along with the unused ITEM tab.
 for(int32 I=Tabs.Num()-1;I>=0;--I)
  if(Inventory->GetPropertyValue_InContainer(Tabs.GetRawPtr(I)) || Index->GetPropertyValue_InContainer(Tabs.GetRawPtr(I))==0 || Index->GetPropertyValue_InContainer(Tabs.GetRawPtr(I))==SaveTabIndex)Tabs.RemoveValues(I);
 if(Tabs.Num()==0)return;
 uint8* NewTab=Tabs.GetRawPtr(Tabs.AddValue());
 Index->SetPropertyValue_InContainer(NewTab,SaveTabIndex);
 Text->SetPropertyValue_InContainer(NewTab,NSLOCTEXT("VTGTerminal","SaveLoad","SAVE / LOAD"));
 Image->SetPropertyValue_InContainer(NewTab,DocumentIcon);
 // PreConstruct may run more than once: rebuild navigation references with the visible buttons.
 if(auto* Buttons=FindFProperty<FArrayProperty>(Menu->GetClass(),TEXT("TabButtonsWidgets")))
  FScriptArrayHelper(Buttons,Buttons->ContainerPtrToValuePtr<void>(Menu)).EmptyValues();
}

bool UVTGTerminalSaveLibrary::HandleTerminalSave(UUserWidget* Terminal, uint8 Index)
{
 if(Index!=SaveTabIndex)return false;
 if(!Terminal)return true;
 auto* Switcher=Cast<UWidgetSwitcher>(Terminal->GetWidgetFromName(TEXT("WidgetSwitcher_132")));
 if(!Switcher)return true;
 UVTGTerminalSavePage* Page=nullptr;
 for(int32 I=0;I<Switcher->GetChildrenCount();++I)
  if(auto* Existing=Cast<UVTGTerminalSavePage>(Switcher->GetChildAt(I))){Page=Existing;break;}
 if(!Page)
 {
  Page=CreateWidget<UVTGTerminalSavePage>(Terminal,UVTGTerminalSavePage::StaticClass(),TEXT("TerminalSavePage"));
  if(Page)Switcher->AddChild(Page);
 }
 if(Page)
 {
  UUserWidget* ScreenSource=nullptr;
  UUserWidget* DocumentSource=nullptr;
  for(int32 I=0;I<Switcher->GetChildrenCount();++I)
   if(auto* Candidate=Cast<UUserWidget>(Switcher->GetChildAt(I)); Candidate)
   {if(Candidate->GetWidgetFromName(TEXT("OverlayForMail")))ScreenSource=Candidate;if(Candidate->GetWidgetFromName(TEXT("FirstOverlay")) && Candidate->GetWidgetFromName(TEXT("Image_AddTerminal")))DocumentSource=Candidate;}
  Page->OpenForTerminal(Terminal,ScreenSource,DocumentSource);
  Switcher->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
  Switcher->SetActiveWidget(Page);
  // This branch bypasses SetActiveTab's page focus. Keep Q/E on this terminal
  // when the old page becomes inactive; the save buttons do not take focus.
  Terminal->SetFocus();
 }
 // Tab selection only opens the page. SaveFromTerminal is called by its button's OnClicked.
 return true;
}
