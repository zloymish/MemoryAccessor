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
 \brief Tests header

  A header that contains definitions of functions used in tests.
*/

#ifndef MEMORYACCESSOR_TESTING_TESTING_H_
#define MEMORYACCESSOR_TESTING_TESTING_H_

#include <sys/types.h>

#include <cstdint>
#include <ios>
#include <sstream>
#include <streambuf>
#include <string>
#include <vector>

#include "segmentinfo.h"

/*!
 \brief Fields used in tests.

  Functions and variables that are used in tests and are related to a specific
 component of project: console, tools, etc.
*/
namespace memoryaccessor_testing {

/*!
 \brief Fields used in testing tools.

  Functions and variables that are used in tests related to Tools class.
*/
namespace tools {

extern "C" {
void SIGINT_handler(int signum);
}

std::string get_self_name();

} // namespace tools

/*!
 \brief Fields used in testing memoryaccessor.

  Functions and variables that are used in tests related to MemoryAccessor
 class.
*/
namespace memoryaccessor {

pid_t get_paused_child();
void read_urandom(char *dst, size_t amount);
bool are_arrays_same(const char *arr1, const char *arr2, const size_t &size);
size_t seg_num_by_name(const std::string &name,
                       const std::vector<SegmentInfo> &infos);
size_t find_gap_start(const std::vector<SegmentInfo> &infos);

} // namespace memoryaccessor

/*!
 \brief Fields used in testing console.

  Functions and variables that are used in tests related to Console class.
*/
namespace console {

std::streambuf *replace_streambuf(std::ios &stream,
                                  const std::ostringstream &oss);
void test_handle_command(std::ostringstream &oss, const std::string &command,
                         const std::string &result_substr);
std::string size_t_to_hex(const size_t &num, size_t width = 0);

} // namespace console

/*!
 \brief Fields used in testing argvparser.

  Functions and variables that are used in tests related to ArgvParser class.
*/
namespace argvparser {

extern "C" {
void exit_handler();
}

void test_parse_argv(const int &argc, char **argv,
                     const std::string &result_substr);

} // namespace argvparser

} // namespace memoryaccessor_testing

#endif // MEMORYACCESSOR_TESTING_TESTING_H_
