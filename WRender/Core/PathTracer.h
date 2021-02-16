#pragma once

#include "../../Common/d3dUtil.h"
class PathTracer
{
public:
	PathTracer(ID3D12Device* device,
		UINT width, UINT height,
		DXGI_FORMAT format);
	PathTracer(const PathTracer& rhs) = delete;
	PathTracer& operator=(const PathTracer& rhs) = delete;
	~PathTracer() = default;
	ID3D12Resource* Output();

	void BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDescriptor,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuDescriptor,
		UINT descriptorSize);

	void OnResize(UINT newWidth, UINT newHeight);

	///<summary>
	/// Execute path tracing
	///</summary>
	void Execute(
		ID3D12GraphicsCommandList* cmdList,
		ID3D12RootSignature* rootSig,
		ID3D12PipelineState* ptPSO,
		ID3D12Resource* input,
		ID3D12Resource* passCB,
		Microsoft::WRL::ComPtr<ID3D12Resource> mGeometryMaterialBuffer,
		Microsoft::WRL::ComPtr<ID3D12Resource> mSphereInputBuffer,
		Microsoft::WRL::ComPtr<ID3D12Resource> mPlaneInputBuffer);

private:

	void BuildDescriptors();
	void BuildResources();

private:

	ID3D12Device* md3dDevice = nullptr;

	UINT mWidth = 0;
	UINT mHeight = 0;
	DXGI_FORMAT mFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mBackBufferCpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mBackBufferCpuUav;

	CD3DX12_GPU_DESCRIPTOR_HANDLE mBackBufferGpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mBackBufferGpuUav;

	Microsoft::WRL::ComPtr<ID3D12Resource> mBackBuffer = nullptr;
};