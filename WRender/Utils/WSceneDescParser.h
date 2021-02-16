#pragma once
#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <DirectXMath.h>
#include <../FrameResource.h>
#include <../Include/tinyxml2.h>
#include <../Include/tiny_obj_loader.h>

using Microsoft::WRL::ComPtr;

#pragma comment(lib, "tinyxml2.lib")

// Some useful help functions
DirectX::XMFLOAT3 parseFloat3(const char* text);
DirectX::XMFLOAT3 parseFloat3(std::string text);
DirectX::XMFLOAT4 parseFloat4(const char* text);
DirectX::XMFLOAT4 parseFloat4(std::string text);
DirectX::XMFLOAT4X4 parseFloat4x4(const char* text);
void getIndicesFromStructShape(const std::vector<tinyobj::shape_t>& p_shapes, std::vector<UINT32>& indices);
void getIndicesFromStructIndex(const std::vector<tinyobj::index_t>& p_indices, std::vector<UINT32>& s_indices);

struct WGeometryRecord
{
	UINT64 vertexOffsetInBytes;  // Offset of the first vertex in the vertex buffer
	UINT32 vertexCount;    // Number of vertices to consider in the buffer
	UINT64 indexOffsetInBytes;  // Offset of the first index in the index buffer
	UINT32 indexCount;    // Number of indices to consider in the buffer
};

struct WCamereConfig
{
	DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 direction = { 0.0f, 0.0f, 1.0f };
};

class WSceneDescParser
{
public:
	WSceneDescParser() = default;
	void Parse(const char* xmlDoc);
public:
	std::map<std::string, WGeometryRecord>& getGeometryMap() { return mGeometryMap; };
	std::vector<WRenderItem>& getRenderItems() { return mRenderItems; };
	std::vector<WMaterialData>& getMaterialBuffer() { return mMaterialBuffer; };
	std::vector<tinyobj::real_t>& getVertexBuffer() { return mVertexBuffer; };
	std::vector<UINT32>& getIndexBuffer() { return mIndexBuffer; };
	WCamereConfig& getCameraConfig() { return mCameraConfig; };
private:
	tinyxml2::XMLDocument mXMLParser;
	std::map<std::string, WGeometryRecord> mGeometryMap;
	std::vector<WRenderItem> mRenderItems;
	std::vector<WMaterialData> mMaterialBuffer;
	std::vector<tinyobj::real_t> mVertexBuffer;
	std::vector<UINT32> mIndexBuffer;
	WCamereConfig mCameraConfig;
};


