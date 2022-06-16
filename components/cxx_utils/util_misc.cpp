#include "util_misc.hpp"

std::string formatBytes(size_t bytes)
{
	if (bytes < 1024) {
		return to_string(bytes) + "B";
	} else if (bytes < (1024 * 1024)) {
		return to_string(bytes / 1024.0) + "KB";
	} else if (bytes < (1024 * 1024 * 1024)) {
		return to_string(bytes / 1024.0 / 1024.0) + "MB";
	} else {
		return to_string(bytes / 1024.0 / 1024.0 / 1024.0) + "GB";
	}
}
