#include "EnginePCH.h"
#include "Editor/EditorUI/ImGuiRenderer.h"

#include "Core/Window.h"
#include "Input/InputSystem.h"

#include <backends/imgui_impl_dx11.h>
#include <backends/imgui_impl_win32.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace
{
	// FWindow가 ImGui를 직접 알지 않도록 메시지 처리기를 훅으로 등록한다.
	bool ImGuiWndProcHook(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		return ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam) != 0;
	}

	void ApplyDefaultStyle()
	{
		ImGuiStyle* style = &ImGui::GetStyle();
		ImVec4* colors = style->Colors;

		// Neutral background
		const ImVec4 Black = ImVec4(0.095f, 0.095f, 0.100f, 1.000f);
		const ImVec4 DarkGray = ImVec4(0.145f, 0.145f, 0.155f, 1.000f);
		const ImVec4 Gray = ImVec4(0.220f, 0.220f, 0.235f, 1.000f);
		const ImVec4 LightGray = ImVec4(0.400f, 0.400f, 0.420f, 1.000f);

		// GWJNS accent
		const ImVec4 Navy = ImVec4(0.153f, 0.200f, 0.424f, 1.000f);		 // #27336C
		const ImVec4 Blue = ImVec4(0.282f, 0.329f, 0.518f, 1.000f);		 // #485484
		const ImVec4 SoftBlue = ImVec4(0.443f, 0.486f, 0.647f, 1.000f);	 // #717CA5
		const ImVec4 LightBlue = ImVec4(0.741f, 0.839f, 0.941f, 1.000f); // #BDD6F0
		const ImVec4 White = ImVec4(0.945f, 0.965f, 0.973f, 1.000f);

		colors[ImGuiCol_Text] = White;
		colors[ImGuiCol_TextDisabled] = LightGray;

		// 배경은 검정/회색 유지
		colors[ImGuiCol_WindowBg] = ImVec4(0.110f, 0.110f, 0.115f, 1.000f);
		colors[ImGuiCol_ChildBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
		colors[ImGuiCol_PopupBg] = DarkGray;

		colors[ImGuiCol_Border] = Gray;
		colors[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);

		colors[ImGuiCol_FrameBg] = Black;
		colors[ImGuiCol_FrameBgHovered] = DarkGray;
		colors[ImGuiCol_FrameBgActive] = Gray;

		colors[ImGuiCol_TitleBg] = Black;
		colors[ImGuiCol_TitleBgActive] = DarkGray;
		colors[ImGuiCol_TitleBgCollapsed] = Black;

		colors[ImGuiCol_MenuBarBg] = DarkGray;

		colors[ImGuiCol_ScrollbarBg] = Black;
		colors[ImGuiCol_ScrollbarGrab] = Gray;
		colors[ImGuiCol_ScrollbarGrabHovered] = LightGray;
		colors[ImGuiCol_ScrollbarGrabActive] = Blue;

		// 포인트 색
		colors[ImGuiCol_CheckboxSelectedBg] = Navy;
		colors[ImGuiCol_CheckMark] = White;

		colors[ImGuiCol_SliderGrab] = Blue;
		colors[ImGuiCol_SliderGrabActive] = LightBlue;

		colors[ImGuiCol_Button] = Navy;
		colors[ImGuiCol_ButtonHovered] = Blue;
		colors[ImGuiCol_ButtonActive] = SoftBlue;

		// 리스트/트리 기본은 회색, interaction만 파랑
		colors[ImGuiCol_Header] = Gray;
		colors[ImGuiCol_HeaderHovered] = Blue;
		colors[ImGuiCol_HeaderActive] = Navy;

		colors[ImGuiCol_Separator] = Gray;
		colors[ImGuiCol_SeparatorHovered] = Blue;
		colors[ImGuiCol_SeparatorActive] = LightBlue;

		colors[ImGuiCol_ResizeGrip] = ImVec4(1, 1, 1, 0.20f);
		colors[ImGuiCol_ResizeGripHovered] = ImVec4(LightBlue.x, LightBlue.y, LightBlue.z, 0.65f);
		colors[ImGuiCol_ResizeGripActive] = LightBlue;

		// 탭도 배경은 검정
		colors[ImGuiCol_Tab] = Black;
		colors[ImGuiCol_TabHovered] = Blue;
		colors[ImGuiCol_TabActive] = DarkGray;
		colors[ImGuiCol_TabUnfocused] = Black;
		colors[ImGuiCol_TabUnfocusedActive] = DarkGray;

		colors[ImGuiCol_DockingPreview] = ImVec4(LightBlue.x, LightBlue.y, LightBlue.z, 0.70f);
		colors[ImGuiCol_DockingEmptyBg] = Black;

		colors[ImGuiCol_PlotLines] = LightGray;
		colors[ImGuiCol_PlotLinesHovered] = LightBlue;
		colors[ImGuiCol_PlotHistogram] = SoftBlue;
		colors[ImGuiCol_PlotHistogramHovered] = LightBlue;

		colors[ImGuiCol_TextSelectedBg] = ImVec4(Blue.x, Blue.y, Blue.z, 0.60f);

		colors[ImGuiCol_DragDropTarget] = LightBlue;
		colors[ImGuiCol_NavHighlight] = LightBlue;
		colors[ImGuiCol_NavWindowingHighlight] = LightBlue;

		colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0, 0, 0, 0.586f);
		colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0, 0, 0, 0.586f);

		style->ChildRounding = 4.0f;
		style->FrameBorderSize = 1.0f;
		style->FrameRounding = 2.0f;
		style->GrabMinSize = 7.0f;
		style->PopupRounding = 2.0f;
		style->ScrollbarRounding = 12.0f;
		style->ScrollbarSize = 13.0f;
		style->TabBorderSize = 1.0f;
		style->TabRounding = 0.0f;
		style->WindowRounding = 4.0f;
	}
}

FImGuiRenderer::~FImGuiRenderer()
{

}

bool FImGuiRenderer::Init(HWND WindowHandle, ID3D11Device* Device, ID3D11DeviceContext* DeviceContext)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

	ImGui_ImplWin32_Init(WindowHandle);
	ImGui_ImplDX11_Init(Device, DeviceContext);
	FWindow::SetWndProcHook(&ImGuiWndProcHook);

	ApplyDefaultStyle();

	const float DpiScale = ImGui_ImplWin32_GetDpiScaleForHwnd(WindowHandle);

	ImGuiStyle& Style = ImGui::GetStyle();
	Style.ScaleAllSizes(DpiScale);
	Style.FontScaleDpi = DpiScale;

	io.ConfigDpiScaleFonts = true;
	io.ConfigDpiScaleViewports = true;

	return true;
}

void FImGuiRenderer::Begin()
{
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();

	ImGui::NewFrame();

	ImGuiIO& io = ImGui::GetIO(); (void)io;
}

void FImGuiRenderer::End()
{
	ImGuiIO& io = ImGui::GetIO();

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	ImGui::UpdatePlatformWindows();
	ImGui::RenderPlatformWindowsDefault();
}

void FImGuiRenderer::Shutdown()
{
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
}
