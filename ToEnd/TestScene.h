#pragma once
#include <vector>
#include <unordered_map>
#include "CGHBaseClass.h"

class TestScene
{
public:
	TestScene();
	~TestScene();

	void Init();
	void Update(float delta);
	void Render();

private:
	CGHNode* node = nullptr;

};