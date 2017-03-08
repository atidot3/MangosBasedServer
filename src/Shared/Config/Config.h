#ifndef CONFIG_H
#define CONFIG_H

#include "../Common.h"
#include "../Define.h"
#include "Singleton.h"

#include <mutex>

#include <string>
#include <unordered_map>

class Config
{
private:
	std::string m_filename;
	std::unordered_map<std::string, std::string> m_entries; // keys are converted to lower case.  values cannot be.

public:
	bool SetSource(const std::string &file);
	bool Reload();

	bool IsSet(const std::string &name) const;

	const std::string GetStringDefault(const std::string &name, const std::string &def = "") const;
	bool GetBoolDefault(const std::string &name, bool def) const;
	int32 GetIntDefault(const std::string &name, int32 def) const;
	float GetFloatDefault(const std::string &name, float def) const;

	const std::string &GetFilename() const { return m_filename; }
	std::mutex m_configLock;
};

#define sConfig Origin::Singleton<Config>::Instance()

#endif
