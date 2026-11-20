#pragma once
#include "Widgets/Input/SMultiLineEditableTextBox.h"
// Distinct type lets the editor input helper limit double-click selection to localization fields.
class SVTGLocalizationTextBox : public SMultiLineEditableTextBox
{
public:
    void Construct(const SMultiLineEditableTextBox::FArguments& Args){SMultiLineEditableTextBox::Construct(Args);}
};
