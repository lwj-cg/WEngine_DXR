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
#include "Utils/ObjFileLoader.h"
#include "Utils/WSceneDescParser.h"
#include "Include/WGUILayout.h"
#include "Include/GeometryShape.h"
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

	void LoadTextures();
	void BuildPathTracingRootSignature();
	void BuildDescriptorHeaps();
	void BuildShaders();
	void BuildShapeGeometry();
	void BuildPSOs();
	void BuildFrameResources();
	void BuildGeometryMaterials();
	void BuildRenderItems();

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
	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
	std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

	// List of all the render items.
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;
	std::vector<Sphere> mSphereItems;
	std::vector<Plane>  mPlaneItems;
	std::vector<PureGeometryMaterial> mGeometryMaterials;

	ComPtr<ID3D12Resource> mInputSphereBuffer = nullptr;
	ComPtr<ID3D12Resource> mInputSphereUploadBuffer = nullptr;
	ComPtr<ID3D12Resource> mInputPlaneBuffer = nullptr;
	ComPtr<ID3D12Resource> mInputPlaneUploadBuffer = nullptr;
	ComPtr<ID3D12Resource> mGeometryMaterialBuffer = nullptr;
	ComPtr<ID3D12Resource> mGeometryMaterialUploadBuffer = nullptr;

	// Render items divided by PSO.
	std::vector<RenderItem*> mRitemLayer[(int)RenderLayer::Count];

	UINT mSkyTexHeapIndex = 0;

	PassConstants mMainPassCB;

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
	void CreateTopLevelAS(
		const std::vector<std::pair<ComPtr<ID3D12Resource>, DirectX::XMMATRIX>>& instances);

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

	ComPtr<ID3D12RootSignature> m_rayGenSignature;
	ComPtr<ID3D12RootSignature> m_hitSignature;
	ComPtr<ID3D12RootSignature> m_hitShadowSignature;
	ComPtr<ID3D12RootSignature> m_hitDiffuseSignature;
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
	ComPtr<ID3D12Resource> m_outputResource;
	ComPtr<ID3D12DescriptorHeap> m_srvUavHeap;

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
	std::vector<WRenderItem> mRenderItems;
	void SetupSceneWithXML(const char* filename);
	void SetupCamera(const WCamereConfig& cameraConfig);


	// Vertex Buffer & Index Buffer
	ComPtr<ID3D12Resource> mVertexBuffer = nullptr;
	ComPtr<ID3D12Resource> mVertexBufferUploader = nullptr;
	ComPtr<ID3D12Resource> mIndexBuffer = nullptr;
	ComPtr<ID3D12Resource> mIndexBufferUploader = nullptr;

	// Material Buffer
	ComPtr<ID3D12Resource> mMaterialBuffer = nullptr;
	ComPtr<ID3D12Resource> mMaterialBufferUploader = nullptr;

	// Per Object Buffer Array
	ComPtr<ID3D12Resource> mObjectBufferArray = nullptr;
	ComPtr<ID3D12Resource> mObjectBufferArrayUploader = nullptr;

	// Per Pass Data
	WPassConstants mPassCB;

	// Light Buffer
	ComPtr<ID3D12Resource> mLightBuffer = nullptr;
	ComPtr<ID3D12Resource> mLightBufferUploader = nullptr;

	// num static frame
	UINT mNumStaticFrame = 0;
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

	LoadTextures();
	BuildPathTracingRootSignature();
	BuildDescriptorHeaps();
	BuildShaders();
	BuildShapeGeometry();
	BuildGeometryMaterials();
	BuildFrameResources();
	BuildPSOs();

	// Setup scene with XML description file
	SetupSceneWithXML("D:\\projects\\WEngine_DXR\\Scenes\\CornellBox.xml");

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
	// Create the shader binding table and indicating which shaders
	// are invoked for each instance in the  AS
	CreateShaderBindingTable();

	return true;
}

void MainApp::OnResize()
{
	D3DApp::OnResize();

	mCamera.SetLens(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);

	if (mPathTracer != nullptr)
	{
		mPathTracer->OnResize(mClientWidth, mClientHeight);
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
	if (mRaster) DrawForRasterize(gt);
	else DrawForRayTracing(gt);
}

void MainApp::DrawForRasterize(const GameTimer& gt)
{
	auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;

	// Reuse the memory associated with command recording.
	// We can only reset when the associated command lists have finished execution on the GPU.
	ThrowIfFailed(cmdListAlloc->Reset());

	// A command list can be reset after it has been added to the command queue via ExecuteCommandList.
	// Reusing the command list reuses memory.
	ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque"].Get()));

	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	// Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	// Clear the back buffer and depth buffer.
	mCommandList->ClearRenderTargetView(CurrentBackBufferView(), (float*)&mMainPassCB.bgColor, 0, nullptr);
	mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// ???Specify the buffers we are going to render to.
	mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

	ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvSrvUavDescriptorHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	auto passCB = mCurrFrameResource->PassCB->Resource();

	mPathTracer->Execute(mCommandList.Get(), mPathTracingRootSignature.Get(),
		mPSOs["PathTracing"].Get(), CurrentBackBuffer(), passCB, mGeometryMaterialBuffer, mInputSphereBuffer, mInputPlaneBuffer);

	// Prepare to copy output of path tracer to the back buffer.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COPY_DEST));

	mCommandList->CopyResource(CurrentBackBuffer(), mPathTracer->Output());

	// Transition to PRESENT state.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT));

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
	mCommandList->ClearRenderTargetView(CurrentBackBufferView(), (float*)&mMainPassCB.clearColor, 0, nullptr);
	mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

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
}

void MainApp::UpdateMaterialBuffer(const GameTimer& gt)
{
}

void MainApp::UpdateMainPassCB(const GameTimer& gt)
{
	++mNumStaticFrame %= 10000;

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
	mPassCB.NumStaticFrame = mNumStaticFrame;

	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mPassCB);
}

void MainApp::LoadTextures()
{
}

void MainApp::BuildPathTracingRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE uavTable;
	uavTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[5];

	// Perfomance TIP: Order from most frequent to least frequent.
	slotRootParameter[0].InitAsConstantBufferView(0);    // passCB : b0
	slotRootParameter[1].InitAsShaderResourceView(0, 1);  // mMaterial   : t0, space1
	slotRootParameter[2].InitAsShaderResourceView(1, 1);  // mSphereList : t1, space1
	slotRootParameter[3].InitAsShaderResourceView(2, 1);  // mPlaneList  : t2, space1
	slotRootParameter[4].InitAsDescriptorTable(1, &uavTable);  // outputBuffer : u0

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(5, slotRootParameter,
		0, nullptr,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mPathTracingRootSignature.GetAddressOf())));
}

void MainApp::BuildDescriptorHeaps()
{

	//
	// Create the SRV heap.
	//
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = 2;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mCbvSrvUavDescriptorHeap)));

	//
	// Fill out the heap with the descriptors to the BlurFilter resources.
	//

	mPathTracer->BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE(mCbvSrvUavDescriptorHeap->GetCPUDescriptorHandleForHeapStart()),
		CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvSrvUavDescriptorHeap->GetGPUDescriptorHandleForHeapStart()),
		mCbvSrvUavDescriptorSize);
}

void MainApp::BuildShaders()
{
	mShaders["pathTracingCS"] = d3dUtil::CompileShader(L"Shaders\\PathTracing.hlsl", nullptr, "PathTracingCS", "cs_5_1");
}

void MainApp::BuildShapeGeometry()
{
	mSphereItems = {
		//Sphere(1e5, Vec(1e5 + 1,40.8,-81.6)   ,0),  //Left red
		//Sphere(1e5, Vec(-1e5 + 99,40.8,-81.6) ,1),  //Rght blue
		//Sphere(1e5, Vec(50,40.8, 1e5)        ,2),  //Back 
		//Sphere(1e5, Vec(50,40.8,-1e5 + 170)  ,3),  //Frnt 
		//Sphere(1e5, Vec(50, 1e5, -81.6)       ,4),  //Botm 
		//Sphere(1e5, Vec(50,-1e5 + 81.6,-81.6) ,5),  //Top 
		Sphere(16.5,Vec(27,16.5,-47)          ,6),  //Mirr 
		Sphere(16.5,Vec(73,16.5,-78)          ,7),  //Glas 
		Sphere(600, Vec(50,681.6 - .27,-81.6) ,8)  //Lite 
	};
	mPlaneItems = {
		Plane(Vec(1,0,0),Vec(1,40.8,-81.6),0),  //Left red
		Plane(Vec(-1,0,0),Vec(99,40.8,-81.6),1), //Rght blue
		Plane(Vec(0,0,-1),Vec(50,40.8,0),2),      //Back
		Plane(Vec(0,1,0),Vec(50,0,-81.6),4),     //Botm
		Plane(Vec(0,-1,0),Vec(50,81.6,-81.6),5)     //Top
	};

	UINT64 sphereByteSize = mSphereItems.size() * sizeof(Sphere);
	UINT64 planeByteSize = mPlaneItems.size() * sizeof(Plane);

	// Create some buffers to be used as SRVs.
	mInputSphereBuffer = d3dUtil::CreateDefaultBuffer(
		md3dDevice.Get(),
		mCommandList.Get(),
		mSphereItems.data(),
		sphereByteSize,
		mInputSphereUploadBuffer);

	mInputPlaneBuffer = d3dUtil::CreateDefaultBuffer(
		md3dDevice.Get(),
		mCommandList.Get(),
		mPlaneItems.data(),
		planeByteSize,
		mInputPlaneUploadBuffer);
}

void MainApp::BuildPSOs()
{
	//
	// PSO for path tracing
	//
	D3D12_COMPUTE_PIPELINE_STATE_DESC ptPSO = {};
	ptPSO.pRootSignature = mPathTracingRootSignature.Get();
	ptPSO.CS =
	{
		reinterpret_cast<BYTE*>(mShaders["pathTracingCS"]->GetBufferPointer()),
		mShaders["pathTracingCS"]->GetBufferSize()
	};
	ptPSO.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	ThrowIfFailed(md3dDevice->CreateComputePipelineState(&ptPSO, IID_PPV_ARGS(&mPSOs["PathTracing"])));
}

void MainApp::BuildFrameResources()
{
	for (int i = 0; i < gNumFrameResources; ++i)
	{
		mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(),
			1, (UINT)mAllRitems.size(), (UINT)mMaterials.size()));
	}
}

void MainApp::BuildGeometryMaterials()
{
	mGeometryMaterials = {
		PureGeometryMaterial(Vec(.75,.25,.25),0), //Left
		PureGeometryMaterial(Vec(.25,.25,.75),0), //Right
		PureGeometryMaterial(Vec(.75,.75,.75),0), //Back 
		PureGeometryMaterial(Vec(0.0,0.0,0.0),0), //Frnt
		PureGeometryMaterial(Vec(.75,.75,.75),0), //Botm
		PureGeometryMaterial(Vec(.75,.75,.75),0), //Top
		PureGeometryMaterial(Vec(.999,.999,.999),1), //Mirror
		PureGeometryMaterial(Vec(.999,.999,.999),2), //Glass
		PureGeometryMaterial(Vec(0.0,0.0,0.0),0,Vec(12,12,12)) //Light
	};

	UINT64 materialBufferByteSize = mGeometryMaterials.size() * sizeof(PureGeometryMaterial);

	// Create some buffers to be used as SRVs.
	mGeometryMaterialBuffer = d3dUtil::CreateDefaultBuffer(
		md3dDevice.Get(),
		mCommandList.Get(),
		mGeometryMaterials.data(),
		materialBufferByteSize,
		mGeometryMaterialUploadBuffer);
}

void MainApp::BuildRenderItems()
{
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
	for (size_t i = 0; i < mRenderItems.size(); i++)
	{
		const auto& r = mRenderItems[i];
		auto& bottomLevelBufferPointer = bottomLevelBuffers[r.geometryName].pResult;
		mInstances[i] = { bottomLevelBufferPointer, r.transform };
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

//-----------------------------------------------------------------------------
// Create the main acceleration structure that holds all instances of the scene.
// Similarly to the bottom-level AS generation, it is done in 3 steps: gathering
// the instances, computing the memory requirements for the AS, and building the
// AS itself
//
void MainApp::CreateTopLevelAS(
	const std::vector<std::pair<ComPtr<ID3D12Resource>, DirectX::XMMATRIX>>
	& instances // pair of bottom level AS and matrix of the instance
) {
	// Gather all the instances into the builder helper
	for (size_t i = 0; i < instances.size(); i++) {
		mTopLevelASGenerator.AddInstance(instances[i].first.Get(),
			instances[i].second, static_cast<UINT>(i),
			static_cast<UINT>(0));
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

	// After all the buffers are allocated, or if only an update is required, we
	// can build the acceleration structure. Note that in the case of the update
	// we also pass the existing AS as the 'previous' AS, so that it can be
	// refitted in place.
	mTopLevelASGenerator.Generate(mCommandList.Get(),
		mTopLevelASBuffers.pScratch.Get(),
		mTopLevelASBuffers.pResult.Get(),
		mTopLevelASBuffers.pInstanceDesc.Get());
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
	rsc.AddHeapRangesParameter(
		{
			{
				0 /*t0*/, 1, 0,
				D3D12_DESCRIPTOR_RANGE_TYPE_SRV /*Top-level acceleration structure*/,
				1
			}
		});
	return rsc.Generate(md3dDevice.Get(), true);
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
	return rsc.Generate(md3dDevice.Get(), true);
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
	m_hitLibrary = nv_helpers_dx12::CompileShaderLibrary(L"Shaders\\RayTracing\\Hit.hlsl");
	m_hitDiffuseLibrary = nv_helpers_dx12::CompileShaderLibrary(L"Shaders\\RayTracing\\ClosestHit_Diffuse.hlsl");
	m_hitShadowLibrary = nv_helpers_dx12::CompileShaderLibrary(L"Shaders\\RayTracing\\AnyHit_Shadow.hlsl");

	// In a way similar to DLLs, each library is associated with a number of
	// exported symbols. This
	// has to be done explicitly in the lines below. Note that a single library
	// can contain an arbitrary number of symbols, whose semantic is given in HLSL
	// using the [shader("xxx")] syntax
	pipeline.AddLibrary(m_rayGenLibrary.Get(), { L"RayGen" });
	pipeline.AddLibrary(m_missLibrary.Get(), { L"Miss" });
	pipeline.AddLibrary(m_missLibrary.Get(), { L"Miss_Shadow" });
	pipeline.AddLibrary(m_hitLibrary.Get(), { L"ClosestHit_Default" });
	pipeline.AddLibrary(m_hitDiffuseLibrary.Get(), { L"ClosestHit_Diffuse" });
	pipeline.AddLibrary(m_hitShadowLibrary.Get(), { L"AnyHit_Shadow" });
	// To be used, each DX12 shader needs a root signature defining which
	// parameters and buffers will be accessed.
	m_rayGenSignature = CreateRayGenSignature();
	m_hitSignature = CreateHitSignature();
	m_hitDiffuseSignature = CreateHitSignature();
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
	pipeline.AddHitGroup(L"HitGroup_Default", L"ClosestHit_Default");
	pipeline.AddHitGroup(L"HitGroup_Diffuse", L"ClosestHit_Diffuse");
	pipeline.AddHitGroup(L"HitGroup_Shadow", L"", L"AnyHit_Shadow");

	// The following section associates the root signature to each shader. Note
	// that we can explicitly show that some shaders share the same root signature
	// (eg. Miss and ShadowMiss). Note that the hit shaders are now only referred
	// to as hit groups, meaning that the underlying intersection, any-hit and
	// closest-hit shaders share the same root signature.
	pipeline.AddRootSignatureAssociation(m_rayGenSignature.Get(), { L"RayGen" });
	pipeline.AddRootSignatureAssociation(m_hitSignature.Get(), { L"HitGroup_Default" });
	pipeline.AddRootSignatureAssociation(m_hitDiffuseSignature.Get(), { L"HitGroup_Diffuse" });
	pipeline.AddRootSignatureAssociation(m_hitShadowSignature.Get(), { L"HitGroup_Shadow" });
	pipeline.AddRootSignatureAssociation(m_missSignature.Get(), { L"Miss" });
	pipeline.AddRootSignatureAssociation(m_missShadowSignature.Get(), { L"Miss_Shadow" });

	// The payload size defines the maximum size of the data carried by the rays,
	// ie. the the data
	// exchanged between shaders, such as the HitInfo structure in the HLSL code.
	// It is important to keep this value as low as possible as a too high value
	// would result in unnecessary memory consumption and cache trashing.
	pipeline.SetMaxPayloadSize(16 * sizeof(float));

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
	pipeline.SetMaxRecursionDepth(10);

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
	// Create a SRV/UAV/CBV descriptor heap. We need 2 entries - 1 UAV for the
	// raytracing output and 1 SRV for the TLAS
	m_srvUavHeap = nv_helpers_dx12::CreateDescriptorHeap(
		md3dDevice.Get(), 2, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);

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
	srvHandle.ptr += md3dDevice->GetDescriptorHandleIncrementSize(
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.RaytracingAccelerationStructure.Location =
		mTopLevelASBuffers.pResult->GetGPUVirtualAddress();
	// Write the acceleration structure view in the heap
	md3dDevice->CreateShaderResourceView(nullptr, &srvDesc, srvHandle);
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

	auto currentPassCB = mFrameResources[0]->PassCB.get();
	auto passCBPointer = reinterpret_cast<UINT64*>(currentPassCB->Resource()->GetGPUVirtualAddress());
	auto objectCBPointer = reinterpret_cast<UINT64*>(mObjectBufferArray->GetGPUVirtualAddress());
	auto materialBufferPointer = reinterpret_cast<UINT64*>(mMaterialBuffer->GetGPUVirtualAddress());
	auto vertexBufferPointer = reinterpret_cast<UINT64*>(mVertexBuffer->GetGPUVirtualAddress());
	auto indexBufferPointer = reinterpret_cast<UINT64*>(mIndexBuffer->GetGPUVirtualAddress());
	auto lightBufferPointer = reinterpret_cast<UINT64*>(mLightBuffer->GetGPUVirtualAddress());
	// The ray generation only uses heap data
	m_sbtHelper.AddRayGenerationProgram(L"RayGen",
		{
			passCBPointer,
			objectCBPointer,
			materialBufferPointer,
			heapPointer
		});

	// The miss and hit shaders do not access any external resources: instead they
	// communicate their results through the ray payload
	m_sbtHelper.AddMissProgram(L"Miss", {});
	m_sbtHelper.AddMissProgram(L"Miss_Shadow", {});

	// Adding the triangle hit shader
	m_sbtHelper.AddHitGroup(L"HitGroup_Default",
		{
			objectCBPointer,
			materialBufferPointer,
			vertexBufferPointer,
			indexBufferPointer,
			lightBufferPointer,
			heapPointer
		});
	m_sbtHelper.AddHitGroup(L"HitGroup_Diffuse",
		{
			objectCBPointer,
			materialBufferPointer,
			vertexBufferPointer,
			indexBufferPointer,
			lightBufferPointer,
			heapPointer
		});
	m_sbtHelper.AddHitGroup(L"HitGroup_Shadow",
		{
			objectCBPointer,
			materialBufferPointer
		});

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
	const auto& materialBuffer = mSceneDescParser.getMaterialBuffer();
	const auto& vertexBuffer = mSceneDescParser.getVertexBuffer();
	const auto& indexBuffer = mSceneDescParser.getIndexBuffer();
	const auto& cameraConfig = mSceneDescParser.getCameraConfig();
	const auto& lights = mSceneDescParser.getLights();

	UINT64 vertexBufferSize = vertexBuffer.size() * sizeof(tinyobj::real_t);
	mVertexBuffer = d3dUtil::CreateDefaultBuffer(
		md3dDevice.Get(), mCommandList.Get(), vertexBuffer.data(),
		vertexBufferSize, mVertexBufferUploader
	);
	UINT64 indexBufferSize = indexBuffer.size() * sizeof(UINT32);
	mIndexBuffer = d3dUtil::CreateDefaultBuffer(
		md3dDevice.Get(), mCommandList.Get(), indexBuffer.data(),
		indexBufferSize, mIndexBufferUploader
	);
	UINT64 materialBufferSize = materialBuffer.size() * sizeof(WMaterialData);
	mMaterialBuffer = d3dUtil::CreateDefaultBuffer(
		md3dDevice.Get(), mCommandList.Get(), materialBuffer.data(),
		materialBufferSize, mMaterialBufferUploader
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

	std::vector<WObjectConstants> objectDataArray;
	for (const auto& r : mRenderItems)
	{
		objectDataArray.emplace_back(
			DirectX::XMMatrixTranspose(r.transform), r.matIdx,
			//r.transform, r.matIdx,
			(UINT)(r.vertexOffsetInBytes / (sizeof(SVertex))),
			(UINT)(r.indexOffsetInBytes / sizeof(UINT))
		);
	}
	UINT64 objectBufferSize = objectDataArray.size() * sizeof(WObjectConstants);
	mObjectBufferArray = d3dUtil::CreateDefaultBuffer(
		md3dDevice.Get(), mCommandList.Get(), objectDataArray.data(),
		objectBufferSize, mObjectBufferArrayUploader
	);
	// Setup the camera based on configs in XML
	SetupCamera(cameraConfig);
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