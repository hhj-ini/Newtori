#include "EnginePCH.h"
#include "Engine.h"

UEngine* GEngine = nullptr;

//bool UEngine::Init()
//{
//
//}

FWorldContext* UEngine::CreateNewWorldContext(EWorldType InType, FName ContextHandle)
{
	UWorld* World = FObjectFactory::ConstructObject<UWorld>();
	if (!World || !World->Init(InType)) return nullptr;

	TUniquePtr NewContext = MakeUnique<FWorldContext>(InType, ContextHandle, World);
	uint32 idx = WorldList.Add(std::move(NewContext));
	
	return WorldList[idx].get();
}

FWorldContext* UEngine::GetWorldContext(EWorldType QueryType)
{
	for (size_t i = 0; i < WorldList.Num(); ++i)
	{
		if (QueryType == WorldList[i].get()->WorldType)
		{
			return WorldList[i].get();
		}
	}

	return nullptr;
}