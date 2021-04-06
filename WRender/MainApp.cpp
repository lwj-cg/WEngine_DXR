//***************************************************************************************
// MainApp.cpp by Frank Luna (C) 2015 All Rights Reserved.
//***************************************************************************************

#include <vector>
#include "../Common/d3dApp.h"
#include "../Common/MathHelper.h"
#include "../Common/UploadBuffer.h"
#include "../Common/GeometryGenerator.h"
#include "../Common/Camera.h"
#include "FrameResource.h"
#include "Utils/WSceneDescParser.h"
#include "Include/WGUILayout.h"
#include "Include/GeometryShape.h"
#include "Include/LowDiscrepancy.h"
#include "Include/rng.h"
#include "Core/PathTracer.h"
// DX12 RayTracing Helpers
#include "DXRHelper.h"
#include <dxcapi.h>
#include "nv_helpers_dx12/BottomLevelASGenerator.h"
#include "nv_helpers_dx12/TopLevelASGenerator.h"
#include "nv_helpers_dx12/RaytracingPipelineGenerator.h"   
#include "nv_helpers_dx12/RootSignatureGenerator.h"
#include "nv_helpers_dx12/ShaderBindingTableGenerator.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;
using namespace GeometryShape;

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")

struct AccelerationStructureBuffers;
// #DXR
struct AccelerationStructureBuffers
{
	ComPtr<ID3D12Resource> pScratch;      // Scratch memory for AS builder
	ComPtr<ID3D12Resource> pResult;       // Where the AS is
	ComPtr<ID3D12Resource> pInstanceDesc; // Hold the matrices of the instances
};

const int gNumFrameResources = 3;
static const UINT gNumRayTypes = 2;

static std::map<std::string, UINT> ShaderToHitGroupTable = {
	{"GlassMaterial", 0},
	{"GlassSpecularMaterial", 1},
	{"MatteMaterial",2},
	{"MetalMaterial",3},
	{"PlasticMaterial",4},
	{"MirrorMaterial",5},
	//{"DisneyMaterial",6},
	{"Default",2}
};

// Lightweight structure stores parameters to draw a shape.  This will
// vary from app-to-app.
struct RenderItem
{
	RenderItem() = default;
	RenderItem(const RenderItem& rhs) = delete;

	// World matrix of the shape that describes the object's local space
	// relative to the world space, which defines the position, orientation,
	// and scale of the object in the world.
	XMFLOAT4X4 World = MathHelper::Identity4x4();

	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

	// Dirty flag indicating the object data has changed and we need to update the constant buffer.
	// Because we have an object cbuffer for each FrameResource, we have to apply the
	// update to each FrameResource.  Thus, when we modify obect data we should set 
	// NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
	int NumFramesDirty = gNumFrameResources;

	// Index into GPU constant buffer corresponding to the ObjectCB for this render item.
	UINT ObjCBIndex = -1;

	Material* Mat = nullptr;
	MeshGeometry* Geo = nullptr;

	// Primitive topology.
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// DrawIndexedInstanced parameters.
	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	int BaseVertexLocation = 0;
};

enum class RenderLayer : int
{
	Opaque = 0,
	Sky,
	Count
};

class MainApp : public D3DApp
{
public:
	MainApp(HINSTANCE hInstance);
	MainApp(const MainApp& rhs) = delete;
	MainApp& operator=(const MainApp& rhs) = delete;
	~MainApp();

	virtual bool Initialize()override;

private:
	virtual void OnResize()override;
	virtual void Update(const GameTimer& gt)override;
	virtual void Draw(const GameTimer& gt)override;

	void DrawForRasterize(const GameTimer& gt);
	void DrawForRayTracing(const GameTimer& gt);

	virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
	virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y)override;
	virtual void OnKeyUp(UINT8 key)override;

	void OnKeyboardInput(const GameTimer& gt);
	void AnimateMaterials(const GameTimer& gt);
	void UpdateObjectCBs(const GameTimer& gt);
	void UpdateMaterialBuffer(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);

	void BuildFrameResources();

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();

private:

	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource = nullptr;
	int mCurrFrameResourceIndex = 0;

	UINT mCbvSrvDescriptorSize = 0;

	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	ComPtr<ID3D12RootSignature> mPathTracingRootSignature = nullptr;

	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;
	ComPtr<ID3D12DescriptorHeap> mCbvSrvUavDescriptorHeap = nullptr;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	//std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<WTexture>> mTextures;
	std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;
	std::unique_ptr<WTexture> mEnvironmentMap;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

	// List of all the render items.
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;

	// Render items divided by PSO.
	std::vector<RenderItem*> mRitemLayer[(int)RenderLayer::Count];

	UINT mSkyTexHeapIndex = 0;

	Camera mCamera;

	POINT mLastMousePos;

	std::unique_ptr<PathTracer> mPathTracer;

private:

	/// Create the acceleration structure of an instance
	///
	/// \param     vVertexBuffers : pair of buffer and vertex count
	/// \return    AccelerationStructureBuffers for TLAS
	void CreateBottomLevelAS(std::map<std::string, AccelerationStructureBuffers>& bottemLevelBuffers);

	/// Create the main acceleration structure that holds
	/// all instances of the scene
	/// \param     instances : pair of BLAS and transform
	/// \param     updateOnly: if true, perform a refit instead of a full build
	void CreateTopLevelAS(
		const std::vector<std::pair<ComPtr<ID3D12Resource>, DirectX::XMMATRIX>>& instances, bool updateOnly = false);

	/// Create all acceleration structures, bottom and top
	void CreateAccelerationStructures();

	// For DXR AccerationStructures
	std::map<std::string, ComPtr<ID3D12Resource>> mBLASBuffers; // Storage for the bottom Level AS
	ComPtr<ID3D12Resource> m_bottomLevelAS; // Storage for the bottom Level AS

	nv_helpers_dx12::TopLevelASGenerator mTopLevelASGenerator;
	AccelerationStructureBuffers mTopLevelASBuffers;
	std::vector<std::pair<ComPtr<ID3D12Resource>, DirectX::XMMATRIX>> mInstances;


	// #DXR
	ComPtr<ID3D12RootSignature> CreateRayGenSignature();
	ComPtr<ID3D12RootSignature> CreateMissSignature();
	ComPtr<ID3D12RootSignature> CreateHitSignature();
	ComPtr<ID3D12RootSignature> CreateHitShadowSignature();
	ComPtr<ID3D12RootSignature> CreateEmptySignature();

	void CreateRayTracingPipeline();

	ComPtr<IDxcBlob> m_rayGenLibrary;
	ComPtr<IDxcBlob> m_hitLibrary;
	ComPtr<IDxcBlob> m_hitDiffuseLibrary;
	ComPtr<IDxcBlob> m_missLibrary;
	ComPtr<IDxcBlob> m_hitShadowLibrary;
	ComPtr<IDxcBlob> m_hitSpecularLibrary;
	ComPtr<IDxcBlob> m_hitMicrofacetLibrary;
	ComPtr<IDxcBlob> m_hitLambertianLibrary;
	ComPtr<IDxcBlob> m_hitGlassLibrary;
	ComPtr<IDxcBlob> m_hitGlassSpecularLibrary;
	ComPtr<IDxcBlob> m_hitMatteLibrary;
	ComPtr<IDxcBlob> m_hitMetalLibrary;
	ComPtr<IDxcBlob> m_hitPlasticLibrary;
	ComPtr<IDxcBlob> m_hitMirrorLibrary;
	ComPtr<IDxcBlob> m_hitDisneyLibrary;

	ComPtr<ID3D12RootSignature> m_rayGenSignature;
	ComPtr<ID3D12RootSignature> m_hitSignature;
	ComPtr<ID3D12RootSignature> m_hitShadowSignature;
	ComPtr<ID3D12RootSignature> m_missSignature;
	ComPtr<ID3D12RootSignature> m_missShadowSignature;

	// Ray tracing pipeline state
	ComPtr<ID3D12StateObject> m_rtStateObject;
	// Ray tracing pipeline state properties, retaining the shader identifiers
	// to use in the Shader Binding Table
	ComPtr<ID3D12StateObjectProperties> m_rtStateObjectProps;

	// #DXR
	void CreateRaytracingOutputBuffer();
	void CreateShaderResourceHeap();
	void CreateTextureShaderResourceHeap();
	ComPtr<ID3D12Resource> m_outputResource = nullptr;
	ComPtr<ID3D12DescriptorHeap> m_srvUavHeap;
	ComPtr<ID3D12DescriptorHeap> m_textureSrvHeap;

	// #DXR
	void CreateShaderBindingTable();
	nv_helpers_dx12::ShaderBindingTableGenerator m_sbtHelper;
	ComPtr<ID3D12Resource> m_sbtStorage;

	// #DXR
	void CreateVertexBuffer();
	ComPtr<ID3D12Resource> m_SimpleVertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW m_SimpleVertexBufferView;

	// My SceneDescParser
	WSceneDescParser mSceneDescParser;
	std::map<std::string, WGeometryRecord> mGeometryMap;
	std::map<std::string, WRenderItem> mRenderItems;
	std::map<std::string, WMaterial> mMaterials;
	WPassConstantsItem mPassItem;
	void SetupSceneWithXML(const char* filename);
	void SetupCamera(const WCamereConfig& cameraConfig);
	void LoadTextures(const std::map<std::string, WTextureRecord>& mTextureItems);

	// Vertex Buffer & Index Buffer
	ComPtr<ID3D12Resource> mVertexBuffer = nullptr;
	ComPtr<ID3D12Resource> mVertexBufferUploader = nullptr;
	ComPtr<ID3D12Resource> mNormalBuffer = nullptr;
	ComPtr<ID3D12Resource> mNormalBufferUploader = nullptr;
	ComPtr<ID3D12Resource> mTexCoordBuffer = nullptr;
	ComPtr<ID3D12Resource> mTexCoordBufferUploader = nullptr;
	ComPtr<ID3D12Resource> mIndexBuffer = nullptr;
	ComPtr<ID3D12Resource> mIndexBufferUploader = nullptr;
	ComPtr<ID3D12Resource> mNormalIndexBuffer = nullptr;
	ComPtr<ID3D12Resource> mNormalIndexBufferUploader = nullptr;
	ComPtr<ID3D12Resource> mTexCoordIndexBuffer = nullptr;
	ComPtr<ID3D12Resource> mTexCoordIndexBufferUploader = nullptr;

	// Frame resource on CPU
	WPassConstants mPassCB;

	// Light Buffer
	ComPtr<ID3D12Resource> mLightBuffer = nullptr;
	ComPtr<ID3D12Resource> mLightBufferUploader = nullptr;

	// num static frame
	UINT mNumStaticFrame = 0;
	UINT mNumFaces = 0;

	std::vector<uint32_t> mRadicalInversePermutations;
	ComPtr<ID3D12Resource> mPermutationsBuffer = nullptr;
	ComPtr<ID3D12Resource> mPermutationsUploadBuffer = nullptr;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
	PSTR cmdLine, int showCmd)
{
	// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try
	{
		MainApp theApp(hInstance);
		if (!theApp.Initialize())
			return 0;

		return theApp.Run();
	}
	catch (DxException& e)
	{
		MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
		return 0;
	}
}

MainApp::MainApp(HINSTANCE hInstance)
	: D3DApp(hInstance)
{
}

MainApp::~MainApp()
{
}

bool MainApp::Initialize()
{
	if (!D3DApp::Initialize())
		return false;

	// Reset the command list to prep for initialization commands.
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	// Get the increment size of a descriptor in this heap type.  This is hardware specific, 
	// so we have to query this information.
	mCbvSrvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	XMFLOAT3 position(50.0f, 52.0f, -295.6f);
	XMFLOAT3 direction(0, -0.042612, 1);
	XMFLOAT3 worldUp(0, 1.0f, 0);
	XMVECTOR vPosition = XMLoadFloat3(&position);
	XMVECTOR vDirection = XMLoadFloat3(&direction);
	XMVECTOR vWorldUP = XMLoadFloat3(&worldUp);
	XMVECTOR vTarget = vPosition + 140.0f * vDirection;
	mCamera.SetPosition(position);
	mCamera.LookAt(vPosition, vTarget, vWorldUP);

	mPathTracer = std::make_unique<PathTracer>(md3dDevice.Get(),
		mClientWidth, mClientHeight, DXGI_FORMAT_R8G8B8A8_UNORM);

	// Setup scene with XML description file
	//SetupSceneWithXML("D:\\projects\\WEngine_DXR\\Scenes\\CornellBox.xml");
	mPassItem.SceneName = "CornellBox";
	//mPassItem.SceneName = "EnvironmentMap";
	char sceneFileName[50];
	std::sprintf(sceneFileName, "D:\\projects\\WEngine_DXR\\Scenes\\%s.xml", mPassItem.SceneName.c_str());
	SetupSceneWithXML(sceneFileName);
	BuildFrameResources();

	RNG rng;
	mRadicalInversePermutations = ComputeRadicalInversePermutations(rng);
	UINT64 byteSize = mRadicalInversePermutations.size() * sizeof(uint32_t);
	mPermutationsBuffer = d3dUtil::CreateDefaultBuffer(
		md3dDevice.Get(),
		mCommandList.Get(),
		mRadicalInversePermutations.data(),
		byteSize,
		mPermutationsUploadBuffer
	);

	// Execute the initialization commands.
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until initialization is complete.
	FlushCommandQueue();

	// Reset the command list to prep for initialization commands.
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	// #DXR
	// Setup the acceleration structures (AS) for raytracing. When setting up
	// geometry, each bottom-level AS has its own transform matrix.
	CreateAccelerationStructures();

	// Command lists are created in the recording state, but there is
	// nothing to record yet. The main loop expects it to be closed, so
	// close it now.
	//ThrowIfFailed(mCommandList->Close());
	// Create the raytracing pipeline, associating the shader code to symbol names
	// and to their root signatures, and defining the amount of memory carried by
	// rays (ray payload)
	CreateRayTracingPipeline();
	// Allocate the buffer storing the raytracing output, with the same dimensions
	// as the target image
	CreateRaytracingOutputBuffer(); // #DXR
	// Create the buffer containing the raytracing result (always output in a
	// UAV), and create the heap referencing the resources used by the raytracing,
	// such as the acceleration structure
	CreateShaderResourceHeap(); // #DXR
	//CreateTextureShaderResourceHeap();
	// Create the shader binding table and indicating which shaders
	// are invoked for each instance in the  AS
	CreateShaderBindingTable();

	return true;
}

void MainApp::OnResize()
{
	D3DApp::OnResize();

	mCamera.SetLens(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);

	//if (mPathTracer != nullptr)
	//{
	//	mPathTracer->OnResize(mClientWidth, mClientHeight);
	//}

	if (m_outputResource)
	{
		// Resize the output buffer
		CreateRaytracingOutputBuffer();
		CreateShaderResourceHeap();
	}

}

void MainApp::Update(const GameTimer& gt)
{
	OnKeyboardInput(gt);

	// Cycle through the circular frame resource array.
	mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

	// Has the GPU finished processing the commands of the current frame resource?
	// If not, wait until the GPU has completed commands up to this fence point.
	if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	AnimateMaterials(gt);
	UpdateObjectCBs(gt);
	UpdateMaterialBuffer(gt);
	UpdateMainPassCB(gt);
}

void MainApp::Draw(const GameTimer& gt)
{
	DrawForRayTracing(gt);
}

void MainApp::DrawForRasterize(const GameTimer& gt)
{
}

void MainApp::DrawForRayTracing(const GameTimer& gt)
{
	auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;

	// Reuse the memory associated with command recording.
	// We can only reset when the associated command lists have finished execution on the GPU.
	ThrowIfFailed(cmdListAlloc->Reset());

	// A command list can be reset after it has been added to the command queue via ExecuteCommandList.
	// Reusing the command list reuses memory.
	ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), nullptr));


	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	// Clear the back buffer and depth buffer.
	mCommandList->ClearRenderTargetView(CurrentBackBufferView(), (float*)&mPassCB.bgColor, 0, nullptr);
	mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// Specify the buffers we are going to render to.
	mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

	CreateTopLevelAS(mInstances, true);

	// #DXR
	// Bind the descriptor heap giving access to the top-level acceleration
	// structure, as well as the raytracing output
	std::vector<ID3D12DescriptorHeap*> heaps = { m_srvUavHeap.Get() };
	mCommandList->SetDescriptorHeaps(static_cast<UINT>(heaps.size()),
		heaps.data());

	// On the last frame, the raytracing output was used as a copy source, to
	// copy its contents into the render target. Now we need to transition it to
	// a UAV so that the shaders can write in it.
	CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(
		m_outputResource.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	mCommandList->ResourceBarrier(1, &transition);

	// Setup the raytracing task
	D3D12_DISPATCH_RAYS_DESC desc = {};
	// The layout of the SBT is as follows: ray generation shader, miss
	// shaders, hit groups. As described in the CreateShaderBindingTable method,
	// all SBT entries of a given type have the same size to allow a fixed stride.

	// The ray generation shaders are always at the beginning of the SBT. 
	uint32_t rayGenerationSectionSizeInBytes = m_sbtHelper.GetRayGenSectionSize();
	desc.RayGenerationShaderRecord.StartAddress = m_sbtStorage->GetGPUVirtualAddress();
	desc.RayGenerationShaderRecord.SizeInBytes = rayGenerationSectionSizeInBytes;

	// The miss shaders are in the second SBT section, right after the ray
	// generation shader. We have one miss shader for the camera rays and one
	// for the shadow rays, so this section has a size of 2*m_sbtEntrySize. We
	// also indicate the stride between the two miss shaders, which is the size
	// of a SBT entry
	uint32_t missSectionSizeInBytes = m_sbtHelper.GetMissSectionSize();
	desc.MissShaderTable.StartAddress =
		m_sbtStorage->GetGPUVirtualAddress() + rayGenerationSectionSizeInBytes;
	desc.MissShaderTable.SizeInBytes = missSectionSizeInBytes;
	desc.MissShaderTable.StrideInBytes = m_sbtHelper.GetMissEntrySize();

	// The hit groups section start after the miss shaders. In this sample we
	// have one 1 hit group for the triangle
	uint32_t hitGroupsSectionSize = m_sbtHelper.GetHitGroupSectionSize();
	desc.HitGroupTable.StartAddress = m_sbtStorage->GetGPUVirtualAddress() +
		rayGenerationSectionSizeInBytes +
		missSectionSizeInBytes;
	desc.HitGroupTable.SizeInBytes = hitGroupsSectionSize;
	desc.HitGroupTable.StrideInBytes = m_sbtHelper.GetHitGroupEntrySize();

	// Dimensions of the image to render, identical to a kernel launch dimension
	desc.Width = mClientWidth;
	desc.Height = mClientHeight;
	desc.Depth = 1;

	// Bind the raytracing pipeline
	mCommandList->SetPipelineState1(m_rtStateObject.Get());
	// Dispatch the rays and write to the raytracing output
	mCommandList->DispatchRays(&desc);

	// The raytracing output needs to be copied to the actual render target used
	// for display. For this, we need to transition the raytracing output from a
	// UAV to a copy source, and the render target buffer to a copy destination.
	// We can then do the actual copy, before transitioning the render target
	// buffer into a render target, that will be then used to display the image
	transition = CD3DX12_RESOURCE_BARRIER::Transition(
		m_outputResource.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_COPY_SOURCE);
	mCommandList->ResourceBarrier(1, &transition);
	transition = CD3DX12_RESOURCE_BARRIER::Transition(
		CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_COPY_DEST);
	mCommandList->ResourceBarrier(1, &transition);
	mCommandList->CopyResource(CurrentBackBuffer(),
		m_outputResource.Get());

	// Transition to PRESENT state.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT));

	WGUILayout::DrawGUILayout(mCommandList, mSrvHeap, mPassItem, mRenderItems, mMaterials, mTextures);

	// Done recording commands.
	ThrowIfFailed(mCommandList->Close());

	// Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Swap the back and front buffers
	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	// Advance the fence value to mark commands up to this fence point.
	mCurrFrameResource->Fence = ++mCurrentFence;

	// Add an instruction to the command queue to set a new fence point. 
	// Because we are on the GPU timeline, the new fence point won't be 
	// set until the GPU finishes processing all the commands prior to this Signal().
	mCommandQueue->Signal(mFence.Get(), mCurrentFence);

}

void MainApp::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	SetCapture(mhMainWnd);
}

void MainApp::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void MainApp::OnMouseMove(WPARAM btnState, int x, int y)
{
	ImGuiIO& io = ImGui::GetIO();
	if ((btnState & MK_LBUTTON) != 0 && !io.WantCaptureMouse)
	{
		// Make each pixel correspond to a quarter of a degree.
		float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
		float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));

		mCamera.Pitch(dy);
		mCamera.RotateY(dx);
	}
	if ((btnState & MK_RBUTTON) != 0 && !io.WantCaptureMouse)
	{
		// Make each pixel correspond to 0.2 unit in the scene.
		float dx = 0.05f * static_cast<float>(x - mLastMousePos.x);
		float dy = 0.05f * static_cast<float>(y - mLastMousePos.y);

		mCamera.Walk(2.0f * (dx - dy));
	}

	mLastMousePos.x = x;
	mLastMousePos.y = y;
}

void MainApp::OnKeyUp(UINT8 key)
{
	if (key == VK_ESCAPE)
	{
		PostQuitMessage(0);
	}
	else if (key == VK_F2)
		Set4xMsaaState(!m4xMsaaState);
	else if (key == VK_SPACE)
		mRaster = !mRaster;

}

void MainApp::OnKeyboardInput(const GameTimer& gt)
{
	const float dt = gt.DeltaTime();

	if (GetAsyncKeyState('W') & 0x8000)
		mCamera.Walk(10.0f * dt);

	if (GetAsyncKeyState('S') & 0x8000)
		mCamera.Walk(-10.0f * dt);

	if (GetAsyncKeyState('A') & 0x8000)
		mCamera.Strafe(-10.0f * dt);

	if (GetAsyncKeyState('D') & 0x8000)
		mCamera.Strafe(10.0f * dt);

	mCamera.UpdateViewMatrix(mNumStaticFrame);
}

void MainApp::AnimateMaterials(const GameTimer& gt)
{

}

void MainApp::UpdateObjectCBs(const GameTimer& gt)
{
	auto currObjectBuffer = mCurrFrameResource->ObjectBuffer.get();
	for (auto& ritem : mRenderItems)
	{
		auto& r = ritem.second;
		// Only update the buffer data if the constants have changed.  
		// This needs to be tracked per frame resource.
		if (r.NumFramesDirty > 0)
		{
			r.UpdateTransform();
			mInstances[r.objIdx].second = r.transform;
			INT32 normalOffset = (INT32)(r.normalOffsetInBytes >= 0 ?
				r.normalOffsetInBytes / (sizeof(SNormal)) : r.normalOffsetInBytes);
			INT32 texCoordOffset = (INT32)(r.texCoordOffsetInBytes >= 0 ?
				r.texCoordOffsetInBytes / (sizeof(STexCoord)) : r.texCoordOffsetInBytes);
			WObjectConstants objConstants(
				DirectX::XMMatrixTranspose(r.transform), r.matIdx,
				(UINT)(r.vertexOffsetInBytes / (sizeof(SVertex))),
				(UINT)(r.indexOffsetInBytes / sizeof(UINT)),
				normalOffset,
				texCoordOffset
			);

			currObjectBuffer->CopyData(r.objIdx, objConstants);

			// Next FrameResource need to be updated too.
			--r.NumFramesDirty;
			mNumStaticFrame = 0;
		}
	}
}

void MainApp::UpdateMaterialBuffer(const GameTimer& gt)
{
	auto currMaterialBuffer = mCurrFrameResource->MaterialBuffer.get();
	for (auto& mitem : mMaterials)
	{
		auto& m = mitem.second;
		// Only update the cbuffer data if the constants have changed.  
		// This needs to be tracked per frame resource.
		if (m.NumFramesDirty > 0)
		{
			WMaterialData materialData(
				m.Albedo, m.Emission, m.Transparent, m.Smoothness, m.Metallic,
				m.DiffuseMapIdx, m.NormalMapIdx
			);
			materialData.TransColor = m.TransColor;
			materialData.F0 = m.F0;
			materialData.k = m.k;
			materialData.kd = m.kd;
			materialData.ks = m.ks;
			materialData.RefractiveIndex = m.RefractiveIndex;
			materialData.specularTint = m.specularTint;
			materialData.anisotropic = m.anisotropic;
			materialData.sheen = m.sheen;
			materialData.sheenTint = m.sheenTint;
			materialData.clearcoat = m.clearcoat;
			materialData.clearcoatGloss = m.clearcoatGloss;
			materialData.specularTrans = m.specularTrans;
			materialData.diffuseTrans = m.diffuseTrans;
			materialData.Sigma = m.Sigma;

			currMaterialBuffer->CopyData(m.MatIdx, materialData);

			// Next FrameResource need to be updated too.
			--m.NumFramesDirty;
			mNumStaticFrame = 0;
		}
	}
}

void MainApp::UpdateMainPassCB(const GameTimer& gt)
{
	++mNumStaticFrame %= 10000000;

	XMMATRIX view = mCamera.GetView();
	XMMATRIX proj = mCamera.GetProj();

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	XMStoreFloat4x4(&mPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	mPassCB.EyePosW = mCamera.GetPosition3f();

	if (mPassItem.NumFramesDirty > 0)
	{
		mPassCB.MaxDepth = mPassItem.MaxDepth;
		mPassCB.SqrtSamples = mPassItem.SqrtSamples;
		mPassCB.SceneEpsilon = mPassItem.SceneEpsilon;
		mNumStaticFrame = 0;
		--mPassItem.NumFramesDirty;
	}
	mPassCB.NumStaticFrame = mNumStaticFrame;
	mPassItem.NumStaticFrame = mPassCB.NumStaticFrame;

	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mPassCB);
}

void MainApp::BuildFrameResources()
{
	for (int i = 0; i < gNumFrameResources; ++i)
	{
		mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(),
			1, (UINT)mRenderItems.size(), (UINT)mMaterials.size()));
	}
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> MainApp::GetStaticSamplers()
{
	// Applications usually only need a handful of samplers.  So just define them all up front
	// and keep them available as part of the root signature.  

	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		4, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		8);                               // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		5, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
		0.0f,                              // mipLODBias
		8);                                // maxAnisotropy

	return {
		pointWrap, pointClamp,
		linearWrap, linearClamp,
		anisotropicWrap, anisotropicClamp };
}

//-----------------------------------------------------------------------------
//
// Combine the BLAS and TLAS builds to construct the entire acceleration
// structure required to raytrace the scene
//
void MainApp::CreateAccelerationStructures() {
	// Build the bottom AS from the Triangle vertex buffer
	std::map<std::string, AccelerationStructureBuffers> bottomLevelBuffers;
	CreateBottomLevelAS(bottomLevelBuffers);

	// Build all the Instances from the RenderItems
	mInstances.resize(mRenderItems.size());
	size_t i = 0;
	for (const auto& ritem : mRenderItems)
	{
		const auto& r = ritem.second;
		auto& bottomLevelBufferPointer = bottomLevelBuffers[r.geometryName].pResult;
		mInstances[r.objIdx] = { bottomLevelBufferPointer, r.transform };
	}
	// Create Top Level Acceleration Structure
	CreateTopLevelAS(mInstances);

	// Flush the command list and wait for it to finish
	// Execute the accerelaration structure create commands.
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
	FlushCommandQueue();

	// Store the AS buffers. The rest of the buffers will be released once we exit
	// the function
	for (auto iter = bottomLevelBuffers.begin(); iter != bottomLevelBuffers.end(); ++iter)
	{
		mBLASBuffers[iter->first] = std::move(iter->second.pResult);
	}
}


//-----------------------------------------------------------------------------
//
// Create a bottom-level acceleration structure based on a list of vertex
// buffers in GPU memory along with their vertex count. The build is then done
// in 3 steps: gathering the geometry, computing the sizes of the required
// buffers, and building the actual AS
//
void
MainApp::CreateBottomLevelAS(
	std::map<std::string, AccelerationStructureBuffers>& bottemLevelBuffers) {
	for (const auto& gItem : mGeometryMap) {
		nv_helpers_dx12::BottomLevelASGenerator bottomLevelAS;

		const auto& g = gItem.second;
		// Add vertex buffer and not transforming their position.
		bottomLevelAS.AddVertexBuffer(
			mVertexBuffer.Get(), g.vertexOffsetInBytes, g.vertexCount, sizeof(SVertex),
			mIndexBuffer.Get(), g.indexOffsetInBytes, g.indexCount,
			0, 0);

		// The AS build requires some scratch space to store temporary information.
		// The amount of scratch memory is dependent on the scene complexity.
		UINT64 scratchSizeInBytes = 0;
		// The final AS also needs to be stored in addition to the existing vertex
		// buffers. It size is also dependent on the scene complexity.
		UINT64 resultSizeInBytes = 0;

		bottomLevelAS.ComputeASBufferSizes(md3dDevice.Get(), false, &scratchSizeInBytes,
			&resultSizeInBytes);

		// Once the sizes are obtained, the application is responsible for allocating
		// the necessary buffers. Since the entire generation will be done on the GPU,
		// we can directly allocate those on the default heap
		AccelerationStructureBuffers buffers;
		buffers.pScratch = nv_helpers_dx12::CreateBuffer(
			md3dDevice.Get(), scratchSizeInBytes,
			D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON,
			nv_helpers_dx12::kDefaultHeapProps);
		buffers.pResult = nv_helpers_dx12::CreateBuffer(
			md3dDevice.Get(), resultSizeInBytes,
			D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
			D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
			nv_helpers_dx12::kDefaultHeapProps);

		// Build the acceleration structure. Note that this call integrates a barrier
		// on the generated AS, so that it can be used to compute a top-level AS right
		// after this method.
		bottomLevelAS.Generate(mCommandList.Get(), buffers.pScratch.Get(),
			buffers.pResult.Get(), false, nullptr);
		bottemLevelBuffers[gItem.first] = std::move(buffers);
	}
}

WRenderItem findRenderItem(const std::map < std::string, WRenderItem > renderItems, int objIdx)
{
	for (const auto& ritem : renderItems)
	{
		if (objIdx == ritem.second.objIdx)
		{
			return ritem.second;
		}
	}
	return WRenderItem();
}

//-----------------------------------------------------------------------------
// Create the main acceleration structure that holds all instances of the scene.
// Similarly to the bottom-level AS generation, it is done in 3 steps: gathering
// the instances, computing the memory requirements for the AS, and building the
// AS itself
//
void MainApp::CreateTopLevelAS(
	const std::vector<std::pair<ComPtr<ID3D12Resource>, DirectX::XMMATRIX>>
	& instances, // pair of bottom level AS and matrix of the instance
	bool updateOnly  // If true the top-level AS will only be refitted and not
				   // rebuilt from scratch
) {

	if (!updateOnly)
	{
		// Gather all the instances into the builder helper
		for (size_t i = 0; i < instances.size(); i++) {
			const auto& ritem = findRenderItem(mRenderItems, i);
			const auto& material = mMaterials[ritem.materialName];
			const auto& Shader = material.Shader;
			if(ShaderToHitGroupTable.find(Shader)!=ShaderToHitGroupTable.end())
				mTopLevelASGenerator.AddInstance(instances[i].first.Get(),
					instances[i].second, static_cast<UINT>(i),
					gNumRayTypes * ShaderToHitGroupTable[Shader]);
			else
				mTopLevelASGenerator.AddInstance(instances[i].first.Get(),
					instances[i].second, static_cast<UINT>(i),
					gNumRayTypes * ShaderToHitGroupTable[Shader]); // MatteMaterial
		}

		// As for the bottom-level AS, the building the AS requires some scratch space
		// to store temporary data in addition to the actual AS. In the case of the
		// top-level AS, the instance descriptors also need to be stored in GPU
		// memory. This call outputs the memory requirements for each (scratch,
		// results, instance descriptors) so that the application can allocate the
		// corresponding memory
		UINT64 scratchSize, resultSize, instanceDescsSize;

		mTopLevelASGenerator.ComputeASBufferSizes(md3dDevice.Get(), true, &scratchSize,
			&resultSize, &instanceDescsSize);

		// Create the scratch and result buffers. Since the build is all done on GPU,
		// those can be allocated on the default heap
		mTopLevelASBuffers.pScratch = nv_helpers_dx12::CreateBuffer(
			md3dDevice.Get(), scratchSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			nv_helpers_dx12::kDefaultHeapProps);
		mTopLevelASBuffers.pResult = nv_helpers_dx12::CreateBuffer(
			md3dDevice.Get(), resultSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
			D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
			nv_helpers_dx12::kDefaultHeapProps);

		// The buffer describing the instances: ID, shader binding information,
		// matrices ... Those will be copied into the buffer by the helper through
		// mapping, so the buffer has to be allocated on the upload heap.
		mTopLevelASBuffers.pInstanceDesc = nv_helpers_dx12::CreateBuffer(
			md3dDevice.Get(), instanceDescsSize, D3D12_RESOURCE_FLAG_NONE,
			D3D12_RESOURCE_STATE_GENERIC_READ, nv_helpers_dx12::kUploadHeapProps);
	}

	// After all the buffers are allocated, or if only an update is required, we
	// can build the acceleration structure. Note that in the case of the update
	// we also pass the existing AS as the 'previous' AS, so that it can be
	// refitted in place.
	mTopLevelASGenerator.Generate(mCommandList.Get(),
		mTopLevelASBuffers.pScratch.Get(),
		mTopLevelASBuffers.pResult.Get(),
		mTopLevelASBuffers.pInstanceDesc.Get(),
		updateOnly, mTopLevelASBuffers.pResult.Get());
}


//-----------------------------------------------------------------------------
// The ray generation shader needs to access 2 resources: the raytracing output
// and the top-level acceleration structure
//
ComPtr<ID3D12RootSignature> MainApp::CreateRayGenSignature() {
	nv_helpers_dx12::RootSignatureGenerator rsc;
	rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_CBV, 0);    // b0 : passCB
	rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_SRV, 0, 1);  // t0, space1 : objectBufferArray
	rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_SRV, 0, 2);  // t0, space2 : MaterialBuffer
	rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_SRV, 0, 100); // t0, space100 : RadicalInversePermutations
	rsc.AddHeapRangesParameter(
		{
			{
				0 /*u0*/, 1 /*1 descriptor */, 0 /*use the implicit register space 0*/,
				D3D12_DESCRIPTOR_RANGE_TYPE_UAV /* UAV representing the output buffer*/,
				0 /*heap slot where the UAV is defined*/
			},
			{
				0 /*t0*/, 1, 0,
				D3D12_DESCRIPTOR_RANGE_TYPE_SRV /*Top-level acceleration structure*/,
				1
			}
		});

	return rsc.Generate(md3dDevice.Get(), true);
}

//-----------------------------------------------------------------------------
// The hit shader communicates only through the ray payload, and therefore does
// not require any resources
//
ComPtr<ID3D12RootSignature> MainApp::CreateHitSignature() {
	nv_helpers_dx12::RootSignatureGenerator rsc;
	rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_SRV, 0, 1);
	rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_SRV, 0, 2);
	rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_SRV, 0, 3);
	rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_SRV, 0, 4);
	rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_SRV, 0, 5);
	rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_SRV, 0, 6);
	rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_SRV, 0, 7);
	rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_SRV, 0, 8);
	rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_SRV, 0, 9);
	rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_SRV, 0, 100); // RadicalInversePermutations
	rsc.AddHeapRangesParameter(
		{
			{
				0 /*t0*/, 1, 0,
				D3D12_DESCRIPTOR_RANGE_TYPE_SRV /*Top-level acceleration structure*/,
				1
			},
			{
				1 /*t1*/, mTextures.size(), 0,
				D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
				2
			}
		});
	auto staticSamplers = GetStaticSamplers();
	return rsc.Generate(md3dDevice.Get(), true, (UINT)staticSamplers.size(), staticSamplers.data());
}

ComPtr<ID3D12RootSignature> MainApp::CreateHitShadowSignature()
{
	nv_helpers_dx12::RootSignatureGenerator rsc;
	rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_SRV, 0, 0);
	rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_SRV, 0, 1);
	return rsc.Generate(md3dDevice.Get(), true);
}

ComPtr<ID3D12RootSignature> MainApp::CreateEmptySignature()
{
	nv_helpers_dx12::RootSignatureGenerator rsc;
	return rsc.Generate(md3dDevice.Get(), true);
}

//-----------------------------------------------------------------------------
// The miss shader communicates only through the ray payload, and therefore
// does not require any resources
//
ComPtr<ID3D12RootSignature> MainApp::CreateMissSignature() {
	nv_helpers_dx12::RootSignatureGenerator rsc;
	rsc.AddHeapRangesParameter(
		{
			{
				1 /*t1*/, 1, 0,
				D3D12_DESCRIPTOR_RANGE_TYPE_SRV /* Environment Map */,
				2+mTextures.size()
			}
		});
	auto staticSamplers = GetStaticSamplers();
	return rsc.Generate(md3dDevice.Get(), true, (UINT)staticSamplers.size(), staticSamplers.data());
}

//-----------------------------------------------------------------------------
//
// The raytracing pipeline binds the shader code, root signatures and pipeline
// characteristics in a single structure used by DXR to invoke the shaders and
// manage temporary memory during raytracing
//
//
void MainApp::CreateRayTracingPipeline()
{
	nv_helpers_dx12::RayTracingPipelineGenerator pipeline(md3dDevice.Get());

	// The pipeline contains the DXIL code of all the shaders potentially executed
	// during the raytracing process. This section compiles the HLSL code into a
	// set of DXIL libraries. We chose to separate the code in several libraries
	// by semantic (ray generation, hit, miss) for clarity. Any code layout can be
	// used.
	m_rayGenLibrary = nv_helpers_dx12::CompileShaderLibrary(L"Shaders\\RayTracing\\RayGen.hlsl");
	m_missLibrary = nv_helpers_dx12::CompileShaderLibrary(L"Shaders\\RayTracing\\Miss.hlsl");
	m_hitShadowLibrary = nv_helpers_dx12::CompileShaderLibrary(L"Shaders\\RayTracing\\Hit_Shadow.hlsl");
	m_hitGlassLibrary = nv_helpers_dx12::CompileShaderLibrary(L"Shaders\\RayTracing\\Hit_GlassMaterial.hlsl");
	m_hitGlassSpecularLibrary = nv_helpers_dx12::CompileShaderLibrary(L"Shaders\\RayTracing\\Hit_GlassSpecularMaterial.hlsl");
	m_hitMatteLibrary = nv_helpers_dx12::CompileShaderLibrary(L"Shaders\\RayTracing\\Hit_MatteMaterial.hlsl");
	m_hitMetalLibrary = nv_helpers_dx12::CompileShaderLibrary(L"Shaders\\RayTracing\\Hit_MetalMaterial.hlsl");
	m_hitPlasticLibrary = nv_helpers_dx12::CompileShaderLibrary(L"Shaders\\RayTracing\\Hit_PlasticMaterial.hlsl");
	m_hitMirrorLibrary = nv_helpers_dx12::CompileShaderLibrary(L"Shaders\\RayTracing\\Hit_MirrorMaterial.hlsl");
	m_hitDisneyLibrary = nv_helpers_dx12::CompileShaderLibrary(L"Shaders\\RayTracing\\Hit_DisneyMaterial.hlsl");

	// In a way similar to DLLs, each library is associated with a number of
	// exported symbols. This
	// has to be done explicitly in the lines below. Note that a single library
	// can contain an arbitrary number of symbols, whose semantic is given in HLSL
	// using the [shader("xxx")] syntax
	pipeline.AddLibrary(m_rayGenLibrary.Get(), { L"RayGen" });
	pipeline.AddLibrary(m_missLibrary.Get(), { L"Miss" });
	pipeline.AddLibrary(m_missLibrary.Get(), { L"Miss_Shadow" });
	pipeline.AddLibrary(m_hitGlassLibrary.Get(), { L"ClosestHit_GlassMaterial" });
	pipeline.AddLibrary(m_hitGlassSpecularLibrary.Get(), { L"ClosestHit_GlassSpecularMaterial" });
	pipeline.AddLibrary(m_hitMatteLibrary.Get(), { L"ClosestHit_MatteMaterial" });
	pipeline.AddLibrary(m_hitMetalLibrary.Get(), { L"ClosestHit_MetalMaterial" });
	pipeline.AddLibrary(m_hitPlasticLibrary.Get(), { L"ClosestHit_PlasticMaterial" });
	pipeline.AddLibrary(m_hitMirrorLibrary.Get(), { L"ClosestHit_MirrorMaterial" });
	pipeline.AddLibrary(m_hitDisneyLibrary.Get(), { L"ClosestHit_DisneyMaterial" });
	pipeline.AddLibrary(m_hitShadowLibrary.Get(), { L"ClosestHit_Shadow" });
	pipeline.AddLibrary(m_hitShadowLibrary.Get(), { L"AnyHit_Shadow" });
	// To be used, each DX12 shader needs a root signature defining which
	// parameters and buffers will be accessed.
	m_rayGenSignature = CreateRayGenSignature();
	m_hitSignature = CreateHitSignature();
	m_hitShadowSignature = CreateHitShadowSignature();
	m_missSignature = CreateMissSignature();
	m_missShadowSignature = CreateEmptySignature();

	// 3 different shaders can be invoked to obtain an intersection: an
	// intersection shader is called
	// when hitting the bounding box of non-triangular geometry. This is beyond
	// the scope of this tutorial. An any-hit shader is called on potential
	// intersections. This shader can, for example, perform alpha-testing and
	// discard some intersections. Finally, the closest-hit program is invoked on
	// the intersection point closest to the ray origin. Those 3 shaders are bound
	// together into a hit group.
	pipeline.AddHitGroup(L"HitGroup_GlassMaterial", L"ClosestHit_GlassMaterial");
	pipeline.AddHitGroup(L"HitGroup_GlassSpecularMaterial", L"ClosestHit_GlassSpecularMaterial");
	pipeline.AddHitGroup(L"HitGroup_MatteMaterial", L"ClosestHit_MatteMaterial");
	pipeline.AddHitGroup(L"HitGroup_MetalMaterial", L"ClosestHit_MetalMaterial");
	pipeline.AddHitGroup(L"HitGroup_PlasticMaterial", L"ClosestHit_PlasticMaterial");
	pipeline.AddHitGroup(L"HitGroup_MirrorMaterial", L"ClosestHit_MirrorMaterial");
	pipeline.AddHitGroup(L"HitGroup_DisneyMaterial", L"ClosestHit_DisneyMaterial");
	pipeline.AddHitGroup(L"HitGroup_Shadow", L"ClosestHit_Shadow", L"AnyHit_Shadow");

	// The following section associates the root signature to each shader. Note
	// that we can explicitly show that some shaders share the same root signature
	// (eg. Miss and ShadowMiss). Note that the hit shaders are now only referred
	// to as hit groups, meaning that the underlying intersection, any-hit and
	// closest-hit shaders share the same root signature.
	pipeline.AddRootSignatureAssociation(m_rayGenSignature.Get(), { L"RayGen" });
	pipeline.AddRootSignatureAssociation(m_hitSignature.Get(), { L"HitGroup_GlassMaterial" });
	pipeline.AddRootSignatureAssociation(m_hitSignature.Get(), { L"HitGroup_GlassSpecularMaterial" });
	pipeline.AddRootSignatureAssociation(m_hitSignature.Get(), { L"HitGroup_MatteMaterial" });
	pipeline.AddRootSignatureAssociation(m_hitSignature.Get(), { L"HitGroup_MetalMaterial" });
	pipeline.AddRootSignatureAssociation(m_hitSignature.Get(), { L"HitGroup_PlasticMaterial" });
	pipeline.AddRootSignatureAssociation(m_hitSignature.Get(), { L"HitGroup_MirrorMaterial" });
	pipeline.AddRootSignatureAssociation(m_hitSignature.Get(), { L"HitGroup_DisneyMaterial" });
	pipeline.AddRootSignatureAssociation(m_hitShadowSignature.Get(), { L"HitGroup_Shadow" });
	pipeline.AddRootSignatureAssociation(m_missSignature.Get(), { L"Miss" });
	pipeline.AddRootSignatureAssociation(m_missShadowSignature.Get(), { L"Miss_Shadow" });

	// The payload size defines the maximum size of the data carried by the rays,
	// ie. the the data
	// exchanged between shaders, such as the HitInfo structure in the HLSL code.
	// It is important to keep this value as low as possible as a too high value
	// would result in unnecessary memory consumption and cache trashing.
	pipeline.SetMaxPayloadSize(19 * sizeof(float));

	// Upon hitting a surface, DXR can provide several attributes to the hit. In
	// our sample we just use the barycentric coordinates defined by the weights
	// u,v of the last two vertices of the triangle. The actual barycentrics can
	// be obtained using float3 barycentrics = float3(1.f-u-v, u, v);
	pipeline.SetMaxAttributeSize(2 * sizeof(float)); // barycentric coordinates

	// The raytracing process can shoot rays from existing hit points, resulting
	// in nested TraceRay calls. Our sample code traces only primary rays, which
	// then requires a trace depth of 1. Note that this recursion depth should be
	// kept to a minimum for best performance. Path tracing algorithms can be
	// easily flattened into a simple loop in the ray generation.
	pipeline.SetMaxRecursionDepth(5);

	// Compile the pipeline for execution on the GPU
	m_rtStateObject = pipeline.Generate();

	// Cast the state object into a properties object, allowing to later access
	// the shader pointers by name
	ThrowIfFailed(
		m_rtStateObject->QueryInterface(IID_PPV_ARGS(&m_rtStateObjectProps)));
}

//-----------------------------------------------------------------------------
//
// Allocate the buffer holding the raytracing output, with the same size as the
// output image
//
void MainApp::CreateRaytracingOutputBuffer() {
	D3D12_RESOURCE_DESC resDesc = {};
	resDesc.DepthOrArraySize = 1;
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	// The backbuffer is actually DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, but sRGB
	// formats cannot be used with UAVs. For accuracy we should convert to sRGB
	// ourselves in the shader
	resDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

	resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	resDesc.Width = mClientWidth;
	resDesc.Height = mClientHeight;
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resDesc.MipLevels = 1;
	resDesc.SampleDesc.Count = 1;
	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&nv_helpers_dx12::kDefaultHeapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
		D3D12_RESOURCE_STATE_COPY_SOURCE, nullptr,
		IID_PPV_ARGS(&m_outputResource)));
}

//-----------------------------------------------------------------------------
//
// Create the main heap used by the shaders, which will give access to the
// raytracing output and the top-level acceleration structure
//
void MainApp::CreateShaderResourceHeap() {
	// Create a SRV/UAV/CBV descriptor heap. We need 2 + m + 1 entries - 1 UAV for the
	// raytracing output and 1 SRV for the TLAS
	// m SRV for all the albeldo, normal etc. texture maps
	// 1 SRV for the environment map
	m_srvUavHeap = nv_helpers_dx12::CreateDescriptorHeap(
		md3dDevice.Get(), 2 + mTextures.size() + 1, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);

	// Get a handle to the heap memory on the CPU side, to be able to write the
	// descriptors directly
	D3D12_CPU_DESCRIPTOR_HANDLE srvHandle =
		m_srvUavHeap->GetCPUDescriptorHandleForHeapStart();

	// Create the UAV. Based on the root signature we created it is the first
	// entry. The Create*View methods write the view information directly into
	// srvHandle
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	md3dDevice->CreateUnorderedAccessView(m_outputResource.Get(), nullptr, &uavDesc,
		srvHandle);

	// Add the Top Level AS SRV right after the raytracing output buffer
	srvHandle.ptr += mCbvSrvDescriptorSize;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.RaytracingAccelerationStructure.Location =
		mTopLevelASBuffers.pResult->GetGPUVirtualAddress();
	// Write the acceleration structure view in the heap
	md3dDevice->CreateShaderResourceView(nullptr, &srvDesc, srvHandle);

	D3D12_SHADER_RESOURCE_VIEW_DESC tSrvDesc = {};
	tSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	tSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	tSrvDesc.Texture2D.MostDetailedMip = 0;
	tSrvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	tSrvDesc.Texture2D.MipLevels = -1;

	// Reorder the texture items
	std::vector<WTexture*> orderedTextureItems(mTextures.size());

	for (const auto& texItem : mTextures)
	{
		orderedTextureItems[texItem.second->TextureIdx] = texItem.second.get();
	}
	for (const auto& texItem : orderedTextureItems)
	{
		srvHandle.ptr += mCbvSrvDescriptorSize;
		auto tex = texItem->Resource;
		tSrvDesc.Format = tex->GetDesc().Format;
		tSrvDesc.Texture2D.MipLevels = tex->GetDesc().MipLevels;
		md3dDevice->CreateShaderResourceView(tex.Get(), &tSrvDesc, srvHandle);
		//if (texItem->TextureIdx < orderedTextureItems.size() - 1)
	}

	// next descriptor
	srvHandle.ptr += mCbvSrvDescriptorSize;
	auto tex = mEnvironmentMap->Resource;
	tSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	tSrvDesc.TextureCube.MostDetailedMip = 0;
	tSrvDesc.TextureCube.MipLevels = tex->GetDesc().MipLevels;
	tSrvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	tSrvDesc.Format = tex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(tex.Get(), &tSrvDesc, srvHandle);
}

void MainApp::CreateTextureShaderResourceHeap()
{
	m_textureSrvHeap = nv_helpers_dx12::CreateDescriptorHeap(
		md3dDevice.Get(), mTextures.size(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);

	CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(m_textureSrvHeap->GetCPUDescriptorHandleForHeapStart());

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	srvDesc.Texture2D.MipLevels = -1;

	// Reorder the texture items
	std::vector<WTexture*> orderedTextureItems(mTextures.size());

	for (const auto& texItem : mTextures)
	{
		orderedTextureItems[texItem.second->TextureIdx] = texItem.second.get();
	}
	for (const auto& texItem : orderedTextureItems)
	{
		auto tex = texItem->Resource;
		srvDesc.Format = tex->GetDesc().Format;
		srvDesc.Texture2D.MipLevels = tex->GetDesc().MipLevels;
		md3dDevice->CreateShaderResourceView(tex.Get(), &srvDesc, srvHandle);
		//if (texItem->TextureIdx < orderedTextureItems.size() - 1)
		srvHandle.Offset(1, mCbvSrvDescriptorSize);
	}

	/*for (const auto& texItem : mTextures)
	{
		auto tex = texItem.second->Resource;
		srvDesc.Format = tex->GetDesc().Format;
		srvDesc.Texture2D.MipLevels = tex->GetDesc().MipLevels;
		srvHandle.Offset(texItem.second->TextureIdx, mCbvSrvDescriptorSize);
		md3dDevice->CreateShaderResourceView(tex.Get(), &srvDesc, srvHandle);
		srvHandle.Offset(-(texItem.second->TextureIdx), mCbvSrvDescriptorSize);
	}*/
}

//-----------------------------------------------------------------------------
//
// The Shader Binding Table (SBT) is the cornerstone of the raytracing setup:
// this is where the shader resources are bound to the shaders, in a way that
// can be interpreted by the raytracer on GPU. In terms of layout, the SBT
// contains a series of shader IDs with their resource pointers. The SBT
// contains the ray generation shader, the miss shaders, then the hit groups.
// Using the helper class, those can be specified in arbitrary order.
//
void MainApp::CreateShaderBindingTable() {
	// The SBT helper class collects calls to Add*Program.  If called several
	// times, the helper must be emptied before re-adding shaders.
	m_sbtHelper.Reset();

	// The pointer to the beginning of the heap is the only parameter required by
	// shaders without root parameters
	D3D12_GPU_DESCRIPTOR_HANDLE srvUavHeapHandle =
		m_srvUavHeap->GetGPUDescriptorHandleForHeapStart();

	// The helper treats both root parameter pointers and heap pointers as void*,
	// while DX12 uses the
	// D3D12_GPU_DESCRIPTOR_HANDLE to define heap pointers. The pointer in this
	// struct is a UINT64, which then has to be reinterpreted as a pointer.
	auto heapPointer = reinterpret_cast<UINT64*>(srvUavHeapHandle.ptr);

	auto currentPassCB = mFrameResources[mCurrFrameResourceIndex]->PassCB.get();
	auto passCBPointer = reinterpret_cast<UINT64*>(currentPassCB->Resource()->GetGPUVirtualAddress());
	auto currentObjectBuffer = mFrameResources[mCurrFrameResourceIndex]->ObjectBuffer.get();
	auto objectBufferPointer = reinterpret_cast<UINT64*>(currentObjectBuffer->Resource()->GetGPUVirtualAddress());
	auto currentMaterialBuffer = mFrameResources[mCurrFrameResourceIndex]->MaterialBuffer.get();
	auto materialBufferPointer = reinterpret_cast<UINT64*>(currentMaterialBuffer->Resource()->GetGPUVirtualAddress());

	auto VertexBufferPointer = reinterpret_cast<UINT64*>(mVertexBuffer->GetGPUVirtualAddress());
	auto NormalBufferPointer = reinterpret_cast<UINT64*>(mNormalBuffer->GetGPUVirtualAddress());
	auto TexCoordBufferPointer = reinterpret_cast<UINT64*>(mTexCoordBuffer->GetGPUVirtualAddress());
	auto IndexBufferPointer = reinterpret_cast<UINT64*>(mIndexBuffer->GetGPUVirtualAddress());
	auto NormalIndexBufferPointer = reinterpret_cast<UINT64*>(mNormalIndexBuffer->GetGPUVirtualAddress());
	auto TexCoordIndexBufferPointer = reinterpret_cast<UINT64*>(mTexCoordIndexBuffer->GetGPUVirtualAddress());
	auto lightBufferPointer = reinterpret_cast<UINT64*>(mLightBuffer->GetGPUVirtualAddress());
	auto permutationsBufferPointer = reinterpret_cast<UINT64*>(mPermutationsBuffer->GetGPUVirtualAddress());
	// The ray generation only uses heap data
	m_sbtHelper.AddRayGenerationProgram(L"RayGen",
		{
			passCBPointer,
			objectBufferPointer,
			materialBufferPointer,
			permutationsBufferPointer,
			heapPointer
		});

	// The miss and hit shaders do not access any external resources: instead they
	// communicate their results through the ray payload
	m_sbtHelper.AddMissProgram(L"Miss", { heapPointer });
	m_sbtHelper.AddMissProgram(L"Miss_Shadow", {});

	// Adding hit groups
	m_sbtHelper.AddHitGroup(L"HitGroup_GlassMaterial",
		{
			objectBufferPointer,
			materialBufferPointer,
			VertexBufferPointer,
			NormalBufferPointer,
			TexCoordBufferPointer,
			IndexBufferPointer,
			NormalIndexBufferPointer,
			TexCoordIndexBufferPointer,
			lightBufferPointer,
			permutationsBufferPointer,
			heapPointer
		});
	m_sbtHelper.AddHitGroup(L"HitGroup_Shadow",
		{
			objectBufferPointer,
			materialBufferPointer
		});
	m_sbtHelper.AddHitGroup(L"HitGroup_GlassSpecularMaterial",
		{
			objectBufferPointer,
			materialBufferPointer,
			VertexBufferPointer,
			NormalBufferPointer,
			TexCoordBufferPointer,
			IndexBufferPointer,
			NormalIndexBufferPointer,
			TexCoordIndexBufferPointer,
			lightBufferPointer,
			permutationsBufferPointer,
			heapPointer
		});
	m_sbtHelper.AddHitGroup(L"HitGroup_Shadow",
		{
			objectBufferPointer,
			materialBufferPointer
		});
	m_sbtHelper.AddHitGroup(L"HitGroup_MatteMaterial",
		{
			objectBufferPointer,
			materialBufferPointer,
			VertexBufferPointer,
			NormalBufferPointer,
			TexCoordBufferPointer,
			IndexBufferPointer,
			NormalIndexBufferPointer,
			TexCoordIndexBufferPointer,
			lightBufferPointer,
			permutationsBufferPointer,
			heapPointer
		});
	m_sbtHelper.AddHitGroup(L"HitGroup_Shadow",
		{
			objectBufferPointer,
			materialBufferPointer
		});
	m_sbtHelper.AddHitGroup(L"HitGroup_MetalMaterial",
		{
			objectBufferPointer,
			materialBufferPointer,
			VertexBufferPointer,
			NormalBufferPointer,
			TexCoordBufferPointer,
			IndexBufferPointer,
			NormalIndexBufferPointer,
			TexCoordIndexBufferPointer,
			lightBufferPointer,
			permutationsBufferPointer,
			heapPointer
		});
	m_sbtHelper.AddHitGroup(L"HitGroup_Shadow",
		{
			objectBufferPointer,
			materialBufferPointer
		});
	m_sbtHelper.AddHitGroup(L"HitGroup_PlasticMaterial",
		{
			objectBufferPointer,
			materialBufferPointer,
			VertexBufferPointer,
			NormalBufferPointer,
			TexCoordBufferPointer,
			IndexBufferPointer,
			NormalIndexBufferPointer,
			TexCoordIndexBufferPointer,
			lightBufferPointer,
			permutationsBufferPointer,
			heapPointer
		});
	m_sbtHelper.AddHitGroup(L"HitGroup_Shadow",
		{
			objectBufferPointer,
			materialBufferPointer
		});
	m_sbtHelper.AddHitGroup(L"HitGroup_MirrorMaterial",
		{
			objectBufferPointer,
			materialBufferPointer,
			VertexBufferPointer,
			NormalBufferPointer,
			TexCoordBufferPointer,
			IndexBufferPointer,
			NormalIndexBufferPointer,
			TexCoordIndexBufferPointer,
			lightBufferPointer,
			permutationsBufferPointer,
			heapPointer
		});
	m_sbtHelper.AddHitGroup(L"HitGroup_Shadow",
		{
			objectBufferPointer,
			materialBufferPointer
		});
	//m_sbtHelper.AddHitGroup(L"HitGroup_DisneyMaterial",
	//	{
	//		objectBufferPointer,
	//		materialBufferPointer,
	//		VertexBufferPointer,
	//		NormalBufferPointer,
	//		TexCoordBufferPointer,
	//		IndexBufferPointer,
	//		NormalIndexBufferPointer,
	//		TexCoordIndexBufferPointer,
	//		lightBufferPointer,
	//		heapPointer
	//	});
	//m_sbtHelper.AddHitGroup(L"HitGroup_Shadow",
	//	{
	//		objectBufferPointer,
	//		materialBufferPointer
	//	});
	

	// Compute the size of the SBT given the number of shaders and their
  // parameters
	uint32_t sbtSize = m_sbtHelper.ComputeSBTSize();

	// Create the SBT on the upload heap. This is required as the helper will use
	// mapping to write the SBT contents. After the SBT compilation it could be
	// copied to the default heap for performance.
	m_sbtStorage = nv_helpers_dx12::CreateBuffer(
		md3dDevice.Get(), sbtSize, D3D12_RESOURCE_FLAG_NONE,
		D3D12_RESOURCE_STATE_GENERIC_READ, nv_helpers_dx12::kUploadHeapProps);
	if (!m_sbtStorage) {
		throw std::logic_error("Could not allocate the shader binding table");
	}

	// Compile the SBT from the shader and parameters info
	m_sbtHelper.Generate(m_sbtStorage.Get(), m_rtStateObjectProps.Get());
}

void MainApp::CreateVertexBuffer()
{
	// Create the vertex buffer.

	// Define the geometry for a triangle.
	RVertex triangleVertices[] = {
		{XMFLOAT3{0.0f, 0.25f, 0.0f}, XMFLOAT4{1.0f, 1.0f, 0.0f, 1.0f}},
		{XMFLOAT3{0.25f, -0.25f, 0.0f}, XMFLOAT4{0.0f, 1.0f, 1.0f, 1.0f}},
		{XMFLOAT3{-0.25f, -0.25f, 0.0f}, XMFLOAT4{1.0f, 0.0f, 1.0f, 1.0f}}
	};

	const UINT vertexBufferSize = sizeof(triangleVertices);

	// Note: using upload heaps to transfer static data like vert buffers is not
	// recommended. Every time the GPU needs it, the upload heap will be
	// marshalled over. Please read up on Default Heap usage. An upload heap is
	// used here for code simplicity and because there are very few verts to
	// actually transfer.
	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		IID_PPV_ARGS(&m_SimpleVertexBuffer)));

	// Copy the triangle data to the vertex buffer.
	UINT8* pVertexDataBegin;
	CD3DX12_RANGE readRange(
		0, 0); // We do not intend to read from this resource on the CPU.
	ThrowIfFailed(m_SimpleVertexBuffer->Map(
		0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
	memcpy(pVertexDataBegin, triangleVertices, sizeof(triangleVertices));
	m_SimpleVertexBuffer->Unmap(0, nullptr);

	// Initialize the vertex buffer view.
	m_SimpleVertexBufferView.BufferLocation = m_SimpleVertexBuffer->GetGPUVirtualAddress();
	m_SimpleVertexBufferView.StrideInBytes = sizeof(RVertex);
	m_SimpleVertexBufferView.SizeInBytes = vertexBufferSize;
}

void MainApp::SetupSceneWithXML(const char* filename)
{
	mSceneDescParser.Parse(filename);
	mGeometryMap = mSceneDescParser.getGeometryMap();
	mRenderItems = mSceneDescParser.getRenderItems();
	mMaterials = mSceneDescParser.getMaterialItems();
	
	const auto& textureItems = mSceneDescParser.getTextureItems();

	const auto& vertexBuffer = mSceneDescParser.getVertexBuffer();
	const auto& normalBuffer = mSceneDescParser.getNormalBuffer();
	const auto& texCoordBuffer = mSceneDescParser.getTexCoordBuffer();
	const auto& indexBuffer = mSceneDescParser.getIndexBuffer();
	const auto& normalIndexBuffer = mSceneDescParser.getNormalIndexBuffer();
	const auto& texCoordIndexBuffer = mSceneDescParser.getTexCoordIndexBuffer();

	const auto& cameraConfig = mSceneDescParser.getCameraConfig();
	const auto& lights = mSceneDescParser.getLights();

	mPassItem.NumFaces = indexBuffer.size() / 3;

	UINT64 vertexBufferSize = vertexBuffer.size() * sizeof(tinyobj::real_t);
	mVertexBuffer = d3dUtil::CreateDefaultBuffer(
		md3dDevice.Get(), mCommandList.Get(), vertexBuffer.data(),
		vertexBufferSize, mVertexBufferUploader
	);
	UINT64 normalBufferSize = normalBuffer.size() * sizeof(tinyobj::real_t);
	mNormalBuffer = d3dUtil::CreateDefaultBuffer(
		md3dDevice.Get(), mCommandList.Get(), normalBuffer.data(),
		normalBufferSize, mNormalBufferUploader
	);
	UINT64 texCoordBufferSize = texCoordBuffer.size() * sizeof(tinyobj::real_t);
	mTexCoordBuffer = d3dUtil::CreateDefaultBuffer(
		md3dDevice.Get(), mCommandList.Get(), texCoordBuffer.data(),
		texCoordBufferSize, mTexCoordBufferUploader
	);
	UINT64 indexBufferSize = indexBuffer.size() * sizeof(UINT32);
	mIndexBuffer = d3dUtil::CreateDefaultBuffer(
		md3dDevice.Get(), mCommandList.Get(), indexBuffer.data(),
		indexBufferSize, mIndexBufferUploader
	);
	UINT64 normalIndexBufferSize = normalIndexBuffer.size() * sizeof(INT32);
	mNormalIndexBuffer = d3dUtil::CreateDefaultBuffer(
		md3dDevice.Get(), mCommandList.Get(), normalIndexBuffer.data(),
		normalIndexBufferSize, mNormalIndexBufferUploader
	);
	UINT64 texCoordIndexBufferSize = texCoordIndexBuffer.size() * sizeof(INT32);
	mTexCoordIndexBuffer = d3dUtil::CreateDefaultBuffer(
		md3dDevice.Get(), mCommandList.Get(), texCoordIndexBuffer.data(),
		texCoordIndexBufferSize, mTexCoordIndexBufferUploader
	);
	UINT64 lightBufferSize = lights.size() * sizeof(ParallelogramLight);
	mLightBuffer = d3dUtil::CreateDefaultBuffer(
		md3dDevice.Get(), mCommandList.Get(), lights.data(),
		lightBufferSize, mLightBufferUploader
	);

	// Use Shader Resource View to declare the size of lightBuffer
	//D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	//srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	//srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	//srvDesc.Buffer.FirstElement = 0;			// 起始元素的索引
	//srvDesc.Buffer.NumElements = lights.size();	// 元素数目

	//ComPtr<ID3D11ShaderResourceView> mLightBufferView;
	//ThrowIfFailed(md3dDevice->CreateShaderResourceView(mLightBuffer.Get(), &srvDesc, &(mLightBufferView.Get())));

	// Setup the camera based on configs in XML
	SetupCamera(cameraConfig);
	// Load Textures based on texture items in XML
	LoadTextures(textureItems);
}


void MainApp::SetupCamera(const WCamereConfig& cameraConfig)
{
	XMFLOAT3 position = cameraConfig.position;
	XMFLOAT3 direction = cameraConfig.direction;
	XMFLOAT3 worldUp(0, 1.0f, 0);
	XMVECTOR vPosition = XMLoadFloat3(&position);
	XMVECTOR vDirection = XMLoadFloat3(&direction);
	XMVECTOR vWorldUP = XMLoadFloat3(&worldUp);
	XMVECTOR vTarget = vPosition + 140.0f * vDirection;
	mCamera.SetPosition(position);
	mCamera.LookAt(vPosition, vTarget, vWorldUP);
}

void MainApp::LoadTextures(const std::map<std::string, WTextureRecord>& textureItems)
{
	for (const auto& titem : textureItems)
	{
		const auto& t = titem.second;
		auto texMap = std::make_unique<WTexture>();
		texMap->TextureIdx = t.TextureIdx;
		texMap->Name = t.Name;
		texMap->Filename = t.Filename;
		texMap->TextureType = t.TextureType;
		ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
			mCommandList.Get(), texMap->Filename.c_str(),
			texMap->Resource, texMap->UploadHeap));
		mTextures[texMap->Name] = std::move(texMap);
	}

	// Add Environment Map
	std::wstring environmentMapFilename = L"../Textures/grasscube1024.dds";
	mEnvironmentMap = std::make_unique<WTexture>();
	mEnvironmentMap->Name = "EnvironmentMap";
	mEnvironmentMap->Filename = environmentMapFilename;
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), mEnvironmentMap->Filename.c_str(),
		mEnvironmentMap->Resource, mEnvironmentMap->UploadHeap));
}