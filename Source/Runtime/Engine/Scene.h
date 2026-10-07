#pragma once

#include "PrimitiveSceneProxy.h"
#include "Component/BillboardComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/ExponentialHeightFogComponent.h"
#include "Math/Frustum.h"
#include "Math/BVH.h"
#include "Render/ExponentialHeightFogSceneInfo.h"
#include "Container/Map.h"
#include <Component/LightComponent.h>
#include "Engine/LightSceneProxy.h"

struct FFogSceneEntry
{
	uint32 Id;
	FExponentialHeightFogSceneInfo Info;
};

struct FLightSceneEntry
{
	uint32 Id;
	TUniquePtr<FLightSceneProxy> Proxy;
};

class FScene
{
public:
	void AddPrimitive(UPrimitiveComponent* Component);
	void RemovePrimitive(UPrimitiveComponent* Component);
	// 모든 프록시를 한 번에 지운다. 액터를 통째로 지우기 전(ClearWorld)에 불러야 지워진 컴포넌트를 가리키는 프록시가 남지 않는다.
	void RemoveAllPrimitives();

	void UpdateAllTransforms();

	void BuildBVH();

	void MarkDirty(FPrimitiveSceneProxy* Proxy);
	void MarkRenderStateDirty(FPrimitiveSceneProxy* Proxy);
	void MarkFogDirty(uint32 ComponentId);
	void MarkLightDirty(uint32 ComponentId);
	void UpdateDirtyFogs();	
	void UpdateDirtyLights();


	TArray<FPrimitiveSceneProxy*> Proxies;
	TArray<FPrimitiveSceneProxy*> DirtyProxies;
	TArray<FPrimitiveSceneProxy*> RenderStateDirtyProxies;
	TArray<FAABB> PrimitiveBounds;
	TArray<uint8> PrimitiveFlags;

	// 컬링 결과로 프록시를 바로 내보내 컴포넌트를 역참조하지 않는다. 경계는 Build/Refit 때만 계산한다.
	TBVH<FPrimitiveSceneProxy*> BVH{
		[](const FPrimitiveSceneProxy* Proxy) -> FBox
		{
			const FAABB& B = Proxy->GetBounds();
			return FBox{ B.Center - B.Extent, B.Center + B.Extent };
		}
	};
	bool bElementListChanged = false;

	TArray<FFogSceneEntry> ExponentialFogs;
	TArray<FLightSceneEntry> Lights;

	void AddExponentialHeightFog(UExponentialHeightFogComponent* Fog);
	void RemoveExponentialHeightFog(UExponentialHeightFogComponent* Fog);
	void RemoveAllExponentialHeightFogs();

	void AddLight(ULightComponent* Light);
	void RemoveLight(ULightComponent* Light);
	void RemoveAllLights();

private:
	TMap<uint32, UExponentialHeightFogComponent*> FogComponentMap;
	TMap<uint32, ULightComponent*> LightComponentMap;
	TArray<uint32> DirtyFogIds;
	TArray<uint32> DirtyLightIds;
};
