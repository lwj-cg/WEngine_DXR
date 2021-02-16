#pragma once
#include <iostream>
//#include <fstream>
//#include <sstream>
#include <string>
#include <vector>
#include "../FrameResource.h"
//#include <Windows.h>
//#include <DirectXMath.h>
//#include <cstdint>

using namespace DirectX;

class ObjFileLoader
{
public:
	ObjFileLoader() = default;
	explicit ObjFileLoader(std::string objFileName) :ofName(objFileName) {};
	void parseObjFile();
	void parseObjFile(std::vector<Vertex>& vertices, std::vector<std::int32_t>& indices);
	const std::vector<Vertex>& getVertices() const { return mVertices; };
	const std::vector<std::int32_t>& getIndices() const { return mIndices; };
	XMVECTOR getVmin() const { return mVmin; };
	XMVECTOR getVmax() const { return mVmax; };

private:
	std::string ofName;
	std::vector<Vertex> mVertices;
	std::vector<std::int32_t> mIndices;
	XMFLOAT3 mVminf3 = { +MathHelper::Infinity, +MathHelper::Infinity, +MathHelper::Infinity };
	XMFLOAT3 mVmaxf3 = { -MathHelper::Infinity, -MathHelper::Infinity, -MathHelper::Infinity };
	XMVECTOR mVmin = XMLoadFloat3(&mVminf3);
	XMVECTOR mVmax = XMLoadFloat3(&mVmaxf3);
};

void ObjFileLoader::parseObjFile()
{
	std::ifstream fin(ofName);
	if (!fin)
	{
		MessageBox(0, L"Object file not found.", 0, 0);
		return;
	}

	// 遍历第一次，统计总数
	int vNum = 0;
	int vnNum = 0;
	int vtNum = 0;
	int fNum = 0;
	std::string line, word;
	while (std::getline(fin, line)) {
		std::istringstream record(line);
		std::string identifier;
		record >> identifier;
		if (identifier == "v") {
			++vNum;
		}
		else if (identifier == "vn") {
			++vnNum;
		}
		else if (identifier == "vt") {
			++vtNum;
		}
		else if (identifier == "f") {
			++fNum;
		}
	}
	fin.close();

	//遍历第二次，读取数据
	std::vector<DirectX::XMFLOAT3> Positions(vNum);
	std::vector<DirectX::XMFLOAT3> Normals(vnNum);
	std::vector<DirectX::XMFLOAT2> TexCoords(vtNum);
	mVertices.resize(3 * fNum);
	mIndices.resize(3 * fNum);
	fin.open(ofName);
	if (!fin.is_open())
	{
		MessageBox(0, L"Can not reopen object file.", 0, 0);
		return;
	}
	int vIdx = 0;
	int vnIdx = 0;
	int vtIdx = 0;
	int fIdx = 0;
	while (std::getline(fin, line)) {
		std::istringstream record(line);
		std::string identifier;
		record >> identifier;
		if (identifier == "v") {
			record >> Positions[vIdx].x >> Positions[vIdx].y >> Positions[vIdx].z;
			XMVECTOR P = XMLoadFloat3(&Positions[vIdx]);
			mVmin = XMVectorMin(mVmin, P);
			mVmax = XMVectorMax(mVmax, P);
			++vIdx;
		}
		else if (identifier == "vn") {
			record >> Normals[vnIdx].x >> Normals[vnIdx].y >> Normals[vnIdx].z;
			++vnIdx;
		}
		else if (identifier == "vt") {
			record >> TexCoords[vtIdx].x >> TexCoords[vtIdx].y;
			++vtIdx;
		}
		else if (identifier == "f") {
			int vIndex;
			int vnIndex;
			int vtIndex;
			std::string ignore;
			while (record >> vIndex >> ignore >> vnIndex >> ignore >> vtIndex)
			{
				Vertex vertex;
				vertex.Pos = Positions[vIndex];
				vertex.Normal = Normals[vnIndex];
				vertex.TexC = TexCoords[vtIndex];
				mVertices[fIdx] = vertex;
				mIndices[fIdx] = fIdx;
				++fIdx;
			}
		}
	}
	fin.close();

}

void ObjFileLoader::parseObjFile(std::vector<Vertex>& vertices, std::vector<std::int32_t>& indices)
{
	std::ifstream fin(ofName);
	if (!fin)
	{
		MessageBox(0, L"Object file not found.", 0, 0);
		return;
	}

	// 遍历第一次，统计总数
	int vNum = 0;
	int vnNum = 0;
	int vtNum = 0;
	int fNum = 0;
	std::string line, word;
	while (std::getline(fin, line)) {
		std::istringstream record(line);
		std::string identifier;
		record >> identifier;
		if (identifier == "v") {
			++vNum;
		}
		else if (identifier == "vn") {
			++vnNum;
		}
		else if (identifier == "vt") {
			++vtNum;
		}
		else if (identifier == "f") {
			++fNum;
		}
	}
	fin.close();

	//遍历第二次，读取数据
	std::vector<DirectX::XMFLOAT3> Positions(vNum);
	std::vector<DirectX::XMFLOAT3> Normals(vnNum);
	std::vector<DirectX::XMFLOAT2> TexCoords(vtNum);
	vertices.resize(3 * fNum);
	indices.resize(3 * fNum);
	fin.open(ofName);
	if (!fin.is_open())
	{
		MessageBox(0, L"Can not reopen object file.", 0, 0);
		return;
	}
	int vIdx = 0;
	int vnIdx = 0;
	int vtIdx = 0;
	int fIdx = 0;
	while (std::getline(fin, line)) {
		std::istringstream record(line);
		std::string identifier;
		record >> identifier;
		if (identifier == "v") {
			if (vIdx < Positions.size())
			{
				record >> Positions[vIdx].x >> Positions[vIdx].y >> Positions[vIdx].z;
				XMVECTOR P = XMLoadFloat3(&Positions[vIdx]);
				mVmin = XMVectorMin(mVmin, P);
				mVmax = XMVectorMax(mVmax, P);
				++vIdx;
			}
		}
		else if (identifier == "vn") {
			if (vnIdx < Normals.size())
			{
				record >> Normals[vnIdx].x >> Normals[vnIdx].y >> Normals[vnIdx].z;
				++vnIdx;
			}
		}
		else if (identifier == "vt") {
			if (vtIdx < TexCoords.size())
			{
				record >> TexCoords[vtIdx].x >> TexCoords[vtIdx].y;
				++vtIdx;
			}
		}
		else if (identifier == "f") {
			int vIndex;
			int vtIndex;
			int vnIndex;
			char ignore;
			while (record >> vIndex >> ignore >> vtIndex >> ignore >> vnIndex)
			{
				if ((vIndex <= Positions.size()) && (vnIndex <= Normals.size()) && (vtIndex <= TexCoords.size()) &&
					(fIdx < vertices.size()) && (fIdx < indices.size()))
				{
					Vertex vertex;
					vertex.Pos = Positions[vIndex-1];
					vertex.TexC = TexCoords[vtIndex-1];
					vertex.Normal = Normals[vnIndex-1];
					vertices[fIdx] = vertex;
					indices[fIdx] = fIdx;
					++fIdx;
				}
			}
		}
	}
	fin.close();

}