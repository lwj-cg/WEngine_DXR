#pragma once

#include "../Common/d3dUtil.h"
#include "../Common/MathHelper.h"
#include "../Common/UploadBuffer.h"

extern const int gNumFrameResources;

struct WObjectConstants
{
	DirectX::XMFLOAT4X4 ObjectToWorld = MathHelper::Identity4x4();
	UINT     MatIdx;
	UINT     VertexOffset;
	UINT     IndexOffset;
	INT32    NormalOffset = -1;
	INT32    TexCoordOffset = -1;

	WObjectConstants() = default;
	WObjectConstants(
		DirectX::XMMATRIX _Transform, UINT _MatIdx, UINT _VertexOffset, 
		UINT _IndexOffset, INT32 _NormalOffset = -1, INT32 _TexCoordOffset = -1)
		: MatIdx(_MatIdx), VertexOffset(_VertexOffset), IndexOffset(_IndexOffset),
		NormalOffset(_NormalOffset), TexCoordOffset(_TexCoordOffset)
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
	UINT NumStaticFrame = 1;
	UINT intPad0 = 0;
	UINT intPad1 = 0;
	UINT intPad2 = 0;
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

struct SNormal
{
	DirectX::XMFLOAT3 Normal;
};

struct STexCoord
{
	DirectX::XMFLOAT2 UV;
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

	WMaterialData() = default;
	WMaterialData(DirectX::XMFLOAT4 _Albedo, DirectX::XMFLOAT3 _Emission, float _Transparent,
		float _Smoothness, float _Metallic, int _DiffuseMapIdx = -1, int _NormalMapIdx = -1) :
		Albedo(_Albedo), Emission(_Emission), Transparent(_Transparent), Smoothness(_Smoothness),
		Metallic(_Metallic), DiffuseMapIdx(_DiffuseMapIdx), NormalMapIdx(_NormalMapIdx)
	{
	}
};

// Simple struct to represent a material for our demos.  A production 3D engine
// would likely create a class hierarchy of Materials.
struct WMaterial
{
	WMaterial() = default;
	// Unique material name for lookup.
	std::string Name;
	std::string DiffuseMapName;
	std::string NormalMapName;
	// Position in material buffer
	UINT MatIdx;

	// The part of useful data
	DirectX::XMFLOAT4 Albedo = { 1.0f, 1.0f, 1.0f, 1.0f };
	DirectX::XMFLOAT3 Emission = { 0.0f, 0.0f, 0.0f };
	float Transparent = 0.0f;
	float Smoothness = 0.5f;
	float Metallic = 0.0f;
	int DiffuseMapIdx = -1;
	int NormalMapIdx = -1;

	// Dirty flag indicating the material has changed and we need to update the constant buffer.
	// Because we have a material constant buffer for each FrameResource, we have to apply the
	// update to each FrameResource.  Thus, when we modify a material we should set 
	// NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
	int NumFramesDirty = gNumFrameResources;

};

struct WTextureRecord
{
	UINT TextureIdx;
	std::string Name;
	std::wstring Filename;
	WTextureRecord() = default;
	WTextureRecord(std::string _Name, std::wstring _Filename, UINT _TextureIdx)
		: Name(_Name), Filename(_Filename), TextureIdx(_TextureIdx)
	{
	}
};

struct WTexture
{
	// Unique material name for lookup.
	UINT TextureIdx;
	std::string Name;
	std::wstring Filename;

	Microsoft::WRL::ComPtr<ID3D12Resource> Resource = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> UploadHeap = nullptr;
};

struct WRenderItem
{
	WRenderItem() = default;

	UINT matIdx;
	UINT objIdx;
	std::string objName;
	std::string materialName;
	std::string geometryName;
	// -1 means not used
	UINT64 vertexOffsetInBytes;  // Offset of the first vertex in the vertex buffer
	INT64 normalOffsetInBytes = -1;  // Offset of the first normal in the normal buffer
	INT64 texCoordOffsetInBytes = -1;  // Offset of the first texcoord in the texcoord buffer
	UINT32 vertexCount;    // Number of vertices to consider in the buffer
	UINT64 indexOffsetInBytes;  // Offset of the first index in the index buffer
	UINT32 indexCount;    // Number of indices to consider in the buffer
	DirectX::XMMATRIX transform;
	DirectX::XMFLOAT3 translation = { 0,0,0 };
	DirectX::XMFLOAT3 rotation = { 0,0,0 };
	DirectX::XMFLOAT3 scaling = { 1,1,1 };

	WMaterial* material = nullptr;

	// Dirty flag indicating the object data has changed and we need to update the constant buffer.
	// Because we have an object cbuffer for each FrameResource, we have to apply the
	// update to each FrameResource.  Thus, when we modify obect data we should set 
	// NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
	int NumFramesDirty = gNumFrameResources;
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

	std::unique_ptr<UploadBuffer<WObjectConstants>> ObjectBuffer = nullptr;
	std::unique_ptr<UploadBuffer<WMaterialData>> MaterialBuffer = nullptr;

	// Fence value to mark commands up to this fence point.  This lets us
	// check if these frame resources are still in use by the GPU.
	UINT64 Fence = 0;
};