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
 \brief ProcessApi source

  A source that contains the realization of ProcessApi class.
*/

#include "processapi.h"

#include <sys/stat.h>
#include <sys/types.h>

#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <unordered_set>

// pgrep shows first 15 characters of process name only, but it also accepts
// such cut names when searching for pids, even with -x

/*!
 \brief Get all PIDs existing in the system.
 \return std::unordered_set with all PIDs in pid_t type.

 Get all process IDs that can be found by running "pgrep .+" shell command.
*/
std::unordered_set<pid_t> ProcessApi::GetAllPids() const noexcept {
  std::FILE *pipe{ShellCommand("pgrep .+ 2>/dev/null")};
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
std::unordered_set<std::string> ProcessApi::GetAllProcessNames() const noexcept {
  std::FILE *pipe{ShellCommand("pgrep -l .+ 2>/dev/null")};
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
ProcessApi::FindPidsByName(const std::string &name) const noexcept {
  std::FILE *pipe{ShellCommand("pgrep -x \"" + name + "\" 2>/dev/null")};
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
uint8_t ProcessApi::PidExists(const pid_t &pid) const noexcept {
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
uint8_t ProcessApi::ProcessExists(const std::string &pname) const noexcept {
  std::FILE *pipe{ShellCommand("pgrep -x \"" + pname + "\" 2>/dev/null")};
  if (!pipe)
    return 2;

  if (std::fgetc(pipe) != EOF)
    return 0;
  return 1;
}

/*!
 \brief Do a command in system shell.
 \param [in] command Command in type of std::string.
 \return FILE*, read-only pipe that is stdout of the done command.

 Do a command in a shell by calling function "popen" and return stdout pipe of
 the command.
*/
std::FILE *ProcessApi::ShellCommand(const std::string &command) const noexcept {
  return popen((command /* + " 2>&1"*/).c_str(), "r");
}