#pragma once

#include <format>
#include "Editor/EditorUI/EditorPanel.h"

#include "Engine/World.h"

struct FTransform;

class FDetailsPanel : public IEditorPanel
{
public:
	FDetailsPanel() = default;
	~FDetailsPanel();

	bool Init() override;
	void Tick(float DeltaTime)override;
	void OnRender() override;

	const char* GetPanelName() const override { return "Details"; }

	ImFont* GetCustomFont() { return CustomFont; }

	void SetWorld(UWorld* InWorld) { World = InWorld; }
	void SetTarget(USceneComponent* InTargetOrNull) { Target = InTargetOrNull; }

private:
	ImFont* CustomFont = nullptr;
	UWorld* World = nullptr;
	USceneComponent* Target = nullptr;

	UActorComponent* SelectedComponent = nullptr;
	void DrawComponentTree(AActor* Owner);
	void DrawSceneComponentNode(USceneComponent* Component);
};

