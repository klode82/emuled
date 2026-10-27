// LINUX (new code): the manifest of a UI template (template.yaml). A template is a folder with a small web application
// that talks only to the daemon API; the manifest describes it. Format: flat YAML ("key: value", comments with #, no lists or nesting),
// the same style as config.yaml. Unknown keys are ignored (so future templates can add some without breaking old ones).
//
//   id: cruscotto             (required: 2-32 characters, lowercase, digits and hyphen; the first is not a hyphen)
//   name: Cruscotto           (required, at most 60 characters)
//   version: 1.0.0            (required: number.number.number)
//   description: ...          (optional, at most 300 characters)
//   author: ...  license: ... (optional)
//   api: 1                    (API version the template requires; default 1; the daemon rejects a number higher than its own)
//   entry: index.html         (entry page, path relative to the template folder; default index.html)
//   preview: preview.png      (preview image, optional, same kind of path)
#pragma once
#include <string>
#include <vector>

namespace emule::templates {

constexpr int kApiVersion = 1;					// template API version this daemon understands
constexpr size_t kMaxManifestBytes = 8 * 1024;

struct Manifest
{
	std::string id, name, version, description, author, license;
	std::string entry = "index.html", preview;
	int api = 1;
};

struct ManifestResult
{
	bool ok = false;
	Manifest manifest;							// valid only if ok
	std::vector<std::string> errors;			// in Italian, with the line number when available
};

bool ValidTemplateId(const std::string &id);	// 2-32 characters: a-z, 0-9, '-'; does not start with '-'
// safe relative path inside the template folder: not empty, at most 200 characters, only A-Za-z0-9 . _ - /, no "..", no leading or double '/'
bool SafeRelativePath(const std::string &p);

// parses the text of a template.yaml; `daemon_api` is the API version the daemon understands (kApiVersion)
ManifestResult ParseManifest(const std::string &text, int daemon_api = kApiVersion);
// reads the file (rejects files larger than kMaxManifestBytes and those that are not regular files)
ManifestResult LoadManifest(const std::string &path, int daemon_api = kApiVersion);

} // namespace emule::templates
