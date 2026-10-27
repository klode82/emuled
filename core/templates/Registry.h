// LINUX (new code): the registry of UI templates. Searches the folders (system, user, development extras), reads and checks the
// template.yaml of each subfolder and keeps the list, valid and invalid (with the reason: the UI also shows the ones that fail to load).
//
// Rules (Bench 838, decision of 26/10/2026):
//  - priority: system (/usr/share/emuled/templates) > user (~/.emuled/templates) > extra (templates_dir, for development and Docker);
//  - the built-in names (console, dashboard, studio, mobile) are reserved: valid only from system or extra, NEVER from the user folder;
//  - the folder name must equal the manifest id (so the /t/<id>/ address and the folder match);
//  - the same id in two places: the higher-priority source wins, the other stays in the list as invalid;
//  - the entry page (entry) must exist and be a regular file inside the template folder.
#pragma once
#include "templates/Manifest.h"
#include <string>
#include <vector>

namespace emule::templates {

enum class Source { System, User, Extra };
const char *SourceName(Source s);				// "system" | "user" | "dev"

struct Dirs { std::string system, user, extra; };		// empty = do not search
// The system folder follows the install prefix (EMULED_SYSTEM_TEMPLATES, set by CMake: <prefix>/share/emuled/templates; /usr/share/emuled/templates
// in packages, which use the /usr prefix); the user one is <home>/.emuled/templates.
Dirs DefaultDirs(const std::string &home);
// Creates the user's template folder if it is missing, with a README.txt that explains what it is for (only when it creates it: if the folder exists, nothing is touched). true = it created it.
bool EnsureUserDir(const std::string &dir);

struct Entry
{
	std::string id;								// from the manifest; if the manifest is broken, the folder name
	std::string dir_name, dir;					// name and (canonical) path of the folder
	Source source = Source::System;
	Manifest manifest;							// valid only if valid
	bool valid = false;
	bool has_preview = false;					// the preview file exists
	std::vector<std::string> errors;
};

bool IsReservedId(const std::string &id);

class Registry
{
public:
	// searches all folders again (can be called any number of times); writes a summary to the daemon Log and a warning for each invalid template
	void Scan(const Dirs &dirs);
	const std::vector<Entry> &entries() const { return entries_; }
	const Entry *Find(const std::string &id) const;			// valid templates only
	size_t ValidCount() const;
private:
	std::vector<Entry> entries_;
};

} // namespace emule::templates
