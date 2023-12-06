#pragma once
#include <fbxsdk.h>
#include "BaseObject.h"

struct AnimTrack
{
	UINT		frame;			// time
	Matrix		animMat;		// self * parent
	Matrix		selfAnimMat;	// self * parent * inv(parent)
	Vector		t;
	Vector		s;
	Quaternion	r;				// self 
};

struct AnimScene
{
	FbxTime::EMode	timeMode;		// universal timeMode
	UINT			startFrame;
	UINT			endFrame;
	float			tickPerFrame;	// 160
	float			frameRate;		// 30fps
	bool			isLooped;

	AnimScene() {};
	AnimScene(UINT startFrame, UINT endFrame, bool isLooped)
	{
		this->startFrame = startFrame;
		this->endFrame = endFrame;
		this->isLooped = isLooped;
	};
};

struct SkinWeight
{
	std::vector<int>	indices;
	std::vector<float>	weights;
	void Insert(int iBone, float weight)
	{
		for (int i = 0; i < indices.size(); i++)
		{
			if (weight > weights[i])
			{
				for (int j = indices.size() - 1; j > i; --j)
				{
					indices[j] = indices[j - 1];
					weights[j] = weights[j - 1];
				}
				indices[i] = iBone;
				weights[i] = weight;
				break;
			}
		}
	}
	SkinWeight()
	{
		indices.resize(8);
		weights.resize(8);
	}
};

class FbxObj : public BaseObject
{
public:
	FbxNode*	_fbxNode		= nullptr;
	FbxNode*	_fbxParentNode	= nullptr;
	FbxObj*		_fbxParentObj	= nullptr;
public:
	std::vector<FbxObj*>				_fbxChildren;
	std::vector<W_STR>					_subTextureNames;
	std::vector<std::vector<Vertex>>	_subVertices;
	std::vector<ID3D11Buffer*>			_subVertexBuffers;
	std::vector<Texture*>				_subTextures;
public:
	Matrix					_animMat;
	std::vector<AnimTrack>	_animTracks;
public:
	void		SetParent(FbxObj* parentNode);
	Matrix		Interpolate(float frame, AnimScene scene);
public:
	virtual void		CreateVertexData()				override;
	virtual HRESULT		CreateVertexBuffer()			override;
	virtual HRESULT		CreateIndexBuffer()				override;
	virtual bool		LoadTexture(W_STR texFileName)	override;
public:
	virtual bool		PostRender()					override;
	virtual bool		Release()						override;
public:
	FbxObj();
	FbxObj(W_STR name, FbxNode* node, FbxNode* parentNode);
};


class FbxObjSkinning : public FbxObj
{
public:
	bool	_isSkinned = false;
public:
	std::vector<VertexIW>				_verticesIW;
	ID3D11Buffer*						_vertexIWBuffer;
	std::vector<std::vector<VertexIW>>	_subVerticesIW;
	std::vector<ID3D11Buffer*>			_subVertexIWBuffers;
public:
	UINT					_boneIndex;
	ID3D11Buffer*			_boneConstantBuffer;
	VSBoneConstantData		_boneConstantData;
	std::vector<SkinWeight>	_weights;
	std::map<UINT, Matrix>	_boneMatPalette;
public:
	HRESULT		CreateConstantBuffer()	override;
	HRESULT		CreateVertexLayout()	override;
	HRESULT		CreateVertexBuffer()	override;
public:
	bool		PostRender()			override;
	bool		Release()				override;
public:
	FbxObjSkinning();
	FbxObjSkinning(UINT iBone, W_STR name, FbxNode* node, FbxNode* parentNode);
};