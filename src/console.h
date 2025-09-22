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
 \brief Console and ConsoleCommand header

 A header that contains the definition of Console class.
*/

#ifndef MEMORYACCESSOR_SRC_CONSOLE_H_
#define MEMORYACCESSOR_SRC_CONSOLE_H_

#include <array>
#include <cstdint>
#include <exception>
#include <memory>
#include <string>
#include <vector>

#include "consolecommand.h"
#include "hexviewer.h"
#include "memoryaccessor.h"
#include "processapi.h"
#include "segmentinfo.h"

struct ConsoleCommand;

/*!
 \brief A class to perform CLI.

 The class provides commandline interface to interract with MemoryAccessor
 effectively. It can handle various commands with arguments. The class can
 either execute a command as a std::string or read a command from stdin. Moving,
 copying and creating more than 1 instance of the class is prohibited.
*/
class Console {
public:
  constexpr static int kCommandsNumber{
      9}; //!< Number of the commands available.

  explicit Console(MemoryAccessor *memory_accessor, HexViewer *hex_viewer,
                   ProcessApi *process_api) noexcept;

  /*!
   \brief Copy constructor (deleted).
   \param [in] origin Console instance to copy from.

   Create a new object by copying an old one. Prohibited.
  */
  Console(const Console &origin) = delete;

  /*!
   \brief Move constructor (deleted).
   \param [in] origin Moved Console object.

   Create a new object by moving an old one. Prohibited.
  */
  Console(Console &&origin) = delete;

  /*!
   \brief Copy-assignment operator (deleted).
   \param [in] origin Console instance to copy from.

   Assign an object by copying other object. Prohibited.
  */
  Console &operator=(const Console &origin) = delete;

  /*!
   \brief Move-assignment operator (deleted).
   \param [in] origin Moved Console object.

   Assign an object by moving other object. Prohibited.
  */
  Console &operator=(Console &&origin) = delete;

  ~Console() noexcept;

  /*!
   \brief Set buffer size of an instance.
   \param [in] buffer_size Desired buffer size in bytes.

   Set buffer size, a number of bytes that are allocated when needed.
  */
  void SetBufferSize(const size_t &buffer_size) { buffer_size_ = buffer_size; }

  void PrintNameVer() const noexcept;
  void Start() noexcept;
  void ReadStdin() noexcept;
  void HandleCommand(const std::string &line) noexcept;

  const std::string kProjectName{"MemoryAccessor"}; //!< Name of the project.
  const std::string kProjectVersion{"v1.0"};        //!< Project version.
  const std::string kProjectDescription{
      "A command-line front-end for exploring virtual memory of a linux "
      "process "
      "by accessing /proc/PID/mem file."}; //!< Project description.
  const std::string kConsolePrefix{
      "(MemAcc)"}; //!< Prefix shown in console input.

  MemoryAccessor
      *memory_accessor_{nullptr};  //!< A pointer to a MemoryAccessor class instance.
  HexViewer *hex_viewer_{nullptr}; //!< A pointer to a HexViewer class instance.
  ProcessApi *process_api_{nullptr};          //!< A pointer to a ProcessApi class instance.

  const ConsoleCommand kCommands[kCommandsNumber]{
      {"help", &Console::CommandHelp, {{"help", "Show help"}}},
      {"name",
       &Console::CommandName,
       {{"name name [pid_num]", "Search for PID by name and set PID if only 1 "
                                "PID found, or set PID number"},
        {"",
         "pid_num of found PIDs if pid_num is specified (starting from 0)."}}},
      {"pid",
       &Console::CommandPid,
       {{"pid PID", "Set PID and parse /proc/PID/maps."}}},
      {"maps",
       &Console::CommandMaps,
       {{"maps", "List memory segments found by parsing /proc/PID/maps."}}},
      {"view",
       &Console::CommandView,
       {{"view SEGMENT", "Print data of memory segment, where SEGMENT is its "
                         "\"maps\" number, or first"},
        {"", "with matching name."},
        {"-h", "show hex (if no -r specified)"},
        {"-r", "print raw data"},
        {"-f file", "output to file"}}},
      {"read",
       &Console::CommandRead,
       {{"read address amount", "Read amount bytes starting from address."},
        {"-h", "show hex (if no -r specified)"},
        {"-r", "print raw data"},
        {"-f file", "output to file"}}},
      {"write",
       &Console::CommandWrite,
       {{"write address amount string",
         "Write amount bytes of string to memory starting from address."},
        {"or", ""},
        {"write address amount -f file",
         "Write amount bytes from file to memory starting from address."}}},
      {"diff",
       &Console::CommandDiff,
       {{"diff length [replacement]",
         "Find difference in memory states by length and replace to string, if "
         "specified."}}},
      {"await",
       &Console::CommandAwait,
       {{"await process_name", "Wait for the process with matching name."},
        {"await -p pid", "Wait for the process with PID."}}},
  }; //!< Definitions of commands.
private:
  /*!
   \brief 0 arguments error enumeration.

   Enumeration that describes existing types of errors that does not require
   arguments and are constant.
  */
  enum class Error0Arg {
    kPidNotSet,               //!< Error: PID is not set.
    kPidNotSetUnexpectably,   //!< Error: PID is not set unexpectably.
    kErrCheckingPid,          //!< Error while checking PID.
    kPrintErrCheckingProcess, //!< Error while checking process name.
    kPrintErrOpenMaps,        //!< Error while opening /proc/PID/maps.
    kPrintErrParseMaps,       //!< Error while parsing /proc/PID/maps.
    kPrintErrOpenMem,         //!< Error while opening /proc/PID/mem.
    kPrintSegNotExist,        //!< Error: the segment does not exist.
    kPrintSegNoAccess,        //!< Error: no access to the segment.
  };

  int SetSigint(void (*handler)(int)) const noexcept;
  
  void PrintDescription(const ConsoleCommand &command, uint32_t left = 2,
                        uint32_t middle = 0) const noexcept;
  void ShowUsage(const ConsoleCommand &command) const noexcept;
  void PrintError0Arg(const Error0Arg &error) const noexcept;
  void PrintFileNotOpened(const std::string &path) const noexcept;
  void PrintFileFail(const std::string &path) const noexcept;

  void PrintSegment(const SegmentInfo &segmentInfo) const noexcept;
  void
  PrintSegments(const std::vector<SegmentInfo> &segment_infos) const noexcept;

  std::vector<std::string> ParseCmdline(std::string line) const noexcept;

  uint8_t ParseAddress(const std::string &s, size_t &result) const noexcept;
  uint8_t StoiWrapper(const std::string &s, int &result,
                      const std::string &name) const noexcept;
  uint8_t StoullWrapper(const std::string &s, uint64_t &result,
                        const std::string &name) const noexcept;
  uint8_t ParseMapsWrapper() const noexcept;
  uint8_t CheckPidWrapper() const noexcept;
  uint8_t CheckSegNumWrapper(const size_t &num) const noexcept;
  uint8_t ReadSegWrapper(char *dst, const size_t &num, size_t start = 0,
                         size_t amount = SIZE_MAX) const noexcept;
  uint8_t WriteSegWrapper(char *src, const size_t &num, size_t start = 0,
                          size_t amount = SIZE_MAX) const noexcept;
  uint8_t ReadWrapper(char *dst, size_t address, size_t amount,
                      size_t &done_amount) const noexcept;
  uint8_t WriteWrapper(char *src, size_t address, size_t amount,
                       size_t &done_amount) const noexcept;

  std::array<std::unique_ptr<char[]>, 2>
  FindDifferencesOfLen(const char *old_str, const char *new_str, size_t str_len,
                       size_t &done, const size_t &len) const noexcept;
  uint8_t DiffReadSeg(std::unique_ptr<char[]> &mem_dump,
                      const size_t &num) noexcept;
  void DiffCompare(const char *old_dump, const char *new_dump,
                   const size_t &o_offs, const size_t &n_offs, size_t amount,
                   size_t start_addr, const size_t &length,
                   const std::string &replacement) noexcept;
  uint8_t DiffOldNext(size_t &i, const size_t &old_segments_amount,
                      std::vector<std::unique_ptr<char[]>>::iterator &it,
                      std::vector<std::unique_ptr<char[]>> &full_dump) noexcept;
  uint8_t DiffNewNext(size_t &j,
                      std::vector<std::unique_ptr<char[]>>::iterator &it,
                      std::unique_ptr<char[]> &mem_dump,
                      std::vector<std::unique_ptr<char[]>> &full_dump) noexcept;

  void CommandHelp(const ConsoleCommand &parent,
                   const std::vector<std::string> &args) noexcept;
  void CommandName(const ConsoleCommand &parent,
                   const std::vector<std::string> &args) noexcept;
  void CommandPid(const ConsoleCommand &parent,
                  const std::vector<std::string> &args) noexcept;
  void CommandMaps(const ConsoleCommand &parent,
                   const std::vector<std::string> &args) noexcept;
  void CommandView(const ConsoleCommand &parent,
                   const std::vector<std::string> &args) noexcept;
  void CommandRead(const ConsoleCommand &parent,
                   const std::vector<std::string> &args) noexcept;
  void CommandWrite(const ConsoleCommand &parent,
                    const std::vector<std::string> &args) noexcept;
  void CommandDiff(const ConsoleCommand &parent,
                   const std::vector<std::string> &args) noexcept;
  void CommandAwait(const ConsoleCommand &parent,
                    const std::vector<std::string> &args) noexcept;

  static bool one_instance_created_; //!< A static variable that is true when
                                     //!< one instance of class exists.
  const std::string kCheckSudoStr{
      "Check if you're running with \"sudo\"."}; //!< A std::string that is used
                                                 //!< to show that elevated
                                                 //!< privileges may not have
                                                 //!< been granted.

  size_t buffer_size_{
      0x1000}; //!< Size of buffers used (less than 128 may cause bugs).

  bool seg_not_exist_msg_enabled_{
      true}; //!< To print messages that segment not exist or not.
  bool seg_no_access_msg_enabled_{
      true}; //!< To print messages that access to the segment denied or not.
};

#endif // MEMORYACCESSOR_SRC_CONSOLE_H_
