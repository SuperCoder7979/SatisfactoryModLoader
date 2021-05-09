#pragma once
#include "Slate.h"
#include "Framework/Text/BaseTextLayoutMarshaller.h"

struct FAssetDumpLogMessage {
    TSharedRef<FString> Message;
    ELogVerbosity::Type Verbosity;
    FName Category;
    FName Style;

    FAssetDumpLogMessage(const TSharedRef<FString>& NewMessage, ELogVerbosity::Type NewVerbosity, FName NewCategory, FName NewStyle = NAME_None)
        : Message(NewMessage)
          , Verbosity(NewVerbosity)
          , Category(NewCategory)
          , Style(NewStyle) {
    }
};

class SAssetDumpConsoleWidget : public SCompoundWidget, public FOutputDevice {
public:
SLATE_BEGIN_ARGS(SAssetDumpConsoleWidget) {
        }

    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, TSharedRef<class FAssetDumpProcessor> DumpProcessor);
    virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
    virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override;

    static TSharedRef<SWindow> CreateForAssetDumper(TSharedRef<class FAssetDumpProcessor> DumpProcessor);

    ~SAssetDumpConsoleWidget();
    SAssetDumpConsoleWidget();
protected:
    void RequestForceScroll();
    void OnUserScrolled(float ScrollOffset);
    FText GetProgressBarText() const;

    TSharedPtr<class SMultiLineEditableTextBox> MessagesTextBox;
    TSharedPtr<class FAssetDumpProcessor> AssetDumpProcessor;
    TSharedPtr<class FAssetDumpConsoleTextLayoutMarshaller> MessagesTextMarshaller;
    bool bIsUserScrolled;
    mutable FText ProgressBarDisplayText;
};

class FAssetDumpConsoleTextLayoutMarshaller : public FBaseTextLayoutMarshaller {
public:
    static TSharedRef<FAssetDumpConsoleTextLayoutMarshaller> Create();
    virtual ~FAssetDumpConsoleTextLayoutMarshaller();

    // ITextLayoutMarshaller
    virtual void SetText(const FString& SourceString, FTextLayout& TargetTextLayout) override;
    virtual void GetText(FString& TargetString, const FTextLayout& SourceTextLayout) override;

    bool AppendPendingMessage(const TCHAR* InText, const ELogVerbosity::Type InVerbosity, const FName& InCategory);
    bool SubmitPendingMessages();
    FORCEINLINE int32 GetNumMessages() const { return Messages.Num(); }
protected:
    FAssetDumpConsoleTextLayoutMarshaller();
    void AppendPendingMessagesToTextLayout();

    /** All log messages to show in the text box */
    TArray<TSharedPtr<FAssetDumpLogMessage>> Messages;
    /** Index of the next entry in the Messages array that is pending submission to the text layout */
    int32 NextPendingMessageIndex;
    FTextLayout* TextLayout;
    TMap<FName, FTextBlockStyle> LogMessageStyles;
};
