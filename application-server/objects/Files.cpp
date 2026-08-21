#include "objects.h"
#include <boost/filesystem.hpp>
#include <algorithm>
#include <cfloat>
#include <ctime>
#include <iomanip>
#include <sstream>
#ifdef __ARM_ARCH
#include <errno.h>
#include <stdio.h>
#include <sys/inotify.h>
#include <unistd.h>
#endif

using std::string;
using std::map;
using std::set;
using std::list;
using std::vector;
using std::valarray;
using std::shared_ptr;
using boost::str;
using boost::format;
using namespace std::chrono_literals;
namespace fs = boost::filesystem;

#ifdef __ARM_ARCH
namespace {

const std::vector<std::string> &watchedDirectoryPaths()
{
	static const std::vector<std::string> paths = {
		"/mnt/data/audio-files",
		"/mnt/data/image-files",
		"/mnt/data/misc-files"
	};
	return paths;
}

bool isSafeRelativePath(const fs::path &p)
{
	if (p.empty() || p.is_absolute())
	{
		return false;
	}

	for (const auto &part : p)
	{
		const std::string token = part.string();
		if (token.empty() || token == "." || token == "..")
		{
			return false;
		}
	}

	return true;
}

bool pathHasPrefix(const fs::path &path, const fs::path &prefix)
{
	auto pIt = path.begin();
	auto pEnd = path.end();
	auto prefixIt = prefix.begin();
	auto prefixEnd = prefix.end();

	for (; prefixIt != prefixEnd; ++prefixIt, ++pIt)
	{
		if (pIt == pEnd || *pIt != *prefixIt)
		{
			return false;
		}
	}

	return true;
}

bool isMetadataSidecarFileName(const char *fileName)
{
	if (fileName == nullptr)
	{
		return false;
	}

	static const std::string suffix = ".metadata.json";
	const std::string name(fileName);
	if (name.size() < suffix.size())
	{
		return false;
	}

	return name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string toDirectoryName(const std::string &directoryPath)
{
	return fs::path(directoryPath).filename().string();
}

std::string formatBytesHumanReadable(uintmax_t bytes)
{
	static const char *units[] = {"B", "KB", "MB", "GB", "TB"};
	double value = static_cast<double>(bytes);
	size_t unitIndex = 0;
	while (value >= 1000.0 && unitIndex < (sizeof(units) / sizeof(units[0])) - 1)
	{
		value /= 1000.0;
		unitIndex++;
	}

	std::ostringstream oss;
	oss << std::fixed << std::setprecision(unitIndex == 0 ? 0 : 2) << value << " " << units[unitIndex];
	return oss.str();
}

std::string formatUtcTimestamp(std::time_t t)
{
	std::tm tmUtc;
	gmtime_r(&t, &tmUtc);
	char out[32];
	std::strftime(out, sizeof(out), "%Y-%m-%dT%H:%M:%SZ", &tmUtc);
	return out;
}

json buildFileJson(const fs::path &filePath)
{
	boost::system::error_code sizeEc;
	const auto bytes = fs::file_size(filePath, sizeEc);

	boost::system::error_code timeEc;
	const auto writeTime = fs::last_write_time(filePath, timeEc);

	json fileObj = json::object();
	fileObj["size"] = sizeEc ? "unknown" : formatBytesHumanReadable(bytes);
	fileObj["timestamp"] = timeEc ? "unknown" : formatUtcTimestamp(writeTime);
	fileObj["metadata"] = json::object();
	return fileObj;
}

json createEmptyDirectoryState(const std::vector<std::string> &watchedPaths)
{
	json state = json::object();
	for (const auto &path : watchedPaths)
	{
		state[toDirectoryName(path)] = json::object();
	}
	return state;
}

void rebuildDirectoryStateEntry(const std::string &directoryPath, json &state)
{
	const std::string directoryName = toDirectoryName(directoryPath);
	json directoryFiles = json::object();
	boost::system::error_code ec;

	if (!fs::exists(directoryPath, ec) || !fs::is_directory(directoryPath, ec))
	{
		state[directoryName] = directoryFiles;
		return;
	}

	/*
	  Build a JSON directory listing for one watched directory, e.g.:
	  {
	    "song.wav": {
	      "size": "12.35 MB",
	      "timestamp": "2026-08-04T14:23:51Z",
	      "metadata": {}
	    },
	    "cover.jpg": {
	      "size": "845 KB",
	      "timestamp": "2026-08-04T14:24:10Z",
	      "metadata": {}
	    }
	  }
	*/
	fs::directory_iterator end;
	for (fs::directory_iterator it(directoryPath, ec); it != end && !ec; it.increment(ec))
	{
		const fs::path &p = it->path();
		boost::system::error_code typeEc;
		if (fs::is_regular_file(p, typeEc))
		{
			directoryFiles[p.filename().string()] = buildFileJson(p);
		}
	}

	state[directoryName] = directoryFiles;
}

class InotifyDirectoryMonitor {
public:
	struct Watch {
		std::string path;
		int watchDesc = -1;
		bool changed = false;
	};

	explicit InotifyDirectoryMonitor(std::vector<std::string> paths)
	{
		for (const auto &path : paths)
		{
			watches.push_back(Watch{path, -1, false});
		}
	}

	~InotifyDirectoryMonitor()
	{
		if (inotifyFd >= 0)
		{
			close(inotifyFd);
		}
	}

	bool initialize()
	{
		if (inotifyFd >= 0)
		{
			return true;
		}

		inotifyFd = inotify_init1(IN_NONBLOCK);
		if (inotifyFd < 0)
		{
			return false;
		}

		bool anyWatchAdded = false;
		for (auto &watch : watches)
		{
			watch.watchDesc = inotify_add_watch(inotifyFd, watch.path.c_str(),
				IN_CLOSE_WRITE | IN_MOVED_TO | IN_DELETE | IN_MOVED_FROM);
			if (watch.watchDesc >= 0)
			{
				anyWatchAdded = true;
			}
		}

		if (!anyWatchAdded)
		{
			close(inotifyFd);
			inotifyFd = -1;
			return false;
		}

		return true;
	}

	void poll()
	{
		if (inotifyFd < 0)
		{
			return;
		}

		char buf[4096] __attribute__((aligned(__alignof__(struct inotify_event))));
		ssize_t len;
		while ((len = read(inotifyFd, buf, sizeof(buf))) > 0)
		{
			const char *ptr = buf;
			while (ptr < buf + len)
			{
				const auto *ev = reinterpret_cast<const struct inotify_event *>(ptr);
				Watch *watch = findWatchByDescriptor(ev->wd);
				if (watch != nullptr)
				{
					const bool hasName = ev->len > 0;
					const bool isMetadataSidecar = hasName && isMetadataSidecarFileName(ev->name);
					if (!isMetadataSidecar)
					{
						watch->changed = true;
						printf("%s inotify event: %s (mask=0x%08x)\n",
							watch->path.c_str(),
							hasName ? ev->name : "(dir)",
							ev->mask);
					}
				}

				ptr += sizeof(struct inotify_event) + ev->len;
			}
		}

		// len < 0 with errno == EAGAIN/EWOULDBLOCK means queue is empty.
		if (len < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
		{
			printf("inotify read error (errno=%d)\n", errno);
		}
	}

	std::vector<std::string> consumeChangedPaths()
	{
		std::vector<std::string> changedPaths;
		for (auto &watch : watches)
		{
			if (watch.changed)
			{
				changedPaths.push_back(watch.path);
				watch.changed = false;
			}
		}

		return changedPaths;
	}

private:
	Watch *findWatchByDescriptor(int watchDesc)
	{
		for (auto &watch : watches)
		{
			if (watch.watchDesc == watchDesc)
			{
				return &watch;
			}
		}

		return nullptr;
	}

	int inotifyFd = -1;
	std::vector<Watch> watches;
};

} // namespace
#endif

Files::Files(string path, Object *parent) : LeafObject(path, parent)
{
  string name;
  name = "directory"; elements[name] = directoryPtr = std::make_shared<JsonSensor>(path + "/" + name, this, json({}));

}

void Files::initialize()
{
  LeafObject::initialize();
	updateDirectory(true);
}

json Files::processJson(const string &method, json &j, int client)
{
	if (method == "delete")
	{
		if (!j.is_string())
		{
			throw std::runtime_error("delete expects a string path");
		}
		const string partialPath = j.get<string>();

		string error;
		if (!deleteWatchedFile(partialPath, error))
		{
			throw std::runtime_error(error);
		}

		// Refresh immediately so subscribers see deletion without waiting for polling cadence.
		updateDirectory(false);
		return true;
	}

	return LeafObject::processJson(method, j, client);
}

void Files::update(bool sensorsOnly, bool refreshVolatileElements)
{
  if (sensorsOnly)
  {
		// call updateDirectory() every few seconds to check for changes in the monitored directories
			updateDirectory(false); // add timer to rate limit?
  }
  else // non-sensors
  {
  }
}

void Files::updateDirectory(bool bootstrap)
{
	#ifdef __ARM_ARCH
	// Note: external actors are responsible for writing to these directories.
	static const std::vector<std::string> watchedPaths = watchedDirectoryPaths();
	static InotifyDirectoryMonitor monitor(watchedPaths);
	static json directoryState = createEmptyDirectoryState(watchedPaths);

	if (!monitor.initialize())
	{
		return;
	}

	/*
	  Example full directoryState JSON for all watched directories:
	  {
	    "audio-files": {
	      "I Like to Rock.mp3": {
	        "size": "2.35 MB",
	        "timestamp": "2026-08-04T14:23:51Z",
	        "metadata": {}
	      }
	    },
	    "image-files": {
	      "cover.jpg": {
	        "size": "845 KB",
	        "timestamp": "2026-08-04T14:24:10Z",
	        "metadata": {}
	      }
	    },
	    "misc-files": {
	      "notes.txt": {
	        "size": "4.20 KB",
	        "timestamp": "2026-08-04T14:25:02Z",
	        "metadata": {}
	      }
	    }
	  }
	*/

	bool directoryStateChanged = false;
	if (bootstrap)
	{
		for (const auto &path : watchedPaths)
		{
			rebuildDirectoryStateEntry(path, directoryState);
			directoryStateChanged = true;
		}
	}
	else
	{
		monitor.poll();
		const auto changedPaths = monitor.consumeChangedPaths();
		for (const auto &path : changedPaths)
		{
			rebuildDirectoryStateEntry(path, directoryState);
			directoryStateChanged = true;
			printf("directory changed: %s\n", path.c_str());
		}
	}

	if (directoryStateChanged)
	{
		directoryPtr->processJson("set", directoryState);
		directoryPtr->setModified();
	}
	#endif
}

bool Files::deleteWatchedFile(const std::string &partialPath, std::string &error)
{
	#ifdef __ARM_ARCH
	const fs::path relativePath = fs::path(partialPath).lexically_normal();
	if (!isSafeRelativePath(relativePath))
	{
		error = "delete path must be a relative path like 'audio-files/tune.mp3'";
		return false;
	}

	auto partIt = relativePath.begin();
	if (partIt == relativePath.end())
	{
		error = "delete path is empty";
		return false;
	}

	const std::string directoryName = partIt->string();
	++partIt;
	if (partIt == relativePath.end())
	{
		error = "delete path must include a file name";
		return false;
	}

	std::string watchedRoot;
	for (const auto &path : watchedDirectoryPaths())
	{
		if (toDirectoryName(path) == directoryName)
		{
			watchedRoot = path;
			break;
		}
	}

	if (watchedRoot.empty())
	{
		error = "delete path must start with one of: audio-files, image-files, misc-files";
		return false;
	}

	fs::path filePathWithinDirectory;
	for (; partIt != relativePath.end(); ++partIt)
	{
		filePathWithinDirectory /= *partIt;
	}

	if (!isSafeRelativePath(filePathWithinDirectory))
	{
		error = "delete path contains invalid path segments";
		return false;
	}

	const fs::path watchedRootPath = fs::path(watchedRoot).lexically_normal();
	const fs::path targetPath = (watchedRootPath / filePathWithinDirectory).lexically_normal();
	if (!pathHasPrefix(targetPath, watchedRootPath))
	{
		error = "delete path escapes watched directory";
		return false;
	}

	boost::system::error_code existsEc;
	if (!fs::exists(targetPath, existsEc))
	{
		error = "file does not exist";
		return false;
	}

	boost::system::error_code typeEc;
	if (!fs::is_regular_file(targetPath, typeEc))
	{
		error = "delete target is not a regular file";
		return false;
	}

	boost::system::error_code removeEc;
	if (!fs::remove(targetPath, removeEc) || removeEc)
	{
		error = "failed to delete file";
		return false;
	}

	const fs::path metadataPath = fs::path(targetPath.string() + ".metadata.json");
	boost::system::error_code metadataExistsEc;
	if (fs::exists(metadataPath, metadataExistsEc))
	{
		boost::system::error_code metadataTypeEc;
		if (!fs::is_regular_file(metadataPath, metadataTypeEc))
		{
			error = "metadata sidecar exists but is not a regular file";
			return false;
		}

		boost::system::error_code metadataRemoveEc;
		if (!fs::remove(metadataPath, metadataRemoveEc) || metadataRemoveEc)
		{
			error = "failed to delete metadata sidecar";
			return false;
		}
	}

	return true;
	#else
	(void)partialPath;
	error = "delete is only supported on ARM builds";
	return false;
	#endif
}