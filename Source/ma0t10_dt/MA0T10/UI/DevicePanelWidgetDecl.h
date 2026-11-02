#pragma once
#include "SensorToolWidgetDecl.h"
#define SNewDeviceTool(Owner,Type) TSensorToolWidgetDecl<Type>(DevicePanelBody::OwnerPtr(Owner),#Type) <<= Type::FArguments()
#define SAssignDeviceTool(Owner,Out,Type) TSensorToolWidgetDecl<Type>(DevicePanelBody::OwnerPtr(Owner),#Type).Expose(Out) <<= Type::FArguments()
