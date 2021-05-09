#pragma once
#include "CoreMinimal.h"
#include "SemVersion.generated.h"

// Wildcard version number value for Major/Minor/Patch in VersionComparator
#define SEMVER_VERSION_NUMBER_WILDCARD -1
// Unspecified version number value for Major/Minor/Patch in VersionComparator
#define SEMVER_VERSION_NUMBER_UNSPECIFIED -2

UENUM(BlueprintType)
enum class EVersionComparisonOp : uint8 {
    EQUALS,
    GREATER,
    GREATER_EQUALS,
    LESS,
    LESS_EQUALS,
    CARET,
    TILDE,
    INVALID
};

/* Struct describing versioning information */
USTRUCT(BlueprintType)
struct SML_API FVersion {
    GENERATED_USTRUCT_BODY()
public:
    /** Major version number. */
    UPROPERTY(BlueprintReadWrite)
    int64 Major;
    /** Minor version number */
    UPROPERTY(BlueprintReadWrite)
    int64 Minor;
    /** Patch number */
    UPROPERTY(BlueprintReadWrite)
    int64 Patch;
    /** Version type (alpha/beta/release) */
    UPROPERTY(BlueprintReadWrite)
    FString Type;
    /** Additional version build info */
    UPROPERTY(BlueprintReadWrite)
    FString BuildInfo;

    FVersion();
    FVersion(int64 MyMajor, int64 MyMinor, int64 MyPatch);
public:
    /* Parses version string for valid version declaration. Does not allow wildcards or unspecified version numbers */
    bool ParseVersion(const FString& String, FString& OutErrorMessage);

    /** Returns true if this version contains version numbers with special meaning */
    bool ContainsSpecialVersionNumbers() const;
    /* Returns the version with special numbers replaced with zeros */
    FVersion RemoveSpecialNumbers(int64 Replacement = 0) const;
    /** Combines version data into the string */
    FString ToString() const;
    /** Compares this version with other version */
    int Compare(const FVersion& other) const;
};

/** A single version comparator of version range */
USTRUCT(BlueprintType)
struct SML_API FVersionComparator {
    GENERATED_USTRUCT_BODY()
public:
    /* Comparison operator used for this comparator */
    UPROPERTY(BlueprintReadWrite)
    EVersionComparisonOp Op;
    /* Version it compares against */
    UPROPERTY(BlueprintReadWrite)
    FVersion MyVersion;

    FVersionComparator();
    FVersionComparator(EVersionComparisonOp Operator, FVersion Version);
public:
    /**
     * Parses version comparator. It accepts any supported version comparator types, including:
     * >=, <=, <, >, equals (=), tilde (~), caret (^)
     * It also supports X-Ranges for equals (1.2.X) and omitting minor/patch
     *
     * Hyphen Ranges (X.Y.Z - A.B.C) are not handled by this, they are handled by FVersionComparatorCollection,
     * because they require special handling and represent 2 versions to represent
     */
    bool ParseVersionComparator(const FString& String, FString& OutErrorMessage);

    FString ToString() const;
    bool Matches(const FVersion& version) const;
};

/** Represents AND-joined version comparator collection. It evaluates to true if all comparators do */
USTRUCT(BlueprintType)
struct SML_API FVersionComparatorCollection {
    GENERATED_USTRUCT_BODY()
public:
    /* List of comparators to join */
    UPROPERTY(BlueprintReadWrite)
    TArray<FVersionComparator> Comparators;

    FVersionComparatorCollection();
public:
    /*
     * Parses AND-joined FVersionComparator collection. It supports all comparator types,
     * And also includes support for Hyphen Ranges (X.Y.Z - A.B.C)
     * Unspecified version numbers in hyphen ranges are handled according to semver documentation
     * They are decomposed into >=/<= upon parsing though, so String() output will never contain them
     */
    bool ParseVersionCollection(const FString& String, FString& OutErrorMessage);

    FString ToString() const;
    bool Matches(const FVersion& version) const;
};

/* Represents version constraints that version can be matched against */
USTRUCT(BlueprintType)
struct SML_API FVersionRange {
    GENERATED_USTRUCT_BODY()
public:
    /* List of collections to check for. Any of them can return true for range to succeed */
    UPROPERTY(BlueprintReadWrite)
    TArray<FVersionComparatorCollection> Collections;

    FVersionRange();
public:
    /** Creates version range that matches any version */
    static FVersionRange CreateAnyVersionRange();

    /* Creates version range with minimal version specified */
    static FVersionRange CreateRangeWithMinVersion(const FVersion& MinVersion);

    /**
     * Parses SemVer version range. It supports variety of syntactic sugar, including:
     *  - Ability to abandon minor/patch versions in certain cases
     *  - Hyphen Version Ranges (X.Y.Z - A.B.C)
     *  - X-Ranges comparators (1.2.X, 1.*.*, 1.3.x)
     *  - Caret version ranges (^1.2.3)
     *  - Tilde version ranges (~1.2)
     *  - All standard comparators, including <= >= < > =
     *
     *  @param String input string
     *  @param OutErrorMessage reason for parsing failure if result is false
     *  @return true when parsing succeeded, false otherwise
     */
    bool ParseVersionRange(const FString& String, FString& OutErrorMessage);

    /* Evaluates this version range against passed version. Returns true if passed version satisfies it */
    bool Matches(const FVersion& Version) const;

    /**
     *    Converts this version range to string.
     *  It can be parsed back into range matching same versions,
     *  but it will not always equal to string used to initialize this version range
     *  For example, output string will never contain hyphen version ranges
     */
    FString ToString() const;
};
