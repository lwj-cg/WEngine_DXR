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
#include "../FrameResource.h"

extern const int gNumFrameResources;

inline static DirectX::XMFLOAT3 fpToXMFLOAT3(float fp[3])
{
	return XMFLOAT3(fp);
}

inline static DirectX::XMFLOAT4 fpToXMFLOAT4(float fp[3])
{
	return XMFLOAT4(fp);
}

inline static void setFrameDirty(int& numFrameDirty, int numFrameResources)
{
	numFrameDirty = numFrameResources;
}

class WGUILayout
{
public:
	typedef WPassConstants PassData;
	typedef std::map<std::string, WRenderItem> RenderItemList;
	typedef std::map<std::string, WMaterial> MaterialList;
	typedef std::unordered_map<std::string, std::unique_ptr<WTexture>> TextureList;
	WGUILayout() = default;
	static void HelpMarker(const char* desc);
	static void ShowAppPropertyEditor(bool* p_open, MaterialList& materials);
	static void ShowObjectInspector(bool* p_open, std::string objName, RenderItemList& renderItems, MaterialList& materials, TextureList& textures);
	static void ShowMaterialAttributes(std::string materialName, MaterialList& materials, TextureList& textures);
	static void ShowMaterialModifier(bool* p_open, std::string materialName, MaterialList& materials, TextureList& textures);
	static void ShowPlaceholderObject(const char* prefix, int uid,
		MaterialList& materials, const char* material_name);
	static void DrawGUILayout(const Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& mCommandList, 
		const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& mSrvHeap, 
		PassData& passData, RenderItemList& renderItems, MaterialList& materials, TextureList& textures
	);
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
void WGUILayout::ShowAppPropertyEditor(bool* p_open, MaterialList& materials)
{
	ImGui::SetNextWindowSize(ImVec2(350, 350), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Material modifier", p_open))
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

// Create a simple object inspector
void WGUILayout::ShowObjectInspector(bool* p_open, std::string objName,
	RenderItemList& renderItems, MaterialList& materials, TextureList& textures)
{
	if (renderItems.find(objName) == renderItems.end()) return;
	ImGui::SetNextWindowSize(ImVec2(350, 350), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Object Inspector", p_open))
	{
		ImGui::End();
		return;
	}

	auto& r = renderItems.at(objName);
	ImGui::Text("Object Name: %s", objName.c_str());
	ImGui::Separator();

	// Object's transform property
	ImGui::Text("Transform");
	if (ImGui::DragFloat3("Translation##value", &r.translation.x, 0.01f))
		r.NumFramesDirty = gNumFrameResources;
	if (ImGui::DragFloat3("Rotation##value", &r.rotation.x, 0.1f, -180, 180))
		r.NumFramesDirty = gNumFrameResources;
	if (ImGui::DragFloat3("Scaling##value", &r.scaling.x, 0.1f, 0))
		r.NumFramesDirty = gNumFrameResources;
	ImGui::Separator();

	// Object's material attributes
	static std::vector<const char*> material_items(materials.size());
	static int item_current = 0;
	int n = 0;
	for (auto iter = materials.begin(); iter != materials.end(); ++iter,++n)
	{
		material_items[n] = iter->first.c_str();
		if (iter->first == r.materialName) item_current = n;
	}
	if (ImGui::Combo("Material", &item_current, material_items.data(), materials.size()))
	{
		r.materialName = std::string(material_items[item_current]);
		r.matIdx = materials[r.materialName].MatIdx;
		r.NumFramesDirty = gNumFrameResources;
	}
	ShowMaterialAttributes(r.materialName, materials, textures);

	ImGui::End();
}

void WGUILayout::ShowMaterialAttributes(std::string materialName,
	MaterialList& materials, TextureList& textures)
{
	if (materials.find(materialName) == materials.end()) return;
	auto& m = materials.at(materialName);
	if (ImGui::ColorEdit4("Albedo##value", &m.Albedo.x))
		m.NumFramesDirty = gNumFrameResources;
	if (ImGui::DragFloat("Smoothness##value", &m.Smoothness, 0.01f, 0, 1))
		m.NumFramesDirty = gNumFrameResources;
	if (ImGui::DragFloat("Metallic##value", &m.Metallic, 0.01f, 0, 1))
		m.NumFramesDirty = gNumFrameResources;
	if (ImGui::DragFloat("Transparent##value", &m.Transparent, 0.01f, 0, 1))
		m.NumFramesDirty = gNumFrameResources;
	static ImGuiComboFlags flags = 0;
	const char* diffuseMapPreviewValue = m.DiffuseMapIdx >= 0 ? m.DiffuseMapName.c_str() : "None";
	const char* normalMapPreviewValue = m.NormalMapIdx >= 0 ? m.NormalMapName.c_str() : "None";
	static int diffuseMapCurrentIdx;
	static int diffuseMapLastIdx;
	static int normalMapCurrentIdx;
	static int normalMapLastIdx;
	diffuseMapCurrentIdx = m.DiffuseMapIdx;
	diffuseMapLastIdx = m.DiffuseMapIdx;
	normalMapCurrentIdx = m.NormalMapIdx;
	normalMapLastIdx = m.NormalMapIdx;
	if (ImGui::BeginCombo("Diffuse Map", diffuseMapPreviewValue, flags))
	{
		for (const auto& titem : textures)
		{
			if (titem.second->TextureType==DIFFUSE_MAP)
			{
				bool is_selected = diffuseMapCurrentIdx == titem.second->TextureIdx;
				if (ImGui::Selectable(titem.first.c_str(), is_selected))
				{
					diffuseMapCurrentIdx = titem.second->TextureIdx;
					// value changed
					if (diffuseMapCurrentIdx != diffuseMapLastIdx)
					{
						m.DiffuseMapName = titem.first.c_str();
						m.DiffuseMapIdx = titem.second->TextureIdx;
						m.NumFramesDirty = gNumFrameResources;
					}
					diffuseMapLastIdx = diffuseMapCurrentIdx;
				}
			}
		}
		bool is_selected = diffuseMapCurrentIdx == -1;
		if (ImGui::Selectable("None", is_selected))
		{
			diffuseMapCurrentIdx = -1;
			// value changed
			if (diffuseMapCurrentIdx != diffuseMapLastIdx)
			{
				m.DiffuseMapName = "";
				m.DiffuseMapIdx = -1;
				m.NumFramesDirty = gNumFrameResources;
			}
			diffuseMapLastIdx = diffuseMapCurrentIdx;
		}
		ImGui::EndCombo();
	}

	if (ImGui::BeginCombo("Normal Map", normalMapPreviewValue, flags))
	{
		for (const auto& titem : textures)
		{
			if (titem.second->TextureType == NORMAL_MAP)
			{
				bool is_selected = normalMapCurrentIdx == titem.second->TextureIdx;
				if (ImGui::Selectable(titem.first.c_str(), is_selected))
				{
					normalMapCurrentIdx = titem.second->TextureIdx;
					// value changed
					if (normalMapCurrentIdx != normalMapLastIdx)
					{
						m.NormalMapName = titem.first.c_str();
						m.NormalMapIdx = titem.second->TextureIdx;
						m.NumFramesDirty = gNumFrameResources;
					}
					normalMapLastIdx = normalMapCurrentIdx;
				}
			}
		}
		bool is_selected = normalMapCurrentIdx == -1;
		if (ImGui::Selectable("None", is_selected))
		{
			normalMapCurrentIdx = -1;
			// value changed
			if (normalMapCurrentIdx != normalMapLastIdx)
			{
				m.NormalMapName = "";
				m.NormalMapIdx = -1;
				m.NumFramesDirty = gNumFrameResources;
			}
			normalMapLastIdx = normalMapCurrentIdx;
		}
		ImGui::EndCombo();
	}
}

void WGUILayout::ShowMaterialModifier(bool* p_open, std::string materialName,
	MaterialList& materials, TextureList& textures)
{
	if (materials.find(materialName) == materials.end()) return;
	ImGui::SetNextWindowSize(ImVec2(350, 350), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Material Modifier", p_open))
	{
		ImGui::End();
		return;
	}

	// Show attributes of the material
	ImGui::Text("Material Name: %s", materialName.c_str());
	ShowMaterialAttributes(materialName, materials, textures);

	ImGui::End();
}


//-----------------------------------------------------------------------------
// [SECTION] Example App: Property Editor / ShowExampleAppPropertyEditor()
//-----------------------------------------------------------------------------

void WGUILayout::ShowPlaceholderObject(const char* prefix, int uid,
	MaterialList& materials, const char* material_name)
{
	// Use object uid as identifier. Most commonly you could also use the object pointer as a base ID.
	ImGui::PushID(uid);

	// Text and Tree nodes are less high than framed widgets, using AlignTextToFramePadding() we add vertical spacing to make the tree lines equal high.
	ImGui::AlignTextToFramePadding();
	bool node_open = ImGui::TreeNode("Material", "%s", prefix, uid);
	auto mat = materials[std::string(material_name)];

	if (node_open)
	{
		// Default settings for color edit
		static bool alpha_preview = true;
		static bool alpha_half_preview = false;
		static bool drag_and_drop = true;
		static bool options_menu = true;
		static bool hdr = false;
		ImGuiColorEditFlags misc_flags = (hdr ? ImGuiColorEditFlags_HDR : 0) | (drag_and_drop ? 0 : ImGuiColorEditFlags_NoDragDrop) | (alpha_half_preview ? ImGuiColorEditFlags_AlphaPreviewHalf : (alpha_preview ? ImGuiColorEditFlags_AlphaPreview : 0)) | (options_menu ? 0 : ImGuiColorEditFlags_NoOptions);

		ImGui::DragFloat("Roughness##value", &mat.Smoothness, 0.01f, 0, 1);

		ImGui::DragFloat("Metallic##value", &mat.Metallic, 0.01f, 0, 1);

		float fpForAlbedo[4] = { mat.Albedo.x, mat.Albedo.y, mat.Albedo.z, mat.Albedo.w };
		ImGui::ColorEdit4("DiffuseAlbedo##2f", fpForAlbedo, ImGuiColorEditFlags_Float | misc_flags);
		mat.Albedo = fpToXMFLOAT4(fpForAlbedo);

		mat.NumFramesDirty = gNumFrameResources;

		ImGui::TreePop();
	}
	ImGui::PopID();
}

void WGUILayout::DrawGUILayout(const Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& mCommandList,
	const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& mSrvHeap,
	PassData& passData, RenderItemList& renderItems, MaterialList& materials, TextureList& textures)
{
	//// Prepare
	//static std::vector<std::string> OrderedRenderItemNames(renderItems.size());
	//for (const auto& ritem : renderItems)
	//{
	//	const auto& r = ritem.second;
	//	OrderedRenderItemNames[r.objIdx] = ritem.first;
	//}
	//static std::vector<std::string> OrderedMaterialNames(materials.size());
	//for (const auto& mitem : materials)
	//{
	//	const auto& m = mitem.second;
	//	OrderedMaterialNames[m.MatIdx] = mitem.first;
	//}

	// Start the Dear ImGui frame
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	ImGuiStyle& style = ImGui::GetStyle();
	static ImGuiStyle ref_saved_style;

	for (size_t i = 0; i < ImGuiCol_COUNT; i++)
	{
		// Set the transparency of window bg
		if (ImGui::GetStyleColorName(i) == "WindowBg")
		{
			style.Colors[i].w = 0.3;
		}
		// Set the transparency of title bg
		if (ImGui::GetStyleColorName(i) == "TitleBg")
		{
			style.Colors[i].w = 0.4;
		}
		if (ImGui::GetStyleColorName(i) == "TitleBgActive")
		{
			style.Colors[i].w = 0.4;
		}
	}

	// 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
	bool show_demo_window = false;
	if (show_demo_window)
		ImGui::ShowDemoWindow(&show_demo_window);

	// Show My GUI Layout
	bool show_gui_layout = true;
	bool* p_open = &show_gui_layout;
	// Demonstrate the various window flags. Typically you would just use the default!
	static bool no_titlebar = false;
	static bool no_scrollbar = false;
	static bool no_menu = true;
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
	ImGui::SetNextWindowPos(ImVec2(450, 20), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(350, 680), ImGuiCond_FirstUseEver);

	// Main body of my GUI Layout starts here.
	if (!ImGui::Begin("ControlPanel", p_open, window_flags))
	{
		// Early out if the window is collapsed, as an optimization.
		ImGui::End();
		mCommandList->SetDescriptorHeaps(1, mSrvHeap.GetAddressOf());
		ImGui::Render();
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), mCommandList.Get());
		return;
	}

	static bool show_hierarchy = false;
	static bool show_inspector = false;
	static bool show_material_modifier = false;
	static bool show_app_property_editor = false;
	if (show_hierarchy)     ShowAppPropertyEditor(&show_hierarchy, materials);

	// e.g. Leave a fixed amount of width for labels (by passing a negative value), the rest goes to widgets.
	ImGui::PushItemWidth(ImGui::GetFontSize() * -12);
	// Menu Bar
	//if (ImGui::BeginMenuBar())
	//{
	//	ImGui::EndMenuBar();
	//}

	ImGui::Text("Scene: CornellBox");

	if (ImGui::CollapsingHeader("Objects"))
	{
		static int selected = -1;
		static std::string selectedObjName;
		int n = 0;
		for (const auto& ritem : renderItems)
		{
			const auto& objName = ritem.first;
			if (ImGui::Selectable(objName.c_str(), selected == n))
			{
				selected = n;
				selectedObjName = objName;
			}
			++n;
		}
		if (selected != -1)
			ShowObjectInspector(&show_inspector, selectedObjName, renderItems, materials, textures);
	}

	if (ImGui::CollapsingHeader("Material Modifier"))
	{
		static int selected = -1;
		static std::string selectedMaterialName;
		int n = 0;
		for (const auto& mitem : materials)
		{
			const auto& materialName = mitem.first;
			if (ImGui::Selectable(materialName.c_str(), selected == n))
			{
				selected = n;
				selectedMaterialName = materialName;
			}
			++n;
		}
		if (selected != -1)
			ShowMaterialModifier(&show_material_modifier, selectedMaterialName, materials, textures);
	}

	ImGui::End();

	mCommandList->SetDescriptorHeaps(1, mSrvHeap.GetAddressOf());
	ImGui::Render();
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), mCommandList.Get());
	// End the Dear ImGui Frame
}