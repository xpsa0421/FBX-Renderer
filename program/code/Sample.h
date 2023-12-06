#pragma once
#include "GameCore.h"
#include "FbxModel.h"
#include "Camera.h"
#include "Character.h"

class Sample : public GameCore
{
public:
	Camera*					_cam;
	BaseObject*				_floor;
	Character*				_basicMan;
	Character*				_swatMan;
	std::vector<FbxModel*>	_swatModels;
	std::vector<FbxModel*>	_models;
	std::vector<Character*> _characters;
	
public:
	virtual bool		Init()		override;
	virtual bool		Frame()		override;
	virtual bool		Render()	override;
	virtual bool		Release()	override;
public:
	bool BuildFloor(BaseObject* obj, UINT width, UINT height);
};

