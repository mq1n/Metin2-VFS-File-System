#pragma once
#include <string>
#include <vector>
#include <map>

namespace VFS
{
	bool GetPropertyObject(const std::string& strPropertyFileContent, uint32_t dwCRC, std::map <std::string /* key */, std::string /* value */>& content);
	std::string LoadPropertyFile(const std::string& strPropertyListFile);
	bool LoadPropertyList(const std::string& strPropertyFileContent, std::vector <std::map <std::string /* key */, std::string /* value */>>& vPropertyItems);
	bool GeneratePropertyList(const std::string& strPropertyDir);
};
