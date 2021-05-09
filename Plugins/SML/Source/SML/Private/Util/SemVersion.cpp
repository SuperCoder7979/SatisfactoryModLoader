#pragma once
#include "Util/SemVersion.h"
#include <regex>

// Use stdlib regex here because UE Regex(which is ICU regex wrapper) requires initialized localization system, and our versioning can be called much earlier than that
std::wregex VersionRegex(TEXT("^(~|v|=|<=|<|>|>=|\\^)?(X|x|\\*|0|[1-9]\\d*)(?:\\.(X|x|\\*|0|[1-9]\\d*)(?:\\.(X|x|\\*|0|[1-9]\\d*)(?:-((?:0|[1-9]\\d*|\\d*[a-zA-Z-][0-9a-zA-Z-]*)(?:\\.(?:0|[1-9]\\d*|\\d*[a-zA-Z-][0-9a-zA-Z-]*))*))?(?:\\+([0-9a-zA-Z-]+(?:\\.[0-9a-zA-Z-]+)*))?)?)?$"), std::regex::ECMAScript | std::regex::optimize);

FString ParseVersionTemplate(const FString& string, FVersion& version, EVersionComparisonOp& Comparison);

EVersionComparisonOp ParseComparisonOp(const FString& type) {
    if (type == TEXT("<=")) return EVersionComparisonOp::LESS_EQUALS;
    if (type == TEXT("<")) return EVersionComparisonOp::LESS;
    if (type == TEXT(">=")) return EVersionComparisonOp::GREATER_EQUALS;
    if (type == TEXT(">")) return EVersionComparisonOp::GREATER;
    if (type == TEXT("^")) return EVersionComparisonOp::CARET;
    if (type == TEXT("~")) return EVersionComparisonOp::TILDE;
    if (type == TEXT("=") || type == TEXT("v") || type == TEXT("")) return EVersionComparisonOp::EQUALS;
    return EVersionComparisonOp::INVALID;
}

const TCHAR* ComparisonString(const EVersionComparisonOp op) {
    switch (op) {
    case EVersionComparisonOp::EQUALS: return TEXT("");
    case EVersionComparisonOp::GREATER: return TEXT(">");
    case EVersionComparisonOp::GREATER_EQUALS: return TEXT(">=");
    case EVersionComparisonOp::LESS: return TEXT("<");
    case EVersionComparisonOp::LESS_EQUALS: return TEXT("<=");
    case EVersionComparisonOp::CARET: return TEXT("^");
    case EVersionComparisonOp::TILDE: return TEXT("~");
    default: return TEXT("");
    }
}

FVersion::FVersion() : Major(0), Minor(0), Patch(0) {
}

FVersion::FVersion(int64 MyMajor, int64 MyMinor, int64 MyPatch) : Major(MyMajor), Minor(MyMinor), Patch(MyPatch) {
}

bool FVersion::ParseVersion(const FString& String, FString& OutErrorMessage) {
    FVersion ResultVersion{};
    EVersionComparisonOp ComparisonOp = EVersionComparisonOp::EQUALS;
    FString ErrorMessage = ParseVersionTemplate(String, ResultVersion, ComparisonOp);
    if (ErrorMessage.IsEmpty()) {
        if (ComparisonOp != EVersionComparisonOp::EQUALS) {
            ErrorMessage = TEXT("Unexpected version comparator");
        }
        if (ResultVersion.ContainsSpecialVersionNumbers()) {
            ErrorMessage = TEXT("Unexpected wildcards in version declaration");
        }
    }
    OutErrorMessage = ErrorMessage;
    if (OutErrorMessage.IsEmpty()) {
        this->Major = ResultVersion.Major;
        this->Minor = ResultVersion.Minor;
        this->Patch = ResultVersion.Patch;
        this->Type = ResultVersion.Type;
        this->BuildInfo = ResultVersion.BuildInfo;
        return true;
    }
    return false;
}

bool FVersion::ContainsSpecialVersionNumbers() const {
    return Major == SEMVER_VERSION_NUMBER_WILDCARD ||
        Major == SEMVER_VERSION_NUMBER_UNSPECIFIED ||
        Minor == SEMVER_VERSION_NUMBER_WILDCARD ||
        Minor == SEMVER_VERSION_NUMBER_UNSPECIFIED ||
        Patch == SEMVER_VERSION_NUMBER_WILDCARD ||
        Patch == SEMVER_VERSION_NUMBER_UNSPECIFIED;
}

FVersion FVersion::RemoveSpecialNumbers(int64 Replacement) const {
    FVersion ResultVersion = *this;
    if (Major == SEMVER_VERSION_NUMBER_WILDCARD ||
        Major == SEMVER_VERSION_NUMBER_UNSPECIFIED) {
        ResultVersion.Major = Replacement;
    }
    if (Minor == SEMVER_VERSION_NUMBER_WILDCARD ||
        Minor == SEMVER_VERSION_NUMBER_UNSPECIFIED) {
        ResultVersion.Minor = Replacement;
    }
    if (Patch == SEMVER_VERSION_NUMBER_WILDCARD ||
        Patch == SEMVER_VERSION_NUMBER_UNSPECIFIED) {
        ResultVersion.Patch = Replacement;
    }
    return ResultVersion;
}

FVersionComparator::FVersionComparator() : Op(EVersionComparisonOp::EQUALS) {
}

FVersionComparator::FVersionComparator(EVersionComparisonOp Operator, FVersion Version) : Op(Operator), MyVersion(Version) {
}

bool FVersionComparator::ParseVersionComparator(const FString& String, FString& OutErrorMessage) {
    FVersion ResultVersion{};
    EVersionComparisonOp ComparisonOp = EVersionComparisonOp::EQUALS;
    FString ErrorMessage = ParseVersionTemplate(String, ResultVersion, ComparisonOp);
    if (ErrorMessage.IsEmpty()) {
        if (ComparisonOp == EVersionComparisonOp::EQUALS || ComparisonOp == EVersionComparisonOp::CARET) {
            // Replace all unspecified version characters with wildcard
            // For EQUALS: because partial version ranges are treated as X-Ranges
            // For CARET (^): because CARET treats all unspecified characters in the same way as wildcards
            ResultVersion = ResultVersion.RemoveSpecialNumbers(SEMVER_VERSION_NUMBER_WILDCARD);
        } else {
            // Ensure that normal version comparators never contain wildcards (except equals and caret)
            if (ResultVersion.Major == SEMVER_VERSION_NUMBER_WILDCARD ||
                ResultVersion.Minor == SEMVER_VERSION_NUMBER_WILDCARD ||
                ResultVersion.Patch == SEMVER_VERSION_NUMBER_WILDCARD) {
                ErrorMessage = TEXT("Unexpected Wildcard inside normal version comparator");
            }
            // Replace unspecified version characters with zeros, which is standard behavior for all operations except tilde (~),
            // Which uses first unspecified version number for determining it's behavior
            if (ComparisonOp != EVersionComparisonOp::TILDE) {
                ResultVersion = ResultVersion.RemoveSpecialNumbers(0);
            }
        }
    }
    // If failure reason is empty, set version and operator
    if (ErrorMessage.IsEmpty()) {
        this->MyVersion = ResultVersion;
        this->Op = ComparisonOp;
        return true;
    }
    OutErrorMessage = ErrorMessage;
    return false;
}

bool FVersionComparator::Matches(const FVersion& version) const {
    // Clear version used for comparison purposes
    const FVersion CleanVersion = MyVersion.RemoveSpecialNumbers();
    const int Result = version.Compare(CleanVersion);
    switch (Op) {
        // Normal comparison operations never encounter wildcards or unspecified version numbers, so no special behavior
    case EVersionComparisonOp::GREATER_EQUALS: return Result >= 0;
    case EVersionComparisonOp::GREATER: return Result > 0;
    case EVersionComparisonOp::LESS_EQUALS: return Result <= 0;
    case EVersionComparisonOp::LESS: return Result < 0;

        // Caret version range can have wildcards and need to handle them
    case EVersionComparisonOp::CARET: {
        // Lower bound is zeroed version we represent for caret range
        if (Result < 0) {
            return false;
        }
        FVersion MaxVersion{};
        // Check if we have any wildcards we need to handle
        if (MyVersion.ContainsSpecialVersionNumbers()) {
            // If major version is wildcard, there is no upper bound set
            // Although i'm not sure if ^X is even legal semver comparator
            if (MyVersion.Major == SEMVER_VERSION_NUMBER_WILDCARD) {
                return true;
            }
            // If minor version is wildcard, upper bound is major + 1
            if (MyVersion.Minor == SEMVER_VERSION_NUMBER_WILDCARD) {
                MaxVersion.Major = MyVersion.Major + 1;
            }
            // If patch version is wildcard, upper bound is either major or minor, but patch can be any
            if (MyVersion.Patch == SEMVER_VERSION_NUMBER_WILDCARD) {
                if (MyVersion.Major == 0) {
                    MaxVersion.Minor = MyVersion.Minor + 1;
                } else {
                    MaxVersion.Major = MyVersion.Major + 1;
                }
            }
        } else {
            // No special version numbers, fallback to normal first-non-zero handling
            if (MyVersion.Major == 0) {
                if (MyVersion.Minor == 0) {
                    // Minor is zero, allow up to next patch version
                    MaxVersion.Patch = MyVersion.Patch + 1;
                } else {
                    // Major is zero, allow up to next minor version update
                    MaxVersion.Minor = MyVersion.Minor + 1;
                }
            } else {
                // Major is not zero, allow up to next major version update
                MaxVersion.Major = MyVersion.Major + 1;
            }
        }
        // We pass if we are below max version required, exclusive
        return version.Compare(MaxVersion) < 0;
    }

        // Tilde version ranges can have unspecified numbers, and need to handle them
    case EVersionComparisonOp::TILDE: {
        // Lower bound is zeroed version we represent for tilde version range
        if (Result < 0) {
            return false;
        }
        FVersion MaxVersion{};
        // Major version number is not specified, no upper bounds
        // Although it's impossible to encounter under normal conditions, let's handle it for sake of completeness
        if (MyVersion.Major == SEMVER_VERSION_NUMBER_UNSPECIFIED) {
            return true;
            // Minor is unspecified, maximum version is Major + 1
        } else if (MyVersion.Minor == SEMVER_VERSION_NUMBER_UNSPECIFIED) {
            MaxVersion.Major = MyVersion.Major + 1;
            // Patch version is unspecified, maximum version is Minor + 1 while keeping normal Major
        } else if (MyVersion.Patch == SEMVER_VERSION_NUMBER_UNSPECIFIED) {
            MaxVersion.Major = MyVersion.Major;
            MaxVersion.Minor = MyVersion.Minor + 1;
            // Version contains no unspecified numbers, so maximum version is Patch + 1 while keeping Major and Minor
        } else {
            MaxVersion.Major = MyVersion.Major;
            MaxVersion.Minor = MyVersion.Minor;
            MaxVersion.Patch = MyVersion.Patch + 1;
        }
        // We pass if we are below max version required, exclusive
        return version.Compare(MaxVersion) < 0;
    }

        // Equals versions can represent X-Ranges, so we need to handle wildcards inside them
    case EVersionComparisonOp::EQUALS: {
        // We have wildcards, so go from Major to Patch to compare them
        // Major is wildcard, we accept any versions
        if (MyVersion.Major == SEMVER_VERSION_NUMBER_WILDCARD) {
            return true;
        }
        // Minor is wildcard, we accept everything as long as Major matches
        if (MyVersion.Minor == SEMVER_VERSION_NUMBER_WILDCARD) {
            return MyVersion.Major == version.Major;
        }
        // Patch is wildcard, we accept everything as long as Major and Minor match
        if (MyVersion.Patch == SEMVER_VERSION_NUMBER_WILDCARD) {
            return MyVersion.Major == version.Major && MyVersion.Minor == version.Minor;
        }
        // We represent fixed version number comparator, so just return true if versions are equal
        return Result == 0;
    }

        // Fallback default case to equals
    default: {
        return Result == 0;
    }
    }
}

FVersionComparatorCollection::FVersionComparatorCollection() {
}

bool ParseVersionForHyphenRange(const FString& String, FVersion& Version, FString& OutErrorMessage) {
    EVersionComparisonOp ComparisonOp = EVersionComparisonOp::EQUALS;
    const FString ParseErrorMessage = ParseVersionTemplate(String, Version, ComparisonOp);
    // Basic version parsing failed, pass failure code
    if (!ParseErrorMessage.IsEmpty()) {
        OutErrorMessage = ParseErrorMessage;
        return false;
    }
    // Hyphen version ranges cannot contain comparison operators
    if (ComparisonOp != EVersionComparisonOp::EQUALS) {
        OutErrorMessage = TEXT("Unexpected version comparator inside Hyphen Version Range");
        return false;
    }
    // Hyphen version ranges cannot contain wildcards
    if (Version.Major == SEMVER_VERSION_NUMBER_WILDCARD ||
        Version.Minor == SEMVER_VERSION_NUMBER_WILDCARD ||
        Version.Patch == SEMVER_VERSION_NUMBER_WILDCARD) {
        OutErrorMessage = TEXT("Unexpected wildcard inside Hyphen Version Range");
        return false;
    }
    // They can contain unspecified version numbers, though
    return true;
}

bool ParseHyphenVersionRange(const FString& LeftSideString, const FString& RightSideString, TArray<FVersionComparator>& OutComparators, FString& OutErrorMessage) {
    // Parse version strings according to hyphen version range rules
    FVersion LeftSideVersion;
    const bool bSuccessLeft = ParseVersionForHyphenRange(LeftSideString, LeftSideVersion, OutErrorMessage);
    FVersion RightSideVersion;
    const bool bSuccessRight = ParseVersionForHyphenRange(RightSideString, RightSideVersion, OutErrorMessage);
    // Parsing of one of the versions failed
    if (!bSuccessLeft || !bSuccessRight) {
        return false;
    }
    // Unspecified numbers in left side are replaced with zeroes
    const FVersion ResultLowerBoundVersion = LeftSideVersion.RemoveSpecialNumbers(0);

    // Unspecified numbers in right side are treated as allowed versions
    bool bIncludeUpperBound;
    FVersion ResultUpperBoundVersion{};
    if (RightSideVersion.Major == SEMVER_VERSION_NUMBER_UNSPECIFIED) {
        // Major is unspecified, this version range is invalid
        OutErrorMessage = TEXT("Invalid Major version in right side of Hyphen Version Range");
        return false;
    } else if (RightSideVersion.Minor == SEMVER_VERSION_NUMBER_UNSPECIFIED) {
        // Minor in right side version is not specified, allow everything up to next major
        ResultUpperBoundVersion.Major = RightSideVersion.Major + 1;
        bIncludeUpperBound = false;
    } else if (RightSideVersion.Patch == SEMVER_VERSION_NUMBER_UNSPECIFIED) {
        // Patch in right side version is not specified, allow everything up to next minor
        ResultUpperBoundVersion.Major = RightSideVersion.Major;
        ResultUpperBoundVersion.Minor = RightSideVersion.Minor + 1;
        bIncludeUpperBound = false;
    } else {
        // No unspecified version numbers, upper bound is inclusive
        // Assign right side version directly because it's complete and can have build information and type
        // we want to retain in resulting comparisons
        ResultUpperBoundVersion = RightSideVersion;
        bIncludeUpperBound = true;
    }
    // Generate comparators from these versions
    OutComparators.Add(FVersionComparator(EVersionComparisonOp::GREATER_EQUALS, ResultLowerBoundVersion));
    const EVersionComparisonOp ResultUpperBoundOp = bIncludeUpperBound ? EVersionComparisonOp::LESS_EQUALS : EVersionComparisonOp::LESS;
    OutComparators.Add(FVersionComparator(ResultUpperBoundOp, ResultUpperBoundVersion));
    return true;
}

bool FVersionComparatorCollection::ParseVersionCollection(const FString& String, FString& OutErrorMessage) {
    // Result comparator list
    TArray<FVersionComparator> ResultArray;
    // Keep last matched version string without making comparator out of it for hyphen ranges
    FString LastMatchedVersionString;
    // Keep left side string of hyphen range we currently process
    FString CachedLeftSideHyphenRangeString;
    int32 CurrentIndex = 0;
    bool bIsCurrentlyInString = true;
    bool bHaveHyphenAfterLastString = false;
    while (CurrentIndex < String.Len()) {
        const TCHAR CurrentChar = String[CurrentIndex++];
        if (CurrentChar == TEXT(' ')) {
            // Current character is space. If we're inside string, reset flag
            bIsCurrentlyInString = false;
        } else if (CurrentChar == TEXT('-')) {
            // Current character is hyphen. We are no longer inside string, but we have hyphen after last string
            bIsCurrentlyInString = false;
            // We shouldn't have hyphen at this point, only previous string
            if (bHaveHyphenAfterLastString) {
                OutErrorMessage = TEXT("Unexpected hyphen");
                return false;
            }
            bHaveHyphenAfterLastString = true;
            // Last matched string shouldn't be empty, otherwise we have hyphen at the start
            // If we have cached left side hyphen range, we shouldn't encounter hyphen before we complete it too
            if (LastMatchedVersionString.IsEmpty() || !CachedLeftSideHyphenRangeString.IsEmpty()) {
                OutErrorMessage = TEXT("Unexpected hyphen");
                return false;
            }
        } else {
            // It's some other character. Append it to string if we are not inside it
            if (!bIsCurrentlyInString) {
                // We are not inside string right now, but it's first character of other string
                // Skip handling code if last matched string is empty (can happen due to excessive spaces)
                if (!LastMatchedVersionString.IsEmpty()) {
                    if (!CachedLeftSideHyphenRangeString.IsEmpty()) {
                        // We have cached left side hyphen range string, join it with last string and process as hyphen version range
                        const bool bSuccess = ParseHyphenVersionRange(CachedLeftSideHyphenRangeString, LastMatchedVersionString, ResultArray, OutErrorMessage);
                        CachedLeftSideHyphenRangeString.Empty();
                        if (!bSuccess) {
                            return false;
                        }
                    } else if (bHaveHyphenAfterLastString) {
                        // We had hyphen before current string. Cache last string into separate field, and continue normally
                        CachedLeftSideHyphenRangeString = LastMatchedVersionString;
                    } else {
                        FVersionComparator Comparator{};
                        const bool bSuccess = Comparator.ParseVersionComparator(LastMatchedVersionString, OutErrorMessage);
                        ResultArray.Add(Comparator);
                        if (!bSuccess) {
                            return false;
                        }
                    }
                }
                // Set inString to true, clear last string, and reset hyphen flag now
                LastMatchedVersionString.Empty();
                bIsCurrentlyInString = true;
                bHaveHyphenAfterLastString = false;
            }
            LastMatchedVersionString.AppendChar(CurrentChar);
        }
    }
    // Process last cached string if it's not empty
    if (!LastMatchedVersionString.IsEmpty()) {
        if (!CachedLeftSideHyphenRangeString.IsEmpty()) {
            // We have cached left side hyphen range string, join it with last string and process as hyphen version range
            const bool bSuccess = ParseHyphenVersionRange(CachedLeftSideHyphenRangeString, LastMatchedVersionString, ResultArray, OutErrorMessage);
            CachedLeftSideHyphenRangeString.Empty();
            if (!bSuccess) {
                return false;
            }
        } else if (bHaveHyphenAfterLastString) {
            // We cannot have hyphen at this point, it means that we have ended with it, which is wrong
            OutErrorMessage = TEXT("Unexpected hyphen");
            return false;
        } else {
            // Otherwise it is a normal version comparator
            FVersionComparator Comparator{};
            const bool bSuccess = Comparator.ParseVersionComparator(LastMatchedVersionString, OutErrorMessage);
            ResultArray.Add(Comparator);
            if (!bSuccess) {
                return false;
            }
        }
    }

    // Comparator array cannot be empty, at least one is required
    if (ResultArray.Num() == 0) {
        OutErrorMessage = TEXT("Version Comparator Collection cannot be empty");
        return false;
    }
    this->Comparators = ResultArray;
    return true;
}

FString FVersionComparatorCollection::ToString() const {
    TArray<FString> ResultString;
    for (const FVersionComparator& Comparator : Comparators) {
        ResultString.Add(Comparator.ToString());
    }
    return FString::Join(ResultString, TEXT(" "));
}

bool FVersionComparatorCollection::Matches(const FVersion& version) const {
    for (const FVersionComparator& Comparator : Comparators) {
        if (!Comparator.Matches(version)) {
            return false;
        }
    }
    // We can only return true if none of our comparators returned false
    return true;
}

FVersionRange::FVersionRange() {
}

FVersionRange FVersionRange::CreateAnyVersionRange() {
    FVersionRange VersionRange{};
    FVersionComparatorCollection VersionComparatorCollection{};
    const FVersion AnyVersion{SEMVER_VERSION_NUMBER_WILDCARD, SEMVER_VERSION_NUMBER_WILDCARD, SEMVER_VERSION_NUMBER_WILDCARD};
    const FVersionComparator Comparator(EVersionComparisonOp::EQUALS, AnyVersion);
    VersionComparatorCollection.Comparators.Add(Comparator);
    VersionRange.Collections.Add(VersionComparatorCollection);
    return VersionRange;
}

FVersionRange FVersionRange::CreateRangeWithMinVersion(const FVersion& MinVersion) {
    FVersionRange VersionRange{};
    FVersionComparatorCollection VersionComparatorCollection{};
    const FVersionComparator Comparator(EVersionComparisonOp::GREATER_EQUALS, MinVersion);
    VersionComparatorCollection.Comparators.Add(Comparator);
    VersionRange.Collections.Add(VersionComparatorCollection);
    return VersionRange;
}

bool FVersionRange::ParseVersionRange(const FString& String, FString& OutErrorMessage) {
    // Just split input string by || and evaluate each piece as individual collection
    TArray<FVersionComparatorCollection> ResultCollections;
    TArray<FString> CollectionStrings;
    String.ParseIntoArray(CollectionStrings, TEXT("||"));

    // Version range cannot be empty
    if (CollectionStrings.Num() == 0) {
        OutErrorMessage = TEXT("Version range cannot be empty");
        return false;
    }

    // Evaluate each string as collection
    for (const FString& CollectionString : CollectionStrings) {
        FVersionComparatorCollection Collection{};
        const bool bSuccess = Collection.ParseVersionCollection(CollectionString, OutErrorMessage);
        ResultCollections.Add(Collection);
        if (!bSuccess) {
            return false;
        }
    }
    this->Collections = ResultCollections;
    return true;
}

FString FVersionRange::ToString() const {
    TArray<FString> ResultArray;
    for (const FVersionComparatorCollection& CollectionElement : Collections) {
        ResultArray.Add(CollectionElement.ToString());
    }
    return FString::Join(ResultArray, TEXT(" || "));
}

bool FVersionRange::Matches(const FVersion& Version) const {
    // Either of collections should match for range to match
    for (const FVersionComparatorCollection& CollectionElement : Collections) {
        if (CollectionElement.Matches(Version)) {
            return true;
        }
    }
    // No collection matched, return false
    return false;
}

bool IsWildcardVersionNumber(const std::wstring& Number) {
    return Number == TEXT("X") || Number == TEXT("x") || Number == TEXT("*");
}

// Parses version number, taking care of wildcard characters and empty string
int64 ParseVersionNumber(const std::wstring& Number) {
    if (IsWildcardVersionNumber(Number)) {
        return SEMVER_VERSION_NUMBER_WILDCARD;
    }
    if (Number == TEXT("")) {
        return SEMVER_VERSION_NUMBER_UNSPECIFIED;
    }
    return std::stoul(Number);
}

FString ParseVersionTemplate(const FString& string, FVersion& version, EVersionComparisonOp& Comparison) {
    std::wsmatch Match;
    const std::wstring WideString(*string);
    if (!std::regex_match(WideString, Match, VersionRegex)) {
        return TEXT("Version doesn't match SemVer pattern");
    }
    const std::wstring& FirstMatch = Match[1];
    Comparison = ParseComparisonOp(FirstMatch.c_str());

    if (Comparison == EVersionComparisonOp::INVALID) {
        return TEXT("Invalid version comparator");
    }
    version.Major = ParseVersionNumber(Match[2]);
    version.Minor = ParseVersionNumber(Match[3]);
    version.Patch = ParseVersionNumber(Match[4]);
    // Make sure patch is always not specified or wildcard if major/minor are wildcard
    if (version.Major == SEMVER_VERSION_NUMBER_WILDCARD || version.Minor == SEMVER_VERSION_NUMBER_WILDCARD) {
        if (version.Patch != SEMVER_VERSION_NUMBER_WILDCARD &&
            version.Patch != SEMVER_VERSION_NUMBER_UNSPECIFIED) {
            return TEXT("Wildcard cannot be followed by version number");
        }
    }
    // Make sure minor is always wildcard or not specified if major is wildcard
    if (version.Major == SEMVER_VERSION_NUMBER_WILDCARD) {
        if (version.Minor != SEMVER_VERSION_NUMBER_WILDCARD &&
            version.Minor != SEMVER_VERSION_NUMBER_UNSPECIFIED) {
            return TEXT("Wildcard cannot be followed by version number");
        }
    }
    version.Type = Match[5].str().c_str();
    version.BuildInfo = Match[6].str().c_str();
    return TEXT("");
}

FString FromIntWithWildcard(const int64 VersionNumber) {
    if (VersionNumber == SEMVER_VERSION_NUMBER_WILDCARD) {
        return TEXT("x");
    }
    return FString::FromInt(VersionNumber);
}

FString FVersion::ToString() const {
    FString Result;
    Result.Append(FromIntWithWildcard(Major)).Append(TEXT("."));

    if (Minor != SEMVER_VERSION_NUMBER_UNSPECIFIED) {
        Result.Append(FromIntWithWildcard(Minor)).Append(TEXT("."));
    }
    if (Patch != SEMVER_VERSION_NUMBER_UNSPECIFIED) {
        Result.Append(FromIntWithWildcard(Patch));
    }
    if (!Type.IsEmpty()) Result.Append(TEXT("-")).Append(Type);
    if (!BuildInfo.IsEmpty()) Result.Append(TEXT("+")).Append(BuildInfo);
    return Result;
}

FString FVersionComparator::ToString() const {
    return ComparisonString(this->Op) + MyVersion.ToString();
}

int FVersion::Compare(const FVersion& other) const {
    if (Major != other.Major) return Major > other.Major ? 1 : -1;
    if (Minor != other.Minor) return Minor > other.Minor ? 1 : -1;
    if (Patch != other.Patch) return Patch > other.Patch ? 1 : -1;
    return Type.Compare(other.Type);
}
