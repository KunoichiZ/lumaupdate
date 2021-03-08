#pragma once

#include "libs.h"

/*! \brief Version data
 */
struct LumaVersion {
	std::string release; // Release number (ie. 6.1.1)
	std::string commit;  // Commit hash (ie. 59ab44a8)
	const std::string toString(bool printBranch = true) const;
	bool isValid() const { return !release.empty(); }
};

/*! \brief Tries to detect the current Luma3DS version by using SVC 0x2e (when supported)
 *
 *  \return LumaVersion struct containing all the information that could be retrieved
 */
LumaVersion versionSvc();

/*! \brief Tries to detect currently installed Luma3DS/AuReiNand version by searching the payload
 *
 *  \param path Path to existing payload
 *
 *  \return LumaVersion struct containing all the information that could be found
 */
LumaVersion versionMemsearch(const std::string& path);