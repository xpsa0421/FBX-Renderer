#pragma once
#include "Std.h"
#include "FbxObj.h"
#pragma comment(lib, "libfbxsdk-md.lib")
#pragma comment(lib, "libxml2-md.lib")
#pragma comment(lib, "zlib-md.lib")

class FbxModel
{
public:
	ComPtr<ID3D11DeviceContext>	_immediateContext = nullptr;
	VSBoneConstantData			_boneConstantData;
	ID3D11Buffer*				_boneConstantBuffer;
public:
	FbxManager*		_fbxManager;
	FbxImporter*	_fbxImporter;
	FbxScene*		_fbxScene;
	FbxNode*		_fbxRootNode;
public:
	AnimScene		_animScene;
	float			_currFrame		= 0;
	float			_animInverse	= 1.0f;
	float			_animSpeed		= 1.0f;
public:
	std::vector<FbxObjSkinning*>		_objects;
	std::vector<FbxObjSkinning*>		_renderObjects;
	std::map<FbxNode*, FbxObjSkinning*>	_objectMap;
	std::map<FbxNode*, UINT>			_objectIDMap;
public:
	HRESULT		CreateConstantBuffer(ID3D11Device* device);
	bool		Load(ID3D11DeviceContext* context, C_STR filename);
	void		PreProcess(FbxNode* node);
	void		ParseMesh(FbxMesh* mesh, FbxObjSkinning* object);
	bool		ParseMeshSkinning(FbxMesh* mesh, FbxObjSkinning* object);
	void		InitAnimation();
	void		LoadAnimation(FbxLongLong frame, FbxTime time);
public:
	W_STR			AdjustTexFilename(STR filepath);
	static Matrix	ConvertFromFbxMatrix(FbxAMatrix& fbxMatrix);
public:
	int			GetSubMaterialIndex(int iPoly, FbxLayerElementMaterial* materialSet);
	FbxColor	GetColour(
		FbxMesh* mesh, FbxLayerElementVertexColor* vertexColourSet,
		int iVertex, int iColour);
	FbxVector2	GetTextureUV(
		FbxMesh* mesh, FbxLayerElementUV* vertexUVSet,
		int iVertex, int iUV);
	FbxVector4	GetNormal(
		FbxMesh* mesh, FbxLayerElementNormal* vertexNormalSet,
		int iVertex, int iColour);
public:
	bool	Init();
	bool	Frame();
	bool	UpdateFrame();
	void	UpdateSkeleton(float frame, VSBoneConstantData& boneAnimMatData);
	void	UpdateSkinning(VSBoneConstantData& boneAnimMatData, 
				std::vector<VSBoneConstantData>& boneConstantData);
	bool	Render();
	bool	Release();
};