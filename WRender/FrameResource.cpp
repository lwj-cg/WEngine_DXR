#include "FrameResource.h"

FrameResource::FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount, UINT materialCount)
{
    ThrowIfFailed(device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(CmdListAlloc.GetAddressOf())));

    PassCB = std::make_unique<UploadBuffer<WPassConstants>>(device, passCount, true);
    ObjectBuffer = std::make_unique<UploadBuffer<WObjectConstants>>(device, objectCount, false);
	MaterialBuffer = std::make_unique<UploadBuffer<WMaterialData>>(device, materialCount, false);
}

FrameResource::~FrameResource()
{

}