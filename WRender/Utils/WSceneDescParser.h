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
void getIndicesFromStructShape(
	const std::vector<tinyobj::shape_t>& p_shapes,
	std::vector<UINT32>& indices, std::vector<INT32>& normal_indices,
	std::vector<INT32>& texcoord_indices);
void getIndicesFromStructIndex(const std::vector<tinyobj::index_t>& p_indices, std::vector<UINT32>& s_indices);
void getNormalIndicesFromStructIndex(const std::vector<tinyobj::index_t>& p_indices, std::vector<INT32>& n_indices);
void getTexCoordIndicesFromStructIndex(const std::vector<tinyobj::index_t>& p_indices, std::vector<INT32>& t_indices);
std::wstring string2wstring(const std::string& str);

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
	std::map<std::string, WRenderItem>& getRenderItems() { return mRenderItems; };
	std::map<std::string, WMaterial>& getMaterialItems() { return mMaterialItems; };
	std::map<std::string, WTextureRecord>& getTextureItems() { return mTextureItems; };
	std::vector<tinyobj::real_t>& getVertexBuffer() { return mVertexBuffer; };
	std::vector<tinyobj::real_t>& getNormalBuffer() { return mNormalBuffer; };
	std::vector<tinyobj::real_t>& getTexCoordBuffer() { return mTexCoordBuffer; };
	std::vector<UINT32>& getIndexBuffer() { return mIndexBuffer; };
	std::vector<INT32>& getNormalIndexBuffer() { return mNormalIndexBuffer; };
	std::vector<INT32>& getTexCoordIndexBuffer() { return mTexCoordIndexBuffer; };
	WCamereConfig& getCameraConfig() { return mCameraConfig; };
	std::vector<ParallelogramLight>& getLights() { return mLights; }
private:
	tinyxml2::XMLDocument mXMLParser;
	std::map<std::string, WGeometryRecord> mGeometryMap;
	std::map<std::string, WRenderItem> mRenderItems;
	std::map<std::string, WMaterial> mMaterialItems;
	std::map<std::string, WTextureRecord> mTextureItems;
	std::vector<tinyobj::real_t> mVertexBuffer;
	std::vector<tinyobj::real_t> mNormalBuffer;
	std::vector<tinyobj::real_t> mTexCoordBuffer;
	std::vector<UINT32> mIndexBuffer;
	std::vector<INT32> mNormalIndexBuffer;
	std::vector<INT32> mTexCoordIndexBuffer;
	std::vector<ParallelogramLight> mLights;
	WCamereConfig mCameraConfig;
};


