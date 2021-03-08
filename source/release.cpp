#include "release.h"

// jsmn includes
#include "jsmn.h"

// Internal includes
#include "archive.h"
#include "http.h"
#include "utils.h"

#ifndef FAKEDL
static int jsoneq(const char *json, const jsmntok_t *tok, const char *s) {
	if (tok->type == JSMN_STRING && (int)strlen(s) == tok->end - tok->start &&
		std::strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
		return 0;
	}
	return -1;
}
#endif

ReleaseInfo releaseGetLatestStable() {
	ReleaseInfo release;

#ifdef FAKEDL
	// Citra doesn't support HTTPc right now, so just fake a successful request
	release.name = "5.2";
	release.description = "- Remade the chainloader to only try to load the right payload for the pressed button. Now the only buttons which have a matching payload will actually do something during boot\r\n- Got rid of the default payload (start now boots \"start_NAME.bin\")\r\n- sel_NAME.bin is now select_NAME.bin as there are no more SFN/8.3 limitations anymore\r\n\r\nRefer to [the wiki](https://github.com/AuroraWright/Luma3DS/wiki/Installation-and-Upgrade#upgrading-from-v531) for upgrade instructions.";
	release.versions.push_back(ReleaseVer{ "CITRA", "CITRA", "https://github.com/LumaTeam/Luma3DS/releases/download/v5.2/Luma3DSv5.2.7z", 143234 });
#else

	static const char* ReleaseURL = "https://api.github.com/repos/LumaTeam/Luma3DS/releases/latest";

	jsmn_parser p = {};
	jsmn_init(&p);

	u8* apiReqData = nullptr;
	u32 apiReqSize = 0;

	logPrintf("Downloading %s...\n", ReleaseURL);

	httpGet(ReleaseURL, &apiReqData, &apiReqSize, true);

	logPrintf("Downloaded %lu bytes\n", apiReqSize);
	gfxFlushBuffers();

	jsmntok_t t[512] = {};
	int r = jsmn_parse(&p, (const char*)apiReqData, apiReqSize, t, sizeof(t) / sizeof(t[0]));
	if (r < 0) {
		throw formatErrMessage("Failed to parse JSON", r);
	}
	logPrintf("JSON parsed successfully!\n");
	gfxFlushBuffers();

	bool namefound = false, bodyfound = false, inassets = false;
	bool verHasName = false, verHasURL = false, verHasSize = false;
	ReleaseVer current;
	for (int i = 0; i < r; i++) {
		if (!namefound && jsoneq((const char*)apiReqData, &t[i], "tag_name") == 0) {
			jsmntok_t val = t[i+1];
			// Strip the "v" in front of the version name
			if (apiReqData[val.start] == 'v') {
				val.start += 1;
			}
			release.name = std::string((const char*)apiReqData + val.start, val.end - val.start);
			logPrintf("Release found: %s\n", release.name.c_str());
			namefound = true;
		}
		if (!bodyfound && jsoneq((const char*)apiReqData, &t[i], "body") == 0) {
			jsmntok_t val = t[i+1];
			release.description = unescape(std::string((const char*)apiReqData + val.start, val.end - val.start));
			logPrintf("Release description found.\n");
			bodyfound = true;
		}
		if (!inassets && jsoneq((const char*)apiReqData, &t[i], "assets") == 0) {
			inassets = true;
		}
		if (inassets) {
			if (jsoneq((const char*)apiReqData, &t[i], "name") == 0) {
				jsmntok_t val = t[i+1];
				current.filename = std::string((const char*)apiReqData + val.start, val.end - val.start);
				current.friendlyName = "stable version";
				verHasName = true;
			}
			if (jsoneq((const char*)apiReqData, &t[i], "browser_download_url") == 0) {
				jsmntok_t val = t[i+1];
				current.url = std::string((const char*)apiReqData + val.start, val.end - val.start);
				verHasURL = true;
			}
			if (jsoneq((const char*)apiReqData, &t[i], "size") == 0) {
				jsmntok_t val = t[i + 1];
				std::string sizeStr = std::string((const char*)apiReqData + val.start, val.end - val.start);
				current.fileSize = std::atoi(sizeStr.c_str());
				verHasSize = true;
			}
			if (verHasName && verHasURL && verHasSize) {
				logPrintf("Found version: %s\n", current.filename.c_str());
				ReleaseVer version = ReleaseVer{ current.filename, current.friendlyName, current.url, current.fileSize };
				release.versions.insert(release.versions.begin(), version);
				verHasName = verHasURL = verHasSize = false;
			}
		}
	}

	gfxFlushBuffers();
	std::free(apiReqData);

#endif

	return release;
}

ReleaseInfo releaseGetLatestHourly() {
	ReleaseInfo hourly;

#ifdef FAKEDL
	// Citra doesn't support HTTPc right now, so just fake a successful request
	hourly.name = "aaaaaaa";
	hourly.versions.push_back(ReleaseVer{ "CITRA", "latest hourly (aaaaaaa)", "https://github.com/LumaTeam/Luma3DS/releases/download/v5.2/Luma3DSv5.2.7z", 143234 });*/
	
#else

	static const char* LastCommitURL = 
	"https://api.github.com/repos/hax0kartik/Luma-hourlies/releases/latest";
	
	static const char* version = LastCommitURL;
	static const char* vertype = "hourly";
	std::string verurl;
	jsmn_parser p = {};
	jsmn_init(&p);

	u8* apiReqData = nullptr;
	u32 apiReqSize = 0;

	logPrintf("Downloading %s...\n", version);

	httpGet(version, &apiReqData, &apiReqSize, true);
	logPrintf("Downloaded %lu bytes\n", apiReqSize);
	gfxFlushBuffers();
	jsmntok_t t[512] = {};
	int r = jsmn_parse(&p, (const char*)apiReqData, apiReqSize, t, sizeof(t) / sizeof(t[0]));
		if (r < 0) {
			throw formatErrMessage("Failed to parse JSON", r);
		}
	logPrintf("JSON parsed successfully!\n");
	gfxFlushBuffers();

	bool namefound = false;
	bool verHasURL = false;

	for (int i = 0; i < r; i++) 
	{
		if (!namefound && jsoneq((const char*)apiReqData, &t[i], "tag_name") == 0) 
		{
			jsmntok_t val = t[i+1];
			hourly.name = std::string((const char*)apiReqData + val.start, val.end - val.start);
			hourly.name = hourly.name.substr(hourly.name.find_last_of('-') + 1);
			logPrintf("Hourly found: %s\n", hourly.name.c_str());
			namefound = true;
		}

		if (!verHasURL && jsoneq((const char*)apiReqData, &t[i], "browser_download_url") == 0) {
			jsmntok_t val = t[i+1];
			verurl = std::string((const char*)apiReqData + val.start, val.end - val.start);
			logPrintf("Hourly download Link Found\n");
			verHasURL = true;
		}

	}

	hourly.versions.push_back(ReleaseVer { hourly.name, "latest " + std::string(vertype) + " (" + hourly.name + ")", verurl, 0 });
	hourly.commits[std::string(vertype)] = hourly.name;
	std::free(apiReqData);

#endif
	return hourly;
}

bool releaseGetPayload(const PayloadType payloadType, const ReleaseVer& release, const bool isHourly, u8** payloadData, size_t* offset, size_t* payloadSize) {
	u8* fileData = nullptr;
	u32 fileSize = 0;
	HTTPResponseInfo info;

	try {
#ifdef FAKEDL
		// Read predownloaded file
		std::ifstream predownloaded(release.filename + ".7z", std::ios::binary | std::ios::ate);
		fileSize = predownloaded.tellg();
		predownloaded.seekg(0, std::ios::beg);
		fileData = (u8*)malloc(fileSize);
		predownloaded.read((char*)fileData, fileSize);
		info.etag = "\"0973d3d5fe62fccc30c8f663aec6918c\"";
#else
		httpGet(release.url.c_str(), &fileData, &fileSize, true, &info);
#endif
	} catch (const std::runtime_error& e) {
		logPrintf("%s\n", e.what());
		return false;
	}
	logPrintf("Download complete! Size: %lu\n", fileSize);

	if (release.fileSize != 0) {
		logPrintf("Integrity check #1");
		if (fileSize != release.fileSize) {
			logPrintf(" [ERR]\r\nReceived file is a different size than expected!\n");
			gfxFlushBuffers();
			return false;
		}
		logPrintf(" [OK]\r\n");
	} else {
		logPrintf("Skipping integrity check #1 (unknown size)\n");
	}

	if (!info.etag.empty()) {
		logPrintf("Integrity check #2");
		if (!httpCheckETag(info.etag, fileData, fileSize)) {
			logPrintf(" [ERR]\r\nMD5 mismatch between server's and local file!\n");
			gfxFlushBuffers();
			return false;
		}
		logPrintf(" [OK]\r\n");
	} else {
		logPrintf("Skipping integrity check #2 (no ETag found)\n");
	}

	gfxFlushBuffers();

	std::string payloadPath;
	switch (payloadType) {
	case PayloadType::SIGHAX:
		payloadPath = DEFAULT_SIGHAX_PATH;
		break;
	/*case PayloadType::Menuhax:
		payloadPath = DEFAULT_MHAX_PATH;
		break;
	case PayloadType::Homebrew:
		payloadPath = DEFAULT_3DSX_PATH;
		break;*/
	}

	try {
		if(release.url.find(".firm") != std::string::npos)
		{
			logPrintf("Writing hourly\n");
			*payloadData = (u8 *)malloc(sizeof(u8) * fileSize);
			memcpy(*payloadData, fileData, fileSize);
			*payloadSize = fileSize;
			offset = 0;
		}
		else
		{
			ZipArchive archive(fileData, fileSize);
			archive.extractFile(payloadPath, payloadData, payloadSize);
			offset = 0;
		}
	} catch (const std::runtime_error& e) {
		logPrintf(" [ERR]\nFATAL: %s", e.what());
		std::free(fileData);
		return false;
	}

	logPrintf(" [OK]\n");
	if(fileData != nullptr) std::free(fileData);
	return true;
}
