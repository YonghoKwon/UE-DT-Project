#include "ma0t10_dt/MA0T10/Player/DxPlayerControllerTest.h"
#include "VirtualSensorTransformGizmoActor.h"

AVirtualSensorTransformGizmoActor* ADxPlayerControllerTest::GetPlacementInputOwner() const
{auto* G=PlacementOwner.Get();return G&&G->IsResponsivePlacement()&&G->IsManipulationEnabled()?G:nullptr;}
void ADxPlayerControllerTest::RegisterPlacementInputOwner(AVirtualSensorTransformGizmoActor* G){PlacementOwner=G;}
void ADxPlayerControllerTest::UnregisterPlacementInputOwner(AVirtualSensorTransformGizmoActor* G)
{if(PlacementOwner.Get()==G){FinishOwnedNavigationInput();PlacementOwner.Reset();}}
IVirtualSensorPlacementInputOwner* ADxPlayerControllerTest::GetPlacementInputView() const
{return GetPlacementInputOwner();}
IVirtualSensorPlacementInputOwner* ADxPlayerControllerTest::GetRegisteredPlacementInputView() const
{return PlacementOwner.Get();}
