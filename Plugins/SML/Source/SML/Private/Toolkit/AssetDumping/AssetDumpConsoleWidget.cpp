#include "Toolkit/AssetDumping/AssetDumpConsoleWidget.h"
#include "Toolkit/AssetDumping/AssetDumpProcessor.h"
#include "Util/GameEditorHelper.h"
#define LOCTEXT_NAMESPACE "SML"

void SAssetDumpConsoleWidget::Construct(const FArguments& InArgs, TSharedRef<FAssetDumpProcessor> DumpProcessor) {
    this->AssetDumpProcessor = DumpProcessor;
    this->MessagesTextMarshaller = FAssetDumpConsoleTextLayoutMarshaller::Create();
    GLog->AddOutputDevice(this);

    this->MessagesTextBox = SNew(SMultiLineEditableTextBox)
        .Marshaller(MessagesTextMarshaller)
        .IsReadOnly(true)
        .AlwaysShowScrollbars(true)
        .AutoWrapText(true)
        .OnVScrollBarUserScrolled(this, &SAssetDumpConsoleWidget::OnUserScrolled);

    ChildSlot[
        SNew(SVerticalBox)
        + SVerticalBox::Slot().FillHeight(1.0f)[
            SNew(SBorder)
            .Padding(3)[
                MessagesTextBox.ToSharedRef()
            ]
        ]
        + SVerticalBox::Slot().AutoHeight()[
            SNew(SOverlay)
            + SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Center)[
                SNew(SProgressBar)
                .Percent_Lambda(
                    [this]() {
                        return AssetDumpProcessor.IsValid() ? AssetDumpProcessor->GetProgressPct() : 1.0f;
                    }
                )
            ]
            + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)[
                SNew(STextBlock)
                .Text_Raw(this, &SAssetDumpConsoleWidget::GetProgressBarText)
            ]
        ]
    ];

    this->bIsUserScrolled = false;
    this->MessagesTextBox->Refresh();
    this->MessagesTextMarshaller->SubmitPendingMessages();
    RequestForceScroll();
}

void SAssetDumpConsoleWidget::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) {
    // Submit queued log messages into the text view and force scroll if user hasn't scrolled himself
    if (MessagesTextMarshaller->SubmitPendingMessages()) {
        if (!bIsUserScrolled) {
            RequestForceScroll();
        }
    }

    // Make sure we are not holding on to the dump processor when it's done and ready to cleanup
    if (AssetDumpProcessor.IsValid() && AssetDumpProcessor->IsFinishedDumping()) {
        this->ProgressBarDisplayText = GetProgressBarText();
        AssetDumpProcessor.Reset();
    }

    // Tick child widgets and parent logic
    SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
}

void SAssetDumpConsoleWidget::OnUserScrolled(const float ScrollOffset) {
    this->bIsUserScrolled = ScrollOffset < 1.0 && !FMath::IsNearlyEqual(ScrollOffset, 1.0f);
}

FText SAssetDumpConsoleWidget::GetProgressBarText() const {
    if (AssetDumpProcessor.IsValid()) {
        if (AssetDumpProcessor->IsFinishedDumping()) {
            ProgressBarDisplayText = FText::Format(
                LOCTEXT("AssetDumping_DumpFinished", "Dumping finished: {0} packages processed, {1} skipped"),
                FText::FromString(FString::FromInt(AssetDumpProcessor->GetPackagesProcessed())),
                FText::FromString(FString::FromInt(AssetDumpProcessor->GetPackagesSkipped()))
            );
        } else {
            ProgressBarDisplayText = FText::Format(
                LOCTEXT("AssetDumping_DumpInProgress", "Dumping in progress: {0}% ({1}/{2})"),
                FText::FromString(FString::FromInt(FMath::RoundToInt(AssetDumpProcessor->GetProgressPct() * 100))),
                FText::FromString(FString::FromInt(AssetDumpProcessor->GetPackagesSkipped() + AssetDumpProcessor->GetPackagesProcessed())),
                FText::FromString(FString::FromInt(AssetDumpProcessor->GetTotalPackages()))
            );
        }
    }
    return ProgressBarDisplayText;
}

SAssetDumpConsoleWidget::~SAssetDumpConsoleWidget() {
    this->AssetDumpProcessor.Reset();
    GLog->RemoveOutputDevice(this);
}

SAssetDumpConsoleWidget::SAssetDumpConsoleWidget() {
    this->bIsUserScrolled = false;
}

void SAssetDumpConsoleWidget::RequestForceScroll() {
    if (MessagesTextMarshaller->GetNumMessages() > 0) {
        MessagesTextBox->ScrollTo(ETextLocation::EndOfDocument);
        bIsUserScrolled = false;
    }
}

void SAssetDumpConsoleWidget::Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) {
    MessagesTextMarshaller->AppendPendingMessage(V, Verbosity, Category);
}

TSharedRef<SWindow> SAssetDumpConsoleWidget::CreateForAssetDumper(TSharedRef<FAssetDumpProcessor> DumpProcessor) {
    TSharedRef<SWindow> Window = SNew(SWindow)
                    .Title(LOCTEXT("AssetDumper_ConsoleTitle", "Asset Dumping Log"))
                    .MinWidth(800).MinHeight(600)
                    .AutoCenter(EAutoCenter::PreferredWorkArea);
    Window->SetContent(SNew(SAssetDumpConsoleWidget, DumpProcessor));

    const TSharedRef<SWindow> ParentWindow = FGameEditorHelper::GetMainWindow().ToSharedRef();
    FSlateApplication::Get().AddWindowAsNativeChild(Window, ParentWindow, true);
    return Window;
}

void FAssetDumpConsoleTextLayoutMarshaller::SetText(const FString& SourceString, FTextLayout& TargetTextLayout) {
    this->TextLayout = &TargetTextLayout;
    this->NextPendingMessageIndex = 0;
    SubmitPendingMessages();
}

void FAssetDumpConsoleTextLayoutMarshaller::GetText(FString& TargetString, const FTextLayout& SourceTextLayout) {
    SourceTextLayout.GetAsText(TargetString);
}

bool FAssetDumpConsoleTextLayoutMarshaller::AppendPendingMessage(const TCHAR* InText, const ELogVerbosity::Type InVerbosity, const FName& InCategory) {
    // Skip SetColor events, they do not represent normal logging
    if (InVerbosity == ELogVerbosity::SetColor) {
        return false;
    }

    // Skip Niagara logs, it likes to spam warnings in the main menu scene, same for Online Identity logging
    if (InCategory == TEXT("LogNiagara") || InCategory == TEXT("LogOnlineIdentity")) {
        return false;
    }

    FName Style;
    if (InVerbosity == ELogVerbosity::Error) {
        Style = FName(TEXT("Log.Error"));
    } else if (InVerbosity == ELogVerbosity::Warning) {
        Style = FName(TEXT("Log.Warning"));
    } else {
        Style = FName(TEXT("Log.Normal"));
    }

    const int32 OldNumMessages = Messages.Num();

    // handle multiline strings by breaking them apart by line
    TArray<FTextRange> LineRanges;
    const FString CurrentLogDump = InText;
    FTextRange::CalculateLineRangesFromString(CurrentLogDump, LineRanges);

    bool bIsFirstLineInMessage = true;
    for (const FTextRange& LineRange : LineRanges) {
        if (LineRange.IsEmpty()) {
            continue;
        }

        FString Line = CurrentLogDump.Mid(LineRange.BeginIndex, LineRange.Len());
        Line = Line.ConvertTabsToSpaces(4);

        // Hard-wrap lines to avoid them being too long
        static const int32 HardWrapLen = 360;
        for (int32 CurrentStartIndex = 0; CurrentStartIndex < Line.Len();) {
            int32 HardWrapLineLen;
            if (bIsFirstLineInMessage) {
                FString MessagePrefix = FOutputDeviceHelper::FormatLogLine(InVerbosity, InCategory, nullptr, ELogTimes::None);

                HardWrapLineLen = FMath::Min(HardWrapLen - MessagePrefix.Len(), Line.Len() - CurrentStartIndex);
                FString HardWrapLine = Line.Mid(CurrentStartIndex, HardWrapLineLen);

                Messages.Add(MakeShared<FAssetDumpLogMessage>(MakeShared<FString>(MessagePrefix + HardWrapLine), InVerbosity, InCategory, Style));
            } else {
                HardWrapLineLen = FMath::Min(HardWrapLen, Line.Len() - CurrentStartIndex);
                FString HardWrapLine = Line.Mid(CurrentStartIndex, HardWrapLineLen);

                Messages.Add(MakeShared<FAssetDumpLogMessage>(MakeShared<FString>(MoveTemp(HardWrapLine)), InVerbosity, InCategory, Style));
            }

            bIsFirstLineInMessage = false;
            CurrentStartIndex += HardWrapLineLen;
        }
    }

    return OldNumMessages != Messages.Num();
}

bool FAssetDumpConsoleTextLayoutMarshaller::SubmitPendingMessages() {
    if (Messages.IsValidIndex(NextPendingMessageIndex)) {
        const int32 CurrentMessagesCount = Messages.Num();
        AppendPendingMessagesToTextLayout();
        NextPendingMessageIndex = CurrentMessagesCount;
        return true;
    }
    return false;
}

void FAssetDumpConsoleTextLayoutMarshaller::AppendPendingMessagesToTextLayout() {
    const int32 CurrentMessagesCount = Messages.Num();
    const int32 NumPendingMessages = CurrentMessagesCount - NextPendingMessageIndex;
    if (NumPendingMessages == 0) {
        return;
    }

    // If we were previously empty, then we'd have inserted a dummy empty line into the document
    // We need to remove this line now as it would cause the message indices to get out-of-sync with the line numbers, which would break auto-scrolling
    if (TextLayout) {
        const bool bWasEmpty = GetNumMessages() == 0;
        if (bWasEmpty) {
            TextLayout->ClearLines();
        }
    } else {
        MakeDirty();
    }

    TArray<FTextLayout::FNewLineData> LinesToAdd;
    LinesToAdd.Reserve(NumPendingMessages);

    int32 NumAddedMessages = 0;

    for (int32 MessageIndex = NextPendingMessageIndex; MessageIndex < CurrentMessagesCount; ++MessageIndex) {
        const TSharedPtr<FAssetDumpLogMessage> Message = Messages[MessageIndex];
        ++NumAddedMessages;

        const FTextBlockStyle& MessageTextStyle = LogMessageStyles.FindChecked(Message->Style);
        TSharedRef<FString> LineText = Message->Message;

        TArray<TSharedRef<IRun>> Runs;
        Runs.Add(FSlateTextRun::Create(FRunInfo(), LineText, MessageTextStyle));
        LinesToAdd.Emplace(MoveTemp(LineText), MoveTemp(Runs));
    }

    TextLayout->AddLines(LinesToAdd);
}

TSharedRef<FAssetDumpConsoleTextLayoutMarshaller> FAssetDumpConsoleTextLayoutMarshaller::Create() {
    return MakeShareable(new FAssetDumpConsoleTextLayoutMarshaller());
}

FAssetDumpConsoleTextLayoutMarshaller::FAssetDumpConsoleTextLayoutMarshaller() {
    this->NextPendingMessageIndex = 0;
    this->TextLayout = NULL;

    const FTextBlockStyle& NormalTextStyle = FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>(TEXT("NormalText"));

    FTextBlockStyle NormalLogStyle = NormalTextStyle;
    NormalLogStyle.ColorAndOpacity = FLinearColor::Gray;
    this->LogMessageStyles.Add(TEXT("Log.Normal"), NormalLogStyle);

    FTextBlockStyle WarningLogStyle = NormalTextStyle;
    WarningLogStyle.ColorAndOpacity = FLinearColor::Yellow;
    this->LogMessageStyles.Add(TEXT("Log.Warning"), WarningLogStyle);

    FTextBlockStyle ErrorLogStyle = NormalTextStyle;
    ErrorLogStyle.ColorAndOpacity = FLinearColor::Red;
    this->LogMessageStyles.Add(TEXT("Log.Error"), ErrorLogStyle);
}

FAssetDumpConsoleTextLayoutMarshaller::~FAssetDumpConsoleTextLayoutMarshaller() {
}
