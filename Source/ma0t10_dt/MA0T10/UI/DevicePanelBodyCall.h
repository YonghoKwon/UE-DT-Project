#pragma once
#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Views/STableRow.h"
#include <type_traits>
/** Old Slate trees may outlive the UWidget. Never invoke a view callback after GC. */
namespace DevicePanelBody
{
    template<class T> T* OwnerPtr(T* Pointer) {return Pointer;}
    template<class T> T* OwnerPtr(TWeakObjectPtr<T> Pointer) {return Pointer.Get();}
    template<class R> struct TEmptyResult {static R Make(){return R{};}};
    template<class W,ESPMode Mode> struct TEmptyResult<TSharedRef<W,Mode>>
    {static TSharedRef<W,Mode> Make(){return SNew(W);}};
    template<class T,class F> auto Invoke(TWeakObjectPtr<T> Owner,F&& Function)->decltype(Function(Owner.Get()))
    {
        using R=decltype(Function(Owner.Get()));
        if(T* Self=Owner.Get())return Function(Self);
        if constexpr(std::is_void_v<R>)return;
        else if constexpr(std::is_same_v<R,FReply>)return FReply::Unhandled();
        else if constexpr(std::is_same_v<R,TSharedRef<SWidget>>)return SNullWidget::NullWidget;
        else return TEmptyResult<R>::Make();
    }
    template<class T,class F> auto InvokeRow(TWeakObjectPtr<T> Owner,const TSharedRef<STableViewBase>& Table,F&& Function)->decltype(Function(Owner.Get()))
    {
        if(T* Self=Owner.Get())return Function(Self);
        using R=decltype(Function(Owner.Get()));
        using W=typename R::ElementType;
        return SNew(W,Table)[SNullWidget::NullWidget];
    }
}
