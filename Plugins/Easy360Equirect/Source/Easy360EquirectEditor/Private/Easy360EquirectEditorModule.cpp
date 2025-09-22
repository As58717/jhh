#include "Modules/ModuleManager.h"
#include "LevelEditor.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EqrCaptureActor.h"
#include "Framework/Docking/TabManager.h"
#include "EngineUtils.h"

static const FName Easy360TabName("Easy360Panel");

class SEasy360Panel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SEasy360Panel) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs)
    {
        ResolutionItems = {
            MakeShareable(new FString("4K (4096x2048)")),
            MakeShareable(new FString("6K (6144x3072)")),
            MakeShareable(new FString("8K (8192x4096)")),
            MakeShareable(new FString("Custom"))
        };
        StereoItems = {
            MakeShareable(new FString("Mono")),
            MakeShareable(new FString("Stereo Top-Bottom")),
            MakeShareable(new FString("Stereo Side-by-Side"))
        };
        EyeItems = {
            MakeShareable(new FString("Both Eyes")),
            MakeShareable(new FString("Left Eye Only")),
            MakeShareable(new FString("Right Eye Only"))
        };
        EncoderItems = {
            MakeShareable(new FString("libx264 (CPU)")),
            MakeShareable(new FString("h264_nvenc (GPU)")),
            MakeShareable(new FString("hevc_nvenc (GPU)"))
        };
        PresetItems = {
            MakeShareable(new FString("p1 (slow)")),
            MakeShareable(new FString("p3")),
            MakeShareable(new FString("p5 (default)")),
            MakeShareable(new FString("p7 (fast)"))
        };

        ChildSlot
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(6)
            [ SNew(STextBlock).Text(FText::FromString(TEXT("Easy360 — Capture Panel (v5.3)"))) ]

            + SVerticalBox::Slot().AutoHeight().Padding(6)
            [
                SNew(SUniformGridPanel)
                + SUniformGridPanel::Slot(0,0)[ SNew(STextBlock).Text(FText::FromString("Resolution")) ]
                + SUniformGridPanel::Slot(1,0)[
                    SAssignNew(ResCombo, SComboBox<TSharedPtr<FString>>)
                    .OptionsSource(&ResolutionItems)
                    .OnSelectionChanged(this, &SEasy360Panel::OnResChanged)
                    .OnGenerateWidget(this, &SEasy360Panel::MakeComboItem)
                    [ SNew(STextBlock).Text(FText::FromString("4K (4096x2048)")) ]
                ]
                + SUniformGridPanel::Slot(0,1)[ SNew(STextBlock).Text(FText::FromString("Width")) ]
                + SUniformGridPanel::Slot(1,1)[ SAssignNew(EditW, SEditableTextBox).Text(FText::FromString("4096")) ]
                + SUniformGridPanel::Slot(0,2)[ SNew(STextBlock).Text(FText::FromString("Height")) ]
                + SUniformGridPanel::Slot(1,2)[ SAssignNew(EditH, SEditableTextBox).Text(FText::FromString("2048")) ]
            ]

            + SVerticalBox::Slot().AutoHeight().Padding(6)
            [
                SNew(SUniformGridPanel)
                + SUniformGridPanel::Slot(0,0)[ SNew(STextBlock).Text(FText::FromString("Stereo")) ]
                + SUniformGridPanel::Slot(1,0)[
                    SAssignNew(StereoCombo, SComboBox<TSharedPtr<FString>>)
                    .OptionsSource(&StereoItems)
                    .OnGenerateWidget(this, &SEasy360Panel::MakeComboItem)
                    [ SNew(STextBlock).Text(FText::FromString("Mono")) ]
                ]
                + SUniformGridPanel::Slot(0,1)[ SNew(STextBlock).Text(FText::FromString("FPS")) ]
                + SUniformGridPanel::Slot(1,1)[ SAssignNew(EditFPS, SEditableTextBox).Text(FText::FromString("30")) ]
                + SUniformGridPanel::Slot(0,2)[ SNew(STextBlock).Text(FText::FromString("IPD (cm)")) ]
                + SUniformGridPanel::Slot(1,2)[ SAssignNew(EditIPD, SEditableTextBox).Text(FText::FromString("6.4")) ]
                + SUniformGridPanel::Slot(0,3)[ SNew(STextBlock).Text(FText::FromString("Stereo Eye")) ]
                + SUniformGridPanel::Slot(1,3)[
                    SAssignNew(EyeCombo, SComboBox<TSharedPtr<FString>>)
                    .OptionsSource(&EyeItems)
                    .OnGenerateWidget(this, &SEasy360Panel::MakeComboItem)
                    [ SNew(STextBlock).Text(FText::FromString("Both Eyes")) ]
                ]
            ]

            + SVerticalBox::Slot().AutoHeight().Padding(6)
            [
                SNew(SUniformGridPanel)
                + SUniformGridPanel::Slot(0,0)[ SNew(STextBlock).Text(FText::FromString("Record PNG")) ]
                + SUniformGridPanel::Slot(1,0)[ SAssignNew(ChkRecord, SCheckBox).IsChecked(ECheckBoxState::Unchecked) ]
                + SUniformGridPanel::Slot(0,1)[ SNew(STextBlock).Text(FText::FromString("FFmpeg CRF (libx264)")) ]
                + SUniformGridPanel::Slot(1,1)[ SAssignNew(EditCRF, SEditableTextBox).Text(FText::FromString("18")) ]
                + SUniformGridPanel::Slot(0,2)[ SNew(STextBlock).Text(FText::FromString("FFmpeg Path")) ]
                + SUniformGridPanel::Slot(1,2)[ SAssignNew(EditFF, SEditableTextBox) ]
            ]

            + SVerticalBox::Slot().AutoHeight().Padding(6)
            [
                SNew(SUniformGridPanel)
                + SUniformGridPanel::Slot(0,0)[ SNew(STextBlock).Text(FText::FromString("FFmpeg Encoder")) ]
                + SUniformGridPanel::Slot(1,0)[
                    SAssignNew(EncCombo, SComboBox<TSharedPtr<FString>>)
                    .OptionsSource(&EncoderItems)
                    .OnGenerateWidget(this, &SEasy360Panel::MakeComboItem)
                    [ SNew(STextBlock).Text(FText::FromString("libx264 (CPU)")) ]
                ]
                + SUniformGridPanel::Slot(0,1)[ SNew(STextBlock).Text(FText::FromString("NVENC Preset")) ]
                + SUniformGridPanel::Slot(1,1)[
                    SAssignNew(PresetCombo, SComboBox<TSharedPtr<FString>>)
                    .OptionsSource(&PresetItems)
                    .OnGenerateWidget(this, &SEasy360Panel::MakeComboItem)
                    [ SNew(STextBlock).Text(FText::FromString("p5 (default)")) ]
                ]
                + SUniformGridPanel::Slot(0,2)[ SNew(STextBlock).Text(FText::FromString("Bitrate (Mbps)")) ]
                + SUniformGridPanel::Slot(1,2)[ SAssignNew(EditBitrate, SEditableTextBox).Text(FText::FromString("40")) ]
            ]

            + SVerticalBox::Slot().AutoHeight().Padding(6)
            [
                SNew(SUniformGridPanel)
                + SUniformGridPanel::Slot(0,0)[ SNew(STextBlock).Text(FText::FromString("Use Native NVENC (D3D11)")) ]
                + SUniformGridPanel::Slot(1,0)[ SAssignNew(ChkNvenc, SCheckBox).IsChecked(ECheckBoxState::Unchecked) ]
                + SUniformGridPanel::Slot(0,1)[ SNew(STextBlock).Text(FText::FromString("FFmpeg Pipe (no PNG)")) ]
                + SUniformGridPanel::Slot(1,1)[ SAssignNew(ChkPipe, SCheckBox).IsChecked(ECheckBoxState::Unchecked) ]
            ]

            + SVerticalBox::Slot().AutoHeight().Padding(6)
            [
                SNew(SUniformGridPanel)
                + SUniformGridPanel::Slot(0,0)[ SNew(STextBlock).Text(FText::FromString("Record Audio")) ]
                + SUniformGridPanel::Slot(1,0)[ SAssignNew(ChkAudio, SCheckBox).IsChecked(ECheckBoxState::Unchecked) ]
                + SUniformGridPanel::Slot(0,1)[ SNew(STextBlock).Text(FText::FromString("Output Dir")) ]
                + SUniformGridPanel::Slot(1,1)[ SAssignNew(EditOut, SEditableTextBox) ]
            ]

            + SVerticalBox::Slot().AutoHeight().Padding(6)
            [
                SNew(SUniformGridPanel)
                + SUniformGridPanel::Slot(0,0)[ SNew(STextBlock).Text(FText::FromString("Preview Screen")) ]
                + SUniformGridPanel::Slot(1,0)[ SAssignNew(ChkPreview, SCheckBox).IsChecked(ECheckBoxState::Checked) ]
            ]

            + SVerticalBox::Slot().AutoHeight().Padding(6)
            [
                SNew(SUniformGridPanel)
                + SUniformGridPanel::Slot(0,0)[ SNew(SButton).Text(FText::FromString("Start (PIE)")).OnClicked(this, &SEasy360Panel::OnStart) ]
                + SUniformGridPanel::Slot(1,0)[ SNew(SButton).Text(FText::FromString("Stop (PIE)")).OnClicked(this, &SEasy360Panel::OnStop) ]
            ]
        ];
    }

private:
    TSharedRef<SWidget> MakeComboItem(TSharedPtr<FString> Item){ return SNew(STextBlock).Text(FText::FromString(*Item)); }
    void OnResChanged(TSharedPtr<FString> NewSel, ESelectInfo::Type){
        if(!NewSel) return;
        if (*NewSel=="4K (4096x2048)") { EditW->SetText(FText::FromString("4096")); EditH->SetText(FText::FromString("2048")); }
        else if (*NewSel=="6K (6144x3072)") { EditW->SetText(FText::FromString("6144")); EditH->SetText(FText::FromString("3072")); }
        else if (*NewSel=="8K (8192x4096)") { EditW->SetText(FText::FromString("8192")); EditH->SetText(FText::FromString("4096")); }
    }
    TWeakObjectPtr<AEqrCaptureActor> FindActor() const{
        if (!GEditor) return nullptr;
        UWorld* World = GEditor->GetEditorWorldContext().World();
        if (!World) return nullptr;
        for (TActorIterator<AEqrCaptureActor> It(World); It; ++It) { return *It; }
        return nullptr;
    }
    FReply OnStart(){
        AEqrCaptureActor* A = FindActor().Get(); if (!A) return FReply::Handled();
        auto toInt=[&](TSharedPtr<SEditableTextBox> E,int d){ int v=FCString::Atoi(*E->GetText().ToString()); return v>0?v:d; };
        auto toFlt=[&](TSharedPtr<SEditableTextBox> E,float d){ float v=FCString::Atof(*E->GetText().ToString()); return v>0?v:d; };
        A->Width=toInt(EditW,4096); A->Height=toInt(EditH,2048); if(A->Width&1)++A->Width; if(A->Height&1)++A->Height;
        A->TargetFPS=toFlt(EditFPS,30.f); A->IPDcm=toFlt(EditIPD,6.4f);
        A->bRecordSequence=(ChkRecord->IsChecked()==ECheckBoxState::Checked);
        A->bRecordAudio=(ChkAudio->IsChecked()==ECheckBoxState::Checked);
        A->bUseNvenc=(ChkNvenc->IsChecked()==ECheckBoxState::Checked);
        A->bPipeEncode=(ChkPipe->IsChecked()==ECheckBoxState::Checked);
        A->bShowPreviewScreen=(ChkPreview->IsChecked()==ECheckBoxState::Checked);
        A->FFmpegCRF=toInt(EditCRF,18); A->NvencBitrateMbps=toInt(EditBitrate,40);
        A->FFmpegExePath=EditFF->GetText().ToString(); A->OutputDir=EditOut->GetText().ToString();

        const FString StereoSel = StereoCombo->GetSelectedItem().IsValid()? *StereoCombo->GetSelectedItem() : FString("Mono");
        if (StereoSel.Contains(TEXT("Top-Bottom"))) A->StereoMode = EEqrStereoMode::StereoTB;
        else if (StereoSel.Contains(TEXT("Side-by-Side"))) A->StereoMode = EEqrStereoMode::StereoSBS;
        else A->StereoMode = EEqrStereoMode::Mono;

        const FString EyeSel = (EyeCombo.IsValid() && EyeCombo->GetSelectedItem().IsValid()) ? *EyeCombo->GetSelectedItem() : FString("Both Eyes");
        if (EyeSel.Contains(TEXT("Left")))
        {
            A->StereoEyeSource = EEqrStereoEyeSource::LeftOnly;
        }
        else if (EyeSel.Contains(TEXT("Right")))
        {
            A->StereoEyeSource = EEqrStereoEyeSource::RightOnly;
        }
        else
        {
            A->StereoEyeSource = EEqrStereoEyeSource::Both;
        }

        A->FFmpegEncoder = ([&](){ const FString s= EncCombo->GetSelectedItem().IsValid()? *EncCombo->GetSelectedItem() : FString("libx264 (CPU)"); if(s.StartsWith(TEXT("h264_nvenc")))return TEXT("h264_nvenc"); if(s.StartsWith(TEXT("hevc_nvenc")))return TEXT("hevc_nvenc"); return TEXT("libx264");})();
        A->FFmpegNvPreset = ([&](){ const FString s= PresetCombo->GetSelectedItem().IsValid()? *PresetCombo->GetSelectedItem() : FString("p5 (default)"); if(s.StartsWith(TEXT("p1")))return TEXT("p1"); if(s.StartsWith(TEXT("p3")))return TEXT("p3"); if(s.StartsWith(TEXT("p7")))return TEXT("p7"); return TEXT("p5");})();

        A->StartCapture(); return FReply::Handled();
    }
    FReply OnStop(){ if (AEqrCaptureActor* A = FindActor().Get()) { A->StopCapture(); } return FReply::Handled(); }

private:
    TSharedPtr<SComboBox<TSharedPtr<FString>>> ResCombo, StereoCombo, EyeCombo, EncCombo, PresetCombo;
    TArray<TSharedPtr<FString>> ResolutionItems, StereoItems, EyeItems, EncoderItems, PresetItems;
    TSharedPtr<SEditableTextBox> EditW, EditH, EditFPS, EditIPD, EditCRF, EditFF, EditOut, EditBitrate;
    TSharedPtr<SCheckBox> ChkRecord, ChkAudio, ChkNvenc, ChkPipe, ChkPreview;
};

class FEasy360EquirectEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        FGlobalTabmanager::Get()->RegisterNomadTabSpawner(Easy360TabName,
            FOnSpawnTab::CreateRaw(this, &FEasy360EquirectEditorModule::SpawnPanel))
            .SetDisplayName(FText::FromString(TEXT("Easy360")))
            .SetMenuType(ETabSpawnerMenuType::Hidden);

        if (UToolMenus::IsToolMenuUIEnabled())
        {
            UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
            FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
            Section.AddMenuEntry("OpenEasy360Panel", FText::FromString("Easy360"), FText::FromString("Open Easy360 panel"),
                FSlateIcon(), FUIAction(FExecuteAction::CreateLambda([](){ FGlobalTabmanager::Get()->TryInvokeTab(Easy360TabName); })));
        }
    }
    virtual void ShutdownModule() override{ FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(Easy360TabName); }
    TSharedRef<SDockTab> SpawnPanel(const FSpawnTabArgs&){ return SNew(SDockTab).TabRole(ETabRole::NomadTab)[ SNew(SEasy360Panel) ]; }
};

IMPLEMENT_MODULE(FEasy360EquirectEditorModule, Easy360EquirectEditor)
