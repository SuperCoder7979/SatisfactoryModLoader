#include "Configuration/RawFileFormat/Json/JsonRawFormatConverter.h"
#include "Configuration/RawFileFormat/RawFormatValueObject.h"
#include "Configuration/RawFileFormat/RawFormatValueArray.h"
#include "Configuration/RawFileFormat/RawFormatValueNumber.h"
#include "Configuration/RawFileFormat/RawFormatValueString.h"
#include "Configuration/RawFileFormat/RawFormatValueBool.h"

TSharedPtr<FJsonValue> FJsonRawFormatConverter::ConvertToJson(const URawFormatValue* RawFormatValue) {
    if (const URawFormatValueNumber* Number = Cast<URawFormatValueNumber>(RawFormatValue)) {
        return MakeShareable(new FJsonValueNumber(Number->Value));
    }
    if (const URawFormatValueString* String = Cast<URawFormatValueString>(RawFormatValue)) {
        return MakeShareable(new FJsonValueString(String->Value));
    }
    if (const URawFormatValueBool* Boolean = Cast<URawFormatValueBool>(RawFormatValue)) {
        return MakeShareable(new FJsonValueBoolean(Boolean->Value));
    }
    if (const URawFormatValueArray* Array = Cast<URawFormatValueArray>(RawFormatValue)) {
        TArray<TSharedPtr<FJsonValue>> OutJsonArray;
        for (const URawFormatValue* ChildValue : Array->GetUnderlyingArrayRef()) {
            OutJsonArray.Add(ConvertToJson(ChildValue));
        }
        return MakeShareable(new FJsonValueArray(OutJsonArray));
    }
    if (const URawFormatValueObject* Object = Cast<URawFormatValueObject>(RawFormatValue)) {
        TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject());
        for (TPair<FString, URawFormatValue*> Pair : Object->Values) {
            JsonObject->SetField(Pair.Key, ConvertToJson(Pair.Value));
        }
        return MakeShareable(new FJsonValueObject(JsonObject));
    }
    checkf(false, TEXT("Unreachable code"));
    return NULL;
}

URawFormatValue* FJsonRawFormatConverter::ConvertToRawFormat(UObject* Outer, const TSharedPtr<FJsonValue>& JsonValue) {
    switch (JsonValue->Type) {
    case EJson::Number: {
        URawFormatValueNumber* Number = NewObject<URawFormatValueNumber>(Outer);
        Number->Value = JsonValue->AsNumber();
        return Number;
    }
    case EJson::String: {
        URawFormatValueString* String = NewObject<URawFormatValueString>(Outer);
        String->Value = JsonValue->AsString();
        return String;
    }
    case EJson::Boolean: {
        URawFormatValueBool* Boolean = NewObject<URawFormatValueBool>(Outer);
        Boolean->Value = JsonValue->AsBool();
        return Boolean;
    }
    case EJson::Array: {
        URawFormatValueArray* Array = NewObject<URawFormatValueArray>(Outer);
        for (const TSharedPtr<FJsonValue>& ChildValue : JsonValue->AsArray()) {
            Array->AddValue(ConvertToRawFormat(Array, ChildValue));
        }
        return Array;
    }
    case EJson::Object: {
        URawFormatValueObject* Object = NewObject<URawFormatValueObject>(Outer);
        for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : JsonValue->AsObject()->Values) {
            Object->AddValue(Pair.Key, ConvertToRawFormat(Object, Pair.Value));
        }
        return Object;
    }
    case EJson::Null:
    checkf(false, TEXT("JSON Null value is not supported"));
        return NULL;
    default:
    checkf(false, TEXT("Unreachable code"));
        return NULL;
    }
}
