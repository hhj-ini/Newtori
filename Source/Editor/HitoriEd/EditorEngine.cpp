#include "EnginePCH.h"

#include "Editor/HitoriEd/EditorEngine.h"

#include "Core/EngineStatics.h"
#include "Core/EngineTimer.h"
#include "Launch/LaunchEngineLoop.h"
#include "Core/StatOverlay.h"
#include "Input/InputSystem.h"

#include "ObjectSystem/ObjectFactory.h"

#include "Render/GeometryGenerator.h"

#include "Engine/World.h"
#include "Engine/Level.h"

#include "Render/Renderer.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Actor/LightActor.h"

#include "Asset/AssetManager.h"
#include "Render/RenderResourceManager.h"

#include "Render/RenderCommand.h"
#include "Render/FogRenderer.h"
#include "Editor/Outliner/OutlinerPanel.h"
#include "Editor/HitoriEd/EditorFileUtils.h"
#include "UObject/UObjectIterator.h"

#include "Core/EngineLog.h"
#include "Core/Stats/LightweightStats.h"

#include "ObjectSystem/ObjectDuplication.h"

namespace
{
	DECLARE_CYCLE_STAT("Viewport Update", STAT_ViewportUpdate);
	DECLARE_CYCLE_STAT("World Tick", STAT_WorldTick);
	DECLARE_CYCLE_STAT("Editor Tick", STAT_EditorTick);
	DECLARE_CYCLE_STAT("Capture World", STAT_CaptureWorld);
	DECLARE_CYCLE_STAT("Build Render Queue", STAT_BuildRenderQueue);
	DECLARE_CYCLE_STAT("ImGui", STAT_ImGui);
}

#include "Core/SplashScreen.h"

FEngineConfig UEditorEngine::GetConfig() const
{
	FEngineConfig Desc;
	Desc.Title = L"GWJNS Engine";
	Desc.Width = 1920;
	Desc.Height = 1080;
	Desc.bBorderless = false;
	Desc.SyncInterval = 0;
	// View는 각자 깊이 버퍼를 쓰고 백버퍼에는 ImGui만 그린다.
	Desc.bCreateDepthBuffer = false;
	Desc.bExitOnEscape = false;
	// 파일이 없으면 검은 배경에 상태 텍스트만 표시된다.
	Desc.SplashImage = "Resources/GWJNS.png";
	return Desc;
}

// 렌더 자원·월드·에디터와 MultipleViewports 연결을 초기화한다.
// Device·Window·Swapchain·AssetManager는 FEngineLoop가 먼저 만들어 둔다.
bool UEditorEngine::Init()
{
	FWorldContext* InitContext = CreateNewWorldContext(EWorldType::Editor, "Editor");

	if (!InitContext) return false;

	MainWindow = GetEngineLoop().GetMainWindow();
	MainWindowSC = GetEngineLoop().GetSwapchain();
	Renderer = GetEngineLoop().GetRenderer();
	FRenderDevice* RenderDevice = GetEngineLoop().GetRenderDevice();

	EditorUI = MakeUnique<FEditorUI>();
	EditorUI->Init();

	EditorUI->SetNewSceneCallback([this]() { CreateNewScene(); });
	EditorUI->SetOpenSceneCallback([this]() { OpenScene(); });
	EditorUI->SetSaveSceneCallback([this]() { SaveCurrentScene(); });
	EditorUI->SetSaveSceneAsCallback([this]() { SaveSceneAs(); });

	EditorUI->SetStartPIECallback([this]() {StartPIE();});
	EditorUI->SetEndPIECallback([this]() {EndPIE();});

	OutputLogPanel = EditorUI->AddEditorPanel<FOutputLogPanel>();
	FLog::AddSink(OutputLogPanel);
	HTR_LOG(Info, "Editor Initialize...");

	HTR_LOG(Info, "Initialize ImGui...");
	ImGuiRenderer = MakeUnique<FImGuiRenderer>();
	if (!ImGuiRenderer->Init(MainWindow->GetHandle(), RenderDevice->GetDevice(), RenderDevice->GetContext()))
	{
		HTR_LOG(Error, "Failed To Initialize ImGui!");
	}
	HTR_LOG(Info, "Initialize ImGui Success!");

	GridRenderer = MakeUnique<FGridRenderer>();
	GridRenderer->Init(Renderer);

	FogRenderer = MakeUnique<FFogRenderer>();
	FogRenderer->Init(RenderDevice);

	GizmoRenderer = MakeUnique<FGizmoRenderer>();
	GizmoRenderer->Init(Renderer);

	Gizmo = MakeUnique<FGizmo>();

	// 필요한 Panel들 추가후 raw pointer 반환(소유권 = EditorUI)
	DetailsPanel = EditorUI->AddEditorPanel<FDetailsPanel>();
	EditorControlsPanel = EditorUI->AddEditorPanel<FEditorControlsPanel>();
	ViewportsPanel = EditorUI->AddEditorPanel<FViewportsPanel>();
	ContentDrawerPanel = EditorUI->AddEditorPanel<FContentDrawerPanel>();

	// OutLine
	OutlineRenderer = MakeUnique<FOutlineRenderer>();
	OutlineRenderer->Init(Renderer);

	SettingsPanel = EditorUI->AddEditorPanel<FSettingsPanel>();

	Outline = MakeUnique<FOutline>();

	SystemFont = UAssetManager::GetAssetByPath<UFont>("Assets/Fonts/Pretendard.json");

	TextRenderer = MakeUnique<FTextRenderer>();
	TextRenderer->Init();

	UWorld* World = InitContext->CurrentWorld;
	// 투영 행렬 생성 
	MultipleViewportsAdapter.InitializeFromWorld(*World);
	// 화면 나눔 비율 설정 가져오기
	MultipleViewportsAdapter.SetSplitRatio({
		SettingsPanel->GetSettings().MultipleViewportsHorizontal,
		SettingsPanel->GetSettings().MultipleViewportsVertical });
	// SingleView에 사용할 인덱스 설정
	MultipleViewportsAdapter.SetSingleViewIndex(
		SettingsPanel->GetSettings().MultipleViewportsSingleViewIndex);
	// 뷰포트 레이아웃 설정
	MultipleViewportsAdapter.SetLayoutMode(
		SettingsPanel->GetSettings().bMultipleViewportsSingle
		? ELayoutMode::Single
		: ELayoutMode::QuadSplit);
	World->GetMainCamera()->GetCameraComponent()->SetExternalInputManaged(true);

	/// 삭제 예정
	//SceneManager = EditorUI->AddEditorPanel<FSceneManager>();
	//SceneManager->SetWorld(World);

	OutlinerPanel = EditorUI->AddEditorPanel<FOutlinerPanel>();
	OutlinerPanel->SetWorld(World);
	OutlinerPanel->SetSelectionCallback([this](AActor* Actor)
	{
		ApplyActorSelection(Actor);
	});
	DetailsPanel->SetComponentSelectionCallback([this](UActorComponent* Component)
	{
		ApplyComponentSelection(Component);
	});

	OutlinerPanel->SetDeleteActorCallback(
		[this](AActor* Actor)
		{
			DeleteActor(Actor);
		}
	);

	LineBatcher = MakeUnique<FLineBatcher>();
	LineBatcher->Init(Renderer, World);

	DetailsPanel->SetWorld(World);

	EditorControlsPanel->SetWorld(World);
	EditorControlsPanel->SetGizmo(Gizmo.get());
	EditorControlsPanel->SetViewportAdapter(&MultipleViewportsAdapter);

	SettingsPanel->SetWorld(World);
	SettingsPanel->SetViewportAdapter(&MultipleViewportsAdapter);
	ViewportsPanel->SetViewportAdapter(&MultipleViewportsAdapter);

	SkyboxRenderer = MakeUnique<FSkyboxRenderer>();
	SkyboxRenderer->Init("Assets/SkySphere/Sky.jpg");


	// Todo: Post process
	PostProcessShader = FRenderResourceManager::GetShaderProgram("Resources/Shader/PostProcessShader.hlsl");
	PostProcessConstantBuffer = RenderCommand::CreateConstantBuffer(sizeof(FPostProcessConstants));

	return true;
}

// 프레임 시작·View 상태·월드 갱신 후 View별 오프스크린 렌더와 UI 합성을 진행한다.
// 입력·창 메시지는 FEngineLoop가 먼저 처리하고 Present는 호출 직후에 한다.
void UEditorEngine::Tick(const float DeltaTime)
{
	BeginFrame(DeltaTime);
	UpdateMultipleViewportState(DeltaTime);
	TickWorldAndEditor(DeltaTime);
	RenderMultipleViewports();
	EndFrame();
}

// DeltaTime을 패널에 전달하고 에디터 단축키를 처리한다.
void UEditorEngine::BeginFrame(const float DeltaTime)
{
	FStatOverlay::Tick(DeltaTime);
	EditorControlsPanel->FEditorControlsPanel::DeltaTime = DeltaTime;

	// PIE 플레이 중에는 위젯을 숨기고, F8로 편집 시점에 전환한다.
	if (bIsPlaying && !ImGui::GetIO().WantTextInput && FInputSystem::IsKeyPressed(EKeyCode::F8))
	{
		bIsEjected = !bIsEjected;
		ViewportsPanel->SetPIEEditing(bIsEjected);
		Gizmo->EndDrag();
	}

	if (!ImGui::GetIO().WantTextInput && FInputSystem::IsKeyPressed(EKeyCode::Delete))
	{
		UActorComponent* Component = DetailsPanel->GetSelectedComponent();
		if (Component)
		{
			AActor* Owner = Component->GetOwner();

			// 파괴 전에 선택을 참조하는 모든 패널과 위젯을 해제한다.
			Gizmo->SetTarget(nullptr);
			Outline->SetTarget(nullptr);
			DetailsPanel->SetActor(nullptr);
			Component->DestroyComponent();

			// Root가 교체됐으므로 에디터가 들고 있는 Target도 새 Root로 갱신한다.
			if (Owner)
			{
				OutlinerPanel->SelectActor(Owner);
			}
		}
		else
		{
			DeleteActor(OutlinerPanel->GetSelectedActor());
		}
	}
}

// 패널의 Layout·Preset 요청과 입력을 Adapter에 반영한다.
void UEditorEngine::UpdateMultipleViewportState(const float DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_ViewportUpdate);

	const FVector2 ViewportSize = ViewportsPanel->GetContentSize();
	const FVector2 LocalMousePosition = ViewportsPanel->GetLocalMousePosition();

	ELayoutMode RequestedLayout{};
	int32 RequestedSingleViewIndex = MultipleViewportsAdapter.GetSingleViewIndex();
	if (ViewportsPanel->ConsumeLayoutRequest(RequestedLayout, RequestedSingleViewIndex))
	{
		if (RequestedLayout == ELayoutMode::Single)
			MultipleViewportsAdapter.SetSingleViewIndex(RequestedSingleViewIndex);
		MultipleViewportsAdapter.SetLayoutMode(RequestedLayout);

		FEditorSettings& Settings = SettingsPanel->GetMutableSettings();
		Settings.bMultipleViewportsSingle = RequestedLayout == ELayoutMode::Single;
		Settings.MultipleViewportsSingleViewIndex = RequestedSingleViewIndex;
	}

	int32 PresetViewIndex = InvalidViewIndex;
	EMultipleViewportsCameraPreset RequestedPreset = EMultipleViewportsCameraPreset::Perspective;
	if (ViewportsPanel->ConsumeCameraPresetRequest(PresetViewIndex, RequestedPreset))
		MultipleViewportsAdapter.ApplyCameraPreset(PresetViewIndex, RequestedPreset);
	MultipleViewportsAdapter.UpdateLayout(ViewportSize, LocalMousePosition);

	const float HorizontalDrag = ViewportsPanel->ConsumeHorizontalDrag();
	const float VerticalDrag = ViewportsPanel->ConsumeVerticalDrag();
	if (HorizontalDrag != 0.0f)
		MultipleViewportsAdapter.ApplySplitterDrag(EDragAxis::Horizontal, HorizontalDrag, ViewportSize);
	if (VerticalDrag != 0.0f)
		MultipleViewportsAdapter.ApplySplitterDrag(EDragAxis::Vertical, VerticalDrag, ViewportSize);
	if (HorizontalDrag != 0.0f || VerticalDrag != 0.0f)
	{
		MultipleViewportsAdapter.UpdateLayout(ViewportSize, LocalMousePosition);
		const FSplitRatio Ratio = MultipleViewportsAdapter.GetSplitRatio();
		SettingsPanel->GetMutableSettings().MultipleViewportsHorizontal = Ratio.Horizontal;
		SettingsPanel->GetMutableSettings().MultipleViewportsVertical = Ratio.Vertical;
	}

	MultipleViewportsAdapter.UpdateInput(
		DeltaTime,
		LocalMousePosition,
		SettingsPanel->GetSettings().CameraSpeed,
		SettingsPanel->GetSettings().MouseSensitivity);
	// 겹친 창은 Hover 선택에서 제외하고 우클릭 Capture를 우선한다.
	if (ViewportsPanel->IsHovered() || MultipleViewportsAdapter.GetCapturedViewIndex() != InvalidViewIndex)
		MultipleViewportsAdapter.SetEditorViewIndex(MultipleViewportsAdapter.GetActiveViewIndex());
}

// 월드를 한 번 Tick·Capture한 뒤 에디터와 피킹을 갱신한다.
void UEditorEngine::TickWorldAndEditor(const float DeltaTime)
{
	for (size_t i = 0; i < WorldList.Num(); ++i)
	{
		UWorld* World = WorldList[i].get()->CurrentWorld;
		if (!World)
		{
			continue;
		}
		// PIE에서는 복제 World만 실행해 Editor의 원본 Tick/미리보기 상태도 보존한다.
		if (bIsPlaying && World->GetWorldType() != EWorldType::PIE) continue;

		// 월드 상태는 프레임마다 정확히 한 번 갱신하고 캡처한다.

		{
			SCOPE_CYCLE_COUNTER(STAT_WorldTick);
			World->Tick(DeltaTime);
		}

		if (World == OutlinerPanel->GetWorld())
		{
			AActor* Actor = OutlinerPanel->GetSelectedActor();
			if (Actor && World->GetPersistentLevel()->GetActors().Find(Actor) == INDEX_NONE)
			{
				ResetSceneSelection();
			}
			else if (Actor && DetailsPanel->GetSelectedComponent() &&
				Actor->GetComponents().Find(DetailsPanel->GetSelectedComponent()) == INDEX_NONE)
			{
				DetailsPanel->ClearSelectedComponent();
			}
		}

		{
			SCOPE_CYCLE_COUNTER(STAT_CaptureWorld);
			MultipleViewportsAdapter.CaptureWorld(World);
		}

		if (CanEditViewport() && World == OutlinerPanel->GetWorld())
		{
			UpdateGizmoAndPicking(World);
		}
	}

	// 1번만 실행되어야 하는 부분은 for 문 외부로 수정
	{
		SCOPE_CYCLE_COUNTER(STAT_EditorTick);
		EditorUI->Tick(DeltaTime);
	}

	
}

// 공유 월드 캡처로 활성 View별 렌더 큐를 만들고 렌더한다.
void UEditorEngine::RenderMultipleViewports()
{
	for (int32 ViewIndex = 0; ViewIndex < 4; ++ViewIndex)
	{
		const bool bActive = MultipleViewportsAdapter.IsViewActive(ViewIndex);
		ViewportsPanel->SetView(ViewIndex, MultipleViewportsAdapter.GetViewRect(ViewIndex), bActive);
		if (!bActive)
			continue;

		{
			SCOPE_CYCLE_COUNTER(STAT_BuildRenderQueue);
			MultipleViewportsAdapter.BuildRenderQueue(ViewIndex, RenderQueue);
		}

		RenderFrame(
			ViewIndex,
			ViewportsPanel->GetRenderingInfo(ViewIndex),
			MultipleViewportsAdapter.GetEngineViewProjection(ViewIndex),
			MultipleViewportsAdapter.GetEngineCameraLocation(ViewIndex),
			MultipleViewportsAdapter.GetEngineCameraForward(ViewIndex),
			RenderQueue,
			MultipleViewportsAdapter.GetViewportWorld(ViewIndex));
	}

	EMultipleViewportsCameraPreset CameraPresets[4]{};
	for (int32 ViewIndex = 0; ViewIndex < 4; ++ViewIndex)
		CameraPresets[ViewIndex] = MultipleViewportsAdapter.GetCameraPreset(ViewIndex);
	ViewportsPanel->SetControlState(
		MultipleViewportsAdapter.GetLayoutMode(),
		MultipleViewportsAdapter.GetSingleViewIndex(),
		CameraPresets);
}

// 화면을 표시하고 UI 변경 후 View 설정을 보관한다.
void UEditorEngine::EndFrame()
{
	PresentFrame();
	// UI 변경 후 설정을 복사해 종료 시 카메라 수명에 의존하지 않는다.
	SettingsPanel->CaptureViewportSettings();
	// 프로파일러 반영
	FStatRegistry::EndFrame();
}

// 입력 View의 Ray와 피킹으로 Gizmo·공유 선택을 갱신한다.
void UEditorEngine::UpdateGizmoAndPicking(UWorld* World)
{
	// Delete는 BeginFrame에서 한 번만 처리하고 여기서는 View 입력만 다룬다.
	const int32 ViewIndex = MultipleViewportsAdapter.GetActiveViewIndex();
	if (ViewIndex == InvalidViewIndex || !ViewportsPanel->IsHovered())
		return;

	const FVector2 LocalMousePosition = ViewportsPanel->GetLocalMousePosition();
	FRay Ray{};
	if (!MultipleViewportsAdapter.TryGetActiveViewRay(LocalMousePosition, Ray))
		return;

	const FRect& Rect = MultipleViewportsAdapter.GetViewRect(ViewIndex);
	const FVector2 ViewLocalMouse(
		LocalMousePosition.X - Rect.X,
		LocalMousePosition.Y - Rect.Y);
	const FMatrix ViewProjection = MultipleViewportsAdapter.GetEngineViewProjection(ViewIndex);
	bool bMouseDown = FInputSystem::IsMouseDown(EMouseButton::Left);

	Gizmo->Update(
		Ray,
		ViewLocalMouse,
		ViewProjection,
		static_cast<int>(Rect.Width),
		static_cast<int>(Rect.Height),
		bMouseDown,
		MultipleViewportsAdapter.GetEngineCameraLocation(ViewIndex),
		MultipleViewportsAdapter.IsOrthographic(ViewIndex));

	if (FInputSystem::IsMousePressed(EMouseButton::Left) && !Gizmo->IsUsing() && Gizmo->GetHoveredAxis() < 0)
	{
		MultipleViewportsAdapter.PickActiveView(LocalMousePosition, *World);
		MultipleViewportsAdapter.ApplyLastPickToOutliner(*OutlinerPanel);
	}

}

// View 행렬로 Scene·Grid·Gizmo·텍스트·Outline을 렌더한다.
void UEditorEngine::RenderFrame(const int32 ViewIndex, const FRenderingInfo& ViewRenderingInfo, const FMatrix& ViewProjection, const FVector& ViewCameraLocation, const FVector& ViewCameraForward, FRenderQueue& RenderQueue, UWorld* InWorld)
{
	const bool bDrawPrimitives = SettingsPanel->GetSettings().bDrawPrimitives;
	FTexture2D* PostProcessSource = ViewportsPanel->GetSceneColor(ViewIndex);
	RenderCommand::BeginRenderPass(ViewRenderingInfo);


	{
		if (SettingsPanel->GetSettings().bDrawBatchLine)
		{
			// 라인 배처는 매 프레임 한 번만 비우고 한 번만 그린다.
			// 바운딩박스는 그 안에 쌓이는 여러 항목 중 하나일 뿐이다.
			LineBatcher->BeginFrame();

			if (SettingsPanel->GetSettings().bDrawBoundingBox)
			{
				LineBatcher->BuildVertexBuffer();
				InWorld->GetPathTracker().OnRender(LineBatcher.get());
			}

			// 선택된 액터에 붙은 LightComponent를 표시한다. 액터 종류에 의존하지 않는다.
			if (Gizmo->GetTarget() && CanEditViewport() && Gizmo->GetTarget()->GetOwner()->GetWorld() == InWorld)
			{
				for (UActorComponent* Component : Gizmo->GetTarget()->GetOwner()->GetComponents())
				{
					if (USpotLightComponent* Light = Cast<USpotLightComponent>(Component))
						Light->DrawDebug(LineBatcher.get());
				}
			}

			LineBatcher->OnRender(ViewProjection);

		}

		// 삼각형 연결은 유지하고 View별 Fill Mode만 선택한다.
		const ERasterizerState SceneRasterizerState = MultipleViewportsAdapter.IsViewWireframe(ViewIndex)
			? ERasterizerState::Wireframe : ERasterizerState::SolidBack;

		// 렌더 루프 — 반드시 RenderAll보다 먼저
		SkyboxRenderer->OnRender(ViewProjection, ViewCameraLocation);
		if (bDrawPrimitives)
		{
			RenderCommand::SetRasterizerState(SceneRasterizerState);
			RenderCommand::SetBlendState(EBlendState::Opaque);
			RenderCommand::SetDepthStencilState(EDepthStencilState::Default);

			RenderCommand::SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			// 반투명은 Grid 뒤에 합성되어야 하므로 불투명만 먼저 그린다.
			Renderer->RenderQueueSorting(RenderQueue, ViewProjection);
			Renderer->UpdatePointLight(InWorld->GetScene());
			Renderer->RenderOpaque(ViewProjection);

			// 장면 Wireframe이 Grid·Gizmo·UI로 전파되지 않도록 복원한다.
			RenderCommand::SetRasterizerState(ERasterizerState::SolidBack);
		}
		RenderCommand::EndRenderPass(ViewRenderingInfo);

		const bool bShowFog = SettingsPanel->GetSettings().bShowFog;

		if (bDrawPrimitives && bShowFog && FogRenderer && !MultipleViewportsAdapter.IsOrthographic(ViewIndex))
		{
			const auto& Fogs = InWorld->GetScene().ExponentialFogs;
			if (!Fogs.IsEmpty())
			{
				const auto& FogInfo = Fogs[0].Info;
				FHeightFogConstants Constants{};
				Constants.FogInscatteringColor = FogInfo.FogInscatteringColor;
				Constants.FogHeight = FogInfo.FogHeight;
				Constants.FogDensity = FogInfo.FogDensity;
				Constants.FogHeightFalloff = FogInfo.FogHeightFalloff;
				Constants.StartDistance = FogInfo.StartDistance;
				Constants.FogCutoffDistance = FogInfo.FogCutoffDistance;
				Constants.FogMaxOpacity = FogInfo.FogMaxOpacity;

				const FRenderingInfo& FogPass = ViewportsPanel->GetFogRenderingInfo(ViewIndex);
				RenderCommand::BeginRenderPass(FogPass);
				const bool bFogDrawn = FogRenderer->OnRender(
					PostProcessSource,
					ViewRenderingInfo.DepthStencil.Texture,
					ViewProjection,
					ViewCameraLocation,
					Constants);
				RenderCommand::EndRenderPass(FogPass);
				if (bFogDrawn)
					PostProcessSource = ViewportsPanel->GetFogColor(ViewIndex);
			}
		}

		// FogColor 또는 SceneColor에서 색 렌더링을 이어가며 기존 깊이를 유지한다.
		FRenderingInfo ContinuationInfo{};
		ContinuationInfo.ViewportSetting = ViewRenderingInfo.ViewportSetting;
		FRenderingDesc ContinuationColor = ViewRenderingInfo.ColorRenderTargets[0];
		ContinuationColor.Texture = PostProcessSource;
		ContinuationColor.LoadOp = ERenderTargetLoadOp::Load;
		ContinuationInfo.ColorRenderTargets.Add(ContinuationColor);
		ContinuationInfo.DepthStencil = ViewRenderingInfo.DepthStencil;
		ContinuationInfo.DepthStencil.LoadOp = ERenderTargetLoadOp::Load;
		RenderCommand::BeginRenderPass(ContinuationInfo);

		if (SettingsPanel->GetSettings().bDrawBatchLine)
		{
			const EGridPlane GridPlane = MultipleViewportsAdapter.GetGridPlane(ViewIndex);

			if (SettingsPanel->GetSettings().bDrawPSGrid && !MultipleViewportsAdapter.IsOrthographic(ViewIndex))
			{
				GridRenderer->OnRenderPSGrid(
					ViewProjection, ViewCameraLocation, SettingsPanel->GetSettings(), ViewRenderingInfo.ViewportSetting
				);
			}
			else
			{
				GridRenderer->OnRenderBatchGrid(
					ViewProjection,
					ViewCameraLocation,
					ViewCameraForward,
					GridPlane,
					static_cast<float>(SettingsPanel->GetSettings().GridSpacing),
					!MultipleViewportsAdapter.IsOrthographic(ViewIndex) ||
					MultipleViewportsAdapter.GetCameraPreset(ViewIndex) == EMultipleViewportsCameraPreset::OrthographicView,
					ViewRenderingInfo.ViewportSetting
				);
			}
		}

		if (bDrawPrimitives)
		{
			// Grid 파이프라인이 바꾼 상태를 장면 기준으로 되돌린 뒤 반투명을 먼 것부터 그린다.
			RenderCommand::SetRasterizerState(SceneRasterizerState);
			RenderCommand::SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			Renderer->RenderTranslucent(ViewProjection);
			RenderCommand::SetRasterizerState(ERasterizerState::SolidBack);
		}

		// TextRenderComponent 렌더링
		for (TObjectIterator<UTextRenderComponent> TextComponent; TextComponent; ++TextComponent)
		{
			if (!TextComponent || TextComponent->HasAnyFlags(EObjectFlags::RF_Transient) ||
				!TextComponent->IsRegistered() || !TextComponent->GetFont() || !TextComponent->IsVisible() ||
				!TextComponent->GetOwner() || TextComponent->GetOwner()->GetWorld() != InWorld)
			{
				continue;
			}

				TextComponent->Render(*TextRenderer,
				TextComponent->GetWorldMatrix(),
				ViewProjection
			);
		}
		RenderCommand::EndRenderPass(ContinuationInfo);
	}

	// Todo: Post process
	const FRenderingInfo& PostProcessInfo = ViewportsPanel->GetPostProcessRenderingInfo(ViewIndex);
	RenderCommand::BeginRenderPass(PostProcessInfo);
	{
		FPipelineState Pipeline{};
		Pipeline.Shader = PostProcessShader;
		Pipeline.Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		Pipeline.RasterizerState = ERasterizerState::SolidNone;
		Pipeline.DepthStencilState = EDepthStencilState::Disabled;

		RenderCommand::BindPipelineState(Pipeline);

		// 실제 장면 렌더링에 사용한 투영 정보를 가져온다.
		const FCameraProjection Projection = MultipleViewportsAdapter.GetRenderProjection(ViewIndex);

		// 구조체의 기본값으로 FXAA 세부 설정도 초기화한다.
		FPostProcessConstants PostProcessData{};
		PostProcessData.DisplayMode = static_cast<uint32>(ViewportsPanel->GetDisplayMode(ViewIndex));
		PostProcessData.NearClip = Projection.NearClip;
		PostProcessData.FarClip = Projection.FarClip;
		PostProcessData.IsOrthographic = (Projection.Mode == EProjectionMode::Orthographic) ? 1U : 0U;

		// 체크하면 1(FXAA 적용), 체크를 해제하면 0(원본 장면 표시).
		PostProcessData.EnableFXAA = SettingsPanel->GetSettings().bEnableFXAA ? 1U : 0U;

		RenderCommand::UpdateBufferData(PostProcessConstantBuffer.get(), &PostProcessData, sizeof(PostProcessData));

		// 픽셀 셰이더의 b0에 연결한다.
		RenderCommand::BindConstantBuffer(0, PostProcessConstantBuffer.get(), EShaderBindFlagBits::Pixel);
		RenderCommand::BindShaderResource(0, PostProcessSource, EShaderBindFlagBits::Pixel);
		RenderCommand::BindShaderResource(1, ViewRenderingInfo.DepthStencil.Texture, EShaderBindFlagBits::Pixel);

		RenderCommand::BindSamplerState(0, ESamplerState::LinearClamp, EShaderBindFlagBits::Pixel);
		RenderCommand::Draw(3);

		// 이후 오버레이에서 Depth를 DSV로 쓰기 전에 해제한다.
		ID3D11ShaderResourceView* NullSRVs[2] = { nullptr, nullptr };
		RenderCommand::GetContext()->PSSetShaderResources(0, 2, NullSRVs);
	}
	RenderCommand::EndRenderPass(PostProcessInfo);

	FRenderingInfo OverlayInfo{};
	OverlayInfo.ViewportSetting = PostProcessInfo.ViewportSetting;

	FRenderingDesc OverlayColor = PostProcessInfo.ColorRenderTargets[0];
	OverlayColor.LoadOp = ERenderTargetLoadOp::Load;
	OverlayInfo.ColorRenderTargets.Add(OverlayColor);

	OverlayInfo.DepthStencil = ViewRenderingInfo.DepthStencil;
	OverlayInfo.DepthStencil.LoadOp = ERenderTargetLoadOp::Load;

	RenderCommand::BeginRenderPass(OverlayInfo);
	{
		// 스텐실 기반이라 선택 대상의 가시성이 꺼져 있어도 외곽선만 그린다.
		if (CanEditViewport() && Outline->GetActor() && Outline->GetActor()->GetWorld() == InWorld)
		{
			OutlineRenderer->OnRender(*Outline, ViewProjection, ViewRenderingInfo.ViewportSetting);
		}

		if (Gizmo->GetTarget() && CanEditViewport() && Gizmo->GetTarget()->GetOwner()->GetWorld() == InWorld)
		{
			RenderCommand::ClearDepthStencil(ViewRenderingInfo.DepthStencil.Texture);

			GizmoRenderer->OnRender(
				*Gizmo,
				ViewProjection,
				ViewCameraLocation,
				MultipleViewportsAdapter.IsOrthographic(ViewIndex));
		}

		RenderCommand::ClearDepthStencil(ViewRenderingInfo.DepthStencil.Texture);

		if (SettingsPanel->GetSettings().bShowUUID)
		{
			for (AActor* Actor : InWorld->GetPersistentLevel()->GetActors())
			{
				if (!Actor)
					continue;

				FVector UUIDLocation = Actor->GetActorLocation();
				if (UPrimitiveComponent* Primitive =
					Cast<UPrimitiveComponent>(Actor->GetRootComponent()))
				{
					const FBox Box =
					Primitive->CalcBounds();
					UUIDLocation = FVector((Box.Min.X + Box.Max.X) * 0.5f, (Box.Min.Y + Box.Max.Y) * 0.5f, Box.Max.Z);
				}
				UUIDLocation.Z += 0.5f;
				UTextRenderComponent* UUIDText = Actor->GetUUIDTextComponent();
				UUIDText->SetFont(SystemFont);
				UUIDText->SetRelativeLocation(UUIDLocation);

				const FMatrix BillboardWorld = MultipleViewportsAdapter.BuildEngineBillboardMatrix(ViewIndex, UUIDLocation, 1.0f, 1.0f);
				UUIDText->Render(*TextRenderer, BillboardWorld, ViewProjection);
			}
		}
	}
	RenderCommand::EndRenderPass(OverlayInfo);
	
}


// View Texture가 포함된 UI를 Swapchain 백버퍼에 합성한다. Present는 FEngineLoop가 한다.
void UEditorEngine::PresentFrame()
{
	// Swapchain 렌더링
	RenderCommand::BeginRenderPass(MainWindowSC->GetRenderingInfo());

	{
		SCOPE_CYCLE_COUNTER(STAT_ImGui);

		ImGuiRenderer->Begin();

		EditorUI->OnRender();

		ImGuiRenderer->End();
	}

	RenderCommand::EndRenderPass(MainWindowSC->GetRenderingInfo());
}

// ImGui를 정리한다. UObject 일괄 삭제와 공용 자원·Device 정리는 FEngineLoop가 이어서 한다.
void UEditorEngine::PreExit()
{
	EndPIE();
	ResetSceneSelection();
	while (!WorldList.IsEmpty())
		DeleteWorldContext(WorldList.Last().get());
	// Todo: Post process
	PostProcessConstantBuffer.reset();
	//

	ImGuiRenderer->Shutdown();
}

// 선택과 Gizmo 참조를 정리한 뒤 Actor를 삭제한다.
void UEditorEngine::DeleteActor(AActor* Actor)
{
	if (!Actor)
		return;

	OutlinerPanel->SelectActor(nullptr);

	Actor->Destroy();
}

void UEditorEngine::SetEditingWorld(UWorld* World)
{
	// 패널들은 같은 World를 편집한다. World 전환은 기존 선택 참조를 먼저 해제한다.
	ResetSceneSelection();
	OutlinerPanel->SetWorld(World);
	DetailsPanel->SetWorld(World);
	EditorControlsPanel->SetWorld(World);
	SettingsPanel->SetWorld(World);
}

void UEditorEngine::ApplyActorSelection(AActor* Actor)
{
	DetailsPanel->SetActor(Actor);

	// Actor 선택 시 Root를 조작하고 소유한 메시 전체를 강조한다. 빈 Actor도 Details에 남는다.
	Gizmo->SetTarget(Actor ? Actor->GetRootComponent() : nullptr);
	Outline->SetActor(Actor);
}

void UEditorEngine::SelectActorAndComponent(AActor* Actor, const FString& ComponentName)
{
	OutlinerPanel->SelectActor(Actor);
	if (!Actor || ComponentName.empty()) return;

	// World가 바뀌어도 같은 이름의 복제/원본 컴포넌트 선택을 이어간다.
	for (UActorComponent* Component : Actor->GetComponents())
	{
		if (Component->GetName() == ComponentName)
		{
			DetailsPanel->SelectComponent(Component);
			break;
		}
	}
}

void UEditorEngine::ApplyComponentSelection(UActorComponent* Component)
{
	AActor* Actor = OutlinerPanel->GetSelectedActor();
	if (!Component)
	{
		Gizmo->SetTarget(Actor ? Actor->GetRootComponent() : nullptr);
		Outline->SetActor(Actor);
		return;
	}

	// 트리에서 컴포넌트를 고르면 위치를 가진 해당 컴포넌트만 조작한다.
	Gizmo->SetTarget(Cast<USceneComponent>(Component));
	Outline->SetTarget(Cast<UPrimitiveComponent>(Component));
}

void UEditorEngine::StartPIE()
{
	if (bIsPlaying) return;
	FWorldContext* EditorContext = GetWorldContext(EWorldType::Editor);
	if (!EditorContext || !EditorContext->CurrentWorld) return;

	UWorld* EditorWorld = EditorContext->CurrentWorld;
	AActor* SelectedActor = OutlinerPanel->GetSelectedActor();
	UActorComponent* SelectedComponent = DetailsPanel->GetSelectedComponent();
	const FString ComponentName = SelectedComponent ? SelectedComponent->GetName() : FString();

	// 복제에 성공한 뒤에만 패널과 View를 PIE로 전환한다.
	UWorld* PIEWorld = FObjectDuplicator::DuplicateWorld(EditorWorld, EWorldType::PIE);
	if (!PIEWorld) return;
	if (!CreateNewWorldContext(EWorldType::PIE, "PIE", PIEWorld))
	{
		delete PIEWorld;
		return;
	}

	EditorToPIEActors.Empty();
	PIEToEditorActors.Empty();
	const auto& SourceActors = EditorWorld->GetPersistentLevel()->GetActors();
	const auto& Copies = PIEWorld->GetPersistentLevel()->GetActors();
	for (int32 Index = 0; Index < SourceActors.Num(); ++Index)
	{
		EditorToPIEActors.Add(SourceActors[Index], Copies[Index]);
		PIEToEditorActors.Add(Copies[Index], SourceActors[Index]);
	}

	PIEIndex = MultipleViewportsAdapter.GetActiveViewIndex();
	if (PIEIndex == static_cast<uint32>(-1)) PIEIndex = 0;
	MultipleViewportsAdapter.SetViewportWorld(PIEIndex, PIEWorld);
	SetEditingWorld(PIEWorld);
	if (AActor** Copy = EditorToPIEActors.Find(SelectedActor))
	{
		SelectActorAndComponent(*Copy, ComponentName);
	}

	PIEWorld->GetMainCamera()->GetCameraComponent()->SetExternalInputManaged(true);
	PIEWorld->BeginPlay();
	bIsEjected = false;
	ViewportsPanel->SetPIEEditing(false);
	bIsPlaying = true;
}

void UEditorEngine::EndPIE()
{
	FWorldContext* PIEContext = GetWorldContext(EWorldType::PIE);
	if (!PIEContext && !bIsPlaying) return;
	FWorldContext* EditorContext = GetWorldContext(EWorldType::Editor);
	UWorld* EditorWorld = EditorContext ? EditorContext->CurrentWorld : nullptr;

	AActor* RestoreActor = nullptr;
	if (AActor** Source = PIEToEditorActors.Find(OutlinerPanel->GetSelectedActor())) RestoreActor = *Source;
	UActorComponent* SelectedComponent = DetailsPanel->GetSelectedComponent();
	const FString ComponentName = SelectedComponent ? SelectedComponent->GetName() : FString();

	// 삭제할 PIE 객체를 참조하는 선택/패널/캡처를 먼저 Editor 쪽으로 전환한다.
	SetEditingWorld(EditorWorld);
	if (PIEIndex != static_cast<uint32>(-1))
		MultipleViewportsAdapter.SetViewportWorld(PIEIndex, EditorWorld);
	if (PIEContext)
	{
		MultipleViewportsAdapter.ForgetWorld(PIEContext->CurrentWorld);
		PIEContext->CurrentWorld->EndPlay();
		DeleteWorldContext(PIEContext);
	}

	EditorToPIEActors.Empty();
	PIEToEditorActors.Empty();
	PIEIndex = -1;
	bIsPlaying = false;
	bIsEjected = false;
	ViewportsPanel->SetPIEEditing(false);

	SelectActorAndComponent(RestoreActor, ComponentName);
}

// 씬 변경으로 무효화된 에디터의 선택 참조를 모두 해제한다.
void UEditorEngine::ResetSceneSelection()
{
	Gizmo->SetTarget(nullptr);
	Outline->SetTarget(nullptr);
	DetailsPanel->SetActor(nullptr);
	OutlinerPanel->SelectActor(nullptr);
}

// 새 씬 생성이 성공하면 에디터 선택 상태를 초기화한다.
void UEditorEngine::CreateNewScene()
{
	EndPIE();
	UWorld* World = nullptr;
	for (UWorld* WorldPtr : MultipleViewportsAdapter.GetViewportWorlds())
	{
		if (EWorldType::Editor == WorldPtr->GetWorldType())
		{
			World = WorldPtr;
		}
	}
	
	if (!World) return;

	if (!FEditorFileUtils::NewScene(World))
		return;

	ResetSceneSelection();
}

// 씬 불러오기가 성공하면 에디터 선택 상태를 초기화한다.
void UEditorEngine::OpenScene()
{
	EndPIE();
	UWorld* World = nullptr;
	for (UWorld* WorldPtr : MultipleViewportsAdapter.GetViewportWorlds())
	{
		if (EWorldType::Editor == WorldPtr->GetWorldType())
		{
			World = WorldPtr;
		}
	}
	if (!World) return;

	if (!FEditorFileUtils::LoadScene(World))
		return;

	ResetSceneSelection();
}

// 공통 파일 유틸리티로 현재 씬을 저장한다.
void UEditorEngine::SaveCurrentScene()
{
	UWorld* World = nullptr;
	for (UWorld* WorldPtr : MultipleViewportsAdapter.GetViewportWorlds())
	{
		if (EWorldType::Editor == WorldPtr->GetWorldType())
		{
			World = WorldPtr;
		}
	}
	if (!World) return;

	FEditorFileUtils::SaveScene(World);
}

// 공통 파일 유틸리티로 새 경로에 씬을 저장한다.
void UEditorEngine::SaveSceneAs()
{
	UWorld* World = nullptr;
	for (UWorld* WorldPtr : MultipleViewportsAdapter.GetViewportWorlds())
	{
		if (EWorldType::Editor == WorldPtr->GetWorldType())
		{
			World = WorldPtr;
		}
	}
	if (!World) return;

	FEditorFileUtils::SaveSceneAs(World);
}
 
