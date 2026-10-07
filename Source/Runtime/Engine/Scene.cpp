#include "EnginePCH.h"
#include "Scene.h"

void FScene::AddPrimitive(UPrimitiveComponent* Component)
{
	if (!Component || Component->SceneProxy) return;

	FPrimitiveSceneProxy* Proxy = new FPrimitiveSceneProxy(Component);
	Proxy->Scene = this;
	Proxy->PackedIndex = Proxies.Num();
	Component->SceneProxy = Proxy;
	MarkDirty(Proxy);
	MarkRenderStateDirty(Proxy);

	Proxies.Add(Proxy);
	PrimitiveBounds.Add(FAABB{});
	PrimitiveFlags.Add(0);

	bElementListChanged = true;
}

void FScene::RemovePrimitive(UPrimitiveComponent* Component)
{
	if (!Component || !Component->SceneProxy) return;

	FPrimitiveSceneProxy* Proxy = Component->SceneProxy;
	if (!Proxy) return;

	const uint32 Index = static_cast<uint32>(Proxy->PackedIndex);

	Proxies.RemoveAtSwap(Index);
	PrimitiveBounds.RemoveAtSwap(Index);
	PrimitiveFlags.RemoveAtSwap(Index);

	if (Index < static_cast<uint32>(Proxies.Num()))
		Proxies[Index]->PackedIndex = static_cast<int32>(Index);

	if (Proxy->bQueuedForUpdate)
	{
		const int32 Found = DirtyProxies.Find(Proxy);   // 프로젝트 TArray의 Find 이름에 맞게
		if (Found != INDEX_NONE) DirtyProxies.RemoveAtSwap(Found);
	}

	if (Proxy->bRenderStateQueued)
	{
		const int32 Found = RenderStateDirtyProxies.Find(Proxy);
		if (Found != INDEX_NONE) RenderStateDirtyProxies.RemoveAtSwap(Found);
	}

	Component->SceneProxy = nullptr;
	delete Proxy;

	bElementListChanged = true;
}

void FScene::RemoveAllPrimitives()
{
	// 하나씩 RemovePrimitive하면 대기열 검색(Find)이 물체 수만큼 반복되므로 통째로 비운다.
	for (FPrimitiveSceneProxy* Proxy : Proxies)
	{
		if (UPrimitiveComponent* Component = Proxy->GetComponent())
			Component->SceneProxy = nullptr;
		delete Proxy;
	}
	Proxies.Reset();
	PrimitiveBounds.Reset();
	PrimitiveFlags.Reset();
	DirtyProxies.Reset();
	RenderStateDirtyProxies.Reset();
	BVH.Clear();
	bElementListChanged = true;
}

void FScene::UpdateAllTransforms()
{
	for (FPrimitiveSceneProxy* Proxy : RenderStateDirtyProxies)
	{
		Proxy->UpdateRenderState();
		Proxy->bRenderStateQueued = false;
	}
	RenderStateDirtyProxies.Reset();

	for (FPrimitiveSceneProxy* Proxy : DirtyProxies)
	{
		Proxy->UpdateTransform();
		PrimitiveBounds[Proxy->PackedIndex] = Proxy->GetBounds();
		PrimitiveFlags[Proxy->PackedIndex] = Proxy->GetComponent()->IsVisible() ? 1 : 0;
		Proxy->bQueuedForUpdate = false;
	}
	
	//const int32 Count = Proxies.Num();
	//for (int32 i = 0; i < Count; ++i)
	//{
	//	FPrimitiveSceneProxy* Proxy = Proxies[i];
	//	Proxy->UpdateTransform();

	//	const FAABB& Bounds = Proxy->GetBounds();
	//	if (PrimitiveBounds[i].Center != Bounds.Center || PrimitiveBounds[i].Extent != Bounds.Extent)
	//	{
	//		bBoundsChanged = true;
	//	}

	//	PrimitiveBounds[i] = Proxy->GetBounds();
	//	PrimitiveFlags[i] = Proxy->GetComponent()->IsVisible() ? 1 : 0;
	//}

	const bool bAnyMoved = DirtyProxies.Num() > 0;
	DirtyProxies.Reset();
	

	if (bElementListChanged)
	{
		BuildBVH();
		bElementListChanged = false;
	}
	else if (bAnyMoved)
	{
		BVH.Refit();
	}

	UpdateDirtyFogs();
}

void FScene::BuildBVH()
{
	BVH.Clear();
	// BillboardComponents.Reset();

	TArray<FPrimitiveSceneProxy*> Elements;
	Elements.Reserve(Proxies.Num());
	for (FPrimitiveSceneProxy* Proxy : Proxies)
	{
		if (Proxy && Proxy->GetComponent())
			Elements.Add(Proxy);
	}

	BVH.Build(std::span(Elements.GetData(), Elements.Num()));
}

void FScene::MarkDirty(FPrimitiveSceneProxy* Proxy)
{
	if (Proxy->bQueuedForUpdate) return;            // 중복 추가 방지
	Proxy->bQueuedForUpdate = true;
	DirtyProxies.Add(Proxy);
}

void FScene::MarkRenderStateDirty(FPrimitiveSceneProxy* Proxy)
{
	if (!Proxy || Proxy->bRenderStateQueued) return;
	Proxy->bRenderStateQueued = true;
	RenderStateDirtyProxies.Add(Proxy);
}

void FScene::MarkFogDirty(uint32 ComponentId)
{
	if (!FogComponentMap.Contains(ComponentId)) return; // 등록되지 않은 안개 컴포넌트인지 확인
	if (DirtyFogIds.Find(ComponentId) != INDEX_NONE) return; // 이미 대기열에 있는지 확인

	DirtyFogIds.Add(ComponentId);
}

void FScene::UpdateDirtyFogs()
{
	for (uint32 id : DirtyFogIds)
	{
		if (UExponentialHeightFogComponent** Fog = FogComponentMap.FindOrNull(id))
		{
			for (FFogSceneEntry& Entry : ExponentialFogs)
			{
				if (Entry.Id == id)
				{
					Entry.Info.UpdateFromComponent(**Fog);
					break;
				}
			}
		}
	}

	DirtyFogIds.Reset();
}

void FScene::AddExponentialHeightFog(UExponentialHeightFogComponent* Fog)
{
	if (!Fog) return;

	const uint32 Id = Fog->GetUUID();

	if (FogComponentMap.Contains(Id))
	{
		// 이미 등록된 안개 컴포넌트라면 갱신
		for (FFogSceneEntry& Entry : ExponentialFogs)
		{
			if (Entry.Id == Id)
			{
				Entry.Info = FExponentialHeightFogSceneInfo(Fog);
				return;
			}
		}
	}
	else
	{
		FogComponentMap.Add(Id, Fog);
		ExponentialFogs.Add(FFogSceneEntry{ Id, FExponentialHeightFogSceneInfo(Fog) });
	}	
}

void FScene::RemoveExponentialHeightFog(UExponentialHeightFogComponent* Fog)
{
	if (!Fog) return;

	const uint32 Id = Fog->GetUUID();
	FogComponentMap.Remove(Id);

	if(DirtyFogIds.Find(Id) != INDEX_NONE)
	{
		DirtyFogIds.RemoveAt(DirtyFogIds.Find(Id), 1);
	}

	// 이미 등록된 안개 컴포넌트라면 갱신
	for (int i = 0; i < ExponentialFogs.Num(); i++)
	{
		const FFogSceneEntry& Entry = ExponentialFogs[i];
		if (Entry.Id == Id)
		{
			ExponentialFogs.RemoveAt(i, 1);
			return;
		}
	}

}

void FScene::RemoveAllExponentialHeightFogs()
{
	ExponentialFogs.Reset();
	FogComponentMap.Reset();
	DirtyFogIds.Reset();
}


