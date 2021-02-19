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
						tinyxml2::XMLElement* emissionElem = materialNode->FirstChildElement("emission");
						if (emissionElem)
						{
							DirectX::XMFLOAT3 emissionVal = parseFloat3(emissionElem->GetText());
							tmpMatData.Emission = emissionVal;
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
							std::vector<tinyobj::real_t> vertices = attrib.vertices;
							r.vertexOffsetInBytes = mVertexBuffer.size() * sizeof(tinyobj::real_t);
							r.vertexCount = vertices.size() / 3; // One vertex is consist of 3 coordinates
							mVertexBuffer.insert(mVertexBuffer.end(), vertices.begin(), vertices.end());

							// Set params in renderItem & update index buffer
							std::vector<UINT32> indices;
							getIndicesFromStructShape(shapes, indices);
							r.indexOffsetInBytes = mIndexBuffer.size() * sizeof(UINT32);
							r.indexCount = indices.size();
							mIndexBuffer.insert(mIndexBuffer.end(), indices.begin(), indices.end());

							// Build temp geometry record & store to mGeometryMap
							WGeometryRecord tempGeometryRecord;
							tempGeometryRecord.vertexOffsetInBytes = r.vertexOffsetInBytes;
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
								auto& transVector = DirectX::XMLoadFloat3(&transFloat3);
								auto& T = DirectX::XMMatrixTranslationFromVector(transVector);
								transformMatrix = DirectX::XMMatrixMultiply(T, transformMatrix);
							}
							if (rotationElem)
							{
								using DirectX::XM_PI;
								auto& rotFloat3 = parseFloat3(rotationElem->GetText());
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

void getIndicesFromStructShape(const std::vector<tinyobj::shape_t>& p_shapes, std::vector<UINT32>& indices)
{
	// At present, assume that there is only one shape
	for (const auto& shape : p_shapes)
	{
		std::vector<UINT32> s_indices;
		getIndicesFromStructIndex(shape.mesh.indices, s_indices);
		indices.insert(indices.end(), s_indices.begin(), s_indices.end());
	}
	return;
}

void getIndicesFromStructIndex(const std::vector<tinyobj::index_t>& p_indices, std::vector<UINT32>& s_indices)
{
	s_indices.resize(p_indices.size());
	for (size_t i = 0; i < p_indices.size(); i++)
	{
		s_indices[i] = static_cast<UINT32>(p_indices[i].vertex_index);
	}
	return;
}