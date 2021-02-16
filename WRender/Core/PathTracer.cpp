#include "PathTracer.h"

PathTracer::PathTracer(ID3D12Device* device,
	UINT width, UINT height,
	DXGI_FORMAT format) :
	md3dDevice(device),
	mWidth(width),
	mHeight(height),
	mFormat(format)
{
	BuildResources();
}

ID3D12Resource* PathTracer::Output()
{
	return mBackBuffer.Get();
}

void PathTracer::BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDescriptor,
	CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuDescriptor,
	UINT descriptorSize)
{
	// Save references to the descriptors. 
	mBackBufferCpuSrv = hCpuDescriptor;
	mBackBufferCpuUav = hCpuDescriptor.Offset(1, descriptorSize);

	mBackBufferGpuSrv = hGpuDescriptor;
	mBackBufferGpuUav = hGpuDescriptor.Offset(1, descriptorSize);

	BuildDescriptors();
}

void PathTracer::OnResize(UINT newWidth, UINT newHeight)
{
	if ((mWidth != newWidth) || (mHeight != newHeight))
	{
		mWidth = newWidth;
		mHeight = newHeight;

		BuildResources();

		// New resource, so we need new descriptors to that resource.
		BuildDescriptors();
	}
}

void PathTracer::BuildDescriptors()
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = mFormat;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};

	uavDesc.Format = mFormat;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Texture2D.MipSlice = 0;

	md3dDevice->CreateShaderResourceView(mBackBuffer.Get(), &srvDesc, mBackBufferCpuSrv);
	md3dDevice->CreateUnorderedAccessView(mBackBuffer.Get(), nullptr, &uavDesc, mBackBufferCpuUav);

}

void PathTracer::BuildResources()
{

	D3D12_RESOURCE_DESC texDesc;
	ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Alignment = 0;
	texDesc.Width = mWidth;
	texDesc.Height = mHeight;
	texDesc.DepthOrArraySize = 1;
	texDesc.MipLevels = 1;
	texDesc.Format = mFormat;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&mBackBuffer)));
}

void PathTracer::Execute(ID3D12GraphicsCommandList* cmdList,
	ID3D12RootSignature* rootSig,
	ID3D12PipelineState* ptPSO,
	ID3D12Resource* input,
	ID3D12Resource* passCB,
	Microsoft::WRL::ComPtr<ID3D12Resource> mGeometryMaterialBuffer,
	Microsoft::WRL::ComPtr<ID3D12Resource> mSphereInputBuffer,
	Microsoft::WRL::ComPtr<ID3D12Resource> mPlaneInputBuffer)
{
	cmdList->SetComputeRootSignature(rootSig);

	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(input,
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE));

	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mBackBuffer.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));

	// Copy the input (back-buffer in this example) to back buffer of path tracer.
	cmdList->CopyResource(mBackBuffer.Get(), input);

	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mBackBuffer.Get(),
		D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

	cmdList->SetPipelineState(ptPSO);

	cmdList->SetComputeRootConstantBufferView(0, passCB->GetGPUVirtualAddress());
	cmdList->SetComputeRootShaderResourceView(1, mGeometryMaterialBuffer->GetGPUVirtualAddress());
	cmdList->SetComputeRootShaderResourceView(2, mSphereInputBuffer->GetGPUVirtualAddress());
	cmdList->SetComputeRootShaderResourceView(3, mPlaneInputBuffer->GetGPUVirtualAddress());
	cmdList->SetComputeRootDescriptorTable(4, mBackBufferGpuUav);

	// How many groups do we dispatch in X and Y dimension
	UINT numGroupsX = (UINT)ceilf(mWidth / 32.0f);
	UINT numGroupsY = (UINT)ceilf(mHeight / 32.0f);
	cmdList->Dispatch(numGroupsX, numGroupsY, 1);
}