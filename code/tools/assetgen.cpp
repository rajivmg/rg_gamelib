#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <map>

#include <vectormath/vectormath.hpp>

#include "pugixml.hpp"

struct Vertex {
	std::vector<float> position;
	std::vector<float> uv;
	std::vector<float> normal;
	std::vector<float> tangent;

	bool operator==(const Vertex &a) const
	{
		return a.position == this->position && a.normal == this->normal && a.uv == this->uv && a.tangent == this->tangent;
		//constexpr float EPS = 0.000001;
		//return (fabs(a.position[0] - this->position[0]) < EPS) && (fabs(a.position[1] - this->position[1]) < EPS) && (fabs(a.position[2] - this->position[2]) < EPS); 
	}
};

cgltf_result loadAndParseGLTF(const char* filename, cgltf_data** outData)
{
	cgltf_data* data = NULL;
	cgltf_options options = {};
	cgltf_result result = cgltf_parse_file(&options, filename, &data);

	if(cgltf_result_success != result)
	{
		std::cout << "Failed to parse gltf file " << filename << " with error " << (uint32_t)result << std::endl;
		assert(false);
		return result;
	}

	result = cgltf_validate(data);
	if(cgltf_result_success != result)
	{
		std::cout << "GLTF validation finished with error " << (uint32_t)result << " for file " << filename << std::endl;
	}

	result = cgltf_load_buffers(&options, data, "./");
	if(cgltf_result_success != result)
	{
		std::cout << "Failed to load buffers from gltf file " << filename << " with error " << (uint32_t)result << std::endl;
		assert(false);
		return result;
	}

	*outData = data;

	return result;
}

/*
static bool processGLTFMesh(char const *pFilename, char const *pMeshName, Asset::VertexFormat vertFormat)
{
	cgltf_data *gltf = NULL;
	void *fileData = NULL;
	cgltf_result result = gltfLoadAndParse(pFilename, &gltf, &fileData);

	if(result != cgltf_result_success)
	{
		LOGF(LogLevel::eERROR, "Can't load and parse %s", pFilename);
		return false;
	}

	// 1. Find node with pMeshName
	cgltf_node *nodeWithMeshName;
	for(int n = 0; n < gltf->nodes_count; ++n)
	{
		if(stricmp(gltf->nodes[n].name, pMeshName) == 0)
		{
			nodeWithMeshName = &gltf->nodes[n];
		}
	}

	// 2. Push all the child nodes that contain mesh into a stack
	eastl::vector<cgltf_node *> nodesContainingMesh;
	nodesContainingMesh.reserve(512);

	eastl::vector<cgltf_node *> nodesMayContainMesh;
	nodesMayContainMesh.reserve(512);
	nodesMayContainMesh.push_back(nodeWithMeshName);

	while(!nodesMayContainMesh.empty())
	{
		cgltf_node *node = nodesMayContainMesh.back();
		nodesMayContainMesh.pop_back();

		if(node->mesh)
		{
			nodesContainingMesh.push_back(node);
		}

		for(int c = 0; c < node->children_count; ++c)
		{
			nodesMayContainMesh.push_back(node->children[c]);
		}
	}

	// 3. Read vertex data of meshes from buffers
	eastl::vector<Asset::SubMeshInfo> submeshes;
	submeshes.reserve(nodesContainingMesh.size());
	eastl::vector<F32> positions, normals, tangents, texcoords;
	eastl::vector<F32> vertices;
	eastl::vector<U16> indices;

	for(int m = 0; m < nodesContainingMesh.size(); ++m)
	{
		cgltf_mesh *mesh = nodesContainingMesh[m]->mesh;
		for(int p = 0; p < mesh->primitives_count; ++p)
		{
			cgltf_primitive *prem = &mesh->primitives[p];

			Asset::SubMeshInfo submesh = {};
			cgltf_node_transform_world(nodesContainingMesh[m], submesh.transform);
			submesh.indexCount = prem->indices->count;
			submesh.startIndexLocation = indices.size();
			submesh.baseVertexLocation = positions.size() / 3;
			submeshes.push_back(submesh);

			// Vertex data
			for(int a = 0; a < prem->attributes_count; ++a)
			{
				cgltf_attribute *att = &prem->attributes[a];
				switch(att->type)
				{
					case cgltf_attribute_type_position:
					{
						ASSERT(att->data->type == cgltf_type_vec3);
						cgltf_buffer_view *bv = att->data->buffer_view;
						U8 *data = (U8 *)bv->buffer->data + bv->offset + att->data->offset;
						for(int co = 0; co < att->data->count; ++co)
						{
							F32 *p = (F32 *)data;
							positions.push_back(p[0]);
							positions.push_back(p[1]);
							positions.push_back(p[2]);
							data += sizeof(F32) * 3 + bv->stride;
						}
					} break;
					case cgltf_attribute_type_normal:
					{
						ASSERT(att->data->type == cgltf_type_vec3);
						cgltf_buffer_view *bv = att->data->buffer_view;
						U8 *data = (U8 *)bv->buffer->data + bv->offset + att->data->offset;
						for(int co = 0; co < att->data->count; ++co)
						{
							F32 *n = (F32 *)data;
							normals.push_back(n[0]);
							normals.push_back(n[1]);
							normals.push_back(n[2]);
							data += sizeof(F32) * 3 + bv->stride;
						}
					} break;
					case cgltf_attribute_type_tangent:
					{
						ASSERT(att->data->type == cgltf_type_vec4);
						cgltf_buffer_view *bv = att->data->buffer_view;
						U8 *data = (U8 *)bv->buffer->data + bv->offset + att->data->offset;
						for(int co = 0; co < att->data->count; ++co)
						{
							F32 *t = (F32 *)data;
							tangents.push_back(t[0]);
							tangents.push_back(t[1]);
							tangents.push_back(t[2]);
							data += sizeof(F32) * 4 + bv->stride;
						}
					} break;
					case cgltf_attribute_type_texcoord:
					{
						ASSERT(att->data->type == cgltf_type_vec2);
						cgltf_buffer_view *bv = att->data->buffer_view;
						U8 *data = (U8 *)bv->buffer->data + bv->offset + att->data->offset;
						for(int co = 0; co < att->data->count; ++co)
						{
							F32 *tc = (F32 *)data;
							texcoords.push_back(tc[0]);
							texcoords.push_back(tc[1]);
							data += sizeof(F32) * 2 + bv->stride;
						}
					} break;
				}
			}

			// Index data
			ASSERT(prem->indices->type == cgltf_type_scalar && prem->indices->component_type == cgltf_component_type_r_16u);
			cgltf_buffer_view *indicesBufferView = prem->indices->buffer_view;
			U16 *indicesBuffer = (U16 *)((U8 *)indicesBufferView->buffer->data + indicesBufferView->offset + prem->indices->offset);
			for(int in = 0; in < prem->indices->count; ++in)
			{
				indices.push_back(indicesBuffer[in]);
			}
		}
	}

	tf_free(fileData);
	cgltf_free(gltf);

	// 4. Arrange vertex data 
	vertices.reserve(positions.size() + normals.size() + tangents.size() + texcoords.size());
	int numVertex = positions.size() / 3;
	switch(vertFormat)
	{
		case Asset::VERTEXFORMAT_POS3f_NOR3f:
		{
			for(int g = 0; g < numVertex; ++g)
			{
				vertices.push_back(positions[g * 3 + 0]);
				vertices.push_back(positions[g * 3 + 1]);
				vertices.push_back(positions[g * 3 + 2]);

				vertices.push_back(normals[g * 3 + 0]);
				vertices.push_back(normals[g * 3 + 1]);
				vertices.push_back(normals[g * 3 + 2]);
			}
		} break;
		case Asset::VERTEXFORMAT_POS3f_NOR3f_UV2f:
		{
			for(int g = 0; g < numVertex; ++g)
			{
				vertices.push_back(positions[g * 3 + 0]);
				vertices.push_back(positions[g * 3 + 1]);
				vertices.push_back(positions[g * 3 + 2]);

				vertices.push_back(normals[g * 3 + 0]);
				vertices.push_back(normals[g * 3 + 1]);
				vertices.push_back(normals[g * 3 + 2]);

				vertices.push_back(texcoords[g * 2 + 0]);
				vertices.push_back(texcoords[g * 2 + 1]);
			}
		} break;
		case Asset::VERTEXFORMAT_POS3f_NOR3f_TAN3f_UV2f:
		{
			for(int g = 0; g < numVertex; ++g)
			{
				vertices.push_back(positions[g * 3 + 0]);
				vertices.push_back(positions[g * 3 + 1]);
				vertices.push_back(positions[g * 3 + 2]);

				vertices.push_back(normals[g * 3 + 0]);
				vertices.push_back(normals[g * 3 + 1]);
				vertices.push_back(normals[g * 3 + 2]);

				vertices.push_back(tangents[g * 3 + 0]);
				vertices.push_back(tangents[g * 3 + 1]);
				vertices.push_back(tangents[g * 3 + 2]);

				vertices.push_back(texcoords[g * 2 + 0]);
				vertices.push_back(texcoords[g * 2 + 1]);
			}
		} break;
	}

	// 5. Write binary file
	char binFilename[FS_MAX_PATH];
	strcpy(binFilename, pMeshName);
	strcat(binFilename, ".MES");
	char mesFileName[FS_MAX_PATH + 4] = {};
	if(binFilename[0] == 'M' && binFilename[1] == 'E' && binFilename[2] == 'S' && binFilename[3] == '_')
	{
		strcpy(mesFileName, binFilename);
	}
	else
	{
		strcpy(mesFileName, "MES_");
		strcat(mesFileName, binFilename);
	}

	FileStream file = {};
	fsOpenStreamFromPath(RD_OUTPUT, mesFileName, FM_WRITE_BINARY, NULL, &file);

	Asset::MeshHeader header = {};
	strcpy(header.name, pMeshName);
	header.formatVertex = vertFormat;
	header.numSubmeshes = submeshes.size();
	header.byteOffsetSubmeshes = sizeof(Asset::MeshHeader);
	header.numVertices = vertices.size() / (U32)vertFormat;
	header.byteOffsetVertices = sizeof(Asset::MeshHeader) + sizeof(Asset::SubMeshInfo) * submeshes.size();
	header.numIndices = indices.size();
	header.byteOffsetIndices = header.byteOffsetVertices + vertices.size() * sizeof(F32);

	fsWriteToStream(&file, &header, sizeof(Asset::MeshHeader));
	fsWriteToStream(&file, &submeshes.front(), sizeof(Asset::SubMeshInfo) * submeshes.size());
	fsWriteToStream(&file, &vertices.front(), sizeof(F32) * vertices.size());
	fsWriteToStream(&file, &indices.front(), sizeof(F32) * indices.size());
	fsCloseStream(&file);

	return true;
}

*/
void convert(std::string input, std::string output, bool transformVertex)
{
	cgltf_data* gltf;
	cgltf_result result = loadAndParseGLTF(input.c_str(), &gltf);

	if(result != cgltf_result_success)
	{
		std::cout << "Can't load and parse " << input << std::endl;
		assert(false);
		return;
	}

	auto findAttribute = [](cgltf_attribute_type type, cgltf_primitive* primitive) -> cgltf_attribute*
	{
		for(int i = 0; i < primitive->attributes_count; ++i)
		{
			if(primitive->attributes[i].type == type)
			{
				return &primitive->attributes[i];
			}
		}

		return nullptr;
	};

	// 1. Find concerned nodes in the scene
	std::vector<cgltf_node*> meshNodes;
	std::vector<cgltf_node*> lightNodes; // TODO
	for(int nodeIdx = 0; nodeIdx < gltf->nodes_count; ++nodeIdx)
	{
		if(gltf->nodes[nodeIdx].mesh != nullptr)
		{
			meshNodes.push_back(&(gltf->nodes[nodeIdx]));
		}
	}

	// 3. Read vertex data of meshes from buffers
	std::vector<float> vertexData;
	std::vector<uint32_t> index32Data;
	std::vector<uint16_t> index16Data;
	vertexData.reserve(50000);
	index32Data.reserve(50000);
	index16Data.reserve(50000);

	struct GfxModel
	{
		char tag[32];

		uint8_t* buffer;
		uint32_t bufferLength;

		struct Mesh
		{
			char tag[32];

			bool hasTexCoord;
			bool hasNormal;
			bool hasTangent;

			bool has32BitIndices;

			float transform[16];

			uint32_t vertexCount;
			uint32_t vertexDataOffset;
			uint32_t indexCount;
			uint32_t indexDataOffset;
		};

		std::vector<Mesh> meshes;
	};

	GfxModel gfxModel;

	for(int meshIdx = 0; meshIdx < meshNodes.size(); ++meshIdx)
	{
		GfxModel::Mesh gfxMesh = {};

		cgltf_mesh* mesh = meshNodes[meshIdx]->mesh;
		assert(mesh->primitives_count == 1); // for now we only support 1 primitive per mesh

		Matrix4 positionTransformMatrix;
		Matrix4 normalTangentTransformMatrix;
		{
			cgltf_node_transform_world(meshNodes[meshIdx], gfxMesh.transform);
            
            if(transformVertex)
            {
                positionTransformMatrix.setCol0(Vector4(gfxMesh.transform[0], gfxMesh.transform[1], gfxMesh.transform[2], gfxMesh.transform[3]));
                positionTransformMatrix.setCol1(Vector4(gfxMesh.transform[4], gfxMesh.transform[5], gfxMesh.transform[6], gfxMesh.transform[7]));
                positionTransformMatrix.setCol2(Vector4(gfxMesh.transform[8], gfxMesh.transform[9], gfxMesh.transform[10], gfxMesh.transform[11]));
                positionTransformMatrix.setCol3(Vector4(gfxMesh.transform[12], gfxMesh.transform[13], gfxMesh.transform[14], gfxMesh.transform[15]));
            }
            else
            {
                positionTransformMatrix = Matrix4::identity();
            }
            
            // convert right-handed coordinates to left-handed coordinate system
            positionTransformMatrix = Matrix4::scale(Vector3(1.0f, 1.0f, -1.0f)) * positionTransformMatrix;
            
			normalTangentTransformMatrix = transpose(inverse(positionTransformMatrix));
		}

		cgltf_primitive* primitive = mesh->primitives;
		assert(primitive->type == cgltf_primitive_type_triangles);

		cgltf_attribute* positionAttrib = findAttribute(cgltf_attribute_type_position,	primitive);
		cgltf_attribute* texCoordAttrib = findAttribute(cgltf_attribute_type_texcoord,	primitive);
		cgltf_attribute* normalAttrib	= findAttribute(cgltf_attribute_type_normal,	primitive);
		cgltf_attribute* tangentAttrib = findAttribute(cgltf_attribute_type_tangent,	primitive);

		assert(positionAttrib->data->type == cgltf_type_vec3);
		if(texCoordAttrib) { assert(texCoordAttrib->data->type == cgltf_type_vec2); gfxMesh.hasTexCoord = true; }
		if(normalAttrib) { assert(normalAttrib->data->type == cgltf_type_vec3); gfxMesh.hasNormal = true; }
		if(tangentAttrib) { assert(tangentAttrib->data->type == cgltf_type_vec4); gfxMesh.hasTangent = true; }

		cgltf_size vertexCount = positionAttrib->data->count;
		if(texCoordAttrib) { assert(texCoordAttrib->data->count == vertexCount); }
		if(normalAttrib) { assert(normalAttrib->data->count == vertexCount); }
		if(tangentAttrib) { assert(tangentAttrib->data->count == vertexCount); }

		cgltf_size indexCount = primitive->indices->count;
		bool is32bitIndex = (primitive->indices->component_type == cgltf_component_type_r_32u) ? true : false;
		gfxMesh.has32BitIndices = is32bitIndex;

		strncpy(gfxMesh.tag, mesh->name, sizeof(GfxModel::Mesh::tag));
		gfxMesh.vertexCount = vertexCount;
		gfxMesh.vertexDataOffset = (uint32_t)(vertexData.size() * sizeof(float));
		gfxMesh.indexCount = indexCount;
		gfxMesh.indexDataOffset = (uint32_t) (is32bitIndex ? (index32Data.size() * sizeof(uint32_t)) : (index16Data.size() * sizeof(uint16_t)));

		gfxModel.meshes.push_back(gfxMesh);

		for(unsigned int x = 0; x < vertexCount; ++x)
		{
			if(positionAttrib)
			{
				cgltf_buffer_view* bufView = positionAttrib->data->buffer_view;
				uint8_t* buf = (uint8_t*)bufView->buffer->data + bufView->offset + positionAttrib->data->offset + (x * (positionAttrib->data->stride + bufView->stride));
				float* dataFloat = (float*)buf;
                
                Vector4 transformedPos = positionTransformMatrix * Vector4(dataFloat[0], dataFloat[1], dataFloat[2], 1.0f);
                vertexData.push_back(transformedPos.getX());
                vertexData.push_back(transformedPos.getY());
                vertexData.push_back(transformedPos.getZ());
			}

			if(texCoordAttrib)
			{
				cgltf_buffer_view* bufView = texCoordAttrib->data->buffer_view;
				uint8_t* buf = (uint8_t*)bufView->buffer->data + bufView->offset + texCoordAttrib->data->offset + (x * (texCoordAttrib->data->stride + bufView->stride));
				float* dataFloat = (float*)buf;
				vertexData.push_back(dataFloat[0]);
				vertexData.push_back(dataFloat[1]);
			}

			if(normalAttrib)
			{
				cgltf_buffer_view* bufView = normalAttrib->data->buffer_view;
				uint8_t* buf = (uint8_t*)bufView->buffer->data + bufView->offset + normalAttrib->data->offset + (x * (normalAttrib->data->stride + bufView->stride));
				float* dataFloat = (float*)buf;
                
                Vector4 transformedNormal = normalTangentTransformMatrix * Vector4(dataFloat[0], dataFloat[1], dataFloat[2], 0);
                vertexData.push_back(transformedNormal.getX());
                vertexData.push_back(transformedNormal.getY());
                vertexData.push_back(transformedNormal.getZ());
			}

			if(tangentAttrib)
			{
				cgltf_buffer_view* bufView = tangentAttrib->data->buffer_view;
				uint8_t* buf = (uint8_t*)bufView->buffer->data + bufView->offset + tangentAttrib->data->offset + (x * (tangentAttrib->data->stride + bufView->stride));
				float* dataFloat = (float*)buf;
                
                Vector4 transformedTangent = normalTangentTransformMatrix * Vector4(dataFloat[0], dataFloat[1], dataFloat[2], dataFloat[3]);
                vertexData.push_back(transformedTangent.getX());
                vertexData.push_back(transformedTangent.getY());
                vertexData.push_back(transformedTangent.getZ());
                vertexData.push_back(transformedTangent.getW());
			}
		}

		assert(primitive->indices);
		assert(primitive->indices->type == cgltf_type_scalar && (primitive->indices->component_type == cgltf_component_type_r_16u || primitive->indices->component_type == cgltf_component_type_r_32u));
		cgltf_buffer_view* indicesBufView = primitive->indices->buffer_view;

		if(primitive->indices->component_type == cgltf_component_type_r_16u)
		{
			assert(primitive->indices->stride == sizeof(uint16_t));
			for(unsigned int d = 0; d < primitive->indices->count; ++d)
			{
				uint16_t* dataU16 = (uint16_t*)((uint8_t*)indicesBufView->buffer->data + indicesBufView->offset + primitive->indices->offset + (d * primitive->indices->stride));
				index16Data.push_back(*dataU16);
			}
		}
		else if(primitive->indices->component_type == cgltf_component_type_r_32u)
		{
			assert(primitive->indices->stride == sizeof(uint32_t));
			for(unsigned int d = 0; d < primitive->indices->count; ++d)
			{
				uint32_t* dataU32 = (uint32_t*)((uint8_t*)indicesBufView->buffer->data + indicesBufView->offset + primitive->indices->offset + (d * primitive->indices->stride));
				index32Data.push_back(*dataU32);
			}
		}
	}

	cgltf_free(gltf);

    uint32_t vertexBufferLength = (vertexData.size() * sizeof(float));
    uint32_t index32BufferLength = (index32Data.size() * sizeof(uint32_t));
    uint32_t index16BufferLength = (index16Data.size() * sizeof(uint16_t));
    uint32_t totalBufferLength = vertexBufferLength + index32BufferLength + index16BufferLength;

	// Write XML and BIN
	pugi::xml_document modelDoc;
 	pugi::xml_node modelNode =  modelDoc.append_child("model");
	modelNode.append_attribute("name").set_value(output.c_str());
	modelNode.append_attribute("meshCount").set_value((uint32_t)gfxModel.meshes.size());
	modelNode.append_attribute("bufferName").set_value((output+".bin").c_str());
	modelNode.append_attribute("vertexBufferOffset").set_value(0);
	modelNode.append_attribute("index32BufferOffset").set_value(vertexBufferLength);
	modelNode.append_attribute("index16BufferOffset").set_value(vertexBufferLength + index32BufferLength);
	modelNode.append_attribute("bufferLength").set_value(totalBufferLength);

	for(int i = 0; i < gfxModel.meshes.size(); ++i)
	{
		GfxModel::Mesh& m = gfxModel.meshes[i];
		pugi::xml_node meshNode = modelNode.append_child("mesh");
		meshNode.append_attribute("name").set_value(m.tag);
		meshNode.append_attribute("hasTexCoord").set_value(m.hasTexCoord);
		meshNode.append_attribute("hasNormal").set_value(m.hasNormal);
		meshNode.append_attribute("hasTangent").set_value(m.hasTangent);
		meshNode.append_attribute("has32BitIndices").set_value(m.has32BitIndices);
		meshNode.append_attribute("vertexCount").set_value(m.vertexCount);
		meshNode.append_attribute("vertexDataOffset").set_value(m.vertexDataOffset);
		meshNode.append_attribute("indexCount").set_value(m.indexCount);
		meshNode.append_attribute("indexDataOffset").set_value(m.indexDataOffset);

		pugi::xml_node transformNode = meshNode.append_child("transform");
		for (int k = 0; k < 16; k++)
		{
			transformNode.append_child("m").text().set(m.transform[k]);
		}
	}

	modelDoc.print(std::cout);
	modelDoc.save_file((output + ".xml").c_str());

	FILE* fs = fopen((output + ".bin").c_str(), "wb");
	fwrite(vertexData.data(), sizeof(float), vertexData.size(), fs);
	fwrite(index32Data.data(), sizeof(uint32_t), index32Data.size(), fs);
	fwrite(index16Data.data(), sizeof(uint16_t), index16Data.size(), fs);
	fclose(fs);
}

int main(int argc, char **argv)
{
	if(argc != 3)
	{
		std::cerr << "Wrong number of arguments, just give me the the name of input and output file. Example: cow.gltf cow" << std::endl;
		return EXIT_FAILURE;
	}
    
    bool transformVertices = false;
    if(argc > 3)
    {
        int i = 3;
        while(i < argc)
        {
            if(strcmp(argv[i], "--transformvertices"))
            {
                transformVertices = true;
            }
            ++i;
        }
    }

    convert(std::string(argv[1]), std::string(argv[2]), transformVertices);

    return EXIT_SUCCESS;
}
