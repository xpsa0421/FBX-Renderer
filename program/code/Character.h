#pragma once
#include "FbxModel.h"
#include "Input.h"

class Character	
{
public:
	FbxModel*	_fbxModel		= nullptr;
	W_STR		_currState		= L"";
public:
	Matrix		_worldMat;
	Matrix		_viewMat;
	Matrix		_projMat;
public:
	AnimScene	_currScene;
	float		_currFrame		= 0;
	float		_animSpeed		= 1.0f;
public:
	ID3D11Buffer*						_boneConstantBuffer;
	VSBoneConstantData					_currBoneData;
	std::vector<VSBoneConstantData>		_boneConstantDatas;
	std::vector<ID3D11Buffer*>			_boneConstantBuffers;
public:
	std::map<W_STR, FbxModel*>	_stateModels;
	std::map<W_STR, AnimScene>	_stateInfos;
public:
	HRESULT	CreateConstantBuffer(ID3D11Device* device);
	void	SetState(W_STR stateName);
	void	AddState(W_STR stateName, FbxModel* stateModel = nullptr, bool isLooped = false, UINT startFrame = -1, UINT endFrame = -1);
	void	UpdateState(DWORD key);
	bool	UpdateFrame();
	void	SetTransformationMatrix(Matrix* worldMat, Matrix* viewMat, Matrix* projMat);
	bool	Render();
	bool	Release();
public:
	Character() {};
	Character(FbxModel* fbxModel) {
		_fbxModel = fbxModel;
		_currScene = fbxModel->_animScene;
	}
};

