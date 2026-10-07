#include "EnginePCH.h"
#include "Editor/EditorUI/EditorUI.h"
#include <imgui_internal.h>

bool FEditorUI::Init(bool bInUseDockSpace, bool bInPassthruCentralNode)
{
	bUseDockSpace = bInUseDockSpace;
	bPassthruCentralNode = bInPassthruCentralNode;
	return true;

	return false;
}

void FEditorUI::Tick(float DeltaTime)
{
	for (auto& Panel : Panels)
	{

		Panel->Tick(DeltaTime);
	}
}

void FEditorUI::OnRender()
{
	DrawMainMenuBar();
	
	if (bUseDockSpace)
	{
		static bool dockspaceOpen = true;
		static bool optFullscreen = true;
		static bool optPadding = false;
		static ImGuiDockNodeFlags dockspaceFlags = bPassthruCentralNode
			? ImGuiDockNodeFlags_PassthruCentralNode
			: ImGuiDockNodeFlags_None;

		// We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
		// because it would be confusing to have two docking targets within each others.
		//ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
		ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDocking;
		if (optFullscreen)
		{
			const ImGuiViewport* viewport = ImGui::GetMainViewport();
			ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + ToolBarHeight));
			ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, viewport->WorkSize.y - ToolBarHeight));
			ImGui::SetNextWindowViewport(viewport->ID);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
			windowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
			windowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
		}
		else
		{
			dockspaceFlags &= ~ImGuiDockNodeFlags_PassthruCentralNode;
		}

		// When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render our background
		// and handle the pass-thru hole, so we ask Begin() to not render a background.
		if (dockspaceFlags & ImGuiDockNodeFlags_PassthruCentralNode)
			windowFlags |= ImGuiWindowFlags_NoBackground;

		// Important: note that we proceed even if Begin() returns false (aka window is collapsed).
		// This is because we want to keep our DockSpace() active. If a DockSpace() is inactive,
		// all active windows docked into it will lose their parent and become undocked.
		// We cannot preserve the docking relationship between an active window and an inactive docking, otherwise
		// any change of dockspace/settings would lead to windows being stuck in limbo and never being visible.
		if (!optPadding)
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

		//ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, Daydream::Application::GetInstance().isMaximzed ? ImVec2(6.0f, 6.0f) : ImVec2(0.0f, 0.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, false ? ImVec2(6.0f, 6.0f) : ImVec2(0.0f, 0.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);
		ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4{ 0.0f, 0.0f, 0.0f, 0.0f });

		ImGui::Begin("DockingSpace", &dockspaceOpen, windowFlags);
		//m_DockSpacePos = { ImGui::GetWindowPos().x, ImGui::GetWindowPos().y };
		//STS_CORE_WARN("Dock Space Coord = {0}, {1}", m_DockSpacePos.x, m_DockSpacePos.y);
		ImGui::PopStyleColor(); // MenuBarBg
		ImGui::PopStyleVar(2);

		if (!optPadding)
			ImGui::PopStyleVar();

		if (optFullscreen)
			ImGui::PopStyleVar(2);

		// Submit the DockSpace
		ImGuiIO& io = ImGui::GetIO();
		ImGuiStyle& style = ImGui::GetStyle();
		float minWinSizeX = style.WindowMinSize.x;
		style.WindowMinSize.x = 370.0f;
		if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
		{
			ImGuiID dockspace_id = ImGui::GetID("EngineDockingSpace");
			ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspaceFlags);
		}

		ImGui::End();
	}
	DrawMainToolBar();

	for (auto& Panel : Panels)
	{
		if (Panel->IsOpen())
		{
			Panel->OnRender();
		}
	}
}

// 파일 메뉴의 요청을 애플리케이션 콜백으로 전달한다.
void FEditorUI::DrawMainMenuBar()
{
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 10.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 10.0f));

	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("New Scene")) { if (OnNewScene) OnNewScene(); }
			if (ImGui::MenuItem("Open Scene...")) { if (OnOpenScene) OnOpenScene(); }

			ImGui::Separator();

			if (ImGui::MenuItem("Save")) { if (OnSaveScene) OnSaveScene(); }
			if (ImGui::MenuItem("Save As...")) { if (OnSaveSceneAs) OnSaveSceneAs(); }

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Window"))
		{
			for (auto& Panel : Panels)
			{
				bool bOpen = Panel->IsOpen();

				if (ImGui::MenuItem(Panel->GetPanelName(), nullptr, bOpen))
				{
					Panel->SetOpen(!bOpen);
				}
			}
			ImGui::EndMenu();
		}

		ImGui::EndMainMenuBar();
	}
	ImGui::PopStyleVar(2);
}

// Unreal Style 따라함
void FEditorUI::DrawMainToolBar()
{
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	
	// 메인 툴 바가 그려질 위치. viewport 의 pos 참고 / ImGuiCond_Always - 매프레임 위치 고정
	ImGui::SetNextWindowPos(viewport->WorkPos, ImGuiCond_Always);
	// 메인 툴 바가 그려질 사이즈. viewport 의 size 참고
	ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, ToolBarHeight), ImGuiCond_Always);
	ImGui::SetNextWindowViewport(viewport->ID);

	// 패딩 및 보더 정리
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);	// 모서리 둥근 정도 - 0 -> 각지게
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);	// 외곽 테두리 두께 - 0
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 3.0f));	// 창 내부 여백
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 0.0f));		// 위젯간 간격
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));		// 구분을 위해서 색상 조정

	constexpr ImGuiWindowFlags Flags =
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoDocking;

	if (ImGui::Begin("MainToolBar", nullptr, Flags))
	{
		// 기본 버튼 사이즈
		const ImVec2 ButtonSize(32.0f, 32.0f);	
		// 저장 버튼
		{
			if (ImGui::Button("Save"))
			{
				 if (OnSaveScene) OnSaveScene(); 
			}

			ImGui::SameLine();
		}
		// 구분선
		ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
		ImGui::SameLine();
		// PIE 관련 버튼
		{
			if (ImGui::Button("Play"))
			{
				if (OnStartPIE) 
					OnStartPIE();
			}
			ImGui::SameLine();
			if (ImGui::Button("Stop"))
			{
				if (OnEndPIE) 
OnEndPIE();
			}
			ImGui::SameLine();
			if (ImGui::Button("Setting"))
			{
				ImGui::OpenPopup("PlaySetting");
			}

			if (ImGui::BeginPopup("PlaySetting"))
			{
				if (ImGui::MenuItem("Selected Viewport")) { ; }
				if (ImGui::MenuItem("New Editor Window (PIE)")) { ; }
				ImGui::EndPopup();
			}
			

		}

		ImGui::End();
	}
	// 앞서 설정한 스타일 popping
	ImGui::PopStyleVar(4);
	ImGui::PopStyleColor(1);
}
