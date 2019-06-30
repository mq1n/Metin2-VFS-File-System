#include <filesystem>
#include <map>
#include <string>
#include <iomanip>
#include <functional>

#include "../include/VFSPropertyManager.h"
#include "../include/LogHelper.h"

#include <rapidjson/document.h>
#include <rapidjson/reader.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
using namespace rapidjson;

// Beautifier: https://jsonformatter.curiousconcept.com

namespace VFS
{
	extern CVFSLog* gs_pVFSLogInstance;

	static std::vector <std::wstring> SplitString(const std::wstring & str, const std::wstring & tok = L" ")
	{
		std::vector <std::wstring> output;

		std::size_t prev = 0;
		auto cur = str.find(tok);
		while (cur != std::wstring::npos)
		{
			output.emplace_back(str.substr(prev, cur - prev));
			prev = cur + tok.size();
			cur = str.find(tok, prev);
		}

		output.emplace_back(str.substr(prev, cur - prev));
		return output;
	}
	static std::wstring ReplaceString(std::wstring& str, const std::wstring& from, const std::wstring& to)
	{
		std::wstring output = str;

		for (size_t pos = 0; ; pos += to.length())
		{
			pos = output.find(from, pos);
			if (pos == std::wstring::npos)
				break;

			output.erase(pos, from.length());
			output.insert(pos, to);
		}

		return output;
	}

	bool GetPropertyObject(const std::string& strPropertyFileContent, uint32_t dwCRC, std::map <std::string /* key */, std::string /* value */>& content)
	{
		Document document;
		document.Parse(strPropertyFileContent.c_str());

		if (document.HasParseError())
		{
			gs_pVFSLogInstance->Log(__FUNCTION__, LL_ERR, "Property file ParseError: %u\n", static_cast<uint32_t>(document.GetParseError()));
			document.Clear();
			return false;
		}

		if (!document.IsArray())
		{
			gs_pVFSLogInstance->Log(__FUNCTION__, LL_ERR, "Property file base is NOT array");
			document.Clear();
			return false;
		}

		for (rapidjson::SizeType i = 0; i < document.Size(); ++i)
		{
			if (!document[i].IsObject())
			{
				gs_pVFSLogInstance->Log(__FUNCTION__, LL_ERR, "Property file group is NOT object");
				document.Clear();
				return false;
			}

			auto parsed = std::map <std::string /* key */, std::string /* value */>();

			for (auto childNode = document[i].MemberBegin(); childNode != document[i].MemberEnd(); ++childNode)
			{
				parsed.emplace(childNode->name.GetString(), childNode->value.GetString());
//				printf("%ls-%ls\n", childNode->name.GetString(), childNode->value.GetString());
			}

			if (parsed["crc"] == std::to_string(dwCRC))
			{
				content.insert(parsed.begin(), parsed.end());
				document.Clear();
				return true;
			}
		}

		document.Clear();
		return false;
	}

	std::string LoadPropertyFile(const std::string& strPropertyListFile)
	{
		std::string out;

		if (std::filesystem::exists(strPropertyListFile) == false)
		{
			gs_pVFSLogInstance->Log(__FUNCTION__, LL_ERR, "Property file: %s NOT exists!", strPropertyListFile.c_str());
			return out;
		}

		if (std::filesystem::file_size(strPropertyListFile) == 0)
		{
			gs_pVFSLogInstance->Log(__FUNCTION__, LL_ERR, "Property file: %s empty!", strPropertyListFile.c_str());
			return out;
		}

		std::wifstream in(strPropertyListFile.c_str(), std::ios_base::binary);
		if (!in)
		{
			gs_pVFSLogInstance->Log(__FUNCTION__, LL_ERR, "Property file can NOT open!");
			return out;
		}

		in.exceptions(std::ios_base::badbit | std::ios_base::failbit | std::ios_base::eofbit);
		auto wstrcontent = std::wstring(std::istreambuf_iterator<wchar_t>(in), std::istreambuf_iterator<wchar_t>());
		in.close();

		out = std::string(wstrcontent.begin(), wstrcontent.end());
		return out;
	}

	bool LoadPropertyList(const std::string& strPropertyFileContent, std::vector <std::map <std::string /* key */, std::string /* value */>>& vPropertyItems)
	{
        /*
		auto strcontent = LoadPropertyFile(strPropertyListFile);
		if (strcontent.empty())
		{
			gs_pVFSLogInstance->Log(__FUNCTION__, LL_ERR, "Property file can NOT load!");
			return false;
		}
        */
       if (strPropertyFileContent.empty())
       {
 			gs_pVFSLogInstance->Log(__FUNCTION__, LL_ERR, "Property file can content is empty!");
			return false;          
       }

		Document document;
		document.Parse(strPropertyFileContent.c_str());

		if (document.HasParseError())
		{
			gs_pVFSLogInstance->Log(__FUNCTION__, LL_ERR, "Property file ParseError: %u\n", static_cast<uint32_t>(document.GetParseError()));
			document.Clear();
			return false;
		}

		if (!document.IsArray())
		{
			gs_pVFSLogInstance->Log(__FUNCTION__, LL_ERR, "Property file base is NOT array");
			document.Clear();
			return false;
		}

		for (rapidjson::SizeType i = 0; i < document.Size(); ++i)
		{
			if (!document[i].IsObject())
			{
				gs_pVFSLogInstance->Log(__FUNCTION__, LL_ERR, "Property file group is NOT object");
				document.Clear();
				return false;
			}

			std::map <std::string /* key */, std::string /* value */> container;
			for (auto childNode = document[i].MemberBegin(); childNode != document[i].MemberEnd(); ++childNode)
			{
				container.emplace(childNode->name.GetString(), childNode->value.GetString());
//				printf("%ls-%ls\n", childNode->name.GetString(), childNode->value.GetString());
			}
			vPropertyItems.emplace_back(container);
		}

		document.Clear();
		return true;
	}

	bool GeneratePropertyList(const std::string& strPropertyDir)
	{
		if (std::filesystem::exists(strPropertyDir) == false)
		{
			gs_pVFSLogInstance->Log(__FUNCTION__, LL_ERR, "Property dir: %ls NOT exists!", strPropertyDir.c_str());
			return false;
		}

		auto propertyFiles = std::map <std::wstring /* filename */, std::map <std::wstring /* propertykey */, std::wstring /* propertyvalue */> >();
		for (const auto& entry : std::filesystem::recursive_directory_iterator(strPropertyDir))
		{
			if (entry.path().extension() == L".pra" || entry.path().extension() == L".prb" ||
				entry.path().extension() == L".prd" || entry.path().extension() == L".pre" || 
				entry.path().extension() == L".prt")
			{
				std::wifstream in(entry.path().c_str(), std::ios_base::binary);
				if (!in)
				{
					gs_pVFSLogInstance->Log(__FUNCTION__, LL_ERR, "Property file can NOT open!");
					return false;
				}
				in.exceptions(std::ios_base::badbit | std::ios_base::failbit | std::ios_base::eofbit);
				auto content = std::wstring(std::istreambuf_iterator<wchar_t>(in), std::istreambuf_iterator<wchar_t>());
				in.close();

                gs_pVFSLogInstance->Log(__FUNCTION__, LL_SYS, "Property file: %ls", entry.path().c_str());

				if (content.empty())
				{
					gs_pVFSLogInstance->Log(__FUNCTION__, LL_ERR, "Null property file detected: %ls", entry.path().wstring().c_str());
					return false;
				}

				auto lines = SplitString(content, L"\n");
				if (lines.empty())
				{
					gs_pVFSLogInstance->Log(__FUNCTION__, LL_ERR, "Null parsed property content for: %ls", entry.path().wstring().c_str());
					return false;
				}

				auto parsedcontents = std::map <std::wstring, std::wstring>();
				for (size_t i = 0; i < lines.size(); ++i)
				{
					auto line = lines[i];
					if (line.empty())
						continue;

//					printf("%ls\n", line.c_str());

					if (i == 0 /* magic (YPRT)*/)
					{
						continue;
					}
					else if (i == 1 /* crc */)
					{
						parsedcontents.emplace(L"crc", line);
						continue;
					}
					else
					{
						auto splittedline = SplitString(line, L"\t\t");
						if (splittedline.size() != 2)
						{
							gs_pVFSLogInstance->Log(__FUNCTION__, LL_ERR, "Bad format for: %ls", entry.path().wstring().c_str());
							return false;
						}

						auto fixedvalue = ReplaceString(splittedline.at(1), L"\"", L"");

//						printf("%ls-%ls\n", splittedline.at(0).c_str(), fixedvalue.c_str());

						parsedcontents.emplace(splittedline.at(0), fixedvalue);
					}
				}
				if (parsedcontents.empty())
				{
					gs_pVFSLogInstance->Log(__FUNCTION__, LL_ERR, "Null property parsed content: %ls", entry.path().wstring().c_str());
					return false;
				}

				parsedcontents.emplace(L"filename", entry.path().wstring());
				propertyFiles.emplace(entry.path().wstring(), parsedcontents);
			}
		}
		gs_pVFSLogInstance->Log(__FUNCTION__, LL_SYS, "Property list size: %u", propertyFiles.size());

		if (propertyFiles.empty())
		{
			gs_pVFSLogInstance->Log(__FUNCTION__, LL_ERR, "Property list is empty!");
			return false;
		}

		StringBuffer s;
		Writer <StringBuffer> writer(s);

		writer.StartArray();
		for (const auto& [file, content] : propertyFiles)
		{
			writer.StartObject();
			for (const auto& [key, value] : content)
			{
				auto stkey = std::string(key.begin(), key.end());
				auto stValue = std::string(value.begin(), value.end());

				writer.Key(stkey.c_str());
				writer.String(stValue.c_str());
			}
			writer.EndObject();
		}
		writer.EndArray();

		std::ofstream f("PropertyList.json");
		f << std::setw(4) << s.GetString() << std::endl;
		f.close();

		return true;
	}
}
