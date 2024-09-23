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
#include "segmentinfo.h"
#include "tools.h"

int argc{0};          //!< Number of arguments sent with the program.
char **argv{nullptr}; //!< Array of arguments sent with the program.

constexpr size_t kBufferSize{0x1000}; //!< Size of buffers used.

Tools tools; //!< tools instance to perform testing on.
MemoryAccessor
    memory_accessor(tools); //!< memory_accessor instance to perform testing on.
HexViewer hex_viewer;       //!< hex_viewer instance to perform testing on.
Console console(memory_accessor, hex_viewer,
                tools); //!< console instance to perform testing on.
ArgvParser
    argv_parser(console); //!< argv_parser instance to perform testing on.

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

  tools.SetBufferSize(kBufferSize);
  console.SetBufferSize(kBufferSize);

  // Заимствование, источник кода:
  // https://github.com/doctest/doctest/blob/master/doc/markdown/main.md
  // Начало заимствования (есть изменения):
  doctest::Context context;

  context.applyCommandLine(argc, argv);

  int res = context.run();

  if (context.shouldExit())
    return res;

  int client_stuff_return_code = 0;

  return res + client_stuff_return_code;
  // Конец заимствования.
}

TEST_SUITE_BEGIN("Tools");

namespace memoryaccessor_testing::tools {

bool check_sigint{false}; //!< Shows if SIGINT was sent.

extern "C" {
/*!
 \brief SIGINT handler.
 \param [in] signum Number of handled signal.

 A function that should be called when SIGINT is sent. Sets check_sigint variable
to true.
*/
void SIGINT_handler(int signum) { check_sigint = true; }
}

} // namespace memoryaccessor_testing::tools

TEST_CASE("Set SIGINT") {
  WARN(tools.SetSigint(memoryaccessor_testing::tools::SIGINT_handler) == 0);
  WARN(raise(SIGINT) == 0);
  CHECK(memoryaccessor_testing::tools::check_sigint == true);
  WARN(tools.SetSigint(SIG_DFL) == 0);
  memoryaccessor_testing::tools::check_sigint = false;
}

TEST_CASE("Set SIGINT to default") {
  WARN(tools.SetSigint(SIG_DFL) == 0);
  struct sigaction sa;
  WARN(sigaction(SIGINT, NULL, &sa) == 0);
  CHECK(sa.sa_handler == SIG_DFL);
}

TEST_CASE("Shell command: echo") {
  FILE *pipe{tools.ShellCommand("echo abcd")};
  auto buf{std::make_unique<char[]>(kBufferSize)};
  WARN(std::fgets(buf.get(), kBufferSize, pipe) != 0);
  REQUIRE(std::strncmp(buf.get(), "abcd", 4) == 0);
  WARN(pclose(pipe) == 0);
}

TEST_CASE("Shell empty command") {
  FILE *pipe{tools.ShellCommand("")};
  REQUIRE(std::fgetc(pipe) == EOF);
  WARN(pclose(pipe) == 0);
}

TEST_CASE("Shell non-existent command") {
  FILE *pipe{tools.ShellCommand("cfsvmpkmcsomcsfmvisf 2>/dev/null")};
  REQUIRE(std::fgetc(pipe) == EOF);
  WARN(pclose(pipe) != 0);
}

TEST_CASE("Get all PIDs including self") {
  auto all_pids = tools.GetAllPids();
  pid_t self_pid{getpid()};
  CHECK(all_pids.contains(self_pid));
}

TEST_CASE("Get all PIDs (amount non-equal zero)") {
  auto all_pids = tools.GetAllPids();
  CHECK(all_pids.size() != 0);
}

namespace memoryaccessor_testing::tools {

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
  WARN(self_name.length() < 15); // pgrep works normally with <= 15 char names

  return self_name;
}

} // namespace memoryaccessor_testing::tools

TEST_CASE("Get all process names including self") {
  auto all_names = tools.GetAllProcessNames();
  std::string self_name{memoryaccessor_testing::tools::get_self_name()};
  CHECK(all_names.contains(self_name));
}

TEST_CASE("Get all process names (amount non-equal zero)") {
  auto all_names = tools.GetAllProcessNames();
  CHECK(all_names.size() != 0);
}

TEST_CASE("PID exists: self") {
  pid_t self_pid{getpid()};
  REQUIRE(tools.PidExists(self_pid) == 0);
}

namespace memoryaccessor_testing::tools {

pid_t max_pid_t{(1 << (sizeof(pid_t) - 1)) -
                1}; //!< Maximum positive value of pid_t.

} // namespace memoryaccessor_testing::tools

TEST_CASE("PID does not exist") {
  REQUIRE(tools.PidExists(memoryaccessor_testing::tools::max_pid_t) == 1);
}

TEST_CASE("Process exists: self name") {
  REQUIRE(tools.ProcessExists(memoryaccessor_testing::tools::get_self_name()) ==
          0);
}

TEST_CASE("Process with name does not exist") {
  REQUIRE(tools.ProcessExists(std::string(16, 'a')) ==
          1); // using pgrep limit to 15 chars
}

TEST_CASE("Decode permissions: return zero") {
  REQUIRE(tools.DecodePermissions("---p") == 0);
}

TEST_CASE("Decode permissions: full") {
  REQUIRE(tools.DecodePermissions("rwxs") == 15);
}

TEST_CASE("Decode permissions: various") {
  REQUIRE(tools.DecodePermissions("--xp") == 2);
  REQUIRE(tools.DecodePermissions("-w-p") == 4);
  REQUIRE(tools.DecodePermissions("r--p") == 8);
  REQUIRE(tools.DecodePermissions("r--s") == 9);
  REQUIRE(tools.DecodePermissions("r-xp") == 10);
}

TEST_CASE("Decode permissions: long") {
  REQUIRE(tools.DecodePermissions("rwxp123456") == 14);
}

TEST_CASE("Decode permissions: short") {
  REQUIRE(tools.DecodePermissions("r") == 255);
}

TEST_CASE("Decode permissions: invalid") {
  REQUIRE(tools.DecodePermissions("rwxa") == 255);
}

TEST_CASE("Encode permissions: zero") {
  REQUIRE(tools.EncodePermissions(0) == "---p");
}

TEST_CASE("Encode permissions: full") {
  REQUIRE(tools.EncodePermissions(15) == "rwxs");
}

TEST_CASE("Encode permissions: various") {
  REQUIRE(tools.EncodePermissions(1) == "---s");
  REQUIRE(tools.EncodePermissions(6) == "-wxp");
  REQUIRE(tools.EncodePermissions(7) == "-wxs");
  REQUIRE(tools.EncodePermissions(11) == "r-xs");
  REQUIRE(tools.EncodePermissions(13) == "rw-s");
}

TEST_CASE("Encode permissions: additional bits") {
  REQUIRE(tools.EncodePermissions(20) == "-w-p");
}

TEST_CASE("Find differences: zeros") {
  size_t done{0};
  auto diffs = tools.FindDifferencesOfLen(nullptr, nullptr, 0, done, 1);
  REQUIRE(diffs[0] == nullptr);
  REQUIRE(diffs[1] == nullptr);
  REQUIRE(done == 0);
  diffs = tools.FindDifferencesOfLen(nullptr, nullptr, 1, done, 0);
  REQUIRE(diffs[0] == nullptr);
  REQUIRE(diffs[1] == nullptr);
  REQUIRE(done == 0);
}

TEST_CASE("Find differences: size less than length") {
  size_t done{0};
  auto diffs = tools.FindDifferencesOfLen(nullptr, nullptr, 1, done, 2);
  REQUIRE(diffs[0] == nullptr);
  REQUIRE(diffs[1] == nullptr);
  REQUIRE(done == 0);
}

TEST_CASE("Find differences: one") {
  std::string s1{"1241"}, s2{"1351"};
  size_t done{0};
  auto diffs = tools.FindDifferencesOfLen(s1.c_str(), s2.c_str(), 4, done, 2);
  REQUIRE(std::strncmp(diffs[0].get(), "24", 2) == 0);
  REQUIRE(done == 3);
}

TEST_CASE("Find differences: two") {
  std::string s1{"1abc2def3"}, s2{"1fed2cba3"};
  size_t done{0};
  auto diffs = tools.FindDifferencesOfLen(s1.c_str(), s2.c_str(), 9, done, 3);
  REQUIRE(std::strncmp(diffs[1].get(), "fed", 3) == 0);
  REQUIRE(done == 4);
  diffs = tools.FindDifferencesOfLen(s1.c_str() + done, s2.c_str() + done,
                                     9 - done, done, 3);
  REQUIRE(std::strncmp(diffs[0].get(), "def", 3) == 0);
  REQUIRE(done == 4);
}

TEST_CASE("Find differences: diff too long") {
  std::string s1{"abcdefg"}, s2{"hijklmn"};
  size_t done{0};
  auto diffs = tools.FindDifferencesOfLen(s1.c_str(), s2.c_str(), 7, done, 6);
  REQUIRE(diffs[0] == nullptr);
  REQUIRE(diffs[1] == nullptr);
  REQUIRE(done == 7);
}

TEST_CASE("Find differences: full arr") {
  std::string s1{"abcdefg"}, s2{"hijklmn"};
  size_t done{0};
  auto diffs = tools.FindDifferencesOfLen(s1.c_str(), s2.c_str(), 7, done, 7);
  REQUIRE(std::strncmp(diffs[0].get(), "abcdefg", 7) == 0);
  REQUIRE(std::strncmp(diffs[1].get(), "hijklmn", 7) == 0);
  REQUIRE(done == 7);
}

TEST_CASE("Find differences: seq length 1") {
  std::string s1{"1a1a1a1a"}, s2{"1b1b1b1b"};
  size_t done{0};
  for (size_t i{0}; i < 8; i += 2) {
    auto diffs =
        tools.FindDifferencesOfLen(s1.c_str() + i, s2.c_str() + i, 2, done, 1);
    REQUIRE(diffs[0][0] == 'a');
    REQUIRE(diffs[1][0] == 'b');
    REQUIRE(done == 2);
  }
}

TEST_SUITE_END();

TEST_SUITE_BEGIN("MemoryAccessor");

TEST_CASE("Set PID") {
  try {
    memory_accessor.SetPid(1);
    REQUIRE(memory_accessor.GetPid() == 1);
    memory_accessor.CheckPid();
  } catch (...) {
    REQUIRE(false);
  }
}

TEST_CASE("Set non-existent PID") {
  try {
    memory_accessor.SetPid(memoryaccessor_testing::tools::max_pid_t);
    REQUIRE(false);
  } catch (const MemoryAccessor::PidNotExistEx &ex) {
    try {
      REQUIRE(memory_accessor.GetPid() !=
              memoryaccessor_testing::tools::max_pid_t);
    } catch (const MemoryAccessor::PidNotSetEx &ex) {
      REQUIRE(false);
    }
  } catch (const MemoryAccessor::ErrCheckingPidEx &ex) {
    REQUIRE(false);
  }
}

TEST_CASE("Parse maps: self") {
  try {
    memory_accessor.SetPid(getpid());
    memory_accessor.ParseMaps();
  } catch (...) {
    REQUIRE(false);
  }

  REQUIRE(memory_accessor.special_segment_found_.size() != 0);
  REQUIRE(memory_accessor.segment_infos_.size() != 0);
}

TEST_CASE("Parse maps: PID not set") {
  try {
    memory_accessor.Reset();
    memory_accessor.ParseMaps();
    REQUIRE(false);
  } catch (const MemoryAccessor::PidNotSetEx &ex) {
  } catch (...) {
    REQUIRE(false);
  }
}

TEST_CASE("Get all segment names") {
  try {
    memory_accessor.SetPid(getpid());
    memory_accessor.ParseMaps();
  } catch (...) {
    REQUIRE(false);
  }

  auto all_seg_names = memory_accessor.GetAllSegmentNames();
  REQUIRE(all_seg_names.size() != 0);
  REQUIRE(all_seg_names.contains("[heap]"));
}

TEST_CASE("Get zero segment names with no PID") {
  memory_accessor.Reset();
  auto all_seg_names = memory_accessor.GetAllSegmentNames();
  REQUIRE(all_seg_names.size() == 0);
}

TEST_CASE("Address in segment") {
  try {
    memory_accessor.SetPid(getpid());
    memory_accessor.ParseMaps();
    REQUIRE(memory_accessor.AddressInSegment(
                memory_accessor.segment_infos_[0].start) == 0);
  } catch (...) {
    REQUIRE(false);
  }
}

TEST_CASE("Address not in segment") {
  try {
    memory_accessor.Reset();
    memory_accessor.AddressInSegment(0);
    REQUIRE(false);
  } catch (const MemoryAccessor::AddressNotInSegmentEx &ex) {
  }
}

TEST_CASE("Check segment number: positive") {
  try {
    memory_accessor.SetPid(getpid());
    memory_accessor.ParseMaps();
    memory_accessor.CheckSegNum(0);
  } catch (...) {
    REQUIRE(false);
  }
}

TEST_CASE("Check segment number: negative") {
  try {
    memory_accessor.SetPid(getpid());
    memory_accessor.CheckSegNum(0);
    REQUIRE(false);
  } catch (const MemoryAccessor::SegmentNotExistEx &ex) {
  } catch (...) {
    REQUIRE(false);
  }
}

TEST_CASE("Check segment number: PID not set") {
  try {
    memory_accessor.Reset();
    memory_accessor.CheckSegNum(0);
    REQUIRE(false);
  } catch (const MemoryAccessor::PidNotSetEx &ex) {
  } catch (...) {
    REQUIRE(false);
  }
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
  memory_accessor.Reset();
  try {
    memory_accessor.CheckPid();
    REQUIRE(false);
  } catch (const MemoryAccessor::PidNotSetEx &ex) {
  }
  REQUIRE(memory_accessor.segment_infos_.size() == 0);
  REQUIRE(memory_accessor.special_segment_found_.size() == 0);
}

TEST_CASE("Double Reset") {
  memory_accessor.Reset();
  memory_accessor.Reset();
  try {
    memory_accessor.CheckPid();
    REQUIRE(false);
  } catch (const MemoryAccessor::PidNotSetEx &ex) {
  }
  REQUIRE(memory_accessor.segment_infos_.size() == 0);
  REQUIRE(memory_accessor.special_segment_found_.size() == 0);
}

namespace memoryaccessor_testing::memoryaccessor {

/*!
 \brief Create paused child process.
 \return PID of the process created.

  Fork the current process and run pause() in it, so parent process could freely
 run experiments with the forked process.
*/
pid_t get_paused_child() {
  pid_t child{fork()};
  if (child == 0) { // is a child
    pause();
  } else if (child == -1) {
    kill(child, SIGKILL);
    REQUIRE(child != -1);
  }
  return child;
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
    if (infos[i].path == name)
      return i;
  return SIZE_MAX;
}

} // namespace memoryaccessor_testing::memoryaccessor

TEST_CASE("Read segment to array and compare to initial array") {
  pid_t child{memoryaccessor_testing::memoryaccessor::get_paused_child()};

  try {
    memory_accessor.SetPid(child);
    memory_accessor.ParseMaps();
    REQUIRE(memory_accessor.segment_infos_.size() != 0);

    size_t seg_size{memory_accessor.segment_infos_[0].end -
                    memory_accessor.segment_infos_[0].start};

    auto arr1 = std::make_unique<char[]>(seg_size);
    auto arr2 = std::make_unique<char[]>(seg_size);

    memoryaccessor_testing::memoryaccessor::read_urandom(arr1.get(), seg_size);
    std::memcpy(arr2.get(), arr1.get(), seg_size);

    REQUIRE(memoryaccessor_testing::memoryaccessor::are_arrays_same(
        arr1.get(), arr2.get(), seg_size));

    memory_accessor.ReadSegment(arr1.get(), 0);

    REQUIRE(!memoryaccessor_testing::memoryaccessor::are_arrays_same(
        arr1.get(), arr2.get(), seg_size));

    kill(child, SIGKILL);
  } catch (...) {
    kill(child, SIGKILL);
    REQUIRE(false);
  }
}

TEST_CASE("Read same segment to arrays in different cases") {
  pid_t child{memoryaccessor_testing::memoryaccessor::get_paused_child()};

  try {
    memory_accessor.SetPid(child);
    memory_accessor.ParseMaps();
    REQUIRE(memory_accessor.segment_infos_.size() != 0);
    size_t seg_size1{memory_accessor.segment_infos_[0].end -
                     memory_accessor.segment_infos_[0].start};
    auto arr1 = std::make_unique<char[]>(seg_size1);
    memory_accessor.ReadSegment(arr1.get(), 0);

    memory_accessor.SetPid(1);
    memory_accessor.SetPid(child);
    memory_accessor.ParseMaps();
    REQUIRE(memory_accessor.segment_infos_.size() != 0);
    size_t seg_size2{memory_accessor.segment_infos_[0].end -
                     memory_accessor.segment_infos_[0].start};
    auto arr2 = std::make_unique<char[]>(seg_size2);
    memory_accessor.ReadSegment(arr2.get(), 0);

    WARN(seg_size1 == seg_size2);
    REQUIRE(memoryaccessor_testing::memoryaccessor::are_arrays_same(
        arr1.get(), arr2.get(), std::min(seg_size1, seg_size2)));

    kill(child, SIGKILL);
  } catch (...) {
    kill(child, SIGKILL);
    REQUIRE(false);
  }
}

TEST_CASE("Read segment to array and compare to parts") {
  pid_t child{memoryaccessor_testing::memoryaccessor::get_paused_child()};

  try {
    memory_accessor.SetPid(child);
    memory_accessor.ParseMaps();
    REQUIRE(memory_accessor.segment_infos_.size() != 0);

    size_t seg_size{memory_accessor.segment_infos_[0].end -
                    memory_accessor.segment_infos_[0].start};
    auto arr = std::make_unique<char[]>(seg_size);
    memory_accessor.ReadSegment(arr.get(), 0);

    size_t part12_size{seg_size / 3};
    size_t part3_size{seg_size - 2 * part12_size};

    auto part1 = std::make_unique<char[]>(part12_size);
    auto part2 = std::make_unique<char[]>(part12_size);
    auto part3 = std::make_unique<char[]>(part3_size);

    memory_accessor.ReadSegment(part1.get(), 0, 0, part12_size);
    memory_accessor.ReadSegment(part2.get(), 0, part12_size, part12_size);
    memory_accessor.ReadSegment(part3.get(), 0, part12_size * 2);

    REQUIRE(memoryaccessor_testing::memoryaccessor::are_arrays_same(
        arr.get(), part1.get(), part12_size));
    REQUIRE(memoryaccessor_testing::memoryaccessor::are_arrays_same(
        arr.get() + part12_size, part2.get(), part12_size));
    REQUIRE(memoryaccessor_testing::memoryaccessor::are_arrays_same(
        arr.get() + 2 * part12_size, part3.get(), part3_size));

    kill(child, SIGKILL);
  } catch (...) {
    kill(child, SIGKILL);
    REQUIRE(false);
  }
}

TEST_CASE("Read segment: exceptions") {
  try {
    memory_accessor.Reset();
    memory_accessor.ReadSegment(nullptr, 0);
    REQUIRE(false);
  } catch (const MemoryAccessor::PidNotSetEx &ex) {
  } catch (...) {
    REQUIRE(false);
  }

  pid_t child{memoryaccessor_testing::memoryaccessor::get_paused_child()};

  try {
    memory_accessor.SetPid(child);
    memory_accessor.ParseMaps();
    REQUIRE(memory_accessor.segment_infos_.size() != 0);
  } catch (...) {
    REQUIRE(false);
  }

  try {
    memory_accessor.ReadSegment(nullptr, memory_accessor.segment_infos_.size());
    REQUIRE(false);
  } catch (const MemoryAccessor::SegmentNotExistEx &ex) {
  } catch (...) {
    REQUIRE(false);
  }

  try {
    memory_accessor.ReadSegment(nullptr, 0,
                                memory_accessor.segment_infos_[0].end);
    REQUIRE(false);
  } catch (const MemoryAccessor::AddressNotInSegmentEx &ex) {
  } catch (...) {
    REQUIRE(false);
  }

  try {
    size_t vsyscall_num{memoryaccessor_testing::memoryaccessor::seg_num_by_name(
        "[vsyscall]", memory_accessor.segment_infos_)};
    if (vsyscall_num != SIZE_MAX) {
      memory_accessor.ReadSegment(nullptr, vsyscall_num);
      REQUIRE(false);
    } else {
      WARN(vsyscall_num == SIZE_MAX);
    }
  } catch (const MemoryAccessor::SegmentAccessDeniedEx &ex) {
  } catch (...) {
    REQUIRE(false);
  }

  kill(child, SIGKILL);
}

TEST_CASE("Write array to segment, read back and compare") {
  pid_t child{memoryaccessor_testing::memoryaccessor::get_paused_child()};

  try {
    memory_accessor.SetPid(child);
    memory_accessor.ParseMaps();
    REQUIRE(memory_accessor.segment_infos_.size() != 0);

    size_t seg_size{memory_accessor.segment_infos_[0].end -
                    memory_accessor.segment_infos_[0].start};

    auto arr1 = std::make_unique<char[]>(seg_size);
    auto arr2 = std::make_unique<char[]>(seg_size);

    memoryaccessor_testing::memoryaccessor::read_urandom(arr1.get(), seg_size);
    memory_accessor.WriteSegment(arr1.get(), 0);

    memory_accessor.ReadSegment(arr2.get(), 0);

    REQUIRE(memoryaccessor_testing::memoryaccessor::are_arrays_same(
        arr1.get(), arr2.get(), seg_size));

    kill(child, SIGKILL);
  } catch (...) {
    kill(child, SIGKILL);
    REQUIRE(false);
  }
}

TEST_CASE("Write array parts to segment, read back and compare") {
  pid_t child{memoryaccessor_testing::memoryaccessor::get_paused_child()};

  try {
    memory_accessor.SetPid(child);
    memory_accessor.ParseMaps();
    REQUIRE(memory_accessor.segment_infos_.size() != 0);

    size_t seg_size{memory_accessor.segment_infos_[0].end -
                    memory_accessor.segment_infos_[0].start};

    auto arr1 = std::make_unique<char[]>(seg_size);
    memoryaccessor_testing::memoryaccessor::read_urandom(arr1.get(), seg_size);

    size_t part12_size{seg_size / 3};
    size_t part3_size{seg_size - 2 * part12_size};

    memory_accessor.WriteSegment(arr1.get(), 0, 0, part12_size);
    memory_accessor.WriteSegment(arr1.get() + part12_size, 0, part12_size,
                                 part12_size);
    memory_accessor.WriteSegment(arr1.get() + 2 * part12_size, 0,
                                 part12_size * 2);

    auto arr2 = std::make_unique<char[]>(seg_size);

    memory_accessor.ReadSegment(arr2.get(), 0, 0, part12_size);
    memory_accessor.ReadSegment(arr2.get() + part12_size, 0, part12_size,
                                part12_size);
    memory_accessor.ReadSegment(arr2.get() + 2 * part12_size, 0,
                                part12_size * 2);

    REQUIRE(memoryaccessor_testing::memoryaccessor::are_arrays_same(
        arr1.get(), arr2.get(), seg_size));

    kill(child, SIGKILL);
  } catch (...) {
    kill(child, SIGKILL);
    REQUIRE(false);
  }
}

TEST_CASE("Write segment: exceptions") {
  try {
    memory_accessor.Reset();
    memory_accessor.WriteSegment(nullptr, 0);
    REQUIRE(false);
  } catch (const MemoryAccessor::PidNotSetEx &ex) {
  } catch (...) {
    REQUIRE(false);
  }

  pid_t child{memoryaccessor_testing::memoryaccessor::get_paused_child()};

  try {
    memory_accessor.SetPid(child);
    memory_accessor.ParseMaps();
    REQUIRE(memory_accessor.segment_infos_.size() != 0);
  } catch (...) {
    REQUIRE(false);
  }

  try {
    memory_accessor.WriteSegment(nullptr,
                                 memory_accessor.segment_infos_.size());
    REQUIRE(false);
  } catch (const MemoryAccessor::SegmentNotExistEx &ex) {
  } catch (...) {
    REQUIRE(false);
  }

  try {
    memory_accessor.WriteSegment(nullptr, 0,
                                 memory_accessor.segment_infos_[0].end);
    REQUIRE(false);
  } catch (const MemoryAccessor::AddressNotInSegmentEx &ex) {
  } catch (...) {
    REQUIRE(false);
  }

  try {
    size_t vsyscall_num{memoryaccessor_testing::memoryaccessor::seg_num_by_name(
        "[vsyscall]", memory_accessor.segment_infos_)};
    if (vsyscall_num != SIZE_MAX) {
      memory_accessor.WriteSegment(nullptr, vsyscall_num);
      REQUIRE(false);
    } else {
      WARN(vsyscall_num == SIZE_MAX);
    }
  } catch (const MemoryAccessor::SegmentAccessDeniedEx &ex) {
  } catch (...) {
    REQUIRE(false);
  }

  kill(child, SIGKILL);
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
    if (infos[num].end != infos[num + 1].start)
      return infos[num].end;
  return 0;
}

} // namespace memoryaccessor_testing::memoryaccessor

TEST_CASE("Read data across segments to array and compare to initial array") {
  pid_t child{memoryaccessor_testing::memoryaccessor::get_paused_child()};

  try {
    memory_accessor.SetPid(child);
    memory_accessor.ParseMaps();
    REQUIRE(memory_accessor.segment_infos_.size() > 1);

    auto arr1 = std::make_unique<char[]>(kBufferSize);
    auto arr2 = std::make_unique<char[]>(kBufferSize);

    memoryaccessor_testing::memoryaccessor::read_urandom(arr1.get(),
                                                         kBufferSize);
    std::memcpy(arr2.get(), arr1.get(), kBufferSize);

    REQUIRE(memoryaccessor_testing::memoryaccessor::are_arrays_same(
        arr1.get(), arr2.get(), kBufferSize));

    REQUIRE(memory_accessor.segment_infos_[0].end ==
            memory_accessor.segment_infos_[1].start);
    size_t done_amount{0};
    memory_accessor.Read(
        arr1.get(), memory_accessor.segment_infos_[0].end - kBufferSize / 2,
        kBufferSize, done_amount);
    REQUIRE(done_amount == kBufferSize);

    REQUIRE(!memoryaccessor_testing::memoryaccessor::are_arrays_same(
        arr1.get(), arr2.get(), kBufferSize));

    kill(child, SIGKILL);
  } catch (...) {
    kill(child, SIGKILL);
    REQUIRE(false);
  }
}

TEST_CASE("Read data across segments to arrays in different cases") {
  pid_t child{memoryaccessor_testing::memoryaccessor::get_paused_child()};

  try {
    size_t done_amount{0};

    memory_accessor.SetPid(child);
    memory_accessor.ParseMaps();
    REQUIRE(memory_accessor.segment_infos_.size() > 1);
    REQUIRE(memory_accessor.segment_infos_[0].end ==
            memory_accessor.segment_infos_[1].start);
    auto arr1 = std::make_unique<char[]>(kBufferSize);
    memory_accessor.Read(
        arr1.get(), memory_accessor.segment_infos_[0].end - kBufferSize / 2,
        kBufferSize, done_amount);
    REQUIRE(done_amount == kBufferSize);

    memory_accessor.SetPid(1);
    memory_accessor.SetPid(child);
    memory_accessor.ParseMaps();
    REQUIRE(memory_accessor.segment_infos_.size() > 1);
    REQUIRE(memory_accessor.segment_infos_[0].end ==
            memory_accessor.segment_infos_[1].start);
    auto arr2 = std::make_unique<char[]>(kBufferSize);
    memory_accessor.Read(
        arr2.get(), memory_accessor.segment_infos_[0].end - kBufferSize / 2,
        kBufferSize, done_amount);
    REQUIRE(done_amount == kBufferSize);

    REQUIRE(memoryaccessor_testing::memoryaccessor::are_arrays_same(
        arr1.get(), arr2.get(), kBufferSize));

    kill(child, SIGKILL);
  } catch (...) {
    kill(child, SIGKILL);
    REQUIRE(false);
  }
}

TEST_CASE("Read data across segments to array and compare to parts") {
  pid_t child{memoryaccessor_testing::memoryaccessor::get_paused_child()};

  try {
    memory_accessor.SetPid(child);
    memory_accessor.ParseMaps();
    REQUIRE(memory_accessor.segment_infos_.size() > 1);
    REQUIRE(memory_accessor.segment_infos_[0].end ==
            memory_accessor.segment_infos_[1].start);

    size_t done_amount{0};

    auto arr = std::make_unique<char[]>(kBufferSize);
    size_t begin{memory_accessor.segment_infos_[0].end - kBufferSize / 2};
    memory_accessor.Read(arr.get(), begin, kBufferSize, done_amount);
    REQUIRE(done_amount == kBufferSize);

    size_t part12_size{kBufferSize / 3};
    size_t part3_size{kBufferSize - 2 * part12_size};

    auto part1 = std::make_unique<char[]>(part12_size);
    auto part2 = std::make_unique<char[]>(part12_size);
    auto part3 = std::make_unique<char[]>(part3_size);

    memory_accessor.Read(part1.get(), begin, part12_size, done_amount);
    begin += part12_size;
    REQUIRE(done_amount == part12_size);
    memory_accessor.Read(part2.get(), begin, part12_size, done_amount);
    begin += part12_size;
    REQUIRE(done_amount == part12_size);
    memory_accessor.Read(part3.get(), begin, part3_size, done_amount);
    REQUIRE(done_amount == part3_size);

    REQUIRE(memoryaccessor_testing::memoryaccessor::are_arrays_same(
        arr.get(), part1.get(), part12_size));
    REQUIRE(memoryaccessor_testing::memoryaccessor::are_arrays_same(
        arr.get() + part12_size, part2.get(), part12_size));
    REQUIRE(memoryaccessor_testing::memoryaccessor::are_arrays_same(
        arr.get() + 2 * part12_size, part3.get(), part3_size));

    kill(child, SIGKILL);
  } catch (...) {
    kill(child, SIGKILL);
    REQUIRE(false);
  }
}

TEST_CASE("Read: exceptions") {
  size_t done_amount{0};

  try {
    memory_accessor.Reset();
    memory_accessor.Read(nullptr, 0, 0, done_amount);
    REQUIRE(false);
  } catch (const MemoryAccessor::PidNotSetEx &ex) {
  } catch (...) {
    REQUIRE(false);
  }

  pid_t child{memoryaccessor_testing::memoryaccessor::get_paused_child()};

  try {
    memory_accessor.SetPid(child);
    memory_accessor.ParseMaps();
    REQUIRE(memory_accessor.segment_infos_.size() != 0);
  } catch (...) {
    REQUIRE(false);
  }

  try {
    memory_accessor.Read(nullptr, 0, 0, done_amount);
    REQUIRE(false);
  } catch (const MemoryAccessor::AddressNotInSegmentEx &ex) {
  } catch (...) {
    REQUIRE(false);
  }

  try {
    size_t vsyscall_num{memoryaccessor_testing::memoryaccessor::seg_num_by_name(
        "[vsyscall]", memory_accessor.segment_infos_)};
    if (vsyscall_num != SIZE_MAX) {
      auto arr = std::make_unique<char[]>(1);
      memory_accessor.Read(arr.get(),
                           memory_accessor.segment_infos_[vsyscall_num].start,
                           1, done_amount);
      REQUIRE(false);
    } else {
      WARN(vsyscall_num == SIZE_MAX);
    }
  } catch (const MemoryAccessor::SegmentAccessDeniedEx &ex) {
  } catch (...) {
    REQUIRE(false);
  }

  kill(child, SIGKILL);
}

TEST_CASE("Write array to memory across segments, read back and compare") {
  pid_t child{memoryaccessor_testing::memoryaccessor::get_paused_child()};

  try {
    size_t done_amount{0};

    memory_accessor.SetPid(child);
    memory_accessor.ParseMaps();
    REQUIRE(memory_accessor.segment_infos_.size() > 1);
    REQUIRE(memory_accessor.segment_infos_[0].end ==
            memory_accessor.segment_infos_[1].start);

    auto arr1 = std::make_unique<char[]>(kBufferSize);
    auto arr2 = std::make_unique<char[]>(kBufferSize);

    memoryaccessor_testing::memoryaccessor::read_urandom(arr1.get(),
                                                         kBufferSize);
    memory_accessor.Write(
        arr1.get(), memory_accessor.segment_infos_[0].end - kBufferSize / 2,
        kBufferSize, done_amount);
    REQUIRE(done_amount == kBufferSize);

    memory_accessor.Read(
        arr2.get(), memory_accessor.segment_infos_[0].end - kBufferSize / 2,
        kBufferSize, done_amount);
    REQUIRE(done_amount == kBufferSize);

    REQUIRE(memoryaccessor_testing::memoryaccessor::are_arrays_same(
        arr1.get(), arr2.get(), kBufferSize));

    kill(child, SIGKILL);
  } catch (...) {
    kill(child, SIGKILL);
    REQUIRE(false);
  }
}

TEST_CASE(
    "Write array parts to memory across segments, read back and compare") {
  pid_t child{memoryaccessor_testing::memoryaccessor::get_paused_child()};

  try {
    memory_accessor.SetPid(child);
    memory_accessor.ParseMaps();
    REQUIRE(memory_accessor.segment_infos_.size() > 1);
    REQUIRE(memory_accessor.segment_infos_[0].end ==
            memory_accessor.segment_infos_[1].start);

    size_t done_amount{0};

    auto arr1 = std::make_unique<char[]>(kBufferSize);
    memoryaccessor_testing::memoryaccessor::read_urandom(arr1.get(),
                                                         kBufferSize);

    size_t part12_size{kBufferSize / 3};
    size_t part3_size{kBufferSize - 2 * part12_size};

    size_t begin{memory_accessor.segment_infos_[0].end - kBufferSize / 2};

    memory_accessor.Write(arr1.get(), begin, part12_size, done_amount);
    begin += part12_size;
    REQUIRE(done_amount == part12_size);
    memory_accessor.Write(arr1.get() + part12_size, begin, part12_size,
                          done_amount);
    begin += part12_size;
    REQUIRE(done_amount == part12_size);
    memory_accessor.Write(arr1.get() + part12_size * 2, begin, part3_size,
                          done_amount);
    REQUIRE(done_amount == part3_size);

    auto arr2 = std::make_unique<char[]>(kBufferSize);

    begin = memory_accessor.segment_infos_[0].end - kBufferSize / 2;

    memory_accessor.Read(arr2.get(), begin, part12_size, done_amount);
    begin += part12_size;
    REQUIRE(done_amount == part12_size);
    memory_accessor.Read(arr2.get() + part12_size, begin, part12_size,
                         done_amount);
    begin += part12_size;
    REQUIRE(done_amount == part12_size);
    memory_accessor.Read(arr2.get() + part12_size * 2, begin, part3_size,
                         done_amount);
    REQUIRE(done_amount == part3_size);

    REQUIRE(memoryaccessor_testing::memoryaccessor::are_arrays_same(
        arr1.get(), arr2.get(), kBufferSize));

    kill(child, SIGKILL);
  } catch (...) {
    kill(child, SIGKILL);
    REQUIRE(false);
  }
}

TEST_CASE("Write: exceptions") {
  size_t done_amount{0};

  try {
    memory_accessor.Reset();
    memory_accessor.Write(nullptr, 0, 0, done_amount);
    REQUIRE(false);
  } catch (const MemoryAccessor::PidNotSetEx &ex) {
  } catch (...) {
    REQUIRE(false);
  }

  pid_t child{memoryaccessor_testing::memoryaccessor::get_paused_child()};

  try {
    memory_accessor.SetPid(child);
    memory_accessor.ParseMaps();
    REQUIRE(memory_accessor.segment_infos_.size() != 0);
  } catch (...) {
    REQUIRE(false);
  }

  try {
    memory_accessor.Write(nullptr, 0, 0, done_amount);
    REQUIRE(false);
  } catch (const MemoryAccessor::AddressNotInSegmentEx &ex) {
  } catch (...) {
    REQUIRE(false);
  }

  try {
    size_t vsyscall_num{memoryaccessor_testing::memoryaccessor::seg_num_by_name(
        "[vsyscall]", memory_accessor.segment_infos_)};
    if (vsyscall_num != SIZE_MAX) {
      auto arr = std::make_unique<char[]>(1);
      memory_accessor.Write(arr.get(),
                            memory_accessor.segment_infos_[vsyscall_num].start,
                            1, done_amount);
      REQUIRE(false);
    } else {
      WARN(vsyscall_num == SIZE_MAX);
    }
  } catch (const MemoryAccessor::SegmentAccessDeniedEx &ex) {
    // Something is wrong with doctest here, it freezes if not perform the write
    // operation below. Without doctest everything works properly though.
    memory_accessor.Write(nullptr, memory_accessor.segment_infos_[0].start, 0,
                          done_amount);
  } catch (...) {
    REQUIRE(false);
  }

  kill(child, SIGKILL);
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
  // Заимствование, источник кода:
  // https://stackoverflow.com/questions/4191089/how-to-unit-test-function-writing-to-stdout-stdcout
  // Начало заимствования (есть изменения):
  std::streambuf *p_streambuf = stream.rdbuf();
  stream.rdbuf(oss.rdbuf());
  return p_streambuf;
  // Конец заимствования.
}

} // namespace memoryaccessor_testing::console

TEST_CASE("Print name and version: not null") {
  std::ostringstream oss;
  std::streambuf *p_cout_streambuf{
      memoryaccessor_testing::console::replace_streambuf(std::cout, oss)};

  console.PrintNameVer();
  REQUIRE(oss.str() != "");

  std::cout.rdbuf(p_cout_streambuf);
}

TEST_CASE("Print name and version") {
  std::ostringstream oss;
  std::streambuf *p_cout_streambuf{
      memoryaccessor_testing::console::replace_streambuf(std::cout, oss)};

  console.PrintNameVer();
  REQUIRE(oss.str() ==
          console.kProjectName + " " + console.kProjectVersion + "\n");

  std::cout.rdbuf(p_cout_streambuf);
}

TEST_CASE("Console start: not null") {
  std::ostringstream oss;
  std::streambuf *p_cout_streambuf{
      memoryaccessor_testing::console::replace_streambuf(std::cout, oss)};

  console.Start();
  REQUIRE(oss.str() != "");

  std::cout.rdbuf(p_cout_streambuf);
}

TEST_CASE("Console start") {
  std::ostringstream oss;
  std::streambuf *p_cout_streambuf{
      memoryaccessor_testing::console::replace_streambuf(std::cout, oss)};

  console.Start();
  REQUIRE(oss.str() == console.kProjectName + " " + console.kProjectVersion +
                           "\nType \"help\" for help.\n");

  std::cout.rdbuf(p_cout_streambuf);
}

namespace memoryaccessor_testing::console {

/*!
 \brief Perform test on Console::HandleCommand function.
 \param [in,out] oss std::ostringstream with redirected std::cout and/or
 std::cerr. \param [in] command Line with command. \param [in] result_substr
 Desired substring of result starting from the beginning.

  Perform test on Console::HandleCommand function and compare result with
 specified substring.
*/
void test_handle_command(std::ostringstream &oss, const std::string &command,
                         const std::string &result_substr) {
  ::console.HandleCommand(command);
  REQUIRE(oss.str().substr(0, result_substr.length()) == result_substr);
  oss.str("");
}

} // namespace memoryaccessor_testing::console

TEST_CASE("Handle empty command") {
  std::ostringstream oss;
  std::streambuf *p_cout_streambuf{
      memoryaccessor_testing::console::replace_streambuf(std::cout, oss)};

  console.HandleCommand("");
  REQUIRE(oss.str().length() == 0);

  std::cout.rdbuf(p_cout_streambuf);
}

TEST_CASE("Handle whitespace command") {
  std::ostringstream oss;
  std::streambuf *p_cout_streambuf{
      memoryaccessor_testing::console::replace_streambuf(std::cout, oss)};

  console.HandleCommand(std::string(5, ' '));
  REQUIRE(oss.str().length() == 0);

  std::cout.rdbuf(p_cout_streambuf);
}

TEST_CASE("Handle unknown command") {
  std::ostringstream oss;
  std::streambuf *p_cerr_streambuf{
      memoryaccessor_testing::console::replace_streambuf(std::cerr, oss)};

  std::string command{"abcdef"};
  memoryaccessor_testing::console::test_handle_command(
      oss, command, command + ": command not found\n");

  std::cerr.rdbuf(p_cerr_streambuf);
}

TEST_CASE("Handle command with quotes") {
  std::ostringstream oss;
  std::streambuf *p_cerr_streambuf{
      memoryaccessor_testing::console::replace_streambuf(std::cerr, oss)};

  std::string command{"abc def"};
  memoryaccessor_testing::console::test_handle_command(
      oss, "\"" + command + "\"", command + ": command not found\n");

  std::cerr.rdbuf(p_cerr_streambuf);
}

TEST_CASE("Handle command with escape sequences") {
  std::ostringstream oss;
  std::streambuf *p_cerr_streambuf{
      memoryaccessor_testing::console::replace_streambuf(std::cerr, oss)};

  std::string command{"\\\\\\\"\\a\\b\\f\\n\\r\\t\\v"};
  memoryaccessor_testing::console::test_handle_command(
      oss, command,
      std::string("\\\"\a\b\f\n\r\t\v") + ": command not found\n");

  std::cerr.rdbuf(p_cerr_streambuf);
}

TEST_CASE("Handle command: help") {
  std::ostringstream oss;
  std::streambuf *p_cout_streambuf{
      memoryaccessor_testing::console::replace_streambuf(std::cout, oss)};

  memoryaccessor_testing::console::test_handle_command(
      oss, "help",
      console.kProjectName + " " + console.kProjectVersion + "\n" +
          console.kProjectDescription + "\nCommands:\n");

  std::cout.rdbuf(p_cout_streambuf);
}

TEST_CASE("Handle command: name") {
  std::ostringstream oss;
  std::streambuf *p_cout_streambuf{
      memoryaccessor_testing::console::replace_streambuf(std::cout, oss)};
  std::streambuf *p_cerr_streambuf{
      memoryaccessor_testing::console::replace_streambuf(std::cerr, oss)};

  memoryaccessor_testing::console::test_handle_command(oss, "name", "Usage:");
  std::string name{std::string(16, 'a')};
  memoryaccessor_testing::console::test_handle_command(
      oss, "name " + name, "No PID found by name: " + name);
  memoryaccessor_testing::console::test_handle_command(
      oss, "name " + memoryaccessor_testing::tools::get_self_name(), "Found");

  std::cout.rdbuf(p_cout_streambuf);
  std::cerr.rdbuf(p_cerr_streambuf);
}

TEST_CASE("Handle command: pid") {
  std::ostringstream oss;
  std::streambuf *p_cout_streambuf{
      memoryaccessor_testing::console::replace_streambuf(std::cout, oss)};
  std::streambuf *p_cerr_streambuf{
      memoryaccessor_testing::console::replace_streambuf(std::cerr, oss)};

  memoryaccessor_testing::console::test_handle_command(oss, "pid", "Usage:");
  std::string pid_str{std::to_string(memoryaccessor_testing::tools::max_pid_t)};
  memoryaccessor_testing::console::test_handle_command(
      oss, "pid " + pid_str,
      "The process with PID " + pid_str + " does not exist.");
  pid_str = std::to_string(getpid());
  memoryaccessor_testing::console::test_handle_command(
      oss, "pid " + pid_str,
      "Set PID: " + pid_str + "\nParsing /proc/" + pid_str + "/maps...\nFound");

  std::cout.rdbuf(p_cout_streambuf);
  std::cerr.rdbuf(p_cerr_streambuf);
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
  std::ostringstream oss;
  std::streambuf *p_cout_streambuf{
      memoryaccessor_testing::console::replace_streambuf(std::cout, oss)};

  console.HandleCommand("pid " + std::to_string(getpid()));
  oss.str("");

  SegmentInfo si0{memory_accessor.segment_infos_[0]};

  memoryaccessor_testing::console::test_handle_command(
      oss, "maps",
      std::string(std::log10(memory_accessor.segment_infos_.size() - 1), ' ') +
          "0. " + memoryaccessor_testing::console::size_t_to_hex(si0.start) +
          "-" + memoryaccessor_testing::console::size_t_to_hex(si0.end) + " " +
          tools.EncodePermissions(si0.mode) + " " +
          memoryaccessor_testing::console::size_t_to_hex(si0.offset, 8) + " " +
          memoryaccessor_testing::console::size_t_to_hex(si0.major_id, 2) +
          ":" +
          memoryaccessor_testing::console::size_t_to_hex(si0.minor_id, 2) +
          " " + std::to_string(si0.inode_id));

  std::cout.rdbuf(p_cout_streambuf);
}

TEST_CASE("Handle command: view") {
  std::ostringstream oss;
  std::streambuf *p_cout_streambuf{
      memoryaccessor_testing::console::replace_streambuf(std::cout, oss)};

  console.HandleCommand("pid " + std::to_string(getpid()));
  oss.str("");

  memoryaccessor_testing::console::test_handle_command(oss, "view", "Usage:");
  memoryaccessor_testing::console::test_handle_command(
      oss, "view 0",
      memoryaccessor_testing::console::size_t_to_hex(
          memory_accessor.segment_infos_[0].start));

  std::cout.rdbuf(p_cout_streambuf);
}

TEST_CASE("Handle command: read") {
  std::ostringstream oss;
  std::streambuf *p_cout_streambuf{
      memoryaccessor_testing::console::replace_streambuf(std::cout, oss)};

  console.HandleCommand("pid " + std::to_string(getpid()));
  oss.str("");

  SegmentInfo si0{memory_accessor.segment_infos_[0]};

  memoryaccessor_testing::console::test_handle_command(oss, "read", "Usage:");
  memoryaccessor_testing::console::test_handle_command(
      oss,
      "read " + memoryaccessor_testing::console::size_t_to_hex(si0.start) +
          " 1",
      memoryaccessor_testing::console::size_t_to_hex(si0.start));

  std::cout.rdbuf(p_cout_streambuf);
}

TEST_CASE("Handle command: write") {
  std::ostringstream oss;
  std::streambuf *p_cout_streambuf{
      memoryaccessor_testing::console::replace_streambuf(std::cout, oss)};

  console.HandleCommand("pid " + std::to_string(getpid()));
  oss.str("");

  SegmentInfo si0{memory_accessor.segment_infos_[0]};

  memoryaccessor_testing::console::test_handle_command(oss, "write", "Usage:");
  memoryaccessor_testing::console::test_handle_command(
      oss,
      "write " + memoryaccessor_testing::console::size_t_to_hex(si0.start) +
          " 0 a",
      "0 bytes written.");

  std::cout.rdbuf(p_cout_streambuf);
}

TEST_CASE("Handle command: diff") {
  std::ostringstream oss;
  std::streambuf *p_cout_streambuf{
      memoryaccessor_testing::console::replace_streambuf(std::cout, oss)};

  console.HandleCommand("pid " + std::to_string(getpid()));
  oss.str("");

  memoryaccessor_testing::console::test_handle_command(oss, "diff", "Usage:");

  std::cout.rdbuf(p_cout_streambuf);
}

TEST_CASE("Handle command: await") {
  std::ostringstream oss;
  std::streambuf *p_cout_streambuf{
      memoryaccessor_testing::console::replace_streambuf(std::cout, oss)};

  memoryaccessor_testing::console::test_handle_command(oss, "await", "Usage:");
  memoryaccessor_testing::console::test_handle_command(
      oss, "await -p 1", "Awaiting PID: 1\nPID was found: 1\n");
  memoryaccessor_testing::console::test_handle_command(
      oss, "await " + memoryaccessor_testing::tools::get_self_name(),
      "Awaiting process: " + memoryaccessor_testing::tools::get_self_name() +
          "\nProcess was found: " +
          memoryaccessor_testing::tools::get_self_name());

  std::cout.rdbuf(p_cout_streambuf);
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
  char *argv[]{::argv[0], "abcdef"};
  argv_parser.ParseArgv(argc, argv);
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
  char *argv[]{::argv[0], "--abcdef"};

  memoryaccessor_testing::argvparser::test_parse_argv(
      argc, argv, console.kProjectName + ": unknown key --abcdef\n");
}

TEST_CASE("Parse argv: key help") {
  int argc{2};
  char *argv[]{::argv[0], "--help"};

  memoryaccessor_testing::argvparser::test_parse_argv(
      argc, argv,
      console.kProjectName + " " + console.kProjectVersion + "\n" +
          console.kProjectDescription + "\n\nUsage: " + console.kProjectName +
          " [OPTION]...\n\n  --help");
}

TEST_CASE("Parse argv: key command") {
  int argc{2};
  char *argv[]{::argv[0], "--command", "help"};

  memoryaccessor_testing::argvparser::test_parse_argv(
      argc, argv,
      console.kProjectName + ": --command requires an argument\nUse --help to "
                             "see help about keys.\n");
  argc = 3;
  memoryaccessor_testing::argvparser::test_parse_argv(
      argc, argv,
      console.kProjectName + " " + console.kProjectVersion + "\n" +
          console.kProjectDescription + "\nCommands:\n");
}

TEST_CASE("Parse argv: key file") {
  std::string file_path{"./script.txt"};

  int argc{2};
  char *argv[]{::argv[0], "--file", strdup(file_path.c_str())};

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
  free(argv[2]);
}

TEST_SUITE_END();
