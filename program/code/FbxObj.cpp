#include "FbxObj.h"

FbxObj::FbxObj()
{
}

FbxObj::FbxObj(W_STR name, FbxNode* node, FbxNode* parentNode)
{
	_name = name;
	_fbxNode = node;
	_fbxParentNode = parentNode;
}

void FbxObj::SetParent(FbxObj* parentObject)
{
	parentObject->_fbxChildren.push_back(this);
	_fbxParentObj = parentObject;
}

Matrix FbxObj::Interpolate(float frame, AnimScene scene)
{
	Matrix identityMat;
	if (_animTracks.empty()) return identityMat;

	AnimTrack currTrack = _animTracks[max(scene.startFrame, frame)];
	AnimTrack nextTrack = _animTracks[min(scene.endFrame-1, frame + 1)];
	if (currTrack.frame == nextTrack.frame)
	{
		return _animTracks[frame].animMat;
	}

	float t = (frame - currTrack.frame) / (nextTrack.frame - currTrack.frame);
	Vector trans	= Vector::XMVectorLerp(currTrack.t, nextTrack.t, t);
	Vector scaling	= Vector::XMVectorLerp(currTrack.s, nextTrack.s, t);
	Quaternion rotQ	= Quaternion::XMQuaternionSlerp(currTrack.r, nextTrack.r, t);

	Matrix scaleMat = Matrix::XMMatrixScaling(scaling.x, scaling.y, scaling.z);
	Matrix rotMat	= Matrix::XMMatrixRotationQuaternion(rotQ);
	
	Matrix ret		= Matrix::XMMatrixMultiply(scaleMat, rotMat);
	ret._41 = trans.x;	ret._42 = trans.y;	ret._43 = trans.z;

	return ret;
}

void FbxObj::CreateVertexData()
{
	return;
}

HRESULT	FbxObj::CreateVertexBuffer()
{
	HRESULT hr = S_OK;
	if (!_subVertices.empty())
	{
		_subVertexBuffers.resize(_subVertices.size());
		for (int i = 0; i < _subVertices.size(); i++)
		{
			if (!_subVertices[i].empty())
			{
				_subVertexBuffers[i] = 
					BaseObject::CreateVertexBuffer(_device.Get(), 
					&_subVertices[i].at(0), _subVertices[i].size(), sizeof(Vertex));
			}
		}
	}
	else
	{
		hr = BaseObject::CreateVertexBuffer();
	}
	return hr;
}

HRESULT FbxObj::CreateIndexBuffer()
{
	return S_OK;
}

bool FbxObj::LoadTexture(W_STR texFileName)
{
	if (!_subTextureNames.empty())
	{
		_subTextures.resize(_subTextureNames.size());
		for (int i = 0; i < _subTextureNames.size(); i++)
		{
			W_STR subTexFileName = L"../../resource/fbx/" + _subTextureNames[i];
			_subTextures[i] = S_TexManager.LoadTexture(subTexFileName);
		}
	}
	else
	{
		_texture = S_TexManager.LoadTexture(texFileName);
		if (_texture)
		{
			_textureSRV = _texture->_textureSRV;
			return true;
		}
	}
	return false;
}
		
bool FbxObj::PostRender()
{
	if (_indexBuffer == nullptr)
	{
		if (!_subVertices.empty())
		{
			for (int i = 0; i < _subVertexBuffers.size(); i++)
			{
				if (_subVertices[i].empty()) continue;

				UINT stride = sizeof(Vertex);
				UINT offset = 0;
				_immediateContext->IASetVertexBuffers(0, 1, &_subVertexBuffers[i], &stride, &offset);
				if (_subTextures[i])
				{
					_immediateContext->PSSetShaderResources(0, 1, &_subTextures[i]->_textureSRV);
				}
				_immediateContext->Draw(_subVertices[i].size(), 0);
			}
		}
		else
		{
			_immediateContext->Draw(_vertices.size(), 0);
		}
	}
	else
	{
		_immediateContext->DrawIndexed(_numFaces * 3, 0, 0);
	}
	return true;
}

bool FbxObj::Release()
{
	for (int i = 0; i < _subVertexBuffers.size(); i++)
	{
		if (_subVertexBuffers[i]) _subVertexBuffers[i]->Release();
	}
	return BaseObject::Release();
}


//---------------------------------------------------------------------
// FBX OBJECT WITH BONE AND SKINNING
//---------------------------------------------------------------------


FbxObjSkinning::FbxObjSkinning()
{
}

FbxObjSkinning::FbxObjSkinning(UINT iBone, W_STR name, FbxNode* node, FbxNode* parentNode)
{
	_boneIndex = iBone;
	_name = name;
	_fbxNode = node;
	_fbxParentNode = parentNode;
}

HRESULT FbxObjSkinning::CreateConstantBuffer()
{
	BaseObject::CreateConstantBuffer();

	D3D11_BUFFER_DESC bufferDesc;
	ZeroMemory(&bufferDesc, sizeof(bufferDesc));
	bufferDesc.ByteWidth = sizeof(VSBoneConstantData) * 1;
	bufferDesc.Usage = D3D11_USAGE_DEFAULT;
	bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	D3D11_SUBRESOURCE_DATA subRsrcData;
	ZeroMemory(&subRsrcData, sizeof(subRsrcData));
	subRsrcData.pSysMem = &_boneConstantBuffer;

	HRESULT result = _device->CreateBuffer(
		&bufferDesc,
		&subRsrcData,
		&_boneConstantBuffer
	);
	return result;
}

HRESULT FbxObjSkinning::CreateVertexLayout()
{
	_ASSERT(_shader->_VSCode);

	D3D11_INPUT_ELEMENT_DESC elementDesc[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{ "TEXTURE", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 40, D3D11_INPUT_PER_VERTEX_DATA, 0},
		
		{ "INDEX", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{ "WEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D11_INPUT_PER_VERTEX_DATA, 0},
	};

	HRESULT result = _device->CreateInputLayout(
		elementDesc,
		sizeof(elementDesc) / sizeof(elementDesc[0]),
		_shader->_VSCode->GetBufferPointer(),
		_shader->_VSCode->GetBufferSize(),
		&_vertexLayout
	);
	return result;
}

HRESULT FbxObjSkinning::CreateVertexBuffer()
{
	HRESULT result = FbxObj::CreateVertexBuffer();

	if (!_subVerticesIW.empty())
	{
		_subVertexIWBuffers.resize(_subVerticesIW.size());
		for (int i = 0; i < _subVerticesIW.size(); i++)
		{
			if (!_subVerticesIW[i].empty())
			{
				_subVertexIWBuffers[i] = 
					BaseObject::CreateVertexBuffer(_device.Get(),
					&_subVerticesIW[i].at(0), _subVerticesIW[i].size(), sizeof(VertexIW));
			}
		}
	}
	else
	{
		_vertexIWBuffer =
			BaseObject::CreateVertexBuffer(_device.Get(),
			&_verticesIW.at(0), _verticesIW.size(), sizeof(VertexIW));
	}

	return result;
}

bool FbxObjSkinning::PostRender()
{
	if (!_indexBuffer)
	{
		if (!_subVertices.empty())
		{
			for (int i = 0; i < _subVertexBuffers.size(); i++)
			{
				if (_subVertices[i].empty()) continue;

				UINT strides[2]				= { sizeof(Vertex), sizeof(VertexIW) };
				UINT offsets[2]				= { 0, 0 };
				ID3D11Buffer* buffers[2]	= {_subVertexBuffers[i], _subVertexIWBuffers[i]};
				_immediateContext->IASetVertexBuffers(0, 2, buffers, strides, offsets);
				
				if (!_subTextures.empty() && _subTextures[i])
				{
					_immediateContext->PSSetShaderResources(0, 1, &_subTextures[i]->_textureSRV);
				}
				_immediateContext->Draw(_subVertices[i].size(), 0);
			}
		}
		else
		{
			UINT strides[2] = { sizeof(Vertex), sizeof(VertexIW) };
			UINT offsets[2] = { 0, 0 };
			ID3D11Buffer* buffers[2] = { _vertexBuffer.Get(), _vertexIWBuffer };
			_immediateContext->IASetVertexBuffers(0, 2, buffers, strides, offsets);
			_immediateContext->Draw(_vertices.size(), 0);
		}
	}
	else
	{
		_immediateContext->DrawIndexed(_numFaces * 3, 0, 0);
	}

	return true;
}

bool FbxObjSkinning::Release()
{
	if (_boneConstantBuffer) _boneConstantBuffer->Release();
	if (_vertexIWBuffer) _vertexIWBuffer->Release();
	for (auto buffer : _subVertexIWBuffers)
	{
		if (buffer) buffer->Release();
	}
	FbxObj::Release();
	return true;
}