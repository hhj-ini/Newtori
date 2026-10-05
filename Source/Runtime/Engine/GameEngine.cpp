#include "EnginePCH.h"
#include "GameEngine.h"

#include "Launch/LaunchEngineLoop.h"

FEngineConfig UGameEngine::GetConfig() const
{
	FEngineConfig Desc;
	Desc.Title = L"Hitori";
	Desc.Width = 0;
	Desc.Height = 0;
	Desc.bBorderless = true;
	Desc.SyncInterval = 0;   // 0 = VSync 끔 (FPS 측정용)
	Desc.bCreateDepthBuffer = true;   
	Desc.bExitOnEscape = true;        
	return Desc;
}

bool UGameEngine::Init()
{
	FWorldContext* InitContext = CreateNewWorldContext(EWorldType::Game, "Game");
	if (!InitContext) return false;

	return true;
}

void UGameEngine::Tick(float DeltaTime)
{
	for (size_t i = 0; i < WorldList.size(); ++i)
	{
		UWorld* World = WorldList[i].get()->CurrentWorld;
		if (!World)
		{
			continue;
		}
		World->Tick(DeltaTime);
	}
}

