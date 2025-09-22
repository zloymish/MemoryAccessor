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
 \brief Tests source

  A source that contains tests of the project.
*/

#define DOCTEST_CONFIG_IMPLEMENT
#define DOCTEST_CONFIG_NO_FILTERS

#include "project_test.h"

#include <doctest/doctest.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm> // std::min
#include <array>
#include <bit> // std::bit_width
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <ios>
#include <iostream>
#include <memory>
#include <sstream>
#include <streambuf>
#include <string>
#include <unordered_set>
#include <vector>

#include "argvparser.h"
#include "console.h"
#include "hexviewer.h"
#include "memoryaccessor.h"
#include "processapi.h"
#include "segmentinfo.h"

int argc{0};          //!< Number of arguments sent with the program.
char **argv{nullptr}; //!< Array of arguments sent with the program.

const size_t kBufferSize{0x1000}; //!< Size of buffers used.

ProcessApi process_api; //!< ProcessApi instance to perform testing on.
MemoryAccessor
    memory_accessor(&process_api); //!< MemoryAccessor instance to perform testing on.
HexViewer hex_viewer;       //!< HexViewer instance to perform testing on.
Console console(&memory_accessor, &hex_viewer,
                &process_api); //!< Console instance to perform testing on.
ArgvParser
    argv_parser(&console); //!< ArgvParser instance to perform testing on.
SegmentInfo segment_info; //!< SegmentInfo instance to perform testing on.

pid_t child{0}; //!< pid_t of the only child of the process. To create a new child, the old one needs to be killed. If there is no child process, set to 0.

/*!
 \brief Main function.
 \param [in] argc Arguments count.
 \param [in] argv Array of C strings representing arguments.
 \return Exit code of the program.

 Sets values to global variables argc and argv, sets equal buffer_size and run
 tests.
*/
int main(int argc, char **argv) {
  ::argc = argc;
  ::argv = argv;

  process_api.SetBufferSize(kBufferSize);
  console.SetBufferSize(kBufferSize);

  doctest::Context context;

  context.applyCommandLine(argc, argv);

  int res = context.run();
  
  if (context.shouldExit())
    return res;
  
  memoryaccessor_testing::memoryaccessor::clear_child();

  int client_stuff_return_code = 0;

  return res + client_stuff_return_code;
}

TEST_SUITE_BEGIN("SegmentInfo");

TEST_CASE("Decode permissions: return zero") {
  REQUIRE(segment_info.DecodePermissions("---p") == 0);
  REQUIRE(segment_info.mode_ == 0);
}

TEST_CASE("Decode permissions: full") {
  REQUIRE(segment_info.DecodePermissions("rwxs") == 0);
  REQUIRE(segment_info.mode_ == 15);
}

TEST_CASE("Decode permissions: various") {
  REQUIRE(segment_info.DecodePermissions("--xp") == 0);
  REQUIRE(segment_info.mode_ == 2);

  REQUIRE(segment_info.DecodePermissions("-w-p") == 0);
  REQUIRE(segment_info.mode_ == 4);

  REQUIRE(segment_info.DecodePermissions("r--p") == 0);
  REQUIRE(segment_info.mode_ == 8);
  
  REQUIRE(segment_info.DecodePermissions("r--s") == 0);
  REQUIRE(segment_info.mode_ == 9);
  
  REQUIRE(segment_info.DecodePermissions("r-xp") == 0);
  REQUIRE(segment_info.mode_ == 10);
}

TEST_CASE("Decode permissions: long") {
  REQUIRE(segment_info.DecodePermissions("rwxp123456") == 0);
  REQUIRE(segment_info.mode_ == 14);
}

TEST_CASE("Decode permissions: short") {
  REQUIRE(segment_info.DecodePermissions("r") == 1);
}

TEST_CASE("Decode permissions: invalid") {
  REQUIRE(segment_info.DecodePermissions("rwxa") == 1);
}

TEST_CASE("Encode permissions: zero") {
  segment_info.mode_ = 0;
  REQUIRE(segment_info.EncodePermissions() == "---p");
}

TEST_CASE("Encode permissions: full") {
  segment_info.mode_ = 15;
  REQUIRE(segment_info.EncodePermissions() == "rwxs");
}

TEST_CASE("Encode permissions: various") {
  segment_info.mode_ = 1;
  REQUIRE(segment_info.EncodePermissions() == "---s");
  
  segment_info.mode_ = 6;
  REQUIRE(segment_info.EncodePermissions() == "-wxp");
  
  segment_info.mode_ = 7;
  REQUIRE(segment_info.EncodePermissions() == "-wxs");
  
  segment_info.mode_ = 11;
  REQUIRE(segment_info.EncodePermissions() == "r-xs");
  
  segment_info.mode_ = 13;
  REQUIRE(segment_info.EncodePermissions() == "rw-s");
}

TEST_CASE("Encode permissions: additional bits") {
  segment_info.mode_ = 20;
  REQUIRE(segment_info.EncodePermissions() == "-w-p");
}

TEST_SUITE_END();

TEST_SUITE_BEGIN("ProcessApi");

// namespace memoryaccessor_testing::process_api {

// bool check_sigint{false}; //!< @cond Shows if SIGINT was sent. @endcond

// extern "C" {
/*! @cond
 \brief SIGINT handler.
 \param [in] signum Number of handled signal.

 A function that should be called when SIGINT is sent. Sets check_sigint variable
to true.
 @endcond
*/
// void SIGINT_handler(int signum) { check_sigint = true; }
// }

// } // namespace memoryaccessor_testing::process_api

// TEST_CASE("Set SIGINT") {
//   WARN(process_api.SetSigint(memoryaccessor_testing::process_api::SIGINT_handler) == 0);
//   WARN(raise(SIGINT) == 0);
//   CHECK(memoryaccessor_testing::process_api::check_sigint == true);
//   WARN(process_api.SetSigint(SIG_DFL) == 0);
//   memoryaccessor_testing::process_api::check_sigint = false;
// }

// TEST_CASE("Set SIGINT to default") {
//   WARN(process_api.SetSigint(SIG_DFL) == 0);
//   struct sigaction sa;
//   WARN(sigaction(SIGINT, NULL, &sa) == 0);
//   CHECK(sa.sa_handler == SIG_DFL);
// }

// TEST_CASE("Shell command: echo") {
//   FILE *pipe{process_api.ShellCommand("echo abcd")};
//   auto buf{std::make_unique<char[]>(kBufferSize)};
//   WARN(std::fgets(buf.get(), kBufferSize, pipe) != 0);
//   REQUIRE(std::strncmp(buf.get(), "abcd", 4) == 0);
//   WARN(pclose(pipe) == 0);
// }

// TEST_CASE("Shell empty command") {
//   FILE *pipe{process_api.ShellCommand("")};
//   REQUIRE(std::fgetc(pipe) == EOF);
//   WARN(pclose(pipe) == 0);
// }

// TEST_CASE("Shell non-existent command") {
//   FILE *pipe{process_api.ShellCommand("cfsvmpkmcsomcsfmvisf 2>/dev/null")};
//   REQUIRE(std::fgetc(pipe) == EOF);
//   WARN(pclose(pipe) != 0);
// }

TEST_CASE("Get all PIDs including self") {
  auto all_pids = process_api.GetAllPids();
  pid_t self_pid{getpid()};
  CHECK(all_pids.contains(self_pid));
}

TEST_CASE("Get all PIDs (amount non-equal zero)") {
  auto all_pids = process_api.GetAllPids();
  CHECK(all_pids.size() != 0);
}

namespace memoryaccessor_testing::process_api {

/*!
 \brief Get name of the current process.
 \return Name of the process in type std::string.

  Get name of the current process from /proc/self/status. In this "file" names
 longer than 15 characters are cut, such shortened names are also used in pgrep.
*/
std::string get_self_name() {
  std::ifstream proc_status;
  proc_status.open("/proc/self/status");
  WARN(proc_status.good());

  std::string self_name;
  proc_status >> self_name >> self_name;
  WARN(self_name.length() <= 15); // pgrep works normally with <= 15 char names

  return self_name;
}

} // namespace memoryaccessor_testing::process_api

TEST_CASE("Get all process names including self") {
  auto all_names = process_api.GetAllProcessNames();
  std::string self_name{memoryaccessor_testing::process_api::get_self_name()};
  CHECK(all_names.contains(self_name));
}

TEST_CASE("Get all process names (amount non-equal zero)") {
  auto all_names = process_api.GetAllProcessNames();
  CHECK(all_names.size() != 0);
}

TEST_CASE("PID exists: self") {
  pid_t self_pid{getpid()};
  REQUIRE(process_api.PidExists(self_pid) == 0);
}

namespace memoryaccessor_testing::process_api {

constexpr pid_t max_pid_t{~(pid_t)0 > 0 ? ~(pid_t)0 : ~(1 << (sizeof(pid_t) * 8 - 1))}; //!< Maximum positive value of pid_t, whether it is signed or unsigned.

} // namespace memoryaccessor_testing::process_api

TEST_CASE("PID does not exist") {
  REQUIRE(process_api.PidExists(memoryaccessor_testing::process_api::max_pid_t) == 1);
}

TEST_CASE("Process exists: self name") {
  REQUIRE(process_api.ProcessExists(memoryaccessor_testing::process_api::get_self_name()) ==
          0);
}

TEST_CASE("Process with name does not exist") {
  REQUIRE(process_api.ProcessExists(std::string(16, 'a')) ==
          1); // using pgrep limit to 15 chars
}

// TEST_CASE("Find differences: zeros") {
//   size_t done{0};
//   auto diffs = process_api.FindDifferencesOfLen(nullptr, nullptr, 0, done, 1);
//   REQUIRE(diffs[0] == nullptr);
//   REQUIRE(diffs[1] == nullptr);
//   REQUIRE(done == 0);
//   diffs = process_api.FindDifferencesOfLen(nullptr, nullptr, 1, done, 0);
//   REQUIRE(diffs[0] == nullptr);
//   REQUIRE(diffs[1] == nullptr);
//   REQUIRE(done == 0);
// }

// TEST_CASE("Find differences: size less than length") {
//   size_t done{0};
//   auto diffs = process_api.FindDifferencesOfLen(nullptr, nullptr, 1, done, 2);
//   REQUIRE(diffs[0] == nullptr);
//   REQUIRE(diffs[1] == nullptr);
//   REQUIRE(done == 0);
// }

// TEST_CASE("Find differences: one") {
//   std::string s1{"1241"}, s2{"1351"};
//   size_t done{0};
//   auto diffs = process_api.FindDifferencesOfLen(s1.c_str(), s2.c_str(), 4, done, 2);
//   REQUIRE(std::strncmp(diffs[0].get(), "24", 2) == 0);
//   REQUIRE(done == 3);
// }

// TEST_CASE("Find differences: two") {
//   std::string s1{"1abc2def3"}, s2{"1fed2cba3"};
//   size_t done{0};
//   auto diffs = process_api.FindDifferencesOfLen(s1.c_str(), s2.c_str(), 9, done, 3);
//   REQUIRE(std::strncmp(diffs[1].get(), "fed", 3) == 0);
//   REQUIRE(done == 4);
//   diffs = process_api.FindDifferencesOfLen(s1.c_str() + done, s2.c_str() + done,
//                                      9 - done, done, 3);
//   REQUIRE(std::strncmp(diffs[0].get(), "def", 3) == 0);
//   REQUIRE(done == 4);
// }

// TEST_CASE("Find differences: diff too long") {
//   std::string s1{"abcdefg"}, s2{"hijklmn"};
//   size_t done{0};
//   auto diffs = process_api.FindDifferencesOfLen(s1.c_str(), s2.c_str(), 7, done, 6);
//   REQUIRE(diffs[0] == nullptr);
//   REQUIRE(diffs[1] == nullptr);
//   REQUIRE(done == 7);
// }

// TEST_CASE("Find differences: full arr") {
//   std::string s1{"abcdefg"}, s2{"hijklmn"};
//   size_t done{0};
//   auto diffs = process_api.FindDifferencesOfLen(s1.c_str(), s2.c_str(), 7, done, 7);
//   REQUIRE(std::strncmp(diffs[0].get(), "abcdefg", 7) == 0);
//   REQUIRE(std::strncmp(diffs[1].get(), "hijklmn", 7) == 0);
//   REQUIRE(done == 7);
// }

// TEST_CASE("Find differences: seq length 1") {
//   std::string s1{"1a1a1a1a"}, s2{"1b1b1b1b"};
//   size_t done{0};
//   for (size_t i{0}; i < 8; i += 2) {
//     auto diffs =
//         process_api.FindDifferencesOfLen(s1.c_str() + i, s2.c_str() + i, 2, done, 1);
//     REQUIRE(diffs[0][0] == 'a');
//     REQUIRE(diffs[1][0] == 'b');
//     REQUIRE(done == 2);
//   }
// }

TEST_SUITE_END();

TEST_SUITE_BEGIN("MemoryAccessor");

TEST_CASE("Set PID") {
  MemoryAccessor::ErrorCode err{MemoryAccessor::ErrorCode::kNoError};
  
  err = memory_accessor.SetPid(1);
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  pid_t pid{0};
  err = memory_accessor.GetPid(pid);
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  REQUIRE(pid == 1);
  
  err = memory_accessor.CheckPid();
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
}

TEST_CASE("Set non-existent PID") {
  MemoryAccessor::ErrorCode err{MemoryAccessor::ErrorCode::kNoError};
  
  err = memory_accessor.SetPid(memoryaccessor_testing::process_api::max_pid_t);
  REQUIRE(err == MemoryAccessor::ErrorCode::kPidNotExistErr);
  
  pid_t pid{0};
  err = memory_accessor.GetPid(pid);
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  REQUIRE(pid != memoryaccessor_testing::process_api::max_pid_t);
}

TEST_CASE("Parse maps: self") {
  MemoryAccessor::ErrorCode err{MemoryAccessor::ErrorCode::kNoError};
  
  err = memory_accessor.SetPid(getpid());
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  err = memory_accessor.ParseMaps();
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);

  REQUIRE(memory_accessor.special_segment_found_.size() != 0);
  REQUIRE(memory_accessor.segment_infos_.size() != 0);
}

TEST_CASE("Parse maps: PID not set") {
  MemoryAccessor::ErrorCode err{MemoryAccessor::ErrorCode::kNoError};
  
  memory_accessor.Reset();
  err = memory_accessor.ParseMaps();
  REQUIRE(err == MemoryAccessor::ErrorCode::kPidNotSetErr);
}

TEST_CASE("Get all segment names") {
  MemoryAccessor::ErrorCode err{MemoryAccessor::ErrorCode::kNoError};
  
  err = memory_accessor.SetPid(getpid());
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  err = memory_accessor.ParseMaps();
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);

  auto all_seg_names = memory_accessor.GetAllSegmentNames();
  REQUIRE(all_seg_names.size() != 0);
  // REQUIRE(all_seg_names.contains("[heap]"));
}

TEST_CASE("Get zero segment names with no PID") {
  memory_accessor.Reset();
  auto all_seg_names = memory_accessor.GetAllSegmentNames();
  REQUIRE(all_seg_names.size() == 0);
}

TEST_CASE("Address in segment") {
  MemoryAccessor::ErrorCode err{MemoryAccessor::ErrorCode::kNoError};
  
  err = memory_accessor.SetPid(getpid());
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  err = memory_accessor.ParseMaps();
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  size_t seg_num{0};
  err = memory_accessor.AddressInSegment(memory_accessor.segment_infos_[0].start_, seg_num);
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  REQUIRE(seg_num == 0);
}

TEST_CASE("Address not in segment") {
  MemoryAccessor::ErrorCode err{MemoryAccessor::ErrorCode::kNoError};
  
  memory_accessor.Reset();
  size_t num{0};
  
  err = memory_accessor.AddressInSegment(0, num);
  REQUIRE(err == MemoryAccessor::ErrorCode::kAddressNotInSegmentErr);
}

TEST_CASE("Check segment number: positive") {
  MemoryAccessor::ErrorCode err{MemoryAccessor::ErrorCode::kNoError};
  
  err = memory_accessor.SetPid(getpid());
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  err = memory_accessor.ParseMaps();
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  err = memory_accessor.CheckSegNum(0);
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
}

TEST_CASE("Check segment number: negative") {
  MemoryAccessor::ErrorCode err{MemoryAccessor::ErrorCode::kNoError};
  
  err = memory_accessor.SetPid(getpid());
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  err = memory_accessor.CheckSegNum(0);
  REQUIRE(err == MemoryAccessor::ErrorCode::kSegmentNotExistErr);
}

TEST_CASE("Check segment number: PID not set") {
  MemoryAccessor::ErrorCode err{MemoryAccessor::ErrorCode::kNoError};
  
  memory_accessor.Reset();
  err = memory_accessor.CheckSegNum(0);
  REQUIRE(err == MemoryAccessor::ErrorCode::kPidNotSetErr);
}

TEST_CASE("Reset segments") {
  memory_accessor.ResetSegments();
  REQUIRE(memory_accessor.segment_infos_.size() == 0);
  REQUIRE(memory_accessor.special_segment_found_.size() == 0);
}

TEST_CASE("Double Reset segments") {
  memory_accessor.ResetSegments();
  memory_accessor.ResetSegments();
  REQUIRE(memory_accessor.segment_infos_.size() == 0);
  REQUIRE(memory_accessor.special_segment_found_.size() == 0);
}

TEST_CASE("Reset") {
  MemoryAccessor::ErrorCode err{MemoryAccessor::ErrorCode::kNoError};
  
  memory_accessor.Reset();
  
  err = memory_accessor.CheckPid();
  REQUIRE(err == MemoryAccessor::ErrorCode::kPidNotSetErr);
  
  REQUIRE(memory_accessor.segment_infos_.size() == 0);
  REQUIRE(memory_accessor.special_segment_found_.size() == 0);
}

TEST_CASE("Double Reset") {
  MemoryAccessor::ErrorCode err{MemoryAccessor::ErrorCode::kNoError};
  
  memory_accessor.Reset();
  memory_accessor.Reset();
  
  err = memory_accessor.CheckPid();
  REQUIRE(err == MemoryAccessor::ErrorCode::kPidNotSetErr);
  
  REQUIRE(memory_accessor.segment_infos_.size() == 0);
  REQUIRE(memory_accessor.special_segment_found_.size() == 0);
}

namespace memoryaccessor_testing::memoryaccessor {

/*!
 \brief Properly clear objects related to child process.

  Do Reset() in memory_accessor, send SIGKILL to the child process and set child variable to 0.
*/
void clear_child() {
  if (child != 0) {
    memory_accessor.Reset();
    kill(child, SIGKILL);
    child = 0;
  }
}

/*!
 \brief Create paused child process and save its PID in the global variable child.

  Check the global variable child, if it is not set to 0, kill the process with the PID child. Then in any way fork the process, save the PID to child variable and run pause() as the child.
*/
void get_paused_child() {
  clear_child();
  
  child = fork();
  if (child == 0) { // is a child
    pause();
  } else if (child == -1) {
    REQUIRE(child != -1);
  }
}

/*!
 \brief Read data from /dev/urandom.
 \param [out] dst Destination to store data.
 \param [in] amount Amount of data in bytes.

  Read some amount of bytes from /dev/urandom to specified destination.
*/
void read_urandom(char *dst, size_t amount) {
  std::ifstream urandom;
  urandom.open("/dev/urandom");
  REQUIRE(urandom.good());
  urandom.read(dst, amount);
}

/*!
 \brief Check if 2 arrays are same.
 \param [in] arr1 1st array.
 \param [in] arr2 2nd array.
 \param [in] size Size of each of arrays.
 \return true if arrays are same, false otherwise.

  Check if 2 arrays of char with equal length are same and return true or false.
*/
bool are_arrays_same(const char *arr1, const char *arr2, const size_t &size) {
  for (size_t n{0}; n < size; n++)
    if (arr1[n] != arr2[n])
      return false;
  return true;
}

/*!
 \brief Find segment number by name.
 \param [in] name Name of the segment.
 \param [in] infos std::vector of SegmentInfo instances to search.
 \return Number starting from 0 if found, SIZE_MAX otherwise.

  Find segment number in std::vector of SegmentInfo by name of the segment.
*/
size_t seg_num_by_name(const std::string &name,
                       const std::vector<SegmentInfo> &infos) {
  size_t size{infos.size()};
  for (size_t i{0}; i < size; i++)
    if (infos[i].path_ == name)
      return i;
  return SIZE_MAX;
}

} // namespace memoryaccessor_testing::memoryaccessor

TEST_CASE("Read segment to array and compare to initial array") {
  MemoryAccessor::ErrorCode err{MemoryAccessor::ErrorCode::kNoError};
  
  err = memory_accessor.SetPid(getpid());
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  err = memory_accessor.ParseMaps();
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  REQUIRE(memory_accessor.segment_infos_.size() != 0);
  
  size_t seg_size{memory_accessor.segment_infos_[0].end_ -
                  memory_accessor.segment_infos_[0].start_};

  auto arr1 = std::make_unique<char[]>(seg_size);
  auto arr2 = std::make_unique<char[]>(seg_size);

  memoryaccessor_testing::memoryaccessor::read_urandom(arr1.get(), seg_size);
  std::memcpy(arr2.get(), arr1.get(), seg_size);

  REQUIRE(memoryaccessor_testing::memoryaccessor::are_arrays_same(
      arr1.get(), arr2.get(), seg_size));
  
  size_t done_amount{0};
  
  err = memory_accessor.ReadSegment(arr1.get(), 0, done_amount);
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  REQUIRE(!memoryaccessor_testing::memoryaccessor::are_arrays_same(arr1.get(), arr2.get(), seg_size));
}

TEST_CASE("Read same segment to arrays in different cases") {
  MemoryAccessor::ErrorCode err{MemoryAccessor::ErrorCode::kNoError};
  
  memoryaccessor_testing::memoryaccessor::get_paused_child();
  
  err = memory_accessor.SetPid(child);
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  err = memory_accessor.ParseMaps();
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  REQUIRE(memory_accessor.segment_infos_.size() != 0);
  size_t seg_size1{memory_accessor.segment_infos_[0].end_ -
                   memory_accessor.segment_infos_[0].start_};
  auto arr1 = std::make_unique<char[]>(seg_size1);
  
  size_t done_amount{0};
  
  err = memory_accessor.ReadSegment(arr1.get(), 0, done_amount);
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  err = memory_accessor.SetPid(1);
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  err = memory_accessor.SetPid(child);
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  err = memory_accessor.ParseMaps();
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  REQUIRE(memory_accessor.segment_infos_.size() != 0);
  size_t seg_size2{memory_accessor.segment_infos_[0].end_ -
                   memory_accessor.segment_infos_[0].start_};
  auto arr2 = std::make_unique<char[]>(seg_size2);
  
  err = memory_accessor.ReadSegment(arr2.get(), 0, done_amount);
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  WARN(seg_size1 == seg_size2);
  REQUIRE(memoryaccessor_testing::memoryaccessor::are_arrays_same(
      arr1.get(), arr2.get(), std::min(seg_size1, seg_size2)));
  
  memoryaccessor_testing::memoryaccessor::clear_child();
}

TEST_CASE("Read segment to array and compare to parts") {
  MemoryAccessor::ErrorCode err{MemoryAccessor::ErrorCode::kNoError};
  
  memoryaccessor_testing::memoryaccessor::get_paused_child();
  
  size_t done_amount{0};
  
  err = memory_accessor.SetPid(child);
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  err = memory_accessor.ParseMaps();
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  REQUIRE(memory_accessor.segment_infos_.size() != 0);
  
  size_t seg_size{memory_accessor.segment_infos_[0].end_ -
                  memory_accessor.segment_infos_[0].start_};
  auto arr = std::make_unique<char[]>(seg_size);
  
  err = memory_accessor.ReadSegment(arr.get(), 0, done_amount);
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  size_t part12_size{seg_size / 3};
  size_t part3_size{seg_size - 2 * part12_size};

  auto part1 = std::make_unique<char[]>(part12_size);
  auto part2 = std::make_unique<char[]>(part12_size);
  auto part3 = std::make_unique<char[]>(part3_size);
  
  err = memory_accessor.ReadSegment(part1.get(), 0, done_amount, 0, part12_size);
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  err = memory_accessor.ReadSegment(part2.get(), 0, done_amount, part12_size, part12_size);
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  err = memory_accessor.ReadSegment(part3.get(), 0, done_amount, part12_size * 2);
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  REQUIRE(memoryaccessor_testing::memoryaccessor::are_arrays_same(
      arr.get(), part1.get(), part12_size));
  REQUIRE(memoryaccessor_testing::memoryaccessor::are_arrays_same(
      arr.get() + part12_size, part2.get(), part12_size));
  REQUIRE(memoryaccessor_testing::memoryaccessor::are_arrays_same(
      arr.get() + 2 * part12_size, part3.get(), part3_size));
  
  memoryaccessor_testing::memoryaccessor::clear_child();
}

TEST_CASE("Read segment: exceptions") {
  MemoryAccessor::ErrorCode err{MemoryAccessor::ErrorCode::kNoError};
  
  size_t done_amount{0};
  
  memory_accessor.Reset();
  char arr[1];
  
  err = memory_accessor.ReadSegment(arr, 0, done_amount);
  REQUIRE(err == MemoryAccessor::ErrorCode::kPidNotSetErr);

  memoryaccessor_testing::memoryaccessor::get_paused_child();
  
  err = memory_accessor.SetPid(child);
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  err = memory_accessor.ParseMaps();
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  REQUIRE(memory_accessor.segment_infos_.size() != 0);
  
  err = memory_accessor.ReadSegment(arr, memory_accessor.segment_infos_.size(), done_amount);
  REQUIRE(err == MemoryAccessor::ErrorCode::kSegmentNotExistErr);
  
  err = memory_accessor.ReadSegment(arr, 0, done_amount, memory_accessor.segment_infos_[0].end_);
  REQUIRE(err == MemoryAccessor::ErrorCode::kAddressNotInSegmentErr);
  
  size_t vsyscall_num{memoryaccessor_testing::memoryaccessor::seg_num_by_name("[vsyscall]", memory_accessor.segment_infos_)};
  if (vsyscall_num != SIZE_MAX) {
    err = memory_accessor.ReadSegment(arr, vsyscall_num, done_amount);
    REQUIRE(err == MemoryAccessor::ErrorCode::kSegmentAccessDeniedErr);
  } else {
    WARN(vsyscall_num == SIZE_MAX);
  }
  
  memoryaccessor_testing::memoryaccessor::clear_child();
}

TEST_CASE("Write array to segment, read back and compare") {
  MemoryAccessor::ErrorCode err{MemoryAccessor::ErrorCode::kNoError};
  
  size_t done_amount{0};
  
  memoryaccessor_testing::memoryaccessor::get_paused_child();
  
  err = memory_accessor.SetPid(child);
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  err = memory_accessor.ParseMaps();
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  REQUIRE(memory_accessor.segment_infos_.size() != 0);
  
  size_t seg_size{memory_accessor.segment_infos_[0].end_ -
                  memory_accessor.segment_infos_[0].start_};

  auto arr1 = std::make_unique<char[]>(seg_size);
  auto arr2 = std::make_unique<char[]>(seg_size);

  memoryaccessor_testing::memoryaccessor::read_urandom(arr1.get(), seg_size);
  
  err = memory_accessor.WriteSegment(arr1.get(), 0, done_amount);
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);

  err = memory_accessor.ReadSegment(arr2.get(), 0, done_amount);
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);

  REQUIRE(memoryaccessor_testing::memoryaccessor::are_arrays_same(arr1.get(), arr2.get(), seg_size));
  
  memoryaccessor_testing::memoryaccessor::clear_child();
}

TEST_CASE("Write array parts to segment, read back and compare") {
  MemoryAccessor::ErrorCode err{MemoryAccessor::ErrorCode::kNoError};
  
  size_t done_amount{0};
  
  memoryaccessor_testing::memoryaccessor::get_paused_child();
  
  err = memory_accessor.SetPid(child);
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  err = memory_accessor.ParseMaps();
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  REQUIRE(memory_accessor.segment_infos_.size() != 0);
  
  size_t seg_size{memory_accessor.segment_infos_[0].end_ -
                  memory_accessor.segment_infos_[0].start_};

  auto arr1 = std::make_unique<char[]>(seg_size);
  memoryaccessor_testing::memoryaccessor::read_urandom(arr1.get(), seg_size);

  size_t part12_size{seg_size / 3};
  size_t part3_size{seg_size - 2 * part12_size};
  
  err = memory_accessor.WriteSegment(arr1.get(), 0, done_amount, 0, part12_size);
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  err = memory_accessor.WriteSegment(arr1.get() + part12_size, 0, done_amount, part12_size,
                               part12_size);
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  err = memory_accessor.WriteSegment(arr1.get() + 2 * part12_size, 0, done_amount,
                               part12_size * 2);
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);

  auto arr2 = std::make_unique<char[]>(seg_size);

  err = memory_accessor.ReadSegment(arr2.get(), 0, done_amount, 0, part12_size);
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  err = memory_accessor.ReadSegment(arr2.get() + part12_size, 0, done_amount, part12_size,
                              part12_size);
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  err = memory_accessor.ReadSegment(arr2.get() + 2 * part12_size, 0, done_amount, part12_size * 2);
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  REQUIRE(memoryaccessor_testing::memoryaccessor::are_arrays_same(arr1.get(), arr2.get(), seg_size));
  
  memoryaccessor_testing::memoryaccessor::clear_child();
}

TEST_CASE("Write segment: exceptions") {
  MemoryAccessor::ErrorCode err{MemoryAccessor::ErrorCode::kNoError};
  
  size_t done_amount{0};
  
  memory_accessor.Reset();
  
  err = memory_accessor.WriteSegment("", 0, done_amount, 0, 1);
  REQUIRE(err == MemoryAccessor::ErrorCode::kPidNotSetErr);

  memoryaccessor_testing::memoryaccessor::get_paused_child();
  
  err = memory_accessor.SetPid(child);
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  err = memory_accessor.ParseMaps();
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  REQUIRE(memory_accessor.segment_infos_.size() != 0);
  
  err = memory_accessor.WriteSegment("", memory_accessor.segment_infos_.size(), done_amount, 0, 1);
  REQUIRE(err == MemoryAccessor::ErrorCode::kSegmentNotExistErr);
  
  err = memory_accessor.WriteSegment("", 0, done_amount, memory_accessor.segment_infos_[0].end_, 1);
  REQUIRE(err == MemoryAccessor::ErrorCode::kAddressNotInSegmentErr);
  
  size_t vsyscall_num{memoryaccessor_testing::memoryaccessor::seg_num_by_name("[vsyscall]", memory_accessor.segment_infos_)};
  if (vsyscall_num != SIZE_MAX) {
    err = memory_accessor.WriteSegment("", vsyscall_num, done_amount, 0, 1);
    REQUIRE(err == MemoryAccessor::ErrorCode::kSegmentAccessDeniedErr);
  } else {
    WARN(vsyscall_num == SIZE_MAX);
  }
  
  memoryaccessor_testing::memoryaccessor::clear_child();
}

namespace memoryaccessor_testing::memoryaccessor {

/*!
 \brief Find the beginning of the 1st gap between segments.
 \param [in] infos std::vector of SegmentInfo instances to search.
 \return Start of the gap.

  Find the beginning of the first gap between segments, i.e. the first end of
 the segment that is not equal to the start of the following segment.
*/
size_t find_gap_start(const std::vector<SegmentInfo> &infos) {
  size_t size{infos.size()};
  for (size_t num{0}; num < size - 1; num++)
    if (infos[num].end_ != infos[num + 1].start_)
      return infos[num].end_;
  return 0;
}

} // namespace memoryaccessor_testing::memoryaccessor

TEST_CASE("Read data across segments to array and compare to initial array") {
  MemoryAccessor::ErrorCode err{MemoryAccessor::ErrorCode::kNoError};
  
  memoryaccessor_testing::memoryaccessor::get_paused_child();
  
  err = memory_accessor.SetPid(child);
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  err = memory_accessor.ParseMaps();
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  REQUIRE(memory_accessor.segment_infos_.size() > 1);
  
  auto arr1 = std::make_unique<char[]>(kBufferSize);
  auto arr2 = std::make_unique<char[]>(kBufferSize);

  memoryaccessor_testing::memoryaccessor::read_urandom(arr1.get(), kBufferSize);
  std::memcpy(arr2.get(), arr1.get(), kBufferSize);

  REQUIRE(memoryaccessor_testing::memoryaccessor::are_arrays_same(arr1.get(), arr2.get(), kBufferSize));
  
  REQUIRE(memory_accessor.segment_infos_[0].end_ ==
          memory_accessor.segment_infos_[1].start_);
  size_t done_amount{0};
  
  err = memory_accessor.Read(arr1.get(), memory_accessor.segment_infos_[0].end_ - kBufferSize / 2, kBufferSize, done_amount);
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  REQUIRE(done_amount == kBufferSize);
  REQUIRE(!memoryaccessor_testing::memoryaccessor::are_arrays_same(arr1.get(), arr2.get(), kBufferSize));
  
  memoryaccessor_testing::memoryaccessor::clear_child();
}

TEST_CASE("Read data across segments to arrays in different cases") {
  MemoryAccessor::ErrorCode err{MemoryAccessor::ErrorCode::kNoError};
  
  memoryaccessor_testing::memoryaccessor::get_paused_child();
  
  size_t done_amount{0};
  
  err = memory_accessor.SetPid(child);
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  err = memory_accessor.ParseMaps();
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  REQUIRE(memory_accessor.segment_infos_.size() > 1);
  REQUIRE(memory_accessor.segment_infos_[0].end_ ==
          memory_accessor.segment_infos_[1].start_);
  auto arr1 = std::make_unique<char[]>(kBufferSize);
  
  err = memory_accessor.Read(
      arr1.get(), memory_accessor.segment_infos_[0].end_ - kBufferSize / 2,
      kBufferSize, done_amount);
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  REQUIRE(done_amount == kBufferSize);
  
  err = memory_accessor.SetPid(1);
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  err = memory_accessor.SetPid(child);
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  err = memory_accessor.ParseMaps();
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  REQUIRE(memory_accessor.segment_infos_.size() > 1);
  REQUIRE(memory_accessor.segment_infos_[0].end_ ==
          memory_accessor.segment_infos_[1].start_);
  auto arr2 = std::make_unique<char[]>(kBufferSize);
  
  err = memory_accessor.Read(
      arr2.get(), memory_accessor.segment_infos_[0].end_ - kBufferSize / 2,
      kBufferSize, done_amount);
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  REQUIRE(done_amount == kBufferSize);
  
  REQUIRE(memoryaccessor_testing::memoryaccessor::are_arrays_same(
      arr1.get(), arr2.get(), kBufferSize));
  
  memoryaccessor_testing::memoryaccessor::clear_child();
}

TEST_CASE("Read data across segments to array and compare to parts") {
  MemoryAccessor::ErrorCode err{MemoryAccessor::ErrorCode::kNoError};
  
  memoryaccessor_testing::memoryaccessor::get_paused_child();
  
  err = memory_accessor.SetPid(child);
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  err = memory_accessor.ParseMaps();
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  REQUIRE(memory_accessor.segment_infos_.size() > 1);
  REQUIRE(memory_accessor.segment_infos_[0].end_ ==
          memory_accessor.segment_infos_[1].start_);
  
  size_t done_amount{0};
  
  auto arr = std::make_unique<char[]>(kBufferSize);
  size_t begin{memory_accessor.segment_infos_[0].end_ - kBufferSize / 2};
  
  err = memory_accessor.Read(arr.get(), begin, kBufferSize, done_amount);
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  REQUIRE(done_amount == kBufferSize);
  
  size_t part12_size{kBufferSize / 3};
  size_t part3_size{kBufferSize - 2 * part12_size};

  auto part1 = std::make_unique<char[]>(part12_size);
  auto part2 = std::make_unique<char[]>(part12_size);
  auto part3 = std::make_unique<char[]>(part3_size);
  
  err = memory_accessor.Read(part1.get(), begin, part12_size, done_amount);
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  begin += part12_size;
  REQUIRE(done_amount == part12_size);
  
  err = memory_accessor.Read(part2.get(), begin, part12_size, done_amount);
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  begin += part12_size;
  REQUIRE(done_amount == part12_size);
  
  err = memory_accessor.Read(part3.get(), begin, part3_size, done_amount);
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  REQUIRE(done_amount == part3_size);
  
  REQUIRE(memoryaccessor_testing::memoryaccessor::are_arrays_same(
      arr.get(), part1.get(), part12_size));
  REQUIRE(memoryaccessor_testing::memoryaccessor::are_arrays_same(
      arr.get() + part12_size, part2.get(), part12_size));
  REQUIRE(memoryaccessor_testing::memoryaccessor::are_arrays_same(
      arr.get() + 2 * part12_size, part3.get(), part3_size));
  
  memoryaccessor_testing::memoryaccessor::clear_child();
}

TEST_CASE("Read: exceptions") {
  MemoryAccessor::ErrorCode err{MemoryAccessor::ErrorCode::kNoError};
  
  size_t done_amount{0};
  
  memory_accessor.Reset();
  char arr[1];
  
  err = memory_accessor.Read(arr, 0, 0, done_amount);
  REQUIRE(err == MemoryAccessor::ErrorCode::kPidNotSetErr);

  memoryaccessor_testing::memoryaccessor::get_paused_child();
  
  err = memory_accessor.SetPid(child);
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  err = memory_accessor.ParseMaps();
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  REQUIRE(memory_accessor.segment_infos_.size() != 0);
  
  err = memory_accessor.Read(arr, 0, 0, done_amount);
  REQUIRE(err == MemoryAccessor::ErrorCode::kAddressNotInSegmentErr);
  
  size_t vsyscall_num{memoryaccessor_testing::memoryaccessor::seg_num_by_name("[vsyscall]", memory_accessor.segment_infos_)};
  if (vsyscall_num != SIZE_MAX) {
    err = memory_accessor.Read(arr, memory_accessor.segment_infos_[vsyscall_num].start_, 1, done_amount);
    REQUIRE(err == MemoryAccessor::ErrorCode::kSegmentAccessDeniedErr);
  } else {
    WARN(vsyscall_num == SIZE_MAX);
  }
  
  memoryaccessor_testing::memoryaccessor::clear_child();
}

TEST_CASE("Write array to memory across segments, read back and compare") {
  MemoryAccessor::ErrorCode err{MemoryAccessor::ErrorCode::kNoError};
  
  memoryaccessor_testing::memoryaccessor::get_paused_child();

  size_t done_amount{0};
  
  err = memory_accessor.SetPid(child);
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  err = memory_accessor.ParseMaps();
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  REQUIRE(memory_accessor.segment_infos_.size() > 1);
  REQUIRE(memory_accessor.segment_infos_[0].end_ ==
          memory_accessor.segment_infos_[1].start_);
  
  auto arr1 = std::make_unique<char[]>(kBufferSize);
  auto arr2 = std::make_unique<char[]>(kBufferSize);

  memoryaccessor_testing::memoryaccessor::read_urandom(arr1.get(),
                                                       kBufferSize);
  
  err = memory_accessor.Write(arr1.get(), memory_accessor.segment_infos_[0].end_ - kBufferSize / 2, kBufferSize, done_amount);
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  REQUIRE(done_amount == kBufferSize);
  
  
  err = memory_accessor.Read(arr2.get(), memory_accessor.segment_infos_[0].end_ - kBufferSize / 2, kBufferSize, done_amount);
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  REQUIRE(done_amount == kBufferSize);
  
  REQUIRE(memoryaccessor_testing::memoryaccessor::are_arrays_same(
      arr1.get(), arr2.get(), kBufferSize));
  
  memoryaccessor_testing::memoryaccessor::clear_child();
}

TEST_CASE(
    "Write array parts to memory across segments, read back and compare") {
  MemoryAccessor::ErrorCode err{MemoryAccessor::ErrorCode::kNoError};
  
  memoryaccessor_testing::memoryaccessor::get_paused_child();
  
  err = memory_accessor.SetPid(child);
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  err = memory_accessor.ParseMaps();
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  REQUIRE(memory_accessor.segment_infos_.size() > 1);
  REQUIRE(memory_accessor.segment_infos_[0].end_ ==
          memory_accessor.segment_infos_[1].start_);
  
  size_t done_amount{0};
  
  auto arr1 = std::make_unique<char[]>(kBufferSize);
  memoryaccessor_testing::memoryaccessor::read_urandom(arr1.get(),
                                                       kBufferSize);

  size_t part12_size{kBufferSize / 3};
  size_t part3_size{kBufferSize - 2 * part12_size};

  size_t begin{memory_accessor.segment_infos_[0].end_ - kBufferSize / 2};
  
  err = memory_accessor.Write(arr1.get(), begin, part12_size, done_amount);
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  begin += part12_size;
  REQUIRE(done_amount == part12_size);
  err = memory_accessor.Write(arr1.get() + part12_size, begin, part12_size,
                        done_amount);
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  begin += part12_size;
  REQUIRE(done_amount == part12_size);
  err = memory_accessor.Write(arr1.get() + part12_size * 2, begin, part3_size,
                        done_amount);
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  REQUIRE(done_amount == part3_size);

  auto arr2 = std::make_unique<char[]>(kBufferSize);

  begin = memory_accessor.segment_infos_[0].end_ - kBufferSize / 2;

  err = memory_accessor.Read(arr2.get(), begin, part12_size, done_amount);
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  begin += part12_size;
  REQUIRE(done_amount == part12_size);
  err = memory_accessor.Read(arr2.get() + part12_size, begin, part12_size,
                       done_amount);
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  begin += part12_size;
  REQUIRE(done_amount == part12_size);
  err = memory_accessor.Read(arr2.get() + part12_size * 2, begin, part3_size,
                       done_amount);
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  REQUIRE(done_amount == part3_size);

  REQUIRE(memoryaccessor_testing::memoryaccessor::are_arrays_same(
      arr1.get(), arr2.get(), kBufferSize));
  
  memoryaccessor_testing::memoryaccessor::clear_child();
}

TEST_CASE("Write: exceptions") {
  MemoryAccessor::ErrorCode err{MemoryAccessor::ErrorCode::kNoError};
  
  size_t done_amount{0};
  
  memory_accessor.Reset();
  
  err = memory_accessor.Write("", 0, 0, done_amount);
  REQUIRE(err == MemoryAccessor::ErrorCode::kPidNotSetErr);

  memoryaccessor_testing::memoryaccessor::get_paused_child();
  
  err = memory_accessor.SetPid(child);
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  err = memory_accessor.ParseMaps();
  REQUIRE(err == MemoryAccessor::ErrorCode::kNoError);
  
  REQUIRE(memory_accessor.segment_infos_.size() != 0);
  
  err = memory_accessor.Write("", 0, 0, done_amount);
  REQUIRE(err == MemoryAccessor::ErrorCode::kAddressNotInSegmentErr);
  
  size_t vsyscall_num{memoryaccessor_testing::memoryaccessor::seg_num_by_name("[vsyscall]", memory_accessor.segment_infos_)};
  if (vsyscall_num != SIZE_MAX) {
    err = memory_accessor.Write("", memory_accessor.segment_infos_[vsyscall_num].start_, 1, done_amount);
    REQUIRE(err == MemoryAccessor::ErrorCode::kSegmentAccessDeniedErr);
  } else {
    WARN(vsyscall_num == SIZE_MAX);
  }

  memoryaccessor_testing::memoryaccessor::clear_child();
}

TEST_SUITE_END();

TEST_SUITE_BEGIN("HexViewer");

TEST_CASE("Print hex: check syntax, show_hex == false") {
  std::ostringstream oss;
  const char *str = "abcdef";
  size_t str_size{6};
  size_t address{0x11};

  hex_viewer.PrintHex(&oss, str, str_size, address);
  REQUIRE(oss.str() == "11  abcdef\n");
}

TEST_CASE("Print hex: zero length, show_hex == false") {
  std::ostringstream oss;

  hex_viewer.PrintHex(&oss, nullptr, 0, 0);
  REQUIRE(oss.str().length() == 0);
}

TEST_CASE("Print hex: check syntax, show_hex == true") {
  std::ostringstream oss;
  const char *str = "abcdef";
  size_t str_size{6};
  size_t address{0x11};

  struct winsize ws;
  ioctl(0, TIOCGWINSZ, &ws);
  size_t width{8};
  if (ws.ws_col - 14 / (width * 4))
    width <<= std::bit_width((ws.ws_col - 14) / (width * 4)) - 1;

  hex_viewer.PrintHex(&oss, str, str_size, address, true);
  REQUIRE(oss.str() == "11  61 62 63 64 65 66 " +
                           std::string((width - str_size) * 3, ' ') +
                           " abcdef\n");
}

TEST_CASE("Print hex: zero length, show_hex == true") {
  std::ostringstream oss;

  hex_viewer.PrintHex(&oss, nullptr, 0, 0, true);
  REQUIRE(oss.str().length() == 0);
}

TEST_SUITE_END();

TEST_SUITE_BEGIN("Console");

namespace memoryaccessor_testing::console {

std::streambuf * const cout_streambuf_p{std::cout.rdbuf()}; //!< Pointer to std::streambuf instance of the default process std::cout.
std::streambuf * const cerr_streambuf_p{std::cerr.rdbuf()}; //!< Pointer to std::streambuf instance of the default process std::cerr.
std::ostringstream redir_oss; //!< Stream to redirect std::cout and std::cerr to.

/*!
   \brief Redirect std::cout and std::cerr to redir_oss.

   Redirect default std::cout and std::cerr to custom stream redir_oss, to examine stdout and stderr data.
  */
void redir_io() {
  redir_oss.str("");
  std::cout.rdbuf(redir_oss.rdbuf());
  std::cerr.rdbuf(redir_oss.rdbuf());
}

/*!
   \brief Assign default destinations to std::cout and std::cerr.

   Set the original std::streambuf instances to std::cout and std::cerr, to restore the default work of these.
  */
void restore_io() {
  std::cout.rdbuf(cout_streambuf_p);
  std::cerr.rdbuf(cerr_streambuf_p);
}

/*!
   \brief Compare string to the output from redirected streams.
   \param [in] expect String that is expected to be equal.
   \return Result of the comparison

   Get the contents of redir_oss as a string and compare it to expect parameter.
  */
bool compare_io(const std::string& expect) {
  std::string output{redir_oss.str()};
  
  // CAPTURE(expect);
  // CAPTURE(output);
  // WARN(false);
  
  return output == expect;
}

/*!
   \brief Compare string to the beginning of the output from redirected streams.
   \param [in] expect Subtring that is expected to be equal to the beginning of the output.
   \return Result of the comparison

   Get the contents of redir_oss as a string, cut it to the length of expect parameter and check the equality.
  */
bool compare_io_substr(const std::string& expect) {
  std::string output{redir_oss.str()};
  
  // CAPTURE(expect);
  // CAPTURE(output);
  // WARN(false);
  
  return output.substr(0, expect.length()) == expect;
}

/*!
 \brief Replace streambuf of stream by one provided in ostringstream.
 \param [in,out] stream Stream to perform replacement.
 \param [in] oss std::ostringstream containing std::streambuf replacement.
 \return Pointer to old std::streambuf of the stream.

  Replace std::streambuf of stream by one provided in std::ostringstream, so all
 data coming from stream will be redirected to std::ostringstream.
*/
std::streambuf *replace_streambuf(std::ios &stream,
                                  const std::ostringstream &oss) {
  std::streambuf *p_streambuf = stream.rdbuf();
  stream.rdbuf(oss.rdbuf());
  return p_streambuf;
}

} // namespace memoryaccessor_testing::console

TEST_CASE("Print name and version: not null") {
  memoryaccessor_testing::console::redir_io();
  console.PrintNameVer();
  memoryaccessor_testing::console::restore_io();
  REQUIRE(!memoryaccessor_testing::console::compare_io(""));
}

TEST_CASE("Print name and version") {
  memoryaccessor_testing::console::redir_io();
  console.PrintNameVer();
  memoryaccessor_testing::console::restore_io();
  std::string expect{console.kProjectName + " " + console.kProjectVersion + "\n"};
  REQUIRE(memoryaccessor_testing::console::compare_io(expect));
}

TEST_CASE("Console start: not null") {
  memoryaccessor_testing::console::redir_io();
  console.Start();
  memoryaccessor_testing::console::restore_io();
  REQUIRE(!memoryaccessor_testing::console::compare_io(""));
}

TEST_CASE("Console start") {
  memoryaccessor_testing::console::redir_io();
  console.Start();
  memoryaccessor_testing::console::restore_io();
  std::string expect{console.kProjectName + " " + console.kProjectVersion + "\nType \"help\" for help.\n"};
  REQUIRE(memoryaccessor_testing::console::compare_io(expect));
}

TEST_CASE("Handle empty command") {
  memoryaccessor_testing::console::redir_io();
  console.HandleCommand("");
  memoryaccessor_testing::console::restore_io();
  REQUIRE(memoryaccessor_testing::console::compare_io(""));
}

TEST_CASE("Handle whitespace command") {
  memoryaccessor_testing::console::redir_io();
  console.HandleCommand(std::string(5, ' '));
  memoryaccessor_testing::console::restore_io();
  REQUIRE(memoryaccessor_testing::console::compare_io(""));
}

TEST_CASE("Handle unknown command") {
  memoryaccessor_testing::console::redir_io();
  std::string command{"abcdef"};
  console.HandleCommand(command);
  memoryaccessor_testing::console::restore_io();
  std::string expect{command + ": command not found\n"};
  REQUIRE(memoryaccessor_testing::console::compare_io(expect));
}

TEST_CASE("Handle command with quotes") {
  memoryaccessor_testing::console::redir_io();
  std::string command{"abc def"};
  console.HandleCommand("\"" + command + "\"");
  memoryaccessor_testing::console::restore_io();
  std::string expect{command + ": command not found\n"};
  REQUIRE(memoryaccessor_testing::console::compare_io(expect));
}

TEST_CASE("Handle command with escape sequences") {
  memoryaccessor_testing::console::redir_io();
  std::string command{"\\\\\\\"\\a\\b\\f\\n\\r\\t\\v"};
  console.HandleCommand("\"" + command + "\"");
  memoryaccessor_testing::console::restore_io();
  std::string expect{std::string("\\\"\a\b\f\n\r\t\v") + ": command not found\n"};
  REQUIRE(memoryaccessor_testing::console::compare_io(expect));
}

TEST_CASE("Handle command: help") {
  memoryaccessor_testing::console::redir_io();
  console.HandleCommand("help");
  memoryaccessor_testing::console::restore_io();
  std::string expect{console.kProjectName + " " + console.kProjectVersion + "\n" + console.kProjectDescription + "\nCommands:\n"};
  REQUIRE(memoryaccessor_testing::console::compare_io_substr(expect));
}

TEST_CASE("Handle command: name") {
  memoryaccessor_testing::console::redir_io();
  console.HandleCommand("name");
  memoryaccessor_testing::console::restore_io();
  REQUIRE(memoryaccessor_testing::console::compare_io_substr("Usage:"));
  
  memoryaccessor_testing::console::redir_io();
  std::string name{std::string(16, 'a')};
  console.HandleCommand("name " + name);
  memoryaccessor_testing::console::restore_io();
  std::string expect{"No PID found by name: " + name};
  REQUIRE(memoryaccessor_testing::console::compare_io_substr(expect));
  
  memoryaccessor_testing::console::redir_io();
  console.HandleCommand("name " + memoryaccessor_testing::process_api::get_self_name());
  memoryaccessor_testing::console::restore_io();
  REQUIRE(memoryaccessor_testing::console::compare_io_substr("Found"));
}

TEST_CASE("Handle command: pid") {
  memoryaccessor_testing::console::redir_io();
  console.HandleCommand("pid");
  memoryaccessor_testing::console::restore_io();
  REQUIRE(memoryaccessor_testing::console::compare_io_substr("Usage:"));
  
  memoryaccessor_testing::console::redir_io();
  std::string pid_str{std::to_string(memoryaccessor_testing::process_api::max_pid_t)};
  console.HandleCommand("pid " + pid_str);
  memoryaccessor_testing::console::restore_io();
  std::string expect{"The process with PID " + pid_str + " does not exist."};
  REQUIRE(memoryaccessor_testing::console::compare_io_substr(expect));
  
  memoryaccessor_testing::console::redir_io();
  pid_str = std::to_string(getpid());
  console.HandleCommand("pid " + pid_str);
  memoryaccessor_testing::console::restore_io();
  expect = "Set PID: " + pid_str + "\nParsing /proc/" + pid_str + "/maps...\nFound";
  REQUIRE(memoryaccessor_testing::console::compare_io_substr(expect));
}

namespace memoryaccessor_testing::console {

/*!
 \brief Convert size_t to hex string.
 \param [in] num Number to convert.
 \param [in] width Minimum amount of digits in output.
 \return String with hex representation of number.

  Convert size_t to hex string with specified width filled with '0'.
*/
std::string size_t_to_hex(const size_t &num, size_t width) {
  std::ostringstream oss;
  oss << std::setfill('0') << std::setw(width) << std::hex << num;
  return oss.str();
}

} // namespace memoryaccessor_testing::console

TEST_CASE("Handle command: maps") {
  memoryaccessor_testing::console::redir_io();
  console.HandleCommand("pid " + std::to_string(getpid()));
  SegmentInfo si0{memory_accessor.segment_infos_[0]};
  
  memoryaccessor_testing::console::redir_io();
  console.HandleCommand("maps");
  memoryaccessor_testing::console::restore_io();
  std::string expect{std::string(std::log10(memory_accessor.segment_infos_.size() - 1), ' ') +
          "0. " + memoryaccessor_testing::console::size_t_to_hex(si0.start_) +
          "-" + memoryaccessor_testing::console::size_t_to_hex(si0.end_) + " " +
          si0.EncodePermissions() + " " +
          memoryaccessor_testing::console::size_t_to_hex(si0.offset_, 8) + " " +
          memoryaccessor_testing::console::size_t_to_hex(si0.major_id_, 2) +
          ":" +
          memoryaccessor_testing::console::size_t_to_hex(si0.minor_id_, 2) +
          " " + std::to_string(si0.inode_id_)};
  REQUIRE(memoryaccessor_testing::console::compare_io_substr(expect));
}

TEST_CASE("Handle command: view") {
  memoryaccessor_testing::console::redir_io();
  console.HandleCommand("pid " + std::to_string(getpid()));
  
  memoryaccessor_testing::console::redir_io();
  console.HandleCommand("view");
  memoryaccessor_testing::console::restore_io();
  REQUIRE(memoryaccessor_testing::console::compare_io_substr("Usage:"));
  
  memoryaccessor_testing::console::redir_io();
  console.HandleCommand("view 0");
  memoryaccessor_testing::console::restore_io();
  std::string expect{memoryaccessor_testing::console::size_t_to_hex(memory_accessor.segment_infos_[0].start_)};
  REQUIRE(memoryaccessor_testing::console::compare_io_substr(expect));
}

TEST_CASE("Handle command: read") {
  memoryaccessor_testing::console::redir_io();
  console.HandleCommand("pid " + std::to_string(getpid()));
  SegmentInfo si0{memory_accessor.segment_infos_[0]};
  
  memoryaccessor_testing::console::redir_io();
  console.HandleCommand("read");
  memoryaccessor_testing::console::restore_io();
  REQUIRE(memoryaccessor_testing::console::compare_io_substr("Usage:"));
  
  memoryaccessor_testing::console::redir_io();
  console.HandleCommand("read " + memoryaccessor_testing::console::size_t_to_hex(si0.start_) + " 1");
  memoryaccessor_testing::console::restore_io();
  std::string expect{memoryaccessor_testing::console::size_t_to_hex(si0.start_)};
  REQUIRE(memoryaccessor_testing::console::compare_io_substr(expect));
}

TEST_CASE("Handle command: write") {
  memoryaccessor_testing::console::redir_io();
  console.HandleCommand("pid " + std::to_string(getpid()));
  SegmentInfo si0{memory_accessor.segment_infos_[0]};
  
  memoryaccessor_testing::console::redir_io();
  console.HandleCommand("write");
  memoryaccessor_testing::console::restore_io();
  REQUIRE(memoryaccessor_testing::console::compare_io_substr("Usage:"));
  
  memoryaccessor_testing::console::redir_io();
  console.HandleCommand("write " + memoryaccessor_testing::console::size_t_to_hex(si0.start_) + " 0 a");
  memoryaccessor_testing::console::restore_io();
  std::string expect{"0 bytes written."};
  REQUIRE(memoryaccessor_testing::console::compare_io_substr(expect));
}

TEST_CASE("Handle command: diff") {
  memoryaccessor_testing::console::redir_io();
  console.HandleCommand("pid " + std::to_string(getpid()));
  
  memoryaccessor_testing::console::redir_io();
  console.HandleCommand("diff");
  memoryaccessor_testing::console::restore_io();
  REQUIRE(memoryaccessor_testing::console::compare_io_substr("Usage:"));
}

TEST_CASE("Handle command: await") {
  memoryaccessor_testing::console::redir_io();
  console.HandleCommand("await");
  memoryaccessor_testing::console::restore_io();
  REQUIRE(memoryaccessor_testing::console::compare_io_substr("Usage:"));
  
  memoryaccessor_testing::console::redir_io();
  console.HandleCommand("await -p 1");
  memoryaccessor_testing::console::restore_io();
  std::string expect{"Awaiting PID: 1\nPID was found: 1\n"};
  REQUIRE(memoryaccessor_testing::console::compare_io_substr(expect));
  
  memoryaccessor_testing::console::redir_io();
  console.HandleCommand("await " + memoryaccessor_testing::process_api::get_self_name());
  memoryaccessor_testing::console::restore_io();
  expect = "Awaiting process: " + memoryaccessor_testing::process_api::get_self_name() + "\nProcess was found: " + memoryaccessor_testing::process_api::get_self_name();
  REQUIRE(memoryaccessor_testing::console::compare_io_substr(expect));
}

TEST_SUITE_END();

TEST_SUITE_BEGIN("ArgvParser");

TEST_CASE("Parse argv: empty") {
  std::ostringstream oss;
  std::streambuf *p_cout_streambuf{
      memoryaccessor_testing::console::replace_streambuf(std::cout, oss)};
  std::streambuf *p_cerr_streambuf{
      memoryaccessor_testing::console::replace_streambuf(std::cerr, oss)};

  int argc{1};
  char *argv[]{::argv[0]};
  argv_parser.ParseArgv(argc, argv);
  REQUIRE(oss.str().length() == 0);

  std::cout.rdbuf(p_cout_streambuf);
  std::cerr.rdbuf(p_cerr_streambuf);
}

TEST_CASE("Parse argv: no key") {
  std::ostringstream oss;
  std::streambuf *p_cout_streambuf{
      memoryaccessor_testing::console::replace_streambuf(std::cout, oss)};
  std::streambuf *p_cerr_streambuf{
      memoryaccessor_testing::console::replace_streambuf(std::cerr, oss)};

  int argc{2};
  char *argv1{strdup("abcdef")};
  char *argv[]{::argv[0], argv1};
  argv_parser.ParseArgv(argc, argv);
  std::free(argv1);
  REQUIRE(oss.str().length() == 0);

  std::cout.rdbuf(p_cout_streambuf);
  std::cerr.rdbuf(p_cerr_streambuf);
}

namespace memoryaccessor_testing::argvparser {

std::ostringstream *cur_oss_p{
    nullptr}; //!< Pointer to ostringstream. Used inside of exit_handler.
int pipe_to_write{0}; //!< File descriptor of writable end of pipe. Used inside
                      //!< of exit_handler.

extern "C" {
/*!
 \brief Exit handler.

 Should be called before the exit in child process. Writes remained data to pipe
and closes write end.
*/
void exit_handler() {
  std::string output{cur_oss_p->str()};
  write(pipe_to_write, output.c_str(), output.length());
  close(pipe_to_write);
}
}

/*!
 \brief Perform test on ArgvParser::ParseArgv function.
 \param [in] argc Desired arguments count.
 \param [in] argv Desired array of arguments.
 \param [in] result_substr Desired substring of result starting from the
 beginning.

  Perform test on ArgvParser::ParseArgv function and compare result with
 specified substring.
*/
void test_parse_argv(const int &argc, char **argv,
                     const std::string &result_substr) {
  int child_stdout[2];
  WARN(pipe(child_stdout) == 0);

  pid_t child{fork()};
  if (child == -1) {
    REQUIRE(child != -1);
    close(child_stdout[0]);
    close(child_stdout[1]);
    return;
  } else if (child == 0) { // child
    close(child_stdout[0]);

    std::ostringstream oss;
    std::streambuf *p_cout_streambuf{
        memoryaccessor_testing::console::replace_streambuf(std::cout, oss)};
    std::streambuf *p_cerr_streambuf{
        memoryaccessor_testing::console::replace_streambuf(std::cerr, oss)};

    memoryaccessor_testing::argvparser::cur_oss_p = &oss;
    memoryaccessor_testing::argvparser::pipe_to_write = child_stdout[1];
    atexit(memoryaccessor_testing::argvparser::exit_handler);

    argv_parser.ParseArgv(argc, argv);

    std::exit(0);
  } else { // parent
           //	wait(NULL);
    close(child_stdout[1]);

    size_t res_sub_len{result_substr.length()};
    auto original_substr = std::make_unique<char[]>(res_sub_len);
    read(child_stdout[0], original_substr.get(), res_sub_len);
    REQUIRE(std::strncmp(result_substr.c_str(), original_substr.get(),
                         res_sub_len) == 0);

    close(child_stdout[0]);
  }
}

} // namespace memoryaccessor_testing::argvparser

TEST_CASE("Parse argv: unknown key") {
  int argc{2};
  char *argv1{strdup("--abcdef")};
  char *argv[]{::argv[0], argv1};

  memoryaccessor_testing::argvparser::test_parse_argv(
      argc, argv, console.kProjectName + ": unknown key --abcdef\n");
  std::free(argv1);
}

TEST_CASE("Parse argv: key help") {
  int argc{2};
  char *argv1{strdup("--help")};
  char *argv[]{::argv[0], argv1};

  memoryaccessor_testing::argvparser::test_parse_argv(
      argc, argv,
      console.kProjectName + " " + console.kProjectVersion + "\n" +
          console.kProjectDescription + "\n\nUsage: " + console.kProjectName +
          " [OPTION]...\n\n  --help");
  std::free(argv1);
}

TEST_CASE("Parse argv: key command") {
  int argc{2};
  char *argv1{strdup("--command")};
  char *argv2{strdup("help")};
  char *argv[]{::argv[0], argv1, argv2};

  memoryaccessor_testing::argvparser::test_parse_argv(
      argc, argv,
      console.kProjectName + ": --command requires an argument\nUse --help to "
                             "see help about keys.\n");
  argc = 3;
  memoryaccessor_testing::argvparser::test_parse_argv(
      argc, argv,
      console.kProjectName + " " + console.kProjectVersion + "\n" +
          console.kProjectDescription + "\nCommands:\n");
  std::free(argv1);
  std::free(argv2);
}

TEST_CASE("Parse argv: key file") {
  int argc{2};
  char *argv1{strdup("--file")};
  std::string file_path{"./script.txt"};
  char *argv[]{::argv[0], argv1, strdup(file_path.c_str())};

  memoryaccessor_testing::argvparser::test_parse_argv(
      argc, argv,
      console.kProjectName + ": --file requires an argument\nUse --help to see "
                             "help about keys.\n");
  argc = 3;
  memoryaccessor_testing::argvparser::test_parse_argv(
      argc, argv, console.kProjectName + ": ./script.txt: file not exist\n");

  std::ofstream script;
  script.open(file_path);
  script << "help";
  script.close();

  memoryaccessor_testing::argvparser::test_parse_argv(
      argc, argv,
      console.kProjectName + " " + console.kProjectVersion + "\n" +
          console.kProjectDescription + "\nCommands:\n");

  WARN(std::remove(file_path.c_str()) == 0);
  free(argv1);
  free(argv[2]);
}

TEST_SUITE_END();