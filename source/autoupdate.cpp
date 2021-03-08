#include "autoupdate.h"

// jsmn includes
#include "jsmn.h"

// Internal includes
#include "archive.h"
#include "console.h"
#include "http.h"
#include "utils.h"

UpdaterInfo updaterGetInfo(const char* path) {
	HomebrewLocation location = HomebrewLocation::Unknown;
	HomebrewType type = HomebrewType::Unknown;
	std::string sdmcLoc, smdcName;

	if (path != nullptr) {
		std::string source(path);

		// Check for SDMC or 3DSLINK
		if (source.compare(0, 5, "sdmc:") == 0) {
			location = HomebrewLocation::SDMC;
		} else if (source.compare(0, 8, "3dslink:") == 0) {
			location = HomebrewLocation::Remote;
		}

		// Check for Homebrew
		if (source.find(".3dsx") != std::string::npos) {
			type = HomebrewType::Homebrew;
			size_t start = source.find_first_of(':') + 1;
			size_t end = source.find_last_of('/');
			sdmcLoc = source.substr(start, end - start);

			size_t extEnd = source.find_last_of('.');
			smdcName = source.substr(end + 1, extEnd - end - 1);
		}
	}

#ifdef UNIQUE_ID
	// Check for CIA
	u64 appid = 0;
	if (APT_GetProgramID(&appid) == 0) {
		if ((appid & UNIQUE_ID) == UNIQUE_ID) {
			type = HomebrewType::CIA;
		}
	}
#endif

	return { type, location, sdmcLoc, smdcName };
}

#ifndef FAKEDL
static int jsoneq(const char *json, jsmntok_t *tok, const char *s) {
	if (tok->type == JSMN_STRING && (int)strlen(s) == tok->end - tok->start &&
		std::strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
		return 0;
	}
	return -1;
}
#endif

LatestUpdaterInfo updaterGetLatest() {
#ifdef FAKEDL
	return {};
#else
	static const char* ReleaseURL = "https://api.github.com/repos/KunoichiZ/lumaupdate/releases/latest";

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
	LatestUpdaterInfo latest;
	for (int i = 0; i < r; i++) {
		if (!namefound && jsoneq((const char*)apiReqData, &t[i], "tag_name") == 0) {
			jsmntok_t val = t[i + 1];
			latest.version = std::string((const char*)apiReqData + val.start, val.end - val.start);
			logPrintf("Release found: %s\n", latest.version.c_str());
			namefound = true;
		}
		if (!bodyfound && jsoneq((const char*)apiReqData, &t[i], "body") == 0) {
			jsmntok_t val = t[i + 1];
			latest.changelog = unescape(std::string((const char*)apiReqData + val.start, val.end - val.start));
			logPrintf("Changelog found.\n");
			bodyfound = true;
		}
		if (!inassets && jsoneq((const char*)apiReqData, &t[i], "assets") == 0) {
			inassets = true;
		}
		if (inassets) {
			if (jsoneq((const char*)apiReqData, &t[i], "browser_download_url") == 0) {
				jsmntok_t val = t[i + 1];
				std::string url = std::string((const char*)apiReqData + val.start, val.end - val.start);
				if (url.find(".zip") != std::string::npos) {
					latest.url = url;
				}
			}
		}
	}

	gfxFlushBuffers();
	std::free(apiReqData);

#ifdef GIT_VER
	latest.isNewer = latest.version > GIT_VER;
#else
	latest.isNewer = false;
#endif

	return latest;
#endif
}

static void installCIA(const u8* ciaData, const size_t ciaSize) {
	Handle handle;
	AM_QueryAvailableExternalTitleDatabase(NULL);
	CHECK(AM_StartCiaInstall(MEDIATYPE_SD, &handle), "Cannot initialize CIA install");
	try {
		CHECK(FSFILE_Write(handle, NULL, 0, ciaData, (u32)ciaSize, 0), "Cannot write CIA data to handle");
		CHECK(AM_FinishCiaInstall(handle), "Cannot finalize CIA install");
	} catch (const std::runtime_error& e) {
		// Abort CIA install and re-throw
		CHECK(AM_CancelCIAInstall(handle), "Cannot cancel CIA install");
		throw;
	}
}

static void copyToFile(const std::string& path, const u8* fileData, const size_t fileSize) {
	std::ofstream fout(path, std::ios::binary | std::ios::out);
	fout.write((const char*)fileData, fileSize);
	fout.close();
}

UpdateResult updaterDoUpdate(LatestUpdaterInfo latest, UpdaterInfo current) {
	consoleScreen(GFX_TOP);
	consoleInitProgress("Updating Luma3DS Updater", "Downloading archive", 0.2);

	consoleScreen(GFX_BOTTOM);
	consoleClear();

	u8* archiveData = nullptr;
	u32 archiveSize = 0;
	HTTPResponseInfo info;

	try {
		logPrintf("Downloading %s...\n", latest.url.c_str());
		httpGet(latest.url.c_str(), &archiveData, &archiveSize, true, &info);
		logPrintf("Download complete! Size: %lu\n", archiveSize);
	} catch (const std::runtime_error& e) {
		logPrintf("\nFATAL: %s", e.what());
		return { false, "DOWNLOAD FAILED" };
	}

	consoleScreen(GFX_TOP);
	consoleSetProgressData("Extracting archive contents", 0.8);
	consoleScreen(GFX_BOTTOM);

	try {
		ZipArchive archive(archiveData, archiveSize);

		switch (current.type) {
		case HomebrewType::CIA:
			// Extract CIA from archive, install it
			u8* ciaData;
			size_t ciaSize;
			logPrintf("Extracting lumaupdater.cia");
			archive.extractFile("lumaupdater.cia", &ciaData, &ciaSize);
			logPrintf(" [OK] (%u bytes)\n", ciaSize);
			try {
				logPrintf("Installing lumaupdater.cia");
				installCIA(ciaData, ciaSize);
				logPrintf(" [OK]\n");
			} catch (const std::runtime_error& e) {
				logPrintf(" [ERR]\n\nFATAL: %s", e.what());
				return { false, "CIA INSTALL FAILED" };
			}
			break;
		case HomebrewType::Homebrew: {
			// Extract 3dsx/smdh from archive
			u8* hbData;
			size_t hbSize;
			logPrintf("Extracting lumaupdater.3dsx");
			archive.extractFile("3ds/lumaupdater/lumaupdater.3dsx", &hbData, &hbSize);
			logPrintf(" [OK] (%u bytes)\n", hbSize);


			const std::string targetHb = current.sdmcLoc + "/" + current.sdmcName + ".3dsx";
			logPrintf("Copying to %s", targetHb.c_str());
			copyToFile(targetHb, hbData, hbSize);
			logPrintf(" [OK]\n");
			std::free(hbData);

			u8* smdhData;
			size_t smdhSize;
			logPrintf("Extracting lumaupdater.3dsx");
			archive.extractFile("3ds/lumaupdater/lumaupdater.smdh", &smdhData, &smdhSize);
			logPrintf(" [OK] (%u bytes)\n", smdhSize);

			const std::string targetSMDH = current.sdmcLoc + "/" + current.sdmcName + ".smdh";
			logPrintf("Copying to %s", targetSMDH.c_str());
			copyToFile(targetSMDH, smdhData, smdhSize);
			logPrintf(" [OK]\n");
			std::free(smdhData);
			break;
		}
		default:
			return { false, "UNKNOWN INSTALL" };
		}
	} catch (const std::runtime_error& e) {
		logPrintf("[ERR]\n\nFATAL: %s", e.what());
		return { false, "EXTRACT FAILED" };
	}

	return { true, "NO ERROR" };
}