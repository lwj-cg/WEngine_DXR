#pragma once

#include "../Common/d3dUtil.h"
#include "../Common/MathHelper.h"
#include "../Common/UploadBuffer.h"

struct WObjectConstants
{
    DirectX::XMFLOAT4X4 ObjectToWorld = MathHelper::Identity4x4();
    UINT     MatIdx;
    UINT     VertexOffset;
    UINT     IndexOffset;
    UINT     ObjPad2;
    WObjectConstants() = default;
    WObjectConstants(DirectX::XMMATRIX _Transform, UINT _MatIdx, UINT _VertexOffset, UINT _IndexOffset) 
        : MatIdx(_MatIdx), VertexOffset(_VertexOffset), IndexOffset(_IndexOffset)
    {
        DirectX::XMStoreFloat4x4(&ObjectToWorld, _Transform);
    };
};

struct ObjectConstants
{
    DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();
	UINT     MaterialIndex;
	UINT     ObjPad0;
	UINT     ObjPad1;
	UINT     ObjPad2;
};

struct WPassConstants
{
    DirectX::XMFLOAT4X4 View = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 InvView = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 Proj = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 InvProj = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 ViewProj = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 InvViewProj = MathHelper::Identity4x4();
    DirectX::XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };
    float PassPad0 = 0.0f;
    DirectX::XMFLOAT4 bgColor = { 0.0f, 0.0f, 0.0f, 1.0f };
};

struct PassConstants
{
    DirectX::XMFLOAT4X4 View = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 InvView = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 Proj = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 InvProj = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 ViewProj = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 InvViewProj = MathHelper::Identity4x4();
    DirectX::XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };
    float cbPerObjectPad1 = 0.0f;
    DirectX::XMFLOAT2 RenderTargetSize = { 0.0f, 0.0f };
    DirectX::XMFLOAT2 InvRenderTargetSize = { 0.0f, 0.0f };
    float NearZ = 0.0f;
    float FarZ = 0.0f;
    float TotalTime = 0.0f;
    float DeltaTime = 0.0f;

    DirectX::XMFLOAT4 AmbientLight = { 0.0f, 0.0f, 0.0f, 1.0f };
    DirectX::XMFLOAT4 FogColor = { 0.7f, 0.7f, 0.7f, 1.0f };
    DirectX::XMFLOAT4 clearColor = { 0.6f, 0.8f, 0.4f, 1.0f };
    DirectX::XMFLOAT4 bgColor = { 0.0f, 0.0f, 0.0f, 1.0f };

};

struct MaterialData
{
	DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
	DirectX::XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
	float Roughness = 0.5f;
    float Metallic = 0.0f;
    float MetalPad0;
    float MetalPad1;
    float MetalPad2;

	// Used in texture mapping.
	DirectX::XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();


	UINT DiffuseMapIndex = 0;
	UINT MaterialPad0;
	UINT MaterialPad1;
	UINT MaterialPad2;
};

struct Vertex
{
    DirectX::XMFLOAT3 Pos;
    DirectX::XMFLOAT3 Normal;
	DirectX::XMFLOAT2 TexC;
};

// Basic Vertex
struct SVertex
{
    DirectX::XMFLOAT3 Pos;
};

struct RVertex
{
    typedef DirectX::XMFLOAT3 XMFLOAT3;
    typedef DirectX::XMFLOAT4 XMFLOAT4;
    XMFLOAT3 Pos;
    XMFLOAT4 Color;

    RVertex(XMFLOAT3 _Pos, XMFLOAT4 _Color) :
        Pos(_Pos), Color(_Color) {}
};

struct WMaterialData
{
    DirectX::XMFLOAT4 Albedo = { 1.0f, 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT3 Emission = { 0.0f, 0.0f, 0.0f };
    float Transparent = 0.0f;
    float Smoothness = 0.5f;
    float Metallic = 0.0f;
    float MetalPad0;
    float MetalPad1;
    int DiffuseMapIdx = -1;
    int NormalMapIdx = -1;
    int MaterialPad0;
    int MaterialPad1;
};

struct WRenderItem
{
    UINT matIdx;
    std::string objName;
    std::string geometryName;
    UINT64 vertexOffsetInBytes;  // Offset of the first vertex in the vertex buffer
    UINT32 vertexCount;    // Number of vertices to consider in the buffer
    UINT64 indexOffsetInBytes;  // Offset of the first index in the index buffer
    UINT32 indexCount;    // Number of indices to consider in the buffer
    DirectX::XMMATRIX transform;
};

struct ParallelogramLight
{
    typedef DirectX::XMFLOAT3 XMFLOAT3;
    DirectX::XMFLOAT3 corner = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 v1 = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 v2 = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 normal = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 emission = { 0.0f, 0.0f, 0.0f };
    float lightPad0;

    ParallelogramLight() = default;
    ParallelogramLight(XMFLOAT3 _corner, XMFLOAT3 _v1, XMFLOAT3 _v2, XMFLOAT3 _emission)
        : corner(_corner), v1(_v1), v2(_v2), emission(_emission)
    {
        auto fv1 = DirectX::XMLoadFloat3(&v1);
        auto fv2 = DirectX::XMLoadFloat3(&v2);
        auto fnormal = DirectX::XMVector3Cross(fv1, fv2);
        DirectX::XMStoreFloat3(&normal, fnormal);
    }
};

// Stores the resources needed for the CPU to build the command lists
// for a frame.  
struct FrameResource
{
public:
    
    FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount, UINT materialCount);
    FrameResource(const FrameResource& rhs) = delete;
    FrameResource& operator=(const FrameResource& rhs) = delete;
    ~FrameResource();

    // We cannot reset the allocator until the GPU is done processing the commands.
    // So each frame needs their own allocator.
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CmdListAlloc;

    // We cannot update a cbuffer until the GPU is done processing the commands
    // that reference it.  So each frame needs their own cbuffers.
    std::unique_ptr<UploadBuffer<WPassConstants>> PassCB = nullptr;
 //   std::unique_ptr<UploadBuffer<ObjectConstants>> ObjectCB = nullptr;

	//std::unique_ptr<UploadBuffer<MaterialData>> MaterialBuffer = nullptr;

    // Fence value to mark commands up to this fence point.  This lets us
    // check if these frame resources are still in use by the GPU.
    UINT64 Fence = 0;
};