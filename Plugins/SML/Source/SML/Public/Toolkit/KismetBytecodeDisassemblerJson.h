#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class SML_API FKismetBytecodeDisassemblerJson {
public:
    /** Converts a single expression into json object */
    TSharedPtr<FJsonObject> SerializeExpression(int32& ScriptIndex);

    /** Parses a block of statements until it hits return */
    TArray<TSharedPtr<FJsonValue>> SerializeFunction(UStruct* Function);

    /** Computes length of the statement in bytes and returns it. Returns false if given index does not correspond to any statement (e.g if it is inside of some statement) */
    bool GetStatementLength(UStruct* Function, int32 StatementIndex, int32& OutStatementLength);

    /** Returns index of the first statement using given opcode */
    bool FindFirstStatementOfType(UStruct* Function, int32 StartIndex, uint8 StatementOpcode, int32& OutStatementIndex);
private:
    TWeakObjectPtr<UClass> SelfScope;
    TArray<uint8> Script;

    // Begin script bytecode parsing methods
    int32 ReadInt(int32& ScriptIndex);
    uint64 ReadQword(int32& ScriptIndex);
    uint8 ReadByte(int32& ScriptIndex);
    FString ReadName(int32& ScriptIndex);
    uint16 ReadWord(int32& ScriptIndex);
    float ReadFloat(int32& ScriptIndex);
    CodeSkipSizeType ReadSkipCount(int32& ScriptIndex);
    FString ReadString8(int32& ScriptIndex);
    FString ReadString16(int32& ScriptIndex);
    FString ReadString(int32& ScriptIndex);

    template<typename T>
    T* ReadPointer(int32& ScriptIndex) {
        return (T*)ReadQword(ScriptIndex);
    }

    // End script bytecode parsing methods
};
