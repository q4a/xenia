/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2015 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/base/filesystem.h"

#include <algorithm>

namespace xe {
namespace filesystem {

std::string CanonicalizePath(const std::string& original_path) {
  auto path_sep = '\\';
  std::string path(xe::fix_path_separators(original_path, path_sep));

  std::vector<std::string::size_type> path_breaks;

  std::string::size_type pos(path.find_first_of(path_sep));
  std::string::size_type pos_n(std::string::npos);

  while (pos != std::string::npos) {
    if ((pos_n = path.find_first_of(path_sep, pos + 1)) == std::string::npos) {
      pos_n = path.size();
    }

    auto diff(pos_n - pos);
    switch (diff) {
      case 0:
        pos_n = std::string::npos;
        break;
      case 1:
        // Duplicate separators.
        path.erase(pos, 1);
        pos_n -= 1;
        break;
      case 2:
        // Potential marker for current directory.
        if (path[pos + 1] == '.') {
          path.erase(pos, 2);
          pos_n -= 2;
        } else {
          path_breaks.push_back(pos);
        }
        break;
      case 3:
        // Potential marker for parent directory.
        if (path[pos + 1] == '.' && path[pos + 2] == '.') {
          if (path_breaks.empty()) {
            // Ensure we don't override the device name.
            std::string::size_type loc(path.find_first_of(':'));
            auto req(pos + 3);
            if (loc == std::string::npos || loc > req) {
              path.erase(0, req);
              pos_n -= req;
            } else {
              path.erase(loc + 1, req - (loc + 1));
              pos_n -= req - (loc + 1);
            }
          } else {
            auto last(path_breaks.back());
            auto last_diff((pos + 3) - last);
            path.erase(last, last_diff);
            pos_n = last;
            // Also remove path reference.
            path_breaks.erase(path_breaks.end() - 1);
          }
        } else {
          path_breaks.push_back(pos);
        }
        break;

      default:
        path_breaks.push_back(pos);
        break;
    }

    pos = pos_n;
  }

  // Remove trailing separator.
  if (!path.empty() && path.back() == path_sep) {
    path.erase(path.size() - 1);
  }

  // Final sanity check for dead paths.
  if ((path.size() == 1 && (path[0] == '.' || path[0] == path_sep)) ||
      (path.size() == 2 && path[0] == '.' && path[1] == '.')) {
    return "";
  }

  return path;
}

bool CreateParentFolder(const std::wstring& path) {
  auto fixed_path = xe::fix_path_separators(path);
  auto base_path = xe::find_base_path(fixed_path);
  if (!base_path.empty() && !PathExists(base_path)) {
    return CreateFolder(base_path);
  } else {
    return true;
  }
}

}  // namespace filesystem
}  // namespace xe
