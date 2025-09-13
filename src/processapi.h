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
 \brief ProcessApi header

 A header that contains the definition of ProcessApi class.
*/

#ifndef MEMORYACCESSOR_SRC_PROCESSAPI_H_
#define MEMORYACCESSOR_SRC_PROCESSAPI_H_

#include <sys/types.h>

#include <cstdint>
#include <cstdio>
#include <string>
#include <unordered_set>

/*!
 \brief A class with functionality to work with system processes.

 This class provides a set of functions that perform work with processes 
 currently existing in system. It contains functions such as finding PIDs by 
 name of the process, getting all PIDs/process names and so on.
*/
class ProcessApi {
public:
  /*!
   \brief Set buffer size of an instance.
   \param [in] buffer_size Desired buffer size in bytes.

   Set buffer size, a number of bytes that are allocated when needed.
  */
  void SetBufferSize(const size_t &buffer_size) { buffer_size_ = buffer_size; }

  std::unordered_set<pid_t> GetAllPids() const noexcept;
  std::unordered_set<std::string> GetAllProcessNames() const noexcept;
  std::unordered_set<pid_t>
  FindPidsByName(const std::string &name) const noexcept;
  uint8_t PidExists(const pid_t &pid) const noexcept;
  uint8_t ProcessExists(const std::string &pname) const noexcept;

private:
  std::FILE *ShellCommand(const std::string &command) const noexcept;

  size_t buffer_size_{
      0x1000}; //!< Size of buffers used (less than 128 may cause bugs).
};

#endif // MEMORYACCESSOR_SRC_PROCESSAPI_H_
