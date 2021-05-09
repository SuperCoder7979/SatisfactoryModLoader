#pragma once
#include "CoreMinimal.h"
#include <functional>
#include <type_traits>

DECLARE_LOG_CATEGORY_EXTERN(LogNativeHookManager, Log, Log);

// NOTE: This struct does not actually fully represent member function pointer even on MSVC,
// because it does not contain additional information for handling unknown inheritance
// Besides, MSVC layout for "unknown inheritance" and "virtual inheritance" is different
// See https://www.codeproject.com/Articles/7150/Member-Function-Pointers-and-the-Fastest-Possible
// So yeah, casting function pointer to this struct is not really safe on other compilers, and even on MSVC
// But we're not really trying to support unknown inheritance, so it's fine.
struct FMemberFunctionPointer {
    void* FunctionAddress;
    uint32 ThisAdjustment;
    uint32 VtableDisplacement;
};

template<typename T>
struct TMemberFunctionPointer {
    T MemberFunctionPointer;
};

template<typename T>
FORCEINLINE FMemberFunctionPointer ConvertFunctionPointer(const TMemberFunctionPointer<T>* SourcePointer) {
    const SIZE_T FunctionPointerSize = sizeof(SourcePointer);

    // We only support non-virtual inheritance, so assert on virtual inheritance and unknown inheritance cases
    // Note that it might also mean that we are dealing with "proper" compiler with static function pointer size
    // (e.g anything different from Intel C++ and MSVC)
    checkf(FunctionPointerSize == 8 || FunctionPointerSize == 16, TEXT("Unsupported function pointer size received: \
        Hooking can only support non-virtual multiple inheritence. \
        This might be also caused by unsupported compiler. Currently, only MSVC and Intel C++ are supported\
        Function pointer size: %d bytes."), FunctionPointerSize);

    const FMemberFunctionPointer* RawFunctionPointer = (const FMemberFunctionPointer*)SourcePointer;

    // TODO we  cannot really make sure there is no virtual inheritance involved, so we just assume it for now
    FMemberFunctionPointer ResultPointer{};
    ResultPointer.FunctionAddress = RawFunctionPointer->FunctionAddress;
    if (FunctionPointerSize >= 16) {
        ResultPointer.ThisAdjustment = RawFunctionPointer->ThisAdjustment;
        ResultPointer.VtableDisplacement = RawFunctionPointer->VtableDisplacement;
    }

    return ResultPointer;
}

class SML_API FNativeHookManagerInternal {
public:
    static void* GetHandlerListInternal(void* RealFunctionAddress);
    static void SetHandlerListInstanceInternal(void* RealFunctionAddress, void* handlerList);
    static void* RegisterHookFunction(const FString& DebugSymbolName, void* OriginalFunctionPointer, void* SampleObjectInstance, int ThisAdjustment, void* HookFunctionPointer, void** OutTrampolineFunction);
};

template<typename T, typename E>
struct THandlerLists {
    TArray<T> HandlersBefore;
    TArray<E> HandlersAfter;
};

template<typename T, typename E>
THandlerLists<T, E>* createHandlerLists(void* RealFunctionAddress) {
    void* handlerListRaw = FNativeHookManagerInternal::GetHandlerListInternal(RealFunctionAddress);
    if (handlerListRaw == nullptr) {
        handlerListRaw = new THandlerLists<T, E>();
        FNativeHookManagerInternal::SetHandlerListInstanceInternal(RealFunctionAddress, handlerListRaw);
    }
    return static_cast<THandlerLists<T, E>*>(handlerListRaw);
}

template<typename TCallable, TCallable Callable>
struct HookInvoker;

template<typename TCallable>
struct CallScope;

// CallResult specialization for void
template<typename... Args>
struct CallScope<void(*)(Args ...)> {
public:
    typedef void HookType(Args ...);
    typedef void HookFuncSig(CallScope<void(*)(Args ...)>&, Args ...);
    typedef std::function<HookFuncSig> HookFunc;

private:
    TArray<HookFunc>* functionList;
    size_t handlerPtr = 0;
    HookType* function;

    bool forwardCall = true;

public:
    CallScope(TArray<HookFunc>* functionList, HookType* function) : functionList(functionList), function(function) {
    }

    inline bool shouldForwardCall() const {
        return forwardCall;
    }

    void Cancel() {
        forwardCall = false;
    }

    inline void operator()(Args ... args) {
        if (functionList == nullptr || handlerPtr >= functionList->Num()) {
            function(args...);
            forwardCall = false;
        } else {
            auto cachePtr = handlerPtr + 1;
            auto& handler = (*functionList)[handlerPtr++];
            handler(*this, args...);
            if (handlerPtr == cachePtr && forwardCall) {
                (*this)(args...);
            }
        }
    }
};

// general template for other types
template<typename Result, typename... Args>
struct CallScope<Result(*)(Args ...)> {
public:
    // typedef Result HookType(Args...);
    typedef void HookFuncSig(CallScope<Result(*)(Args ...)>&, Args ...);
    typedef std::function<HookFuncSig> HookFunc;

    typedef std::function<Result(Args ...)> HookType;
private:
    TArray<HookFunc>* functionList;
    size_t handlerPtr = 0;
    HookType function;

    bool forwardCall = true;
    Result result;

public:
    CallScope(TArray<HookFunc>* functionList, HookType function) : functionList(functionList), function(function) {
    }

    inline bool shouldForwardCall() {
        return forwardCall;
    }

    inline Result getResult() {
        return result;
    }

    void Override(const Result& newResult) {
        this->forwardCall = false;
        this->result = newResult;
    }

    inline Result operator()(Args ... args) {
        if (functionList == nullptr || handlerPtr >= functionList->Num()) {
            result = function(args...);
            this->forwardCall = false;
        } else {
            auto cachePtr = handlerPtr + 1;
            auto handler = (*functionList)[handlerPtr++];
            handler(*this, args...);
            if (handlerPtr == cachePtr && forwardCall) {
                (*this)(args...);
            }
        }
        return result;
    }
};

template<typename Ret, typename... A>
class HandlerAfterFunc {
public:
    typedef void Value(const Ret&, A ...);
};

template<typename... A>
class HandlerAfterFunc<void, A...> {
public:
    typedef void Value(A ...);
};

template<typename T>
struct FMemberFunctionStruct {
    T MemberFunctionPtr;
};

// Hook invoker for global functions
template<typename TCallable, TCallable Callable, typename ReturnType, typename... ArgumentTypes>
struct HookInvokerExecutorGlobalFunction {
public:
    using HookType = TCallable;
    using ScopeType = CallScope<TCallable>;
    using HandlerSignature = void(ScopeType&, ArgumentTypes ...);
    using HandlerSignatureAfter = typename HandlerAfterFunc<ReturnType, ArgumentTypes...>::Value;
    using Handler = std::function<HandlerSignature>;
    using HandlerAfter = std::function<HandlerSignatureAfter>;
private:
    static TArray<Handler>* handlersBefore;
    static TArray<HandlerAfter>* handlersAfter;
    static TCallable functionPtr;
    static bool bHookInitialized;
public:
    static ReturnType applyCall(ArgumentTypes ... args) {
        ScopeType scope(handlersBefore, functionPtr);
        scope(args...);
        for (HandlerAfter& handler : *handlersAfter) handler(scope.getResult(), args...);
        return scope.getResult();
    }

    static void applyCallVoid(ArgumentTypes ... args) {
        ScopeType scope(handlersBefore, functionPtr);
        scope(args...);
        for (HandlerAfter& handler : *handlersAfter) handler(args...);
    }

private:
    static TCallable getApplyRef(std::true_type) {
        return &applyCallVoid;
    }

    static TCallable getApplyRef(std::false_type) {
        return &applyCall;
    }

    static TCallable getApplyCall() {
        return getApplyRef(std::is_same<ReturnType, void>{});
    }

public:
    // This hook invoker is for global non-member static functions, so we don't have to deal with
    // member function pointers and virtual functions here
    static void InstallHook(const FString& DebugSymbolName) {
        if (!bHookInitialized) {
            bHookInitialized = true;
            void* HookFunctionPointer = static_cast<void*>(getApplyCall());
            void* RealFunctionAddress = FNativeHookManagerInternal::RegisterHookFunction(DebugSymbolName, Callable, NULL, 0, HookFunctionPointer, (void**)&functionPtr);
            auto* HandlerLists = createHandlerLists<Handler, HandlerAfter>(RealFunctionAddress);
            handlersBefore = &HandlerLists->HandlersBefore;
            handlersAfter = &HandlerLists->HandlersAfter;
        }
    }

    static void addHandlerBefore(Handler handler) {
        handlersBefore->Add(handler);
    }

    static void addHandlerAfter(HandlerAfter handler) {
        handlersAfter->Add(handler);
    }
};

// Hook invoker for member functions
template<typename TCallable, TCallable Callable, bool bIsConst, typename ReturnType, typename CallableType, typename... ArgumentTypes>
struct HookInvokerExecutorMemberFunction {
public:
    using ConstCorrectThisPtr = std::conditional_t<bIsConst, const CallableType*, CallableType*>;
    using CallScopeFunctionSignature = ReturnType(*)(ConstCorrectThisPtr, ArgumentTypes ...);
    typedef CallScope<CallScopeFunctionSignature> ScopeType;

    typedef void HandlerSignature(ScopeType&, ConstCorrectThisPtr, ArgumentTypes ...);
    typedef typename HandlerAfterFunc<ReturnType, ConstCorrectThisPtr, ArgumentTypes...>::Value HandlerSignatureAfter;
    typedef ReturnType HookType(ConstCorrectThisPtr, ArgumentTypes ...);

    using Handler = std::function<HandlerSignature>;
    using HandlerAfter = std::function<HandlerSignatureAfter>;
private:
    static TArray<Handler>* handlersBefore;
    static TArray<HandlerAfter>* handlersAfter;
    static HookType* functionPtr;
    static bool bHookInitialized;

    // Methods which return class/struct/union by value have out pointer inserted
    // as first parameter after this pointer, with all arguments shifted right by 1 for it
    static ReturnType* applyCallUserTypeByValue(CallableType* self, ReturnType* outReturnValue, ArgumentTypes ... args) {
        // Capture the pointer of the return value
        // so ScopeType does not have to know about that special case
        auto Trampoline = [&](ConstCorrectThisPtr self_, ArgumentTypes ... args_) ->ReturnType {
            (reinterpret_cast<ReturnType*(*)(ConstCorrectThisPtr, ReturnType*, ArgumentTypes ...)>(functionPtr))(self_, outReturnValue, args_...);
            return *outReturnValue;
        };

        ScopeType scope(handlersBefore, Trampoline);
        scope(self, args...);
        for (HandlerAfter& handler : *handlersAfter) handler(scope.getResult(), self, args...);
        // We always return outReturnValue, so copy our result to output variable and return it
        *outReturnValue = scope.getResult();
        return outReturnValue;
    }

    // Normal scalar type call, where no additional arguments are inserted
    // If it were returning user type by value, first argument would be R*, which is incorrect - that's why we need separate
    // applyCallUserType with correct argument order
    static ReturnType applyCallScalar(CallableType* self, ArgumentTypes ... args) {
        ScopeType scope(handlersBefore, functionPtr);
        scope(self, args...);
        for (HandlerAfter& handler : *handlersAfter) handler(scope.getResult(), self, args...);
        return scope.getResult();
    }

    // Call for void return type - nothing special to do with void
    static void applyCallVoid(CallableType* self, ArgumentTypes ... args) {
        ScopeType scope(handlersBefore, functionPtr);
        scope(self, args...);
        for (HandlerAfter& handler : *handlersAfter) handler(self, args...);
    }

    static void* getApplyCall1(std::true_type) {
        return (void*)applyCallVoid; // true - type is void
    }

    static void* getApplyCall1(std::false_type) {
        return getApplyCall2(std::is_class<ReturnType>{}); // not a void, try call 2
    }

    static void* getApplyCall2(std::true_type) {
        return (void*)applyCallUserTypeByValue; // true - type is class
    }

    static void* getApplyCall2(std::false_type) {
        return getApplyCall3(std::is_union<ReturnType>{});
    }

    static void* getApplyCall3(std::true_type) {
        return (void*)applyCallUserTypeByValue; // true - type is union
    }

    static void* getApplyCall3(std::false_type) {
        return (void*)applyCallScalar; // false - type is scalar type
    }

    static void* getApplyCall() {
        return getApplyCall1(std::is_same<ReturnType, void>{});
    }

public:
    // Handles normal member function hooking, e.g hooking fixed symbol implementation in executable
    static void InstallHook(const FString& DebugSymbolName, void* SampleObjectInstance = NULL) {
        if (!bHookInitialized) {
            bHookInitialized = true;
            void* HookFunctionPointer = getApplyCall();
            TMemberFunctionPointer<TCallable> RawFunctionPointer{};
            RawFunctionPointer.MemberFunctionPointer = Callable;
            const FMemberFunctionPointer MemberFunctionPointer = ConvertFunctionPointer(&RawFunctionPointer);

            void* RealFunctionAddress = FNativeHookManagerInternal::RegisterHookFunction(
                DebugSymbolName,
                MemberFunctionPointer.FunctionAddress,
                SampleObjectInstance,
                MemberFunctionPointer.ThisAdjustment,
                HookFunctionPointer,
                (void**)&functionPtr
            );

            auto* HandlerLists = createHandlerLists<Handler, HandlerAfter>(RealFunctionAddress);
            handlersBefore = &HandlerLists->HandlersBefore;
            handlersAfter = &HandlerLists->HandlersAfter;
        }
    }

    static void addHandlerBefore(Handler handler) {
        handlersBefore->Add(handler);
    }

    static void addHandlerAfter(HandlerAfter handler) {
        handlersAfter->Add(handler);
    }
};

// Hook invoker for member non-const functions
template<typename R, typename C, typename... A, R(C::*Callable)(A ...)>
struct HookInvoker<R(C::*)(A ...), Callable> : HookInvokerExecutorMemberFunction<R(C::*)(A ...), Callable, false, R, C, A...> {
};

// Hook invoker for member const functions
template<typename R, typename C, typename... A, R(C::*Callable)(A ...) const>
struct HookInvoker<R(C::*)(A ...) const, Callable> : HookInvokerExecutorMemberFunction<R(C::*)(A ...) const, Callable, true, R, C, A...> {
};

// Hook invoker for global functions
template<typename R, typename... A, R(*Callable)(A ...)>
struct HookInvoker<R(*)(A ...), Callable> : HookInvokerExecutorGlobalFunction<R(*)(A ...), Callable, R, A...> {
};

template<typename TCallable, TCallable Callable, bool bIsConst, typename ReturnType, typename CallableType, typename... ArgumentTypes>
typename HookInvokerExecutorMemberFunction<TCallable, Callable, bIsConst, ReturnType, CallableType, ArgumentTypes...>::HookType* HookInvokerExecutorMemberFunction<TCallable, Callable, bIsConst, ReturnType, CallableType, ArgumentTypes...>::functionPtr = nullptr;

template<typename TCallable, TCallable Callable, bool bIsConst, typename ReturnType, typename CallableType, typename... ArgumentTypes>
bool HookInvokerExecutorMemberFunction<TCallable, Callable, bIsConst, ReturnType, CallableType, ArgumentTypes...>::bHookInitialized = false;

template<typename TCallable, TCallable Callable, bool bIsConst, typename ReturnType, typename CallableType, typename... ArgumentTypes>
TArray<typename HookInvokerExecutorMemberFunction<TCallable, Callable, bIsConst, ReturnType, CallableType, ArgumentTypes...>::Handler>* HookInvokerExecutorMemberFunction<TCallable, Callable, bIsConst, ReturnType, CallableType, ArgumentTypes...>::handlersBefore = nullptr;

template<typename TCallable, TCallable Callable, bool bIsConst, typename ReturnType, typename CallableType, typename... ArgumentTypes>
TArray<typename HookInvokerExecutorMemberFunction<TCallable, Callable, bIsConst, ReturnType, CallableType, ArgumentTypes...>::HandlerAfter>* HookInvokerExecutorMemberFunction<TCallable, Callable, bIsConst, ReturnType, CallableType, ArgumentTypes...>::handlersAfter = nullptr;

template<typename TCallable, TCallable Callable, typename ReturnType, typename... ArgumentTypes>
typename HookInvokerExecutorGlobalFunction<TCallable, Callable, ReturnType, ArgumentTypes...>::HookType HookInvokerExecutorGlobalFunction<TCallable, Callable, ReturnType, ArgumentTypes...>::functionPtr = nullptr;

template<typename TCallable, TCallable Callable, typename ReturnType, typename... ArgumentTypes>
bool HookInvokerExecutorGlobalFunction<TCallable, Callable, ReturnType, ArgumentTypes...>::bHookInitialized = false;

template<typename TCallable, TCallable Callable, typename ReturnType, typename... ArgumentTypes>
TArray<typename HookInvokerExecutorGlobalFunction<TCallable, Callable, ReturnType, ArgumentTypes...>::Handler>* HookInvokerExecutorGlobalFunction<TCallable, Callable, ReturnType, ArgumentTypes...>::handlersBefore = nullptr;

template<typename TCallable, TCallable Callable, typename ReturnType, typename... ArgumentTypes>
TArray<typename HookInvokerExecutorGlobalFunction<TCallable, Callable, ReturnType, ArgumentTypes...>::HandlerAfter>* HookInvokerExecutorGlobalFunction<TCallable, Callable, ReturnType, ArgumentTypes...>::handlersAfter = nullptr;

#define SUBSCRIBE_METHOD(MethodReference, Handler) \
HookInvoker<decltype(&MethodReference), &MethodReference>::InstallHook(TEXT(#MethodReference)); \
HookInvoker<decltype(&MethodReference), &MethodReference>::addHandlerBefore(Handler);

#define SUBSCRIBE_METHOD_AFTER(MethodReference, Handler) \
HookInvoker<decltype(&MethodReference), &MethodReference>::InstallHook(TEXT(#MethodReference)); \
HookInvoker<decltype(&MethodReference), &MethodReference>::addHandlerAfter(Handler);

#define SUBSCRIBE_METHOD_VIRTUAL(MethodReference, SampleObjectInstance, Handler) \
HookInvoker<decltype(&MethodReference), &MethodReference>::InstallHook(TEXT(#MethodReference), SampleObjectInstance); \
HookInvoker<decltype(&MethodReference), &MethodReference>::addHandlerBefore(Handler);

#define SUBSCRIBE_METHOD_VIRTUAL_AFTER(MethodReference, SampleObjectInstance, Handler) \
HookInvoker<decltype(&MethodReference), &MethodReference>::InstallHook(TEXT(#MethodReference), SampleObjectInstance); \
HookInvoker<decltype(&MethodReference), &MethodReference>::addHandlerAfter(Handler);
