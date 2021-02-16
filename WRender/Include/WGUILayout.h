#pragma once
#include <d3d12.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <DirectXColors.h>
#include <DirectXCollision.h>
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"
#include "../../Common/d3dUtil.h"

inline static DirectX::XMFLOAT3 fpToXMFLOAT3(float fp[3])
{
	return XMFLOAT3(fp);
}

inline static DirectX::XMFLOAT4 fpToXMFLOAT4(float fp[3])
{
	return XMFLOAT4(fp);
}

class WGUILayout 
{
public:
	typedef std::unordered_map<std::string, std::unique_ptr<Material>> MaterialTable;
	WGUILayout() = default;
	static void HelpMarker(const char* desc);
	static void ShowAppPropertyEditor(bool* p_open, MaterialTable& materials);
	static void ShowPlaceholderObject(const char* prefix, int uid,
		MaterialTable& materials, const char* material_name);
	static void DrawGUILayout(const Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& mCommandList,
		const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& mSrvHeap, MaterialTable& materials);
};

// Helper to display a little (?) mark which shows a tooltip when hovered.
// In your own code you may want to display an actual icon if you are using a merged icon fonts (see docs/FONTS.md)
void WGUILayout::HelpMarker(const char* desc)
{
	ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered())
	{
		ImGui::BeginTooltip();
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
		ImGui::TextUnformatted(desc);
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
}

// Demonstrate create a simple property editor.
void WGUILayout::ShowAppPropertyEditor(bool* p_open, MaterialTable& materials)
{
	ImGui::SetNextWindowSize(ImVec2(430, 450), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Material editor", p_open))
	{
		ImGui::End();
		return;
	}

	// List placeholder objects 
	ShowPlaceholderObject("bricks", 0, materials, "bricks0");
	ShowPlaceholderObject("tile", 1, materials, "tile0");
	ShowPlaceholderObject("mirror", 2, materials, "mirror0");
	ShowPlaceholderObject("skull", 3, materials, "skullMat");
	ShowPlaceholderObject("sky", 4, materials, "sky");

	ImGui::End();
}

//-----------------------------------------------------------------------------
// [SECTION] Example App: Property Editor / ShowExampleAppPropertyEditor()
//-----------------------------------------------------------------------------

void WGUILayout::ShowPlaceholderObject(const char* prefix, int uid,
	MaterialTable& materials, const char* material_name)
{
	// Use object uid as identifier. Most commonly you could also use the object pointer as a base ID.
	ImGui::PushID(uid);

	// Text and Tree nodes are less high than framed widgets, using AlignTextToFramePadding() we add vertical spacing to make the tree lines equal high.
	ImGui::AlignTextToFramePadding();
	bool node_open = ImGui::TreeNode("Material", "%s", prefix, uid);
	Material* mat = materials[std::string(material_name)].get();

	if (node_open)
	{
		// Default settings for color edit
		static bool alpha_preview = true;
		static bool alpha_half_preview = false;
		static bool drag_and_drop = true;
		static bool options_menu = true;
		static bool hdr = false;
		ImGuiColorEditFlags misc_flags = (hdr ? ImGuiColorEditFlags_HDR : 0) | (drag_and_drop ? 0 : ImGuiColorEditFlags_NoDragDrop) | (alpha_half_preview ? ImGuiColorEditFlags_AlphaPreviewHalf : (alpha_preview ? ImGuiColorEditFlags_AlphaPreview : 0)) | (options_menu ? 0 : ImGuiColorEditFlags_NoOptions);
		
		float fpForFresnelR0[3] = {mat->FresnelR0.x, mat->FresnelR0.y, mat->FresnelR0.z};
		ImGui::DragFloat3("FresnelR0##value", fpForFresnelR0, 0.01f);
		mat->FresnelR0 = fpToXMFLOAT3(fpForFresnelR0);

		ImGui::DragFloat("Roughness##value", &mat->Roughness, 0.01f);

		ImGui::DragFloat("Metallic##value", &mat->Metallic, 0.01f);

		float fpForAlbedo[4] = { mat->DiffuseAlbedo.x, mat->DiffuseAlbedo.y, mat->DiffuseAlbedo.z, mat->DiffuseAlbedo.z };
		ImGui::ColorEdit4("DiffuseAlbedo##2f", fpForAlbedo, ImGuiColorEditFlags_Float | misc_flags);
		mat->DiffuseAlbedo = fpToXMFLOAT4(fpForAlbedo);
	

		mat->NumFramesDirty = 3;

		ImGui::TreePop();
	}
	ImGui::PopID();
}

void WGUILayout::DrawGUILayout(const Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& mCommandList,
	const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& mSrvHeap, MaterialTable& materials)
{
	// Start the Dear ImGui frame
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	// 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
	bool show_demo_window = true;
	if (show_demo_window)
		ImGui::ShowDemoWindow(&show_demo_window);

	// Show My GUI Layout
	bool show_gui_layout = true;
	bool* p_open = &show_gui_layout;
	// Demonstrate the various window flags. Typically you would just use the default!
	static bool no_titlebar = false;
	static bool no_scrollbar = false;
	static bool no_menu = false;
	static bool no_move = false;
	static bool no_resize = false;
	static bool no_collapse = false;
	static bool no_close = false;
	static bool no_nav = false;
	static bool no_background = false;
	static bool no_bring_to_front = false;

	ImGuiWindowFlags window_flags = 0;
	if (no_titlebar)        window_flags |= ImGuiWindowFlags_NoTitleBar;
	if (no_scrollbar)       window_flags |= ImGuiWindowFlags_NoScrollbar;
	if (!no_menu)           window_flags |= ImGuiWindowFlags_MenuBar;
	if (no_move)            window_flags |= ImGuiWindowFlags_NoMove;
	if (no_resize)          window_flags |= ImGuiWindowFlags_NoResize;
	if (no_collapse)        window_flags |= ImGuiWindowFlags_NoCollapse;
	if (no_nav)             window_flags |= ImGuiWindowFlags_NoNav;
	if (no_background)      window_flags |= ImGuiWindowFlags_NoBackground;
	if (no_bring_to_front)  window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
	if (no_close)           p_open = NULL; // Don't pass our bool* to Begin

	// We specify a default position/size in case there's no data in the .ini file.
	// We only do it to make the demo applications a little more welcoming, but typically this isn't required.
	ImGui::SetNextWindowPos(ImVec2(650, 20), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(550, 680), ImGuiCond_FirstUseEver);

	// Main body of my GUI Layout starts here.
	if (!ImGui::Begin("WRender", p_open, window_flags))
	{
		// Early out if the window is collapsed, as an optimization.
		ImGui::End();
		return;
	}

	static bool show_hierarchy = false;
	static bool show_app_property_editor = false;
	if (show_hierarchy)     ShowAppPropertyEditor(&show_hierarchy, materials);

	// e.g. Leave a fixed amount of width for labels (by passing a negative value), the rest goes to widgets.
	ImGui::PushItemWidth(ImGui::GetFontSize() * -12);
	// Menu Bar
	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("Resource"))
		{
			ImGui::MenuItem("Hierarchy", NULL, &show_hierarchy);
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}
	ImGui::End();

	mCommandList->SetDescriptorHeaps(1, mSrvHeap.GetAddressOf());
	ImGui::Render();
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), mCommandList.Get());
	// End the Dear ImGui Frame
}