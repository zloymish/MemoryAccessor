//    MemoryAccessor - A tool for accessing /proc/PID/mem
//    Copyright (C) 2024  zloymish
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <https://www.gnu.org/licenses/>.

/*!
 \file
 \brief Tools source

  A source that contains the realization of Tools class.
*/

#include "tools.h"

#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <array>
#include <cstdio>
#include <memory>
#include <string>
#include <unordered_set>

/*!
 \brief Attach handler to SIGINT signal.
 \param [in] handler Pointer to handler function.
 \return Return value of calling sigaction function (success is 0, error is
 other value).

 Attach handler function to SIGINT, which will be called every time SIGINT is
 received.
*/
int Tools::SetSigint(void (*handler)(int)) const noexcept {
  // Заимствование, источник кода:
  // https://stackoverflow.com/questions/51920435/how-to-call-sigaction-from-c
  // Начало заимствования (присутствуют небольшие изменения):
  struct sigaction sigbreak;
  sigbreak.sa_handler = handler;
  sigemptyset(&sigbreak.sa_mask);
  sigbreak.sa_flags = 0;

  return sigaction(SIGINT, &sigbreak, nullptr);
  // Конец заимствования.
}

/*!
 \brief Do a command in system shell.
 \param [in] command Command in type of std::string.
 \return FILE*, read-only pipe that is stdout of the done command.

 Do a command in a shell by calling function "popen" and return stdout pipe of
 the command.
*/
std::FILE *Tools::ShellCommand(const std::string &command) const noexcept {
  return popen((command /* + " 2>&1"*/).c_str(), "r");
}

// pgrep shows first 15 characters of process name only, but it also accepts
// such cut names when searching for pids, even with -x

/*!
 \brief Get all PIDs existing in the system.
 \return std::unordered_set with all PIDs in pid_t type.

 Get all process IDs that can be found by running "pgrep .+" shell command.
*/
std::unordered_set<pid_t> Tools::GetAllPids() const noexcept {
  std::FILE *pipe{ShellCommand("pgrep .+")};
  std::unordered_set<pid_t> result;
  if (!pipe)
    return result;

  auto buf{std::make_unique<char[]>(buffer_size_)};

  while (std::fgets(buf.get(), buffer_size_, pipe)) {
    std::string s(buf.get());
    if (s.length() > 1) {
      s.pop_back(); // delete end line char
      try {
        result.insert(std::stoul(s));
      } catch (...) {
      }
    }
  }
  pclose(pipe);

  return result;
}

/*!
 \brief Get all names of processes existing in the system.
 \return std::unordered_set with all names in std::string type.

 Get all names of processes that can be found by running "pgrep -l .+" shell
 command.
*/
std::unordered_set<std::string> Tools::GetAllProcessNames() const noexcept {
  std::FILE *pipe{ShellCommand("pgrep -l .+")};
  std::unordered_set<std::string> result;
  if (!pipe)
    return result;

  auto buf{std::make_unique<char[]>(buffer_size_)};

  size_t space_pos{0};
  while (std::fgets(buf.get(), buffer_size_, pipe)) {
    std::string s(buf.get());
    try {
      if ((space_pos = s.find(' ')) != std::string::npos) {
        s.pop_back(); // delete end line char
        s = s.substr(space_pos + 1);
        result.insert(s);
      }
    } catch (...) {
    }
  }
  pclose(pipe);

  return result;
}

/*!
 \brief Get all PIDs by name of the process.
 \param [in] name Name of the process in type of std::string.
 \return std::unordered_set with PIDs in pid_t type.

 Get all process IDs that can be found by running "pgrep -x "process_name""
 shell command.
*/
std::unordered_set<pid_t>
Tools::FindPidsByName(const std::string &name) const noexcept {
  std::FILE *pipe{ShellCommand("pgrep -x \"" + name + "\"")};
  std::unordered_set<pid_t> result;
  if (!pipe)
    return result;

  auto buf{std::make_unique<char[]>(buffer_size_)};

  while (std::fgets(buf.get(), buffer_size_, pipe)) {
    std::string s(buf.get());
    if (s.length() > 1) {
      s.pop_back(); // delete end line char
      try {
        result.insert(std::stoul(s));
      } catch (...) {
      }
    }
  }
  pclose(pipe);

  return result;
}

/*!
 \brief Check if a process with the given PID exists.
 \param [in] pid PID of the process in pid_t type.
 \return 0 if process exists, 1 if process does not exist, 2 if there was an
 error while checking.

 Check if a process with the given process ID exists in the system by checking
 if /proc/PID directory exists.
*/
uint8_t Tools::PidExists(const pid_t &pid) const noexcept {
  struct stat buffer;
  try {
    return static_cast<uint8_t>(
        stat(("/proc/" + std::to_string(pid)).c_str(), &buffer) != 0);
  } catch (...) {
  }
  return 2;
}

/*!
 \brief Check if a process with the given name exists.
 \param [in] pname Name of the process in type of std::string.
 \return 0 if process exists, 1 if process does not exist, 2 if there was an
 error while checking.

 Check if a process with the given process name exists in the system by running
 "pgrep -x "process_name"" shell command.
*/
uint8_t Tools::ProcessExists(const std::string &pname) const noexcept {
  std::FILE *pipe{ShellCommand("pgrep -x \"" + pname + "\"")};
  if (!pipe)
    return 2;

  if (std::fgetc(pipe) != EOF)
    return 0;
  return 1;
}

/*!
 \brief Get permissions stored as uint8_t from std::string.
 \param [in] permissions Permissions stored as std::string, for example, "rwxp".
 Some additional characters after are not prohibited. \return Value where the
 last 4 bits represent permissions (rwxs are 1, others are 0). If the input
 std::string is too short or an unexpected character is found, the return value
 is -1 (255).

 Process permissions of a memory segment from std::string to uint8_t.
*/
uint8_t
Tools::DecodePermissions(const std::string &permissions) const noexcept {
  if (kModesLength > permissions.length())
    return -1;

  uint8_t mode{0}, i{0};

  for (; i < kModesLength - 1; i++) {
    if (permissions[i] == kModes[i])
      mode |= 1 << (kModesLength - 1 - i);
    else if (permissions[i] != '-')
      return -1;
  }

  if (permissions[i] == kModes[i])
    mode |= 1 << (kModesLength - 1 - i);
  else if (permissions[i] != 'p')
    return -1;

  return mode;
}

/*!
 \brief Get permissions stored as std::string from uint8_t.
 \param [in] mode Value, where the last 4 bits represent permissions (rwxs are
 1, others are 0). \return Permissions stored as std::string, for example,
 "rwxp".

 Process permissions of a memory segment from uint8_t to std::string.
*/
std::string Tools::EncodePermissions(const uint8_t &mode) const noexcept {
  std::string permissions;
  for (uint8_t i{0}; i < kModesLength - 1; i++) {
    if (mode & (1 << (kModesLength - 1 - i)))
      permissions.push_back(kModes[i]);
    else
      permissions.push_back('-');
  }
  if (mode & 1)
    permissions.push_back('s');
  else
    permissions.push_back('p');
  return permissions;
}

/*!
 \brief Find differences of given length comparing two arrays of char.
 \param [in] old_str First "old" array of char.
 \param [in] new_str Second "new" array of char.
 \param [in] str_len Length of both arrays.
 \param [in] len Length of different sequences.
 \param [out] done Amount of bytes processed.
 \return std::array of length of 2 containing "old" and
 "new" versions of a changed substring in unique_ptr containers.

 Find first pair of different substrings of given length on equal positions
 comparing 2 given arrays. Each char of the substrings must be different. If
 longer substrings differ, their shorter versions are not returned.
*/
std::array<std::unique_ptr<char[]>, 2>
Tools::FindDifferencesOfLen(const char *old_str, const char *new_str,
                            size_t str_len, size_t &done,
                            const size_t &len) const noexcept {
  if (!str_len || !len || str_len < len)
    return {};

  std::array<std::unique_ptr<char[]>, 2> result{{
      {std::make_unique<char[]>(len)},
      {std::make_unique<char[]>(len)},
  }};
  size_t found_len{0};

  done = 0;

  for (; str_len; str_len--, old_str++, new_str++, done++) {
    if (*old_str != *new_str) {
      if (found_len < len) {
        result[0][found_len] = *old_str;
        result[1][found_len] = *new_str;
      }
      found_len++;
    } else {
      if (found_len == len) {
        return result;
      }
      found_len = 0;
    }
  }

  if (found_len == len)
    return result;
  return {};
}
