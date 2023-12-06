#include "Sample.h"

bool Sample::BuildFloor(BaseObject* obj, UINT width, UINT height)
{
	float cellWidth = 2.0f;

	obj->_vertices.resize(width * height);
	for (int row = 0; row < height; row++)
	{
		for (int col = 0; col < width; col++)
		{
			obj->_vertices[row * width + col].p = {
				(float)(col - float(width / 2.0)) * cellWidth,
				0,
				(float)(float(height / 2.0) - row) * cellWidth
			};
			obj->_vertices[row * width + col].c = { 1, 1, 1, 1 };
			obj->_vertices[row * width + col].t = {
				((float)col / (float)(width - 1)) * (float)(width - 1),
				((float)row / (float)(height - 1)) * (float)(height - 1)
			};
		}
	}

	obj->_indices.resize((width - 1) * (height - 1) * 2.0f * 3.0f);
	int idx = 0;
	for (int row = 0; row < height - 1; row++)
	{
		for (int col = 0; col < width - 1; col++)
		{
			int newRow = row + 1;

			obj->_indices[idx + 0] = row * width + col;
			obj->_indices[idx + 1] = obj->_indices[idx + 0] + 1;
			obj->_indices[idx + 2] = newRow * width + col;

			obj->_indices[idx + 3] = obj->_indices[idx + 2];
			obj->_indices[idx + 4] = obj->_indices[idx + 1];
			obj->_indices[idx + 5] = obj->_indices[idx + 3] + 1;

			idx += 6;
		}
	}

	return true;
}

bool Sample::Init()
{
	W_STR defaultFbxDir = L"../../resource/fbx/";
	W_STR shaderFilename = L"../../resource/shader/skinningShader.txt";

	FbxModel* fbxModel = new FbxModel;
	fbxModel->Init();
	fbxModel->Load(_immediateContext.Get(), "../../resource/fbx/Man.fbx");
	fbxModel->CreateConstantBuffer(_device.Get());
	_models.push_back(fbxModel);

	fbxModel = new FbxModel;
	fbxModel->Init();
	fbxModel->Load(_immediateContext.Get(), "../../resource/fbx/Swat.fbx");
	fbxModel->CreateConstantBuffer(_device.Get());
	_swatModels.push_back(fbxModel);
	_models.push_back(fbxModel);

	fbxModel = new FbxModel;
	fbxModel->Init();
	fbxModel->Load(_immediateContext.Get(), "../../resource/fbx/Swat@rifle_aiming_idle.fbx");
	fbxModel->CreateConstantBuffer(_device.Get());
	_swatModels.push_back(fbxModel);
	_models.push_back(fbxModel);

	fbxModel = new FbxModel;
	fbxModel->Init();
	fbxModel->Load(_immediateContext.Get(), "../../resource/fbx/Swat@walking.fbx");
	fbxModel->CreateConstantBuffer(_device.Get());
	_swatModels.push_back(fbxModel);
	_models.push_back(fbxModel);

	fbxModel = new FbxModel;
	fbxModel->Init();
	fbxModel->Load(_immediateContext.Get(), "../../resource/fbx/Swat@rifle_jump.fbx");
	fbxModel->CreateConstantBuffer(_device.Get());
	_swatModels.push_back(fbxModel);
	_models.push_back(fbxModel);


	for (FbxModel* model : _models)
	{
		for (int i = 0; i < model->_renderObjects.size(); i++)
		{
			FbxObj* object = model->_renderObjects[i];
			object->Create(
				_device.Get(), _immediateContext.Get(),
				shaderFilename,
				defaultFbxDir + object->_name,
				"VSMain", "PSTexture"
			);
		}
	}

	_basicMan = new Character(_models[0]);
	_basicMan->AddState(L"walking", nullptr, true, 61, 91);
	_basicMan->AddState(L"jumping", nullptr, false, 120, 225);
	_basicMan->SetState(L"walking");
	_basicMan->_worldMat._41 = -2;
	_basicMan->CreateConstantBuffer(_device.Get());
	_characters.push_back(_basicMan);

	_swatMan = new Character(_swatModels[0]);
	_swatMan->AddState(L"idle", _swatModels[1], true, 0, 70);
	_swatMan->AddState(L"walk", _swatModels[2], true, 0, 60);
	_swatMan->AddState(L"jump", _swatModels[3], true, 0, 50);
	_swatMan->SetState(L"idle");
	_swatMan->CreateConstantBuffer(_device.Get());
	_characters.push_back(_swatMan);

	_floor = new BaseObject;
	BuildFloor(_floor, 100, 100);
	_floor->Create(
		_device.Get(), _immediateContext.Get(),
		L"../../resource/shader/default3DShader.txt",
		L"../../resource/map/grass.bmp",
		"VSMain", "PSTexture"
	);

	_cam = new Camera;
	_cam->Create(_device.Get(), _immediateContext.Get());
	_cam->CreateViewMatrix(Vector(-3.65, 1.15, -1.95), Vector(0, 1.3, 0), Vector(0, 1, 0));
	_cam->CreateProjMatrix(1.0f, 10000.0f, PI * 0.25f,
		(float)g_rectClient.right / (float)g_rectClient.bottom);
	
	return true;
}

bool Sample::Frame()
{
	_cam->Frame(); 

	_basicMan->UpdateState(0x31);
	_swatMan->UpdateState(0x32);

	for (Character* character : _characters)
	{
		character->UpdateFrame();
	}

	return true;
}

bool Sample::Render()
{
	float color[4] = { 0.387f, 0.440f, 0.450f, 1.0f };
	_immediateContext->ClearRenderTargetView(_rtv.Get(), color);

	// render fbx models
	W_STR text;
	for (int i = 0; i < _characters.size(); i++)
	{
		text += mtw("[ Player ") + std::to_wstring(i) + mtw(" ] ") + _characters[i]->_currState + mtw("\n");
		_characters[i]->SetTransformationMatrix(nullptr, &_cam->_viewMat, &_cam->_projMat);
		_characters[i]->Render();
	}
	_writer.Draw(5, 90, text, _writer._textColor);

	_floor->SetTransformationMatrix(nullptr, &_cam->_viewMat, &_cam->_projMat);
	_floor->Render();

	// camera information rendering
	text = mtw("Camera position:\n") + std::to_wstring(_cam->_pos.x) +
		L"   " + std::to_wstring(_cam->_pos.y) +
		L"   " + std::to_wstring(_cam->_pos.z);
	_writer._textContent = text.c_str();
	
	T_STR str;
	TCHAR pBuffer[256];
	memset(pBuffer, 0, sizeof(TCHAR) * 256);
	_stprintf_s(pBuffer, _T("Look:%10.4f,%10.4f,%10.4f \n"), _cam->_look.x, _cam->_look.y, _cam->_look.z);
	str += pBuffer;

	memset(pBuffer, 0, sizeof(TCHAR) * 256);
	_stprintf_s(pBuffer, _T("Up:%10.4f,%10.4f,%10.4f \n"), _cam->_up.x, _cam->_up.y, _cam->_up.z);
	str += pBuffer;

	memset(pBuffer, 0, sizeof(TCHAR) * 256);
	_stprintf_s(pBuffer, _T("Right:%10.4f,%10.4f,%10.4f "), _cam->_right.x, _cam->_right.y, _cam->_right.z);
	str += pBuffer;

	_writer.Draw(1280-370, 720-90, str, _writer._textColor);
	return true;
}

bool Sample::Release()
{
	for (FbxModel* model : _models)
	{
		model->Release();
		delete model;
	}
	for (Character* character : _characters)
	{
		character->Release();
		delete character;
	}
	if (_cam) _cam->Release();
	delete _cam;
	if (_floor) _floor->Release();
	delete _floor;
	return true;
}

GAME_RUN(Fbx Character Viewer, 1280, 720)