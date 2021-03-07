#include "WSceneDescParser.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include <../Include/tiny_obj_loader.h>


void WSceneDescParser::Parse(const char* xmlDoc)
{
	std::ifstream XMLFileStream(xmlDoc);
	std::stringstream XMLContentBuffer;
	XMLContentBuffer << XMLFileStream.rdbuf();
	std::string XMLContentStr(XMLContentBuffer.str());
	mXMLParser.Parse(XMLContentStr.c_str());


	tinyxml2::XMLHandle docHandle(&mXMLParser);

	// Star parsing the root of XML doc ---- "scene"
	tinyxml2::XMLElement* entry = docHandle.FirstChildElement("scene").ToElement();

	if (entry)
	{
		for (tinyxml2::XMLNode* node = entry->FirstChildElement(); node; node = node->NextSibling())
		{
			tinyxml2::XMLElement* e = node->ToElement();
			if (!e) continue;
			std::string nodeType(e->Value());
			if (nodeType == "object")
			{
				WRenderItem r;
				const char* objectName = e->Attribute("name");
				r.objName = objectName;
				tinyxml2::XMLNode* materialNode = e->FirstChildElement("Material");
				if (materialNode)
				{
					tinyxml2::XMLElement* materialElem = materialNode->ToElement();
					const char* materialName = materialElem->Attribute("name");
					std::string sMaterialName(materialName);
					if (mMaterialItems.find(sMaterialName) == mMaterialItems.end())
					{
						// Build new material from XML & append to MaterialBuffer
						WMaterial tmpMatData;
						tmpMatData.Name = sMaterialName;
						tinyxml2::XMLElement* transparentElem = materialNode->FirstChildElement("transparent");
						if (transparentElem)
						{
							float transparentVal = std::stof(transparentElem->GetText());
							tmpMatData.Transparent = transparentVal;
						}
						tinyxml2::XMLElement* smoothnessElem = materialNode->FirstChildElement("smoothness");
						if (smoothnessElem)
						{
							float smoothnessVal = std::stof(smoothnessElem->GetText());
							tmpMatData.Smoothness = smoothnessVal;
						}
						tinyxml2::XMLElement* metallicElem = materialNode->FirstChildElement("metallic");
						if (metallicElem)
						{
							float metallicVal = std::stof(metallicElem->GetText());
							tmpMatData.Metallic = metallicVal;
						}
						tinyxml2::XMLElement* albedoElem = materialNode->FirstChildElement("albedo");
						if (albedoElem)
						{
							DirectX::XMFLOAT4 albedoVal = parseFloat4(albedoElem->GetText());
							tmpMatData.Albedo = albedoVal;
						}
						tinyxml2::XMLElement* transColorElem = materialNode->FirstChildElement("transColor");
						if (transColorElem)
						{
							DirectX::XMFLOAT4 transColorVal = parseFloat4(transColorElem->GetText());
							tmpMatData.TransColor = transColorVal;
						}
						tinyxml2::XMLElement* F0Elem = materialNode->FirstChildElement("F0");
						if (F0Elem)
						{
							DirectX::XMFLOAT3 F0Val = parseFloat3(F0Elem->GetText());
							tmpMatData.F0 = F0Val;
						}
						tinyxml2::XMLElement* emissionElem = materialNode->FirstChildElement("emission");
						if (emissionElem)
						{
							DirectX::XMFLOAT3 emissionVal = parseFloat3(emissionElem->GetText());
							tmpMatData.Emission = emissionVal;
						}
						tinyxml2::XMLElement* ShaderElem = materialNode->FirstChildElement("Shader");
						if (ShaderElem)
						{
							std::string ShaderVal = ShaderElem->GetText();
							tmpMatData.Shader = ShaderVal;
						}

						tinyxml2::XMLElement* diffuseMapElem = materialNode->FirstChildElement("diffuseMap");
						if (diffuseMapElem)
						{
							std::string textureName = diffuseMapElem->Attribute("name");
							std::wstring textureFileName = string2wstring(diffuseMapElem->GetText());
							tmpMatData.DiffuseMapName = textureName;
							if (mTextureItems.find(textureName) == mTextureItems.end())
							{
								WTextureRecord tmpTextureRecord(textureName,textureFileName,mTextureItems.size());
								tmpTextureRecord.TextureType = DIFFUSE_MAP;
								tmpMatData.DiffuseMapIdx = tmpTextureRecord.TextureIdx;
								mTextureItems[textureName] = std::move(tmpTextureRecord);
							}
							else
							{
								tmpMatData.DiffuseMapIdx = mTextureItems.at(textureName).TextureIdx;
							}
						}
						tinyxml2::XMLElement* normalMapElem = materialNode->FirstChildElement("normalMap");
						if (normalMapElem)
						{
							std::string textureName = normalMapElem->Attribute("name");
							std::wstring textureFileName = string2wstring(normalMapElem->GetText());
							tmpMatData.NormalMapName = textureName;
							if (mTextureItems.find(textureName) == mTextureItems.end())
							{
								WTextureRecord tmpTextureRecord(textureName, textureFileName, mTextureItems.size());
								tmpTextureRecord.TextureType = NORMAL_MAP;
								tmpMatData.NormalMapIdx = tmpTextureRecord.TextureIdx;
								mTextureItems[textureName] = std::move(tmpTextureRecord);
							}
							else
							{
								tmpMatData.NormalMapIdx = mTextureItems.at(textureName).TextureIdx;
							}
						}
						r.materialName = sMaterialName;
						r.matIdx = mMaterialItems.size();
						tmpMatData.MatIdx = mMaterialItems.size();
						mMaterialItems[sMaterialName] = (std::move(tmpMatData));
					}
					else
					{
						// Direct use the record in materialMap
						r.materialName = sMaterialName;
						r.matIdx = mMaterialItems[sMaterialName].MatIdx;
					}
				}
				tinyxml2::XMLNode* meshNode = e->FirstChildElement("Mesh");
				if (meshNode)
				{
					tinyxml2::XMLElement* geometryElem = meshNode->FirstChildElement("geometry");
					if (geometryElem)
					{
						const char* filename = geometryElem->GetText();
						std::string sFilename(filename);
						if (mGeometryMap.find(sFilename) == mGeometryMap.end())
						{
							// Load obj with the help of 3rdparty library "tiny_obj_loader"
							tinyobj::ObjReaderConfig reader_config;
							reader_config.mtl_search_path = "./"; // Path to material files

							tinyobj::ObjReader reader;

							if (!reader.ParseFromFile(filename, reader_config))
							{
								if (!reader.Error().empty()) std::cerr << "TinyObjReader: " << reader.Error();
								exit(1);
							}

							if (!reader.Warning().empty()) {
								std::cout << "TinyObjReader: " << reader.Warning();
							}

							auto& attrib = reader.GetAttrib();
							auto& shapes = reader.GetShapes();
							auto& materials = reader.GetMaterials();

							// Set params in renderItem & update vertex buffer
							const auto& vertices = attrib.vertices;
							const auto& normals = attrib.normals;
							const auto& texcoords = attrib.texcoords;
							r.vertexOffsetInBytes = mVertexBuffer.size() * sizeof(tinyobj::real_t);
							r.vertexCount = vertices.size() / 3; // One vertex is consist of 3 coordinates
							mVertexBuffer.insert(mVertexBuffer.end(), vertices.begin(), vertices.end());
							if (normals.size() > 0)
							{
								r.normalOffsetInBytes = mNormalBuffer.size() * sizeof(tinyobj::real_t);
								mNormalBuffer.insert(mNormalBuffer.end(), normals.begin(), normals.end());
							}
							if (texcoords.size() > 0)
							{
								r.texCoordOffsetInBytes = mTexCoordBuffer.size() * sizeof(tinyobj::real_t);
								mTexCoordBuffer.insert(mTexCoordBuffer.end(), texcoords.begin(), texcoords.end());
							}

							// Set params in renderItem & update index buffer
							std::vector<UINT32> indices;
							std::vector<INT32> normal_indices;
							std::vector<INT32> texcoord_indices;
							getIndicesFromStructShape(shapes, indices, normal_indices, texcoord_indices);
							r.indexOffsetInBytes = mIndexBuffer.size() * sizeof(UINT32);
							r.indexCount = indices.size();
							mIndexBuffer.insert(mIndexBuffer.end(), indices.begin(), indices.end());
							mNormalIndexBuffer.insert(mNormalIndexBuffer.end(), normal_indices.begin(), normal_indices.end());
							mTexCoordIndexBuffer.insert(mTexCoordIndexBuffer.end(), texcoord_indices.begin(), texcoord_indices.end());

							// Build temp geometry record & store to mGeometryMap
							WGeometryRecord tempGeometryRecord;
							tempGeometryRecord.vertexOffsetInBytes = r.vertexOffsetInBytes;
							tempGeometryRecord.normalOffsetInBytes = r.normalOffsetInBytes;
							tempGeometryRecord.texCoordOffsetInBytes = r.texCoordOffsetInBytes;
							tempGeometryRecord.vertexCount = r.vertexCount;
							tempGeometryRecord.indexOffsetInBytes = r.indexOffsetInBytes;
							tempGeometryRecord.indexCount = r.indexCount;
							mGeometryMap[sFilename] = std::move(tempGeometryRecord);
							r.geometryName = sFilename;
						}
						else
						{
							// Direct use the record in mGeometryMap
							const auto& geometryRecord = mGeometryMap[sFilename];
							r.vertexOffsetInBytes = geometryRecord.vertexOffsetInBytes;
							r.normalOffsetInBytes = geometryRecord.normalOffsetInBytes;
							r.texCoordOffsetInBytes = geometryRecord.texCoordOffsetInBytes;
							r.vertexCount = geometryRecord.vertexCount;
							r.indexOffsetInBytes = geometryRecord.indexOffsetInBytes;
							r.indexCount = geometryRecord.indexCount;
							r.geometryName = sFilename;
						}
					}
					tinyxml2::XMLElement* transformElem = meshNode->FirstChildElement("transform");
					if (transformElem)
					{
						tinyxml2::XMLElement* transformMatrixElem = transformElem->FirstChildElement("transformMatrix");
						if (transformMatrixElem)
						{
							DirectX::XMFLOAT4X4 transformF4x4 = parseFloat4x4(transformElem->GetText());
							r.transform = DirectX::XMLoadFloat4x4(&transformF4x4);
						}
						else
						{
							auto& transformF4x4 = MathHelper::Identity4x4();
							auto& transformMatrix = DirectX::XMLoadFloat4x4(&transformF4x4);
							tinyxml2::XMLElement* translationElem = transformElem->FirstChildElement("translation");
							tinyxml2::XMLElement* rotationElem = transformElem->FirstChildElement("rotation");
							tinyxml2::XMLElement* scalingElem = transformElem->FirstChildElement("scale");
							if (translationElem)
							{
								auto& transFloat3 = parseFloat3(translationElem->GetText());
								r.translation = transFloat3;
								auto& transVector = DirectX::XMLoadFloat3(&transFloat3);
								auto& T = DirectX::XMMatrixTranslationFromVector(transVector);
								transformMatrix = DirectX::XMMatrixMultiply(T, transformMatrix);
							}
							if (rotationElem)
							{
								using DirectX::XM_PI;
								auto& rotFloat3 = parseFloat3(rotationElem->GetText());
								r.rotation = rotFloat3;
								auto& Rx = DirectX::XMMatrixRotationX(rotFloat3.x * XM_PI / 180.0f);
								transformMatrix = DirectX::XMMatrixMultiply(Rx, transformMatrix);
								auto& Ry = DirectX::XMMatrixRotationY(rotFloat3.y * XM_PI / 180.0f);
								transformMatrix = DirectX::XMMatrixMultiply(Ry, transformMatrix);
								auto& Rz = DirectX::XMMatrixRotationZ(rotFloat3.z * XM_PI / 180.0f);
								transformMatrix = DirectX::XMMatrixMultiply(Rz, transformMatrix);
							}
							if (scalingElem)
							{
								auto& scaleFloat3 = parseFloat3(scalingElem->GetText());
								r.scaling = scaleFloat3;
								auto& scaleVector = DirectX::XMLoadFloat3(&scaleFloat3);
								auto& S = DirectX::XMMatrixScalingFromVector(scaleVector);
								transformMatrix = DirectX::XMMatrixMultiply(S, transformMatrix);
							}
							r.transform = transformMatrix;
						}
					}
				}
				r.objIdx = mRenderItems.size();
				mRenderItems[objectName] = (std::move(r));
			}
			else if (nodeType == "light")
			{
				ParallelogramLight light;
				DirectX::XMFLOAT3 corner = { 0.0f, 0.0f, 0.0f };
				DirectX::XMFLOAT3 v1 = { 0.0f, 0.0f, 0.0f };
				DirectX::XMFLOAT3 v2 = { 0.0f, 0.0f, 0.0f };
				DirectX::XMFLOAT3 emission = { 0.0f, 0.0f, 0.0f };
				tinyxml2::XMLElement* cornerElem = node->FirstChildElement("corner");
				if (cornerElem)
				{
					corner = parseFloat3(cornerElem->GetText());
				}
				tinyxml2::XMLElement* v1Elem = node->FirstChildElement("v1");
				if (v1Elem)
				{
					v1 = parseFloat3(v1Elem->GetText());
				}
				tinyxml2::XMLElement* v2Elem = node->FirstChildElement("v2");
				if (v2Elem)
				{
					v2 = parseFloat3(v2Elem->GetText());
				}
				tinyxml2::XMLElement* emissionElem = node->FirstChildElement("emission");
				if (emissionElem)
				{
					emission = parseFloat3(emissionElem->GetText());
				}
				mLights.emplace_back(corner,v1,v2,emission);
			}
			else if (nodeType == "camera")
			{
				tinyxml2::XMLElement* positionElem = e->FirstChildElement("position");
				if (positionElem)
				{
					mCameraConfig.position = parseFloat3(positionElem->GetText());
				}
				tinyxml2::XMLElement* directionElem = e->FirstChildElement("direction");
				if (directionElem)
				{
					mCameraConfig.direction = parseFloat3(directionElem->GetText());
				}
			}
		}
	}
}

DirectX::XMFLOAT3 parseFloat3(const char* text)
{
	std::istringstream tstream(text);
	std::string tmpVal;
	DirectX::XMFLOAT3 floatVal;
	std::getline(tstream, tmpVal, ',');
	floatVal.x = std::stof(tmpVal);
	std::getline(tstream, tmpVal, ',');
	floatVal.y = std::stof(tmpVal);
	std::getline(tstream, tmpVal, ',');
	floatVal.z = std::stof(tmpVal);
	return floatVal;
}

DirectX::XMFLOAT3 parseFloat3(std::string text)
{
	std::istringstream tstream(text);
	std::string tmpVal;
	DirectX::XMFLOAT3 floatVal;
	std::getline(tstream, tmpVal, ',');
	floatVal.x = std::stof(tmpVal);
	std::getline(tstream, tmpVal, ',');
	floatVal.y = std::stof(tmpVal);
	std::getline(tstream, tmpVal, ',');
	floatVal.z = std::stof(tmpVal);
	return floatVal;
}

DirectX::XMFLOAT4 parseFloat4(const char* text)
{
	std::istringstream tstream(text);
	std::string tmpVal;
	DirectX::XMFLOAT4 floatVal;
	std::getline(tstream, tmpVal, ',');
	floatVal.x = std::stof(tmpVal);
	std::getline(tstream, tmpVal, ',');
	floatVal.y = std::stof(tmpVal);
	std::getline(tstream, tmpVal, ',');
	floatVal.z = std::stof(tmpVal);
	std::getline(tstream, tmpVal, ',');
	floatVal.w = std::stof(tmpVal);
	return floatVal;
}

DirectX::XMFLOAT4 parseFloat4(std::string text)
{
	std::istringstream tstream(text);
	std::string tmpVal;
	DirectX::XMFLOAT4 floatVal;
	std::getline(tstream, tmpVal, ',');
	floatVal.x = std::stof(tmpVal);
	std::getline(tstream, tmpVal, ',');
	floatVal.y = std::stof(tmpVal);
	std::getline(tstream, tmpVal, ',');
	floatVal.z = std::stof(tmpVal);
	std::getline(tstream, tmpVal, ',');
	floatVal.w = std::stof(tmpVal);
	return floatVal;
}

DirectX::XMFLOAT4X4 parseFloat4x4(const char* text)
{
	DirectX::XMFLOAT4X4 xMatrix;
	DirectX::XMFLOAT4 r0;
	DirectX::XMFLOAT4 r1;
	DirectX::XMFLOAT4 r2;
	DirectX::XMFLOAT4 r3;
	std::istringstream tsream(text);
	std::string tmpRow;
	// Parse 4 rows
	std::getline(tsream, tmpRow, ';');
	r0 = parseFloat4(tmpRow);
	std::getline(tsream, tmpRow, ';');
	r1 = parseFloat4(tmpRow);
	std::getline(tsream, tmpRow, ';');
	r2 = parseFloat4(tmpRow);
	std::getline(tsream, tmpRow, ';');
	r3 = parseFloat4(tmpRow);
	// Store to Matrix
	xMatrix(0, 0) = r0.x;
	xMatrix(0, 1) = r0.y;
	xMatrix(0, 2) = r0.z;
	xMatrix(0, 3) = r0.w;

	xMatrix(1, 0) = r1.x;
	xMatrix(1, 1) = r1.y;
	xMatrix(1, 2) = r1.z;
	xMatrix(1, 3) = r1.w;

	xMatrix(2, 0) = r2.x;
	xMatrix(2, 1) = r2.y;
	xMatrix(2, 2) = r2.z;
	xMatrix(2, 3) = r2.w;

	xMatrix(3, 0) = r3.x;
	xMatrix(3, 1) = r3.y;
	xMatrix(3, 2) = r3.z;
	xMatrix(3, 3) = r3.w;

	return xMatrix;
}

void getIndicesFromStructShape(
	const std::vector<tinyobj::shape_t>& p_shapes, 
	std::vector<UINT32>& indices, std::vector<INT32>& normal_indices,
	std::vector<INT32>& texcoord_indices)
{
	// At present, assume that there is only one shape
	for (const auto& shape : p_shapes)
	{
		std::vector<UINT32> v_indices;
		std::vector<INT32> n_indices;
		std::vector<INT32> t_indices;
		getIndicesFromStructIndex(shape.mesh.indices, v_indices);
		getNormalIndicesFromStructIndex(shape.mesh.indices, n_indices);
		getTexCoordIndicesFromStructIndex(shape.mesh.indices, t_indices);
		indices.insert(indices.end(), v_indices.begin(), v_indices.end());
		normal_indices.insert(normal_indices.end(), n_indices.begin(), n_indices.end());
		texcoord_indices.insert(texcoord_indices.end(), t_indices.begin(), t_indices.end());
	}
	return;
}

void getIndicesFromStructIndex(const std::vector<tinyobj::index_t>& p_indices, std::vector<UINT32>& v_indices)
{
	v_indices.resize(p_indices.size());
	for (size_t i = 0; i < p_indices.size(); i++)
	{
		v_indices[i] = static_cast<UINT32>(p_indices[i].vertex_index);
	}
	return;
}

void getNormalIndicesFromStructIndex(const std::vector<tinyobj::index_t>& p_indices, std::vector<INT32>& n_indices)
{
	n_indices.resize(p_indices.size());
	for (size_t i = 0; i < p_indices.size(); i++)
	{
		n_indices[i] = static_cast<INT32>(p_indices[i].normal_index);
	}
	return;
}

void getTexCoordIndicesFromStructIndex(const std::vector<tinyobj::index_t>& p_indices, std::vector<INT32>& t_indices)
{
	t_indices.resize(p_indices.size());
	for (size_t i = 0; i < p_indices.size(); i++)
	{
		t_indices[i] = static_cast<INT32>(p_indices[i].texcoord_index);
	}
	return;
}

std::wstring string2wstring(const std::string& s) {
	size_t convertedChars = 0;
	setlocale(LC_ALL, "chs");
	std::string curLocale = setlocale(LC_ALL, NULL);   //curLocale="C"
	setlocale(LC_ALL, "chs");
	const char* source = s.c_str();
	size_t charNum = sizeof(char) * s.size() + 1;
	wchar_t* dest = new wchar_t[charNum];
	mbstowcs_s(&convertedChars, dest, charNum, source, _TRUNCATE);
	std::wstring result = dest;
	delete[] dest;
	setlocale(LC_ALL, curLocale.c_str());
	return result;
}