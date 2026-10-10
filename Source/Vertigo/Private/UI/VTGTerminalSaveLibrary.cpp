#include "UI/VTGTerminalSaveLibrary.h"
#include "UI/VTGTerminalSavePage.h"
#include "Components/WidgetSwitcher.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "UObject/UnrealType.h"

namespace TerminalSave
{
FProperty* Field(UStruct* Struct, const TCHAR* Prefix)
{
 for(TFieldIterator<FProperty> P(Struct);P;++P)
  if(P->GetName().StartsWith(Prefix)) return *P;
 return nullptr;
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
 // Filter only the runtime copy. Authored ITEM data and its inventory page remain intact.
 for(int32 I=Tabs.Num()-1;I>=0;--I)
  if(Inventory->GetPropertyValue_InContainer(Tabs.GetRawPtr(I)) || Index->GetPropertyValue_InContainer(Tabs.GetRawPtr(I))==SaveTabIndex)Tabs.RemoveValues(I);
 // Some terminals use this widget with no authored tabs. Do not add a save action there.
 if(Tabs.Num()==0)return;
 uint8* NewTab=Tabs.GetRawPtr(Tabs.AddValue());
 Index->SetPropertyValue_InContainer(NewTab,SaveTabIndex);
 Text->SetPropertyValue_InContainer(NewTab,NSLOCTEXT("VTGTerminal","SaveLoad","SAVE / LOAD"));
 Image->SetPropertyValue_InContainer(NewTab,FSoftObjectPtr(FSoftObjectPath(TEXT("/InventorySystemX/InventorySystemX/Textures/Tabs/T_Note.T_Note"))));
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
  for(int32 I=0;I<Switcher->GetChildrenCount();++I)
   if(auto* Candidate=Cast<UUserWidget>(Switcher->GetChildAt(I)); Candidate&&Candidate->GetWidgetFromName(TEXT("OverlayForMail"))){ScreenSource=Candidate;break;}
  Page->OpenForTerminal(Terminal,ScreenSource);
  Switcher->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
  Switcher->SetActiveWidget(Page);
 }
 // Tab selection only opens the page. SaveFromTerminal is called by its button's OnClicked.
 return true;
}
