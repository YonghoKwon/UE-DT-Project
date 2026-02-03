#pragma once

UENUM(BlueprintType)
enum class EDxWidgetFlag : uint8
{
	None,

	ShipFreeView,
	ShipTopView,

	CraneFreeView,
	CraneTopView,

	UpdateWidget,
};