#pragma once

#include "Editor/EditorUI/EditorPanel.h"

#include <functional>

class UTexture2D;

class FEditorUI
{
public:
	bool Init(bool bInUseDockSpace = true, bool bInPassthruCentralNode = false);
	void Tick(float DeltaTime);
	void OnRender();

	template <typename T>
	T* AddEditorPanel()
	{
		TUniquePtr<T> newPanel = MakeUnique<T>();
		T* Ret = newPanel.get();
		Ret->Init();
		Panels.Add(std::move(newPanel));
		
		return Ret;
	}

	void SetNewSceneCallback(std::function<void()> InCallback) { OnNewScene = InCallback; }
	void SetOpenSceneCallback(std::function<void()> InCallback) { OnOpenScene = InCallback; }
	void SetSaveSceneCallback(std::function<void()> InCallback) { OnSaveScene = InCallback; }
	void SetSaveSceneAsCallback(std::function<void()> InCallback) { OnSaveSceneAs = InCallback; }

	// PIE Function Setter
	void SetStartPIECallback(std::function<void()> InCallback) { OnStartPIE = InCallback; }
	void SetEndPIECallback(std::function<void()> InCallback) { OnEndPIE = InCallback; }

private:
	bool bUseDockSpace = true;
	bool bPassthruCentralNode = false;

	void DrawMainMenuBar();
	void DrawMainToolBar();

	TArray<TUniquePtr<IEditorPanel>> Panels;

	std::function<void()> OnNewScene;
	std::function<void()> OnOpenScene;
	std::function<void()> OnSaveScene;
	std::function<void()> OnSaveSceneAs;


private:	// PIE 관련
	// 툴바 높이
	float ToolBarHeight = 65.0f;

	// PIE 연동
	std::function<void()> OnStartPIE;
	std::function<void()> OnEndPIE;

	UTexture2D* PlayIcon = nullptr;
	UTexture2D* StopIcon = nullptr;
	UTexture2D* SaveIcon = nullptr;
};
