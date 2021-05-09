#pragma once
#include "UObject/Object.h"
#include "UObject/Stack.h"

/** 
 * Holds contextual information about function execution by the time hook is called
 * Can be used to retrieve self object reference, local variable values and
 * manipulate execution of the byte-code beyond hook
 */
class SML_API FBlueprintHookHelper {
public:
    FFrame& FramePointer;
private:
    int32 ReturnInstructionOffset;
public:
    FORCEINLINE FBlueprintHookHelper(FFrame& Frame, const int32 MyReturnInstructionOffset) : FramePointer(Frame), ReturnInstructionOffset(MyReturnInstructionOffset) {
    }

    /**
     * Returns object pointer to the object which current function is executed on
     */
    FORCEINLINE UObject* GetContext() const { return FramePointer.Object; }

    /**
     * Jumps to function's EX_Return instruction to terminate function execution
     * Function execution will be finished after all hooks are processed, so other
     * hooks registered for same offset will be called anyway
     * @note Other hooks at the same offset will be still called in the normal order, even if one of them jumps to function return
     */
    void JumpToFunctionReturn() const {
        FramePointer.Code = &FramePointer.Node->Script[ReturnInstructionOffset];
    }

    /**
     * Retrieves local variable pointer from stack frame by the provided name
     * This method won't work on [out] variables, as these are stored separately from local stack frame
     * Passed pointer is guaranteed to be valid for the duration of hook execution
     *
     * @param ParameterName name of the local variable to retrieve
     * @param ArrayIndex if variable represents a statically sized array, retrieve value at specified index
     * @return pointer to the local variable value
     * @note Will check false if provided variable name represents an [out] variable
     */
    template<typename T>
    typename T::TCppType* GetLocalVarPtr(const TCHAR* ParameterName, int32 ArrayIndex = 0) const {
        T* Property = Cast<T>(FramePointer.Node->FindPropertyByName(ParameterName));
        check(Property);
        UProperty* Prop;
        checkf(!Property->HasAnyPropertyFlags(CPF_OutParm), TEXT("Attempt to use GetLocalVarPtr on [out] variable"));
        return Property->GetPropertyValuePtr_InContainer(FramePointer.Locals, ArrayIndex);
    }

    /**
     * Retrieves variable passed to method as [out] parameter (pass by reference basically), e.g which
     * Can be set by blueprint method and changes will be visible to caller
     * These variables are stored separately from other local variables, so
     * GetLocalVarPtr won't work on them, you should use this method instead
     * Passed pointer is guaranteed to be valid for the duration of hook execution
     * 
     * @param VariableName name of the [out] variable
     * @return pointer to the current value stored in the [out] variable
     * @note Only works on variables which are passed to the function by reference, including ReturnValue
     */
    template<typename T>
    typename T::TCppType* GetOutVariablePtr(const TCHAR* VariableName = TEXT("ReturnValue")) const {
        FOutParmRec* Out = FramePointer.OutParms;
        check(Out);
        while (Out->Property->GetName() != VariableName) {
            Out = Out->NextOutParm;
            check(Out);
        }
        check(Out->Property->GetName() == VariableName);
        T* Property = Cast<T>(Out->Property);
        check(Property);
        return Property->GetPropertyValuePtr(Out->PropAddr);
    }
};
