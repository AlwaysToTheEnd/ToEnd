#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>

const static std::string s_LRTokens[] = { "LEFT", "RIGHT" };


// leftRightOut = -1 (left), leftRightOut = 0 (none), leftRightOut = 1 (right)
// indexOut = -1 : not has Index
inline void FiltAniNodeName(const char* nodeName, const std::vector<std::string>& filters, std::string& filtedNameOut, char& leftRightOut, int& indexOut, int indexOffset = 0)
{
	std::string capitalName = nodeName;

	std::transform(capitalName.begin(), capitalName.end(), capitalName.begin(), [](char c)
		{
			return std::toupper(c);
		});

	leftRightOut = 0;

	for (int i = 0; i < _countof(s_LRTokens); i++)
	{
		size_t result = capitalName.find(s_LRTokens[i]);

		if (result != std::string::npos)
		{
			leftRightOut = i % 2 ? 1 : -1;
			capitalName = capitalName.substr(0, result) + capitalName.substr(result + s_LRTokens[i].size());
			break;
		}
	}

	if (capitalName.size() && leftRightOut == 0)
	{
		unsigned int lastIndex = capitalName.size() - 1;
		char LRToken[2] = { 'L', 'R' };

		for (int i = 0; i < _countof(LRToken); i++)
		{
			size_t currIndex = capitalName.find_first_of(LRToken[i]);

			while (currIndex != std::string::npos)
			{
				if (currIndex == lastIndex)
				{
					if (currIndex > 0)
					{
						if (capitalName[currIndex - 1] == '_')
						{
							leftRightOut = i % 2 ? 1 : -1;
							capitalName = capitalName.substr(0, currIndex - 1) + capitalName.substr(currIndex + 1);
							break;
						}
					}
				}
				else
				{
					if (currIndex + 1 < capitalName.size() && currIndex > 0)
					{
						if (capitalName[currIndex + 1] == '_' && capitalName[currIndex - 1] == '_')
						{
							leftRightOut = i % 2 ? 1 : -1;
							capitalName = capitalName.substr(0, currIndex - 1) + capitalName.substr(currIndex + 1);
							break;
						}
					}
				}

				currIndex = capitalName.find_first_of(LRToken[i], currIndex + 1);
			}

		}
	}

	for (auto& iter : filters)
	{
		size_t result = capitalName.find(iter);

		if (result != std::string::npos)
		{
			capitalName = capitalName.substr(0, result) + capitalName.substr(result + iter.size());
		}
	}

	std::string withoutIndex;
	std::string currIndex;
	std::vector<unsigned int> indexStack;
	for (auto& iter : capitalName)
	{
		if (iter >= '0' && iter <= '9')
		{
			currIndex += iter;
		}
		else
		{
			withoutIndex += iter;
			if (currIndex.size())
			{
				indexStack.push_back(std::stoi(currIndex));
				currIndex.clear();
			}
		}
	}

	if (currIndex.size())
	{
		indexStack.push_back(std::stoi(currIndex));
	}

	filtedNameOut = withoutIndex;

	indexOut = -1;

	if (indexStack.size())
	{
		indexOut = indexStack.back() + indexOffset;
	}
}

const static std::unordered_map<std::string, std::string> s_mixamoRigMapping = {
	{"mixamorig:Head",	"cf_J_Head" },
	{ "mixamorig:Hips", "cf_J_Hips"},
	{ "mixamorig:LeftArm", "cf_J_ArmUp00_L"},
	{ "mixamorig:LeftFoot", "cf_J_Foot01_L"},
	{ "mixamorig:LeftForeArm", "cf_J_ArmLow01_L"},
	{ "mixamorig:LeftHand",	"cf_J_Hand_L"},
	{ "mixamorig:LeftHandIndex1", "cf_J_Hand_Index01_L"},
	{ "mixamorig:LeftHandIndex2", "cf_J_Hand_Index02_L"},
	{ "mixamorig:LeftHandIndex3", "cf_J_Hand_Index03_L"},
	{ "mixamorig:LeftHandMiddle1", "cf_J_Hand_Middle01_L"},
	{ "mixamorig:LeftHandMiddle2", "cf_J_Hand_Middle02_L"},
	{ "mixamorig:LeftHandMiddle3", "cf_J_Hand_Middle03_L"},
	{ "mixamorig:LeftHandPinky1", "cf_J_Hand_Little01_L"},
	{ "mixamorig:LeftHandPinky2", "cf_J_Hand_Little02_L"},
	{ "mixamorig:LeftHandPinky3", "cf_J_Hand_Little03_L"},
	{ "mixamorig:LeftHandRing1", "cf_J_Hand_Ring01_L"},
	{ "mixamorig:LeftHandRing2" , "cf_J_Hand_Ring02_L"},
	{ "mixamorig:LeftHandRing3" , "cf_J_Hand_Ring03_L"},
	{ "mixamorig:LeftHandThumb1", "cf_J_Hand_Thumb01_L"},
	{ "mixamorig:LeftHandThumb2", "cf_J_Hand_Thumb02_L"},
	{ "mixamorig:LeftHandThumb3", "cf_J_Hand_Thumb03_L"},
	{ "mixamorig:LeftLeg", "cf_J_LegLow01_L"},
	{ "mixamorig:LeftShoulder", "cf_J_ShoulderIK_L"},
	{ "mixamorig:LeftToeBase", "cf_J_Toes01_L"},
	{ "mixamorig:LeftUpLeg", "cf_J_LegUp00_L"},
	{ "mixamorig:Neck", "cf_J_Neck"},
	{ "mixamorig:RightArm", "cf_J_ArmUp00_R"},
	{ "mixamorig:RightFoot", "cf_J_Foot01_R"},
	{ "mixamorig:RightForeArm", "cf_J_ArmLow01_R"},
	{ "mixamorig:RightHand", "cf_J_Hand_R"},
	{ "mixamorig:RightHandIndex1", "cf_J_Hand_Index01_R"},
	{ "mixamorig:RightHandIndex2", "cf_J_Hand_Index02_R"},
	{ "mixamorig:RightHandIndex3", "cf_J_Hand_Index03_R"},
	{ "mixamorig:RightHandMiddle1", "cf_J_Hand_Middle01_R"},
	{ "mixamorig:RightHandMiddle2", "cf_J_Hand_Middle02_R"},
	{ "mixamorig:RightHandMiddle3", "cf_J_Hand_Middle03_R"},
	{ "mixamorig:RightHandPinky1", "cf_J_Hand_Little01_R"},
	{ "mixamorig:RightHandPinky2", "cf_J_Hand_Little02_R"},
	{ "mixamorig:RightHandPinky3", "cf_J_Hand_Little03_R"},
	{ "mixamorig:RightHandRing1", "cf_J_Hand_Ring01_R"},
	{ "mixamorig:RightHandRing2", "cf_J_Hand_Ring02_R"},
	{ "mixamorig:RightHandRing3", "cf_J_Hand_Ring03_R"},
	{ "mixamorig:RightHandThumb1", "cf_J_Hand_Thumb01_R"},
	{ "mixamorig:RightHandThumb2", "cf_J_Hand_Thumb02_R"},
	{ "mixamorig:RightHandThumb3", "cf_J_Hand_Thumb03_R"},
	{ "mixamorig:RightLeg", "cf_J_LegLow01_R"},
	{ "mixamorig:RightShoulder", "cf_J_ShoulderIK_R"},
	{ "mixamorig:RightToeBase", "cf_J_Toes01_R"},
	{ "mixamorig:RightUpLeg", "cf_J_LegUp00_R"},
	{ "mixamorig:Spine", "cf_J_Spine01"},
	{ "mixamorig:Spine1", "cf_J_Spine02"},
	{ "mixamorig:Spine2",	"cf_J_Spine03"}
};

//const static std::unordered_map<std::string, std::string> s_mixamoRigMapping = {
//	{"cf_J_Head",				"mixamorig:Head"},
//	{"cf_J_Hips",				"mixamorig:Hips"},
//	{"cf_J_ArmUp00_L",			"mixamorig:LeftArm"},
//	{"cf_J_Foot01_L",			"mixamorig:LeftFoot"},
//	{"cf_J_ArmLow01_L",			"mixamorig:LeftForeArm"},
//	{"cf_J_Hand_L",				"mixamorig:LeftHand"},
//	{"cf_J_Hand_Index01_L",		"mixamorig:LeftHandIndex1"},
//	{"cf_J_Hand_Index02_L",		"mixamorig:LeftHandIndex2"},
//	{"cf_J_Hand_Index03_L",		"mixamorig:LeftHandIndex3"},
//	{"cf_J_Hand_Middle01_L",	"mixamorig:LeftHandMiddle1"},
//	{"cf_J_Hand_Middle02_L",	"mixamorig:LeftHandMiddle2"},
//	{"cf_J_Hand_Middle03_L",	"mixamorig:LeftHandMiddle3"},
//	{"cf_J_Hand_Little01_L",	"mixamorig:LeftHandPinky1"},
//	{"cf_J_Hand_Little02_L",	"mixamorig:LeftHandPinky2"},
//	{"cf_J_Hand_Little03_L",	"mixamorig:LeftHandPinky3"},
//	{"cf_J_Hand_Ring01_L",		"mixamorig:LeftHandRing1"},
//	{"cf_J_Hand_Ring02_L",		"mixamorig:LeftHandRing2"},
//	{"cf_J_Hand_Ring03_L",		"mixamorig:LeftHandRing3"},
//	{"cf_J_Hand_Thumb01_L",		"mixamorig:LeftHandThumb1"},
//	{"cf_J_Hand_Thumb02_L",		"mixamorig:LeftHandThumb2"},
//	{"cf_J_Hand_Thumb03_L",		"mixamorig:LeftHandThumb3"},
//	{"cf_J_LegLow01_L",			"mixamorig:LeftLeg"},
//	{"cf_J_Shoulder_L",			"mixamorig:LeftShoulder"},
//	{"cf_J_Toes01_L",			"mixamorig:LeftToeBase"},
//	{"cf_J_LegUp01_L",			"mixamorig:LeftUpLeg"},
//	{"cf_J_Neck",				"mixamorig:Neck"},
//	{"cf_J_ArmUp00_R",			"mixamorig:RightArm"},
//	{"cf_J_Foot01_R",			"mixamorig:RightFoot"},
//	{"cf_J_ArmLow01_R",			"mixamorig:RightForeArm"},
//	{"cf_J_Hand_R",				"mixamorig:RightHand"},
//	{"cf_J_Hand_Index01_R",		"mixamorig:RightHandIndex1"},
//	{"cf_J_Hand_Index02_R",		"mixamorig:RightHandIndex2"},
//	{"cf_J_Hand_Index03_R",		"mixamorig:RightHandIndex3"},
//	{"cf_J_Hand_Middle01_R", 	"mixamorig:RightHandMiddle1"},
//	{"cf_J_Hand_Middle02_R", 	"mixamorig:RightHandMiddle2"},
//	{"cf_J_Hand_Middle03_R", 	"mixamorig:RightHandMiddle3"},
//	{"cf_J_Hand_Little01_R", 	"mixamorig:RightHandPinky1"},
//	{"cf_J_Hand_Little02_R", 	"mixamorig:RightHandPinky2"},
//	{"cf_J_Hand_Little03_R", 	"mixamorig:RightHandPinky3"},
//	{"cf_J_Hand_Ring01_R",		"mixamorig:RightHandRing1"},
//	{"cf_J_Hand_Ring02_R",		"mixamorig:RightHandRing2"},
//	{"cf_J_Hand_Ring03_R",		"mixamorig:RightHandRing3"},
//	{"cf_J_Hand_Thumb01_R",		"mixamorig:RightHandThumb1"},
//	{"cf_J_Hand_Thumb02_R",		"mixamorig:RightHandThumb2"},
//	{"cf_J_Hand_Thumb03_R",		"mixamorig:RightHandThumb3"},
//	{"cf_J_LegLow01_R",			"mixamorig:RightLeg"},
//	{"cf_J_Shoulder_R",			"mixamorig:RightShoulder"},
//	{"cf_J_Toes01_R",			"mixamorig:RightToeBase"},
//	{"cf_J_LegUp01_R",			"mixamorig:RightUpLeg"},
//	{"cf_J_Spine01",			"mixamorig:Spine"},
//	{"cf_J_Spine02",			"mixamorig:Spine1"},
//	{"cf_J_Spine03",			"mixamorig:Spine2"}
//};
