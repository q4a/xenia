/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2017 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/base/assert.h"
#include "xenia/base/filesystem.h"
#include "xenia/base/string.h"

#include <dirent.h>
#include <fcntl.h>
#include <ftw.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

namespace xe {
namespace filesystem {

std::wstring GetExecutablePath() {
  char buff[FILENAME_MAX] = "";
  readlink("/proc/self/exe", buff, FILENAME_MAX);
  std::string s(buff);
  return to_wstring(s);
}

std::wstring GetExecutableFolder() {
  auto path = GetExecutablePath();
  return xe::find_base_path(path);
}

std::wstring GetUserFolder() {
  // get preferred data home
  char* dataHome = std::getenv("XDG_DATA_HOME");

  // if XDG_DATA_HOME not set, fallback to HOME directory
  if (dataHome == nullptr) {
    dataHome = std::getenv("HOME");
  } else {
    std::string home(dataHome);
    return to_wstring(home);
  }

  // if HOME not set, fall back to this
  if (dataHome == nullptr) {
    struct passwd pw1;
    struct passwd* pw;
    char buf[4096];  // could potentially lower this
    getpwuid_r(getuid(), &pw1, buf, sizeof(buf), &pw);
    assert_true(&pw1 == pw);  // sanity check
    dataHome = pw->pw_dir;
  }

  std::string home(dataHome);
  return xe::join_paths(to_wstring(home), L".local/share");
}

bool PathExists(const std::wstring& path) {
  struct stat st;
  return stat(xe::to_string(path).c_str(), &st) == 0;
}

FILE* OpenFile(const std::wstring& path, const char* mode) {
  auto fixed_path = xe::fix_path_separators(path);
  return fopen(xe::to_string(fixed_path).c_str(), mode);
}

bool CreateFolder(const std::wstring& path) {
  return mkdir(xe::to_string(path).c_str(), 0774);
}

static int removeCallback(const char* fpath, const struct stat* sb,
                          int typeflag, struct FTW* ftwbuf) {
  int rv = remove(fpath);
  return rv;
}

bool DeleteFolder(const std::wstring& path) {
  return nftw(xe::to_string(path).c_str(), removeCallback, 64,
              FTW_DEPTH | FTW_PHYS) == 0;
}

static uint64_t convertUnixtimeToWinFiletime(const timespec& unixtime) {
  // Linux uses number of nanoseconds since 1/1/1970, and Windows uses
  // number of nanoseconds since 1/1/1601
  // so we add the number of nanoseconds from 1601 to 1970
  // see https://msdn.microsoft.com/en-us/library/ms724228
  uint64_t filetime =
      (unixtime.tv_sec * 10000000) + unixtime.tv_nsec + 116444736000000000;
  return filetime;
}

bool IsFolder(const std::wstring& path) {
  struct stat st;
  if (stat(xe::to_string(path).c_str(), &st) == 0) {
    if (S_ISDIR(st.st_mode)) return true;
  }
  return false;
}

bool CreateFile(const std::wstring& path) {
  int file = creat(xe::to_string(path).c_str(), 0774);
  if (file >= 0) {
    close(file);
    return true;
  }
  return false;
}

bool DeleteFile(const std::wstring& path) {
  return xe::to_string(path).c_str() == nullptr;
}

class PosixFileHandle : public FileHandle {
 public:
  PosixFileHandle(std::wstring path, int handle)
      : FileHandle(std::move(path)), handle_(handle) {}
  ~PosixFileHandle() override {
    close(handle_);
    handle_ = -1;
  }
  bool Read(size_t file_offset, void* buffer, size_t buffer_length,
            size_t* out_bytes_read) override {
    ssize_t out = pread(handle_, buffer, buffer_length, file_offset);
    *out_bytes_read = out;
    return out >= 0;
  }
  bool Write(size_t file_offset, const void* buffer, size_t buffer_length,
             size_t* out_bytes_written) override {
    ssize_t out = pwrite(handle_, buffer, buffer_length, file_offset);
    *out_bytes_written = out;
    return out >= 0;
  }
  bool SetLength(size_t length) override {
    return ftruncate(handle_, length) >= 0;
  }
  void Flush() override { fsync(handle_); }

 private:
  int handle_ = -1;
};

std::unique_ptr<FileHandle> FileHandle::OpenExisting(std::wstring path,
                                                     uint32_t desired_access) {
  int open_access = 0;
  if (desired_access & FileAccess::kGenericRead) {
    open_access |= O_RDONLY;
  }
  if (desired_access & FileAccess::kGenericWrite) {
    open_access |= O_WRONLY;
  }
  if (desired_access & FileAccess::kGenericExecute) {
    open_access |= O_RDONLY;
  }
  if (desired_access & FileAccess::kGenericAll) {
    open_access |= O_RDWR;
  }
  if (desired_access & FileAccess::kFileReadData) {
    open_access |= O_RDONLY;
  }
  if (desired_access & FileAccess::kFileWriteData) {
    open_access |= O_WRONLY;
  }
  if (desired_access & FileAccess::kFileAppendData) {
    open_access |= O_APPEND;
  }
  int handle = open(xe::to_string(path).c_str(), open_access);
  if (handle == -1) {
    // TODO(benvanik): pick correct response.
    return nullptr;
  }
  return std::make_unique<PosixFileHandle>(path, handle);
}

bool GetInfo(const std::wstring& path, FileInfo* out_info) {
  struct stat st;
  if (stat(xe::to_string(path).c_str(), &st) == 0) {
    if (S_ISDIR(st.st_mode)) {
      out_info->type = FileInfo::Type::kDirectory;
      // On Linux st.st_size can have non-zero size (generally 4096) so make 0
      out_info->total_size = 0;
    } else {
      out_info->type = FileInfo::Type::kFile;
      out_info->total_size = st.st_size;
    }
    out_info->name = find_name_from_path(path);
    out_info->path = find_base_path(path);
    out_info->create_timestamp = convertUnixtimeToWinFiletime(st.st_ctim);
    out_info->access_timestamp = convertUnixtimeToWinFiletime(st.st_atim);
    out_info->write_timestamp = convertUnixtimeToWinFiletime(st.st_mtim);
    return true;
  }
  return false;
}

std::vector<FileInfo> ListFiles(const std::wstring& path) {
  std::vector<FileInfo> result;

  DIR* dir = opendir(xe::to_string(path).c_str());
  if (!dir) {
    return result;
  }

  while (auto ent = readdir(dir)) {
    if (std::strcmp(ent->d_name, ".") == 0 ||
        std::strcmp(ent->d_name, "..") == 0) {
      continue;
    }

    FileInfo info;

    info.name = xe::to_wstring(ent->d_name);
    struct stat st;
    auto full_path = xe::to_string(xe::join_paths(path, info.name));
    auto ret = stat(full_path.c_str(), &st);
    assert_zero(ret);
    info.create_timestamp = convertUnixtimeToWinFiletime(st.st_ctim);
    info.access_timestamp = convertUnixtimeToWinFiletime(st.st_atim);
    info.write_timestamp = convertUnixtimeToWinFiletime(st.st_mtim);
    if (ent->d_type == DT_DIR) {
      info.type = FileInfo::Type::kDirectory;
      info.total_size = 0;
    } else {
      info.type = FileInfo::Type::kFile;
      info.total_size = static_cast<size_t>(st.st_size);
    }
    result.push_back(info);
  }

  return result;
}

}  // namespace filesystem
}  // namespace xe
