#pragma once

// Contains macros useful for automatically generating boilerplate code among various places in FactoryGame
// Currently it only contains IFGSaveInterface implementation, but you can add extra stuff here!

// Declares standard method overrides for IFGSaveInterface
#define DECLARE_FG_SAVE_INTERFACE() \
public: \
    virtual void PreSaveGame_Implementation(int32 saveVersion, int32 gameVersion) override; \
    virtual void PostSaveGame_Implementation(int32 saveVersion, int32 gameVersion) override; \
    virtual void PreLoadGame_Implementation(int32 saveVersion, int32 gameVersion) override; \
    virtual void PostLoadGame_Implementation(int32 saveVersion, int32 gameVersion) override; \
    virtual void GatherDependencies_Implementation( TArray< UObject* >& out_dependentObjects) override; \
    virtual bool NeedTransform_Implementation() override; \
    virtual bool ShouldSave_Implementation() const override;

#define IMPLEMENT_FG_SAVE_INTERFACE(ClassName, NeedsTransform, ShouldSave) \
    void ClassName::PreLoadGame_Implementation(int32 saveVersion, int32 gameVersion) {} \
    void ClassName::PostSaveGame_Implementation(int32 saveVersion, int32 gameVersion) {} \
    void ClassName::PreSaveGame_Implementation(int32 saveVersion, int32 gameVersion) {} \
    bool ClassName::NeedTransform_Implementation() { return NeedsTransform; } \
    bool ClassName::ShouldSave_Implementation() const { return ShouldSave; } \
    void ClassName::PostLoadGame_Implementation(int32 saveVersion, int32 gameVersion) {} \
    void ClassName::GatherDependencies_Implementation(TArray<UObject*>& out_dependentObjects) {}
