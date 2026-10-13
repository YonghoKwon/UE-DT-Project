#pragma once
#include "VirtualSensorPanelWidgetBase.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

/** Explicit construction-site registration, never walks another widget's children. */
template<class WidgetType>
struct TSensorToolWidgetDecl
{
	explicit TSensorToolWidgetDecl(UVirtualSensorPanelWidgetBase* InOwner,const ANSICHAR* InTypeName) : Owner(InOwner),TypeName(InTypeName) {}
	TSensorToolWidgetDecl& Expose(TSharedPtr<WidgetType>& Out) { Exposed = &Out; return *this; }
	TSharedRef<WidgetType> operator<<=(const typename WidgetType::FArguments& Args)
	{
		auto Required = RequiredArgs::MakeRequiredArgs();
		auto Widget = MakeTDecl<WidgetType>(TypeName, __FILE__, __LINE__, MoveTemp(Required)) <<= Args;
		Owner->RegisterSensorNativeFont(Widget, Args);
		if (Exposed) *Exposed = Widget;
		return Widget;
	}
	UVirtualSensorPanelWidgetBase* Owner;
	const ANSICHAR* TypeName;
	TSharedPtr<WidgetType>* Exposed = nullptr;
};
#define SNewSensorTool(Type) TSensorToolWidgetDecl<Type>(this,#Type) <<= Type::FArguments()
#define SAssignSensorTool(Out, Type) TSensorToolWidgetDecl<Type>(this,#Type).Expose(Out) <<= Type::FArguments()
