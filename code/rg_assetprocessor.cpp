#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

#include <iostream>
#include <fstream>
#include <string>
#include <vector>

struct Vertex {
	std::vector<float> position;
	std::vector<float> uv;
	std::vector<float> normal;
	std::vector<float> binormal;

	bool operator==(const Vertex &a) const
	{
		return a.position == this->position && a.normal == this->normal && a.uv == this->uv && a.binormal == this->binormal;
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
void convert(std::string input, std::string output)
{
	cgltf_data* data;
	cgltf_result result = loadAndParseGLTF("shaderball.glb", &data);
#if 0
//**************************************************
	tinyobj::attrib_t attributes;
    	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string warning, error;
	std::vector<Vertex> vertices;
	std::vector<unsigned int> indices;
	
	if(!tinyobj::LoadObj(&attributes, &shapes, &materials, &error, input.c_str(), NULL, true))
		throw std::runtime_error("Obj loader:" + warning + "\n" + error);
	
	bool isNormal = attributes.normals.size() > 0;
	bool isUv = attributes.texcoords.size() > 0;

	for(const auto& shape : shapes)
		for(const auto& index : shape.mesh.indices)
		{
			Vertex vertex;
			int offset = 3*index.vertex_index;
			vertex.position = {attributes.vertices[offset], attributes.vertices[offset+1], attributes.vertices[offset+2]};
			if(isNormal)
			{	offset = 3*index.normal_index;
				vertex.normal = {attributes.normals[offset], attributes.normals[offset+1], attributes.normals[offset+2]};
			}
			if(isUv)
			{	
				offset = 2*index.texcoord_index;
				vertex.uv = {attributes.texcoords[offset], attributes.texcoords[offset+1]};
			}
			//TODO use map or set
			unsigned int uniqueIndex = vertices.size();
			bool found = false;
			for(int i=0; i<vertices.size(); i++)
				if(vertices[i] == vertex)
				{
					uniqueIndex = i;
					found = true;
					break;
				}
			if(!found)
				vertices.push_back(vertex);
			indices.push_back(uniqueIndex);
		}

	std::ofstream file;
	file.open(output);
	file << "struct ModelVertex {" << std::endl;
	file << "float position[3];" << std::endl;
	if(isNormal)
		file << "float normal[3];" << std::endl;
	if(isUv)
		file << "float uv[2];" << std::endl;
	file << "};" << std::endl << std::endl;

	file << "struct ModelVertex const modelVertices[" << vertices.size() << "] = {" << std::endl;
	file.unsetf ( std::ios::floatfield );      
	file.precision(5);
	file.setf( std::ios::fixed, std:: ios::floatfield );
	for(const auto& vertex : vertices)
	{
		file << "{ { " << vertex.position[0] << "f, " << vertex.position[1] << "f, " << vertex.position[2] << "f }, ";
		if(isNormal)
			file << "{ " << vertex.normal[0] << "f, " << vertex.normal[1] << "f, " << vertex.normal[2] << "f }, ";
		if(isUv)
			file << "{ " << vertex.uv[0] << "f, " << vertex.uv[1] << "f } ";
		file << " }," << std::endl;
	}
	file << " };" << std::endl << std::endl;

	file << "const unsigned int modelIndices[" << indices.size() << "] = {" << std::endl;
	unsigned int i = 0;
	for(const auto& index : indices)
	{
		file << index << ", ";
		i++;
		if((i % 3) == 0)
			file << std::endl;
	}	
	file << "};";	

	file.close();
#endif
}

int main(int argc, char **argv)
{
	if(argc != 3)
	{
		std::cerr << "Wrong number of arguments, just give me the the name of input and output file. Example: cow.gltf cow.bin" << std::endl;
		return EXIT_FAILURE;
	}

	try
	{
		convert(std::string(argv[1]), std::string(argv[2]));
	}
	catch(const std::exception &e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}
	
	return EXIT_SUCCESS;
}
