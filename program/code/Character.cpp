#include "Character.h"

HRESULT Character::CreateConstantBuffer(ID3D11Device* device)
{
	D3D11_BUFFER_DESC bufferDesc;
	ZeroMemory(&bufferDesc, sizeof(bufferDesc));
	bufferDesc.ByteWidth	= sizeof(VSBoneConstantData) * 1; 
	bufferDesc.Usage		= D3D11_USAGE_DEFAULT;
	bufferDesc.BindFlags	= D3D11_BIND_CONSTANT_BUFFER;

	D3D11_SUBRESOURCE_DATA  subRsrcData;
	ZeroMemory(&subRsrcData, sizeof(subRsrcData));
	subRsrcData.pSysMem = &_currBoneData;
	HRESULT hr = device->CreateBuffer(
		&bufferDesc,
		&subRsrcData,
		&_boneConstantBuffer);

	_boneConstantBuffers.resize(_fbxModel->_renderObjects.size());
	_boneConstantDatas.resize(_fbxModel->_renderObjects.size());
	for (int i = 0; i < _fbxModel->_renderObjects.size(); i++)
	{
		hr = device->CreateBuffer(
			&bufferDesc,
			&subRsrcData,
			&_boneConstantBuffers[i]);
	}

	return hr;
}

void Character::SetState(W_STR stateName)
{
	if (_currState != stateName)
	{
		_currState = stateName;
		_currScene = _stateInfos.find(stateName)->second;

	}
	_currFrame = _currScene.startFrame;
}

void Character::AddState(W_STR stateName, FbxModel* stateModel, 
	bool isLooped, UINT startFrame, UINT endFrame)
{
	AnimScene stateScene;
	if (stateModel != nullptr)
	{
		stateScene = stateModel->_animScene;
		_stateModels.insert(std::make_pair(stateName, stateModel));
	}
	else
	{
		stateScene = _fbxModel->_animScene;
	}
	
	stateScene.isLooped = isLooped;
	if (int(startFrame) > -1)	stateScene.startFrame = startFrame;
	if (int(endFrame) > -1)		stateScene.endFrame = endFrame;

	_stateInfos.insert(std::make_pair(stateName, stateScene));
}

bool Character::UpdateFrame()
{
	_currFrame = _currFrame + g_secondPerFrame * _animSpeed * _currScene.frameRate;

	if (_currFrame > _currScene.endFrame ||
		_currFrame < _currScene.startFrame)
	{
		_currFrame = min(_currFrame, _currScene.endFrame);
		_currFrame = max(_currFrame, _currScene.startFrame);
	}

	if (_currFrame >= _currScene.endFrame)
	{
		if (_currScene.isLooped)
		{
			_currFrame = _currScene.startFrame;
		}
		else
		{
			SetState(_stateInfos.begin()->first);
		}
	}

	if (!_stateModels.empty())
	{
		FbxModel* currStateModel = _stateModels.find(_currState)->second;
		currStateModel->UpdateSkeleton(_currFrame, _currBoneData);
		_fbxModel->UpdateSkinning(_currBoneData, _boneConstantDatas);
	}
	else
	{
		_fbxModel->UpdateSkeleton(_currFrame, _currBoneData);
		_fbxModel->UpdateSkinning(_currBoneData, _boneConstantDatas);
	}
	for (int i = 0; i < _boneConstantBuffers.size(); i++)
	{
		_fbxModel->_immediateContext->UpdateSubresource(
			_boneConstantBuffers[i], 0, nullptr,
			&_boneConstantDatas[i], 0, 0);
	}

	for (int i = 0; i < _fbxModel->_objects.size(); i++)
	{
		_currBoneData.animMat[i] = Matrix::XMMatrixTranspose(_currBoneData.animMat[i]);
	}
	_fbxModel->_immediateContext->UpdateSubresource(
		_boneConstantBuffer, 0, nullptr, &_currBoneData, 0, 0);

	return true;
}

void Character::UpdateState(DWORD key)
{
	if (S_Input.GetKey(key) == KeyState::KEY_DOWN)
	{
		auto nextStateIter = std::next(_stateInfos.find(_currState));
		if (nextStateIter == _stateInfos.end()) 
			nextStateIter = _stateInfos.begin();
		SetState(nextStateIter->first);
	}
}

void Character::SetTransformationMatrix(Matrix* worldMat, Matrix* viewMat, Matrix* projMat)
{
	if (worldMat)	_worldMat = *worldMat;
	if (viewMat)	_viewMat = *viewMat;
	if (projMat)	_projMat = *projMat;
}

bool Character::Render()
{
	_fbxModel->_immediateContext->VSSetConstantBuffers(1, 1, &_boneConstantBuffer);
	for (int iMesh = 0; iMesh < _fbxModel->_renderObjects.size(); iMesh++)
	{
		if (_fbxModel->_renderObjects[iMesh]->_isSkinned)
		{
			_fbxModel->_immediateContext->
				VSSetConstantBuffers(1, 1, &_boneConstantBuffers[iMesh]);
		}
		_fbxModel->_renderObjects[iMesh]
			->SetTransformationMatrix(&_worldMat, &_viewMat, &_projMat);
		_fbxModel->_renderObjects[iMesh]->Render();
	}
	return true;
}

bool Character::Release()
{
	if (_boneConstantBuffer) _boneConstantBuffer->Release();
	for (auto buffer : _boneConstantBuffers)
	{
		buffer->Release();
	}
	return true;
}
