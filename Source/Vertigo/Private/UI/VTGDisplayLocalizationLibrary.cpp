#include "UI/VTGDisplayLocalizationLibrary.h"
#include "Engine/DataTable.h"
#include "UObject/UnrealType.h"

FText UVTGDisplayLocalizationLibrary::GetMailSenderText(const UDataTable* Table,FName SenderID)
{
    if(Table && Table->GetRowStruct() && Table->GetRowStruct()->GetPathName()==TEXT("/Game/Narrative/Mail/S_MailRow.S_MailRow"))
    {
        FNameProperty* ID=nullptr;FTextProperty* Display=nullptr;
        for(TFieldIterator<FProperty> It(Table->GetRowStruct());It;++It)
        {
            if(It->GetName().StartsWith(TEXT("SenderName_")))ID=CastField<FNameProperty>(*It);
            if(It->GetName().StartsWith(TEXT("SenderDisplayName_")))Display=CastField<FTextProperty>(*It);
        }
        if(ID && Display)for(const auto& Row:Table->GetRowMap())
            if(ID->GetPropertyValue_InContainer(Row.Value)==SenderID)
            {
                const FText Text=Display->GetPropertyValue_InContainer(Row.Value);
                if(!Text.IsEmpty())return Text;
            }
    }
    return FText::FromName(SenderID);
}
