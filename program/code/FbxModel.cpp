#include "FbxModel.h"

HRESULT FbxModel::CreateConstantBuffer(ID3D11Device* device)
{
	D3D11_BUFFER_DESC bufferDesc;
	ZeroMemory(&bufferDesc, sizeof(bufferDesc));
	bufferDesc.ByteWidth = sizeof(VSBoneConstantData) * 1;
	bufferDesc.Usage = D3D11_USAGE_DEFAULT;
	bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	D3D11_SUBRESOURCE_DATA subRsrcData;
	ZeroMemory(&subRsrcData, sizeof(subRsrcData));
	subRsrcData.pSysMem = &_boneConstantData;

	HRESULT result = device->CreateBuffer(
		&bufferDesc,
		&subRsrcData,
		&_boneConstantBuffer
	);
	return result;
}

bool FbxModel::Load(ID3D11DeviceContext* context, C_STR filename)
{
	_immediateContext = context;
	_fbxImporter->Initialize(filename.c_str());
	_fbxImporter->Import(_fbxScene);

	FbxAxisSystem SceneAxisSystem = _fbxScene->GetGlobalSettings().GetAxisSystem();
	FbxSystemUnit::m.ConvertScene(_fbxScene);
	FbxAxisSystem::MayaZUp.ConvertScene(_fbxScene);
	InitAnimation();

	_fbxRootNode = _fbxScene->GetRootNode();
	PreProcess(_fbxRootNode);

	for (FbxObjSkinning* object : _objects)
	{
		FbxMesh* mesh = object->_fbxNode->GetMesh();
		if (mesh)
		{
			ParseMesh(mesh, object);
		}
	}

	FbxTime time;
	for (FbxLongLong frame = _animScene.startFrame; frame <= _animScene.endFrame; frame++)
	{
		time.SetFrame(frame, _animScene.timeMode);
		LoadAnimation(frame, time);
	}

	return true;
}

void FbxModel::PreProcess(FbxNode* node)
{
	if (node == nullptr) return;
	if (node && (node->GetCamera() || node->GetLight()))
	{
		return;
	}

	// only process mesh, skeleton, null nodes
	FbxObjSkinning* object = new FbxObjSkinning(
		_objects.size(), mtw(node->GetName()), node, node->GetParent());

	if (object->_fbxParentNode)
	{
		auto iter = _objectMap.find(object->_fbxParentNode);
		object->SetParent(iter->second);
	}
	_objects.push_back(object);
	_objectMap.insert(std::make_pair(node, object));
	_objectIDMap.insert(std::make_pair(node, object->_boneIndex));

	int numChildren = node->GetChildCount();
	for (int i = 0; i < numChildren; i++)
	{
		FbxNode* childNode = node->GetChild(i);
		FbxNodeAttribute::EType type = childNode->GetNodeAttribute()->GetAttributeType();
		if (type == FbxNodeAttribute::eMesh ||
			type == FbxNodeAttribute::eSkeleton ||
			type == FbxNodeAttribute::eNull)
		{
			PreProcess(childNode);
		}
	}
}

void FbxModel::InitAnimation()
{
	/** The Animation stack is a collection of animation layers. The Fbx document can have one or
	  * more animation stacks. Each stack can be viewed as one "take". The "stack" terminology comes from the fact that
	  * the object contains 1 to n animation layers that are evaluated according to their blending modes
	  * to produce a resulting animation for a given attribute.
	  */
	FbxTime::EMode	timeMode		= FbxTime::GetGlobalTimeMode();
	FbxAnimStack*	animStack		= _fbxScene->GetSrcObject<FbxAnimStack>(0);
	FbxLongLong		startFrameCount = 0;
	FbxLongLong		endFrameCount	= 0;
	float			frameRate;

	if (animStack)
	{
		FbxTakeInfo*	stack			= _fbxScene->GetTakeInfo(animStack->GetName());
		FbxTimeSpan		localTimeSpan	= stack->mLocalTimeSpan;
		FbxTime			startTime		= localTimeSpan.GetStart();
		FbxTime			endTime			= localTimeSpan.GetStop();
		FbxTime			duration		= localTimeSpan.GetDuration();
		FbxTime::SetGlobalTimeMode(FbxTime::eFrames30);	

		frameRate		= FbxTime::GetFrameRate(timeMode);
		startFrameCount = startTime.GetFrameCount(timeMode);
		endFrameCount	= endTime.GetFrameCount(timeMode);
	}

	_animScene.timeMode		= timeMode;
	_animScene.startFrame	= startFrameCount;
	_animScene.endFrame		= endFrameCount;
	_animScene.frameRate	= 30.0f;
	_animScene.tickPerFrame = 160;
}

void FbxModel::LoadAnimation(FbxLongLong frame, FbxTime time)
{
	FbxNode*	node;
	AnimTrack	track;
	FbxAMatrix	fbxMatrix;

	for (FbxObjSkinning* object : _objects)
	{
		node = object->_fbxNode;
		track.frame = frame;
		fbxMatrix = node->EvaluateGlobalTransform(time);
		track.animMat = FbxModel::ConvertFromFbxMatrix(fbxMatrix);
		Matrix::XMMatrixDecompose(track.s, track.r, track.t, track.animMat);
		if (node == _fbxRootNode) track.t = { 0,0,0 };
		object->_animTracks.push_back(track);
	}
}

void FbxModel::ParseMesh(FbxMesh* mesh, FbxObjSkinning* object)
{
	object->_isSkinned = ParseMeshSkinning(mesh, object);
	FbxNode* node = object->_fbxNode;
	
	FbxVector4 translation	= node->GetGeometricTranslation(FbxNode::eSourcePivot);
	FbxVector4 rotation		= node->GetGeometricRotation(FbxNode::eSourcePivot);
	FbxVector4 scaling		= node->GetGeometricScaling(FbxNode::eSourcePivot);
	FbxAMatrix geom			= FbxAMatrix(translation, rotation, scaling);
	
	FbxAMatrix normalLocalM = geom;
	normalLocalM = geom.Inverse();
	normalLocalM = geom.Transpose(); 

	FbxLayer* layer = mesh->GetLayer(0);
	FbxLayerElementUV*			vertexUVSet			= layer->GetUVs();
	FbxLayerElementVertexColor* vertexColourSet		= layer->GetVertexColors();
	FbxLayerElementNormal*		vertexNormalSet		= layer->GetNormals();
	FbxLayerElementMaterial*	materialSet			= layer->GetMaterials();
	
	STR filePath;
	std::vector<C_STR> texFilePaths;
	int numMaterials = node->GetMaterialCount();
	texFilePaths.resize(numMaterials);

	for (int i = 0; i < numMaterials; i++)
	{
		FbxSurfaceMaterial* material = node->GetMaterial(i);
		if (material)
		{
			FbxProperty property = material->FindProperty(FbxSurfaceMaterial::sDiffuse);
			if (property.IsValid())
			{
				const FbxFileTexture* texture = property.GetSrcObject<FbxFileTexture>(0);
				if (texture)
				{
					filePath = texture->GetFileName();
					texFilePaths[i] = filePath;
				}
			}
		}
	}

	if (numMaterials == 1)
	{
		object->_name = AdjustTexFilename(filePath);
	}
	if (numMaterials > 1)
	{
		object->_subVertices.resize(numMaterials);
		object->_subVerticesIW.resize(numMaterials);
		object->_subTextureNames.resize(numMaterials);
		for (int i = 0; i < numMaterials; i++)
		{
			object->_subTextureNames[i] = AdjustTexFilename(texFilePaths[i]);
		}
	}
	
	FbxVector4* vertices = mesh->GetControlPoints();
	int numPolygons = mesh->GetPolygonCount();
	int numFaces = 0;
	int iSubMaterial, iBasePolygon = 0;
	
	for (int iPoly = 0; iPoly < numPolygons; iPoly++)
	{
		int polySize = mesh->GetPolygonSize(iPoly);
		numFaces = polySize - 2;
		if (materialSet)
		{
			iSubMaterial = GetSubMaterialIndex(iPoly, materialSet);
		}
		for (int iFace = 0; iFace < numFaces; iFace++)
		{
			int vertexColourIndices[3] = { 0, iFace + 2, iFace + 1 }; // 0 tocheck
		
			int vertexIndices[3];
			vertexIndices[0] = mesh->GetPolygonVertex(iPoly, 0);
			vertexIndices[1] = mesh->GetPolygonVertex(iPoly, iFace + 2);
			vertexIndices[2] = mesh->GetPolygonVertex(iPoly, iFace + 1);
		
			int uvIndices[3];
			uvIndices[0] = mesh->GetTextureUVIndex(iPoly, 0);
			uvIndices[1] = mesh->GetTextureUVIndex(iPoly, iFace + 2);
			uvIndices[2] = mesh->GetTextureUVIndex(iPoly, iFace + 1);
		
			for (int iIndex = 0; iIndex < 3; iIndex++)
			{
				int iVertex = vertexIndices[iIndex];
				FbxVector4 fbxVertex = vertices[iVertex];
				fbxVertex = geom.MultT(fbxVertex);

				VertexIW vertexIW;
				Vertex vertex;
				vertex.p.x = fbxVertex.mData[0];
				vertex.p.y = fbxVertex.mData[2];
				vertex.p.z = fbxVertex.mData[1];
				vertex.c = Vector4(1, 1, 1, 1);

				if (vertexColourSet)
				{
					FbxColor fbxC = GetColour(
						mesh,
						vertexColourSet,
						iVertex,
						iBasePolygon + vertexColourIndices[iIndex]
					);
					vertex.c.x = fbxC.mRed;
					vertex.c.y = fbxC.mGreen;
					vertex.c.z = fbxC.mBlue;
					vertex.c.w = 1.0f;
				}

				if (vertexUVSet)
				{
					FbxVector2 fbxT = GetTextureUV(
						mesh,
						vertexUVSet,
						iVertex,
						uvIndices[iIndex]
					);
					vertex.t.x = fbxT.mData[0];
					vertex.t.y = 1.0f - fbxT.mData[1];
				}

				if (vertexNormalSet)
				{
					FbxVector4 fbxN = GetNormal(
						mesh,
						vertexNormalSet,
						iVertex,
						iBasePolygon + vertexColourIndices[iIndex]
					);
					fbxN = normalLocalM.MultT(fbxN);
					vertex.n.x = fbxN.mData[0];
					vertex.n.y = fbxN.mData[2];
					vertex.n.z = fbxN.mData[1];
				}

				if (!object->_isSkinned)
				{
					vertexIW.i.x = _objectIDMap.find(node)->second;
					vertexIW.i.y = vertexIW.i.z = vertexIW.i.w = 0;

					vertexIW.w.x = 1.0f;
					vertexIW.w.y = vertexIW.w.z = vertexIW.w.w = 0.0f;
				}
				else
				{
					SkinWeight* weight = &object->_weights[iVertex];
					vertexIW.i.x = weight->indices[0];
					vertexIW.i.y = weight->indices[1];
					vertexIW.i.z = weight->indices[2];
					vertexIW.i.w = weight->indices[3];

					vertexIW.w.x = weight->weights[0];
					vertexIW.w.y = weight->weights[1];
					vertexIW.w.z = weight->weights[2];
					vertexIW.w.w = weight->weights[3];
				}

				if (numMaterials <= 1)
				{
					object->_vertices.push_back(vertex);
					object->_verticesIW.push_back(vertexIW);
				}
				else
				{
					object->_subVertices[iSubMaterial].push_back(vertex);
					object->_subVerticesIW[iSubMaterial].push_back(vertexIW);
				}
			}
		}
		iBasePolygon += polySize;
	}

	_renderObjects.push_back(object);
}

bool FbxModel::ParseMeshSkinning(FbxMesh* mesh, FbxObjSkinning* object)
{
	/**
	* A skin deformer contains clusters (FbxCluster). Each cluster acts on a subset 
	* of the geometry's control points, with different weights.
	* 
	* The link node of a cluster is the node which influences the displacement
    * of the control points. Typically, the link node is the bone a skin is attached to.
	*/
	int numDeformers = mesh->GetDeformerCount(FbxDeformer::eSkin);
	if (numDeformers == 0) return false;

	int numVertices = mesh->GetControlPointsCount();
	object->_weights.resize(numVertices);

	for (int iDeformer = 0; iDeformer < numDeformers; iDeformer++)
	{
		FbxSkin* skin = (FbxSkin*)(mesh->GetDeformer(iDeformer, FbxDeformer::eSkin));

		int numClusters = skin->GetClusterCount();
		for (int iCluster = 0; iCluster < numClusters; iCluster++)
		{
			FbxCluster* cluster = skin->GetCluster(iCluster);
			FbxNode* node = cluster->GetLink();
			int iBone = _objectIDMap.find(node)->second;
			
			// local-to-world space transform of the joint
			FbxAMatrix boneToBindPosMat;
			cluster->GetTransformLinkMatrix(boneToBindPosMat);

			// local-to-world space transform of the mesh vertices
			FbxAMatrix transformMat;
			cluster->GetTransformMatrix(transformMat);

			/*FbxAMatrix globalBindPoseInvMatFbx = transformMat.Inverse() * bindPoseMat;
			Matrix globalBindPoseInvMat = FbxModel::ConvertFromFbxMatrix(globalBindPoseInvMatFbx);
			object->_boneMatPalette.insert(std::make_pair(iBone, globalBindPoseInvMat.XMMatrixInverse()));*/
			FbxAMatrix globalBindPoseInvMatFbx = boneToBindPosMat.Inverse() * transformMat;
			Matrix globalBindPoseInvMat = FbxModel::ConvertFromFbxMatrix(globalBindPoseInvMatFbx);
			object->_boneMatPalette.insert(std::make_pair(iBone, globalBindPoseInvMat)); 

			int		numWeights	= cluster->GetControlPointIndicesCount();
			int*	indices		= cluster->GetControlPointIndices();
			double* weights		= cluster->GetControlPointWeights();
			
			for (int i = 0; i < numWeights; i++)
			{
				int iVertex = indices[i];
				float weight = weights[i];
				object->_weights[iVertex].Insert(iBone, weight);
			}
		}
	}
	return true;
}

/**
 * Split full file path into file name and extension
 * Converts file from TGA to DDS extension
 *
 * @param	filepath the full filepath of a texture image
 * @return	texture filename with its extension
 */
W_STR FbxModel::AdjustTexFilename(STR filepath)
{
	WCHAR drive[MAX_PATH] = { 0, };
	WCHAR dir[MAX_PATH] = { 0, };
	WCHAR filename[MAX_PATH] = { 0, };
	WCHAR ext[MAX_PATH] = { 0, };
	_tsplitpath_s(mtw(filepath).c_str(), drive, dir, filename, ext);
	W_STR ret = ext;
	if (ext == L".tga" || ext == L".TGA")
	{
		OutputDebugString(L"WARNING: TGA file conversion");
		ret = L".dds";
	}
	return filename + ret;
}
/**
 * Converts FbxAMatrix into Matrix
 *
 * @param	fbxMatrix	the matrix in FbxAMatrix format
 * @return	converted Matrix
 */
Matrix FbxModel::ConvertFromFbxMatrix(FbxAMatrix& fbxMatrix)
{
	Matrix fbxMat, mat;
	float*	matArray	= (float*)(&fbxMat);
	double* fbxArray	= (double*)(&fbxMatrix);

	for (int i = 0; i < 16; i++)
	{
		matArray[i] = fbxArray[i];
	}

	mat._11 = fbxMat._11;	mat._12 = fbxMat._13;	mat._13 = fbxMat._12;	mat._14 = 0.0f;
	mat._21 = fbxMat._31;	mat._22 = fbxMat._33;	mat._23 = fbxMat._32;	mat._24 = 0.0f;
	mat._31 = fbxMat._21;	mat._32 = fbxMat._23;	mat._33 = fbxMat._22;	mat._34 = 0.0f;
	mat._41 = fbxMat._41;	mat._42 = fbxMat._43;	mat._43 = fbxMat._42;	mat._44 = 1.0f;
	
	return mat;
}

/**	\enum EMappingMode     Determines how the element is mapped to a surface.
  * - \e eByControlPoint      There will be one mapping coordinate for each surface control point/vertex.
  * - \e eByPolygonVertex     There will be one mapping coordinate for each vertex, for every polygon of which it is a part.
  							This means that a vertex will have as many mapping coordinates as polygons of which it is a part.
  *
  * \enum EReferenceMode   Determines how the mapping information is stored in the array of coordinates.
  * - \e eDirect              This indicates that the mapping information for the n'th element is found in the n'th place of
  							FbxLayerElementTemplate::mDirectArray.
  * - \e eIndexToDirect       This indicates that the FbxLayerElementTemplate::mIndexArray
  							contains, for the n'th element, an index in the FbxLayerElementTemplate::mDirectArray
  							array of mapping elements. eIndexToDirect is usually useful for storing eByPolygonVertex mapping
  							mode elements coordinates.
  */
int FbxModel::GetSubMaterialIndex(int iPoly, FbxLayerElementMaterial* materialSet)
{
	int iSubMaterial = 0;
	if (materialSet)
	{
		switch (materialSet->GetMappingMode())
		{
		case FbxLayerElement::eByPolygon:
		{
			switch (materialSet->GetReferenceMode())
			{
			case FbxLayerElement::eIndex:
			{
				iSubMaterial = iPoly;
				break;
			}
			case FbxLayerElement::eIndexToDirect:
			{
				iSubMaterial = materialSet->GetIndexArray().GetAt(iPoly);
				break;
			}
			}
		}
		default:
			break;
		}
	}
	return iSubMaterial;
}

FbxColor FbxModel::GetColour(
	FbxMesh* mesh, FbxLayerElementVertexColor* vertexColourSet,
	int iVertex, int iColour)
{
	FbxColor fbxColour(1, 1, 1, 1);
	switch (vertexColourSet->GetMappingMode())
	{
	case FbxLayerElementUV::eByControlPoint:
	{
		switch (vertexColourSet->GetReferenceMode())
		{
		case FbxLayerElementUV::eDirect:
		{
			fbxColour = vertexColourSet->GetDirectArray().GetAt(iVertex);
			break;
		}
		case FbxLayerElementUV::eIndexToDirect:
		{
			int idx = vertexColourSet->GetIndexArray().GetAt(iVertex);
			fbxColour = vertexColourSet->GetDirectArray().GetAt(idx);
			break;
		}
		}break;
	}
	case FbxLayerElementUV::eByPolygonVertex:
	{
		switch (vertexColourSet->GetReferenceMode())
		{
		case FbxLayerElementUV::eDirect:
		{
			fbxColour = vertexColourSet->GetDirectArray().GetAt(iColour);
			break;
		}
		case FbxLayerElementUV::eIndexToDirect:
		{
			int idx = vertexColourSet->GetIndexArray().GetAt(iColour);
			fbxColour = vertexColourSet->GetDirectArray().GetAt(idx);
			break;
		}
		}break;
	}
	}
	return fbxColour;
}

FbxVector2 FbxModel::GetTextureUV(
	FbxMesh* mesh, FbxLayerElementUV* vertexUVSet,
	int iVertex, int iUV)
{
	FbxVector2 fbxUV;
	switch (vertexUVSet->GetMappingMode())
	{
	case FbxLayerElementUV::eByControlPoint:
	{
		switch (vertexUVSet->GetReferenceMode())
		{
		case FbxLayerElementUV::eDirect:
		{
			fbxUV = vertexUVSet->GetDirectArray().GetAt(iVertex);
			break;
		}
		case FbxLayerElementUV::eIndexToDirect:
		{
			int idx = vertexUVSet->GetIndexArray().GetAt(iVertex);
			fbxUV = vertexUVSet->GetDirectArray().GetAt(idx);
			break;
		}
		}break;
	}
	case FbxLayerElementUV::eByPolygonVertex:
	{
		switch (vertexUVSet->GetReferenceMode())
		{
		case FbxLayerElementUV::eDirect:
		case FbxLayerElementUV::eIndexToDirect:
		{
			fbxUV = vertexUVSet->GetDirectArray().GetAt(iUV);
			break;
		}
		}break;
	}
	}
	return fbxUV;
}

FbxVector4 FbxModel::GetNormal(
	FbxMesh* mesh, FbxLayerElementNormal* vertexNormalSet,
	int iVertex, int iColour)
{
	FbxVector4 fbxNormal(1, 1, 1, 1);
	switch (vertexNormalSet->GetMappingMode())
	{
	case FbxLayerElementUV::eByControlPoint:
	{
		switch (vertexNormalSet->GetReferenceMode())
		{
		case FbxLayerElementUV::eDirect:
		{
			fbxNormal = vertexNormalSet->GetDirectArray().GetAt(iVertex);
			break;
		}
		case FbxLayerElementUV::eIndexToDirect:
		{
			int idx = vertexNormalSet->GetIndexArray().GetAt(iVertex);
			fbxNormal = vertexNormalSet->GetDirectArray().GetAt(idx);
			break;
		}
		}break;
	}
	case FbxLayerElementUV::eByPolygonVertex:
	{
		switch (vertexNormalSet->GetReferenceMode())
		{
		case FbxLayerElementUV::eDirect:
		{
			fbxNormal = vertexNormalSet->GetDirectArray().GetAt(iColour);
			break;
		}
		case FbxLayerElementUV::eIndexToDirect:
		{
			int idx = vertexNormalSet->GetIndexArray().GetAt(iColour);
			fbxNormal = vertexNormalSet->GetDirectArray().GetAt(idx);
			break;
		}
		}break;
	}
	}
	return fbxNormal;
}

bool FbxModel::Init()
{
	_fbxManager		= FbxManager::Create();
	_fbxImporter	= FbxImporter::Create(_fbxManager, "");
	_fbxScene		= FbxScene::Create(_fbxManager, "");

	return true;
}

bool FbxModel::Frame()
{
	for (FbxObjSkinning* obj : _renderObjects)
	{
		obj->Frame();
	}
	return true;
}

bool FbxModel::UpdateFrame()
{
	_currFrame = _currFrame + g_secondPerFrame * _animSpeed
		* _animScene.frameRate * _animInverse;

	if (_currFrame > _animScene.endFrame ||
		_currFrame < _animScene.startFrame)
	{
		_currFrame = min(_currFrame, _animScene.endFrame - 1);
		_currFrame = max(_currFrame, _animScene.startFrame);
		_animInverse *= -1.0f;
	}

	VSBoneConstantData boneConstantData;
	for (int i = 0; i < _objects.size(); i++)
	{
		Matrix animMat = _objects[i]->Interpolate(_currFrame, _animScene);
		_boneConstantData.animMat[i] = Matrix::XMMatrixTranspose(animMat);
		boneConstantData.animMat[i] = animMat;
	}
	_immediateContext->UpdateSubresource(_boneConstantBuffer,
		0, nullptr, &_boneConstantData, 0, 0);

	for (int iObj = 0; iObj < _renderObjects.size(); iObj++)
	{
		if (!_renderObjects[iObj]->_boneMatPalette.empty())
		{
			for (int iBone = 0; iBone < _objects.size(); iBone++)
			{
				auto iter = _renderObjects[iObj]->_boneMatPalette.find(iBone);
				if (iter != _renderObjects[iObj]->_boneMatPalette.end())
				{
					Matrix worldTransformMat = iter->second;
					Matrix animMat = worldTransformMat * boneConstantData.animMat[iBone];
					_boneConstantData.animMat[iBone] = Matrix::XMMatrixTranspose(animMat);
				}
			}
			_immediateContext->UpdateSubresource(
				_renderObjects[iObj]->_boneConstantBuffer,
				0, nullptr, &_boneConstantData, 0, 0
			);
		}
	}

	return true;
}

void FbxModel::UpdateSkeleton(float frame, VSBoneConstantData& boneAnimMatData)
{
	for (int i = 0; i < _objects.size(); i++)
	{
		Matrix animMat = _objects[i]->Interpolate(frame, _animScene);
		boneAnimMatData.animMat[i] = animMat;
	}
}

void FbxModel::UpdateSkinning(VSBoneConstantData& boneAnimMatData,
			std::vector<VSBoneConstantData>& boneConstantData)
{
	for (int iObj = 0; iObj < _renderObjects.size(); iObj++)
	{
		if (!_renderObjects[iObj]->_boneMatPalette.empty())
		{
			for (int iBone = 0; iBone < _objects.size(); iBone++)
			{
				auto iter = _renderObjects[iObj]->_boneMatPalette.find(iBone);
				if (iter != _renderObjects[iObj]->_boneMatPalette.end())
				{
					Matrix worldTransformMat = iter->second;
					Matrix animMat = worldTransformMat * boneAnimMatData.animMat[iBone];
					boneConstantData[iObj].animMat[iBone] = Matrix::XMMatrixTranspose(animMat);
				}
			}
		}
	}
}

bool FbxModel::Render()
{
	for (FbxObjSkinning* obj : _renderObjects)
	{
		obj->Render();
	}
	return true;
}

bool FbxModel::Release()
{
	for (FbxObjSkinning* obj : _objects)
	{
		obj->Release();
		delete obj;
	}
	_boneConstantBuffer->Release();
	_fbxScene->Destroy();
	if (_fbxImporter)
	{
		_fbxImporter->Destroy();
		_fbxImporter = nullptr;
	}
	if (_fbxManager)
	{
		_fbxManager->Destroy();
		_fbxManager = nullptr;
	}
	_objects.clear();
	_objectMap.clear();
	_objectIDMap.clear();
	_renderObjects.clear();

	return true;
}
