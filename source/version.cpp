#include "version.h"

#include "utils.h"
#include <iostream>
#include <string>

std::string getCommit(std::string commitString) {
	std::string commit = "";
	for (int i = 0; i < 7; i++)
	{
		commit += commitString[i];
	}
	return commit;
}

const std::string LumaVersion::toString(bool printBranch) const {
	std::string currentVersionStr = release;
	if (!commit.empty())
		currentVersionStr += "-" + getCommit(commit);
	
	return currentVersionStr;
}

/* Luma3DS 0x2e svc version struct */
struct PACKED SvcLumaVersion {
	char magic[4];
	uint8_t major;
	uint8_t minor;
	uint8_t build;
	uint8_t flags;
	uint32_t commit;
	uint32_t unused;
};


int NAKED svcGetLumaVersion(SvcLumaVersion UNUSED *info) {
	asm volatile(
		"svc 0x2E\n"
		"bx lr"
	);
}


LumaVersion versionSvc() {
	SvcLumaVersion info;
	if (svcGetLumaVersion(&info) != 0) {
		return LumaVersion{};
	}

	LumaVersion version;

	std::stringstream releaseBuilder;
	releaseBuilder << (int)info.major << "." << (int)info.minor;
	if (info.build > 0) {
		releaseBuilder << "." << (int)info.build;
	}
	version.release = releaseBuilder.str();

	if (info.commit > 0) {
		std::stringstream commitBuilder;
		commitBuilder << std::hex << info.commit;
		version.commit = commitBuilder.str();
	}

	logPrintf("%s\n", version.toString().c_str());
	return version;
}

LumaVersion versionMemsearch(const std::string& path) {
	const static char searchString[] = "Luma3DS v";
	const static size_t searchStringLen = sizeof(searchString)/sizeof(char) - 1;

	std::ifstream payloadFile(path, std::ios::binary | std::ios::ate);
	if (!payloadFile) {
		logPrintf("Could not open existing payload, does it exists?\n");
		return LumaVersion{};
	}

	/* Load entire file into local buffer */
	size_t payloadSize = payloadFile.tellg();
	payloadFile.seekg(0, std::ios::beg);
	char* payloadData = (char*)std::malloc(payloadSize);
	payloadFile.read(payloadData, payloadSize);
	payloadFile.close();

	logPrintf("Loaded existing payload in memory, searching for version number...\n");

	size_t curProposedOffset = 0;
	unsigned short curStringIndex = 0;
	bool found = false;
	std::string versionString = "";

	// Byte-by-byte search. (memcmp might be faster?)
	// Since " " (1st char) is only used once in the whole string we can search in O(n)
	for (size_t offset = 0; offset < payloadSize - searchStringLen; ++offset) {
		if (payloadData[offset] == searchString[curStringIndex]) {
			if (curStringIndex == searchStringLen - 1) {
				found = true;
				break;
			}
			if (curStringIndex == 0) {
				curProposedOffset = offset;
			}
			curStringIndex++;
		}
		else {
			if (curStringIndex > 0) {
				curStringIndex = 0;
			}
		}
	}

	if (found) {
		// Version is what comes after " v" and before " configuration"
		curProposedOffset += searchStringLen;
		size_t verOffset = curProposedOffset;
		for (; verOffset < payloadSize; ++verOffset) {
			if (payloadData[verOffset] == 'c' && payloadData[verOffset-1] == ' ') {
				break;
			}
		}
		// Get full version string
		versionString = std::string(payloadData + curProposedOffset, verOffset - curProposedOffset - 1);
	}

	std::free(payloadData);

	const size_t separator = versionString.find("-");
	if (separator == std::string::npos) {
		return LumaVersion{ versionString, ""};
	}

	LumaVersion version;
	version.release = versionString.substr(0, separator);

	const size_t end = versionString.find(" ", separator);
	if (end == std::string::npos) {
		version.commit = versionString.substr(separator + 1);
	} else {
		version.commit = versionString.substr(separator + 1, end - separator - 1);
	}

	return version;
}