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
 \brief Tools header

 A header that contains the definition of Tools class.
*/

#ifndef MEMORYACCESSOR_SRC_TOOLS_H_
#define MEMORYACCESSOR_SRC_TOOLS_H_

#include <sys/types.h>

#include <array>
#include <cstdio>
#include <memory>
#include <string>
#include <unordered_set>

/*!
 \brief A class with various tools that are independent or depend on operating
 system.

 This class provides a set of functions that are useful in some parts of the
 project, but cannot be attributed to any existing category. These functions do
 not depend on any parts of the program. The class includes such functionality
 as working with signals (SIGINT), getting terminal window size, making shell
 commands, comparing memory arrays and so on.
*/
class Tools {
public:
  /*!
   \brief Set buffer size of an instance.
   \param [in] buffer_size Desired buffer size in bytes.

   Set buffer size, a number of bytes that are allocated when needed.
  */
  void SetBufferSize(const size_t &buffer_size) { buffer_size_ = buffer_size; }

  std::FILE *ShellCommand(const std::string &command) const noexcept;
  std::unordered_set<pid_t> GetAllPids() const noexcept;
  std::unordered_set<std::string> GetAllProcessNames() const noexcept;
  std::unordered_set<pid_t>
  FindPidsByName(const std::string &name) const noexcept;
  uint8_t PidExists(const pid_t &pid) const noexcept;
  uint8_t ProcessExists(const std::string &pname) const noexcept;

private:
  size_t buffer_size_{
      0x1000}; //!< Size of buffers used (less than 128 may cause bugs).
};

#endif // MEMORYACCESSOR_SRC_TOOLS_H_
