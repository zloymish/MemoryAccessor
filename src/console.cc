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
 \brief Console source and ReadLine integration

  A source that contains the realization of Console class and the integration of
 GNU ReadLine library. Also, this file defines ctrl_c_pressed variable, which is
 used to detect if Ctrl-C was pressed when Console class is used.
*/

#include "console.h"

#include <readline/history.h>
#include <readline/readline.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#include <array>
#include <cmath> // log10
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip> // setfill, setw
#include <iostream>
#include <map>
#include <memory>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

#include "hexviewer.h"
#include "memoryaccessor.h"
#include "segmentinfo.h"
#include "tools.h"

bool ctrl_c_pressed{
    false}; //!< Shows if Ctrl-C was pressed if the instance of class Console is
            //!< created. When console is reading stdin, it goes to new line and
            //!< sets the variable to false.

bool Console::one_instance_created_{false};

/*!
 \brief Namespace of the file.

  This namespace is dedicated for definitions that are not in classes.
*/
namespace memoryaccessor_console_src {

static std::unordered_set<pid_t>
    completion_found_pids; //!< PIDs found in command_completion function.
static std::unordered_set<std::string>
    completion_found_names; //!< Process names found in command_completion
                            //!< function.
static std::unordered_set<std::string>
    completion_found_segment_names; //!< Segment names found in
                                    //!< command_completion function.

static Console *current_console_p{
    nullptr}; //!< Pointer to the current instance of Console class.

extern "C" {
/*!
 \brief Console Ctrl-C handler.
 \param [in] signum Signal number.

 Needs to be attached to SIGINT. Does all the needed actions to re-draw the
 console if it is getting input and sets ctrl_c_pressed to true.
*/
static void CtrlC(int signum) noexcept {
  std::cout << std::endl;

  if (rl_readline_state & RL_STATE_READCMD) {
    // Заимствование, источник кода:
    // https://stackoverflow.com/questions/16828378/readline-get-a-new-prompt-on-sigint
    // Начало заимствования:
    rl_on_new_line();       // Regenerate the prompt on a newline
    rl_replace_line("", 0); // Clear the previous text
    rl_redisplay();
    // Конец заимствования.
  }

  ctrl_c_pressed = true;
}

// Ниже 4 функции, за основу которых была взята функция из
// документации: command_generator

/*!
 \brief Command completion generator.
 \param [in] text C string that has to be the same as the beginning of a command
 name to return. \param [in] state When this variable is 0, function has to
 assign new values to its static variables. \return Found command name.

 A function that generates matches between text in parameter and available
 commands. It returns next match every time it is called. If it runs out of
 matches, it returns nullptr.
*/
// Заимствование, источник кода:
// https://tiswww.case.edu/php/chet/readline/readline.html 
// Начало заимствования (есть изменения):
static char *CompletionCommandGenerator(const char *text, int state) noexcept {
  static int list_index, len;

  if (!state) {
    list_index = 0;
    len = strlen(text);
  }

  std::string name;
  while (list_index < Console::kCommandsNumber) {
    name = current_console_p->kCommands[list_index].name;
    list_index++;
    if (name.compare(0, len, text) == 0)
      return strdup(name.c_str());
  }
  return (char *)nullptr;
}
// Конец заимствования.

/*!
 \brief PID completion generator.
 \param [in] text C string that has to be the same as the beginning of PID in
 string format. \param [in] state When this variable is 0, function has to
 assign new values to its static variables. \return Found PID C string.

 A function that generates matches between text in parameter and found PIDs in
 std::string format. It returns next match every time it is called. If it runs
 out of matches, it returns nullptr.
*/
// Заимствование, источник кода:
// https://tiswww.case.edu/php/chet/readline/readline.html
//Начало заимствования (есть изменения):
static char *CompletionPidGenerator(const char *text, int state) noexcept {
  static int len;
  static std::unordered_set<pid_t>::iterator it, end_it;

  if (!state) {
    it = completion_found_pids.begin();
    end_it = completion_found_pids.end();
    len = strlen(text);
  }

  std::string name;
  while (it != end_it) {
    name = std::to_string(*it);
    it++;
    if (name.compare(0, len, text) == 0)
      return strdup(name.c_str());
  }
  return (char *)nullptr;
}
// Конец заимствования.

/*!
 \brief Process name completion generator.
 \param [in] text C string that has to be the same as the beginning of a process
 name to return. \param [in] state When this variable is 0, function has to
 assign new values to its static variables. \return Found process name.

 A function that generates matches between text in parameter and found process
 names. It returns next match every time it is called. If it runs out of
 matches, it returns nullptr.
*/
// Заимствование, источник кода:
// https://tiswww.case.edu/php/chet/readline/readline.html
// Начало заимствования (есть изменения):
static char *CompletionNameGenerator(const char *text, int state) noexcept {
  static int len;
  static std::unordered_set<std::string>::iterator it, end_it;

  if (!state) {
    it = completion_found_names.begin();
    end_it = completion_found_names.end();
    len = strlen(text);
  }

  std::string name;
  while (it != end_it) {
    name = *it;
    it++;
    if (name.compare(0, len, text) == 0)
      return strdup(name.c_str());
  }
  return (char *)nullptr;
}
// Конец заимствования.

/*!
 \brief Segment name completion generator.
 \param [in] text C string that has to be the same as the beginning of a segment
 name to return. \param [in] state When this variable is 0, function has to
 assign new values to its static variables. \return Found segment name.

 A function that generates matches between text in parameter and found segment
 names. It returns next match every time it is called. If it runs out of
 matches, it returns nullptr.
*/
// Заимствование, источник кода:
// https://tiswww.case.edu/php/chet/readline/readline.html
// Начало заимствования (есть изменения):
static char *CompletionSegmentNameGenerator(const char *text,
                                            int state) noexcept {
  static int len;
  static std::unordered_set<std::string>::iterator it, end_it;

  if (!state) {
    it = completion_found_segment_names.begin();
    end_it = completion_found_segment_names.end();
    len = strlen(text);
  }

  std::string name;
  while (it != end_it) {
    name = *it;
    it++;
    if (name.compare(0, len, text) == 0)
      return strdup(name.c_str());
  }
  return (char *)nullptr;
}
// Конец заимствования.

// Ниже функция, за основу которой была взята функция из документации
// (command_completion)

/*!
 \brief Completion function.
 \param [in] text C string that has to be the same as the beginning of a segment
 name to return. \param [in] start Start boundary of the parameter "text" in
 rl_line_buffer global variable (the line that already have been typed). \param
 [in] end End boundary of the parameter "text" in rl_line_buffer global variable
 (the line that already have been typed). \return Found completions in type of
 an array of C strings.

 A function that generates matches between text in parameter and some collection
 depending on what have been typed in the console. If completion is needed for
 the first word ("start" parameter is 0), it returns a command completion array.
 If the line starts from "pid " or "await -p " a PID completion array is
 returned. If the line starts from "name " or "await ", a process name
 completion array is returned. If the line starts from "view ", a segment name
 completion array is returned.
*/
// Заимствование, источник кода:
// https://tiswww.case.edu/php/chet/readline/readline.html Начало заимствования
// (есть изменения):
static char **completion(const char *text, int start, int end) noexcept {
  char **matches{(char **)nullptr};

  if (start == 0)
    matches = rl_completion_matches(text, CompletionCommandGenerator);
  else {
    std::string s(rl_line_buffer);
    if (s.substr(0, 4) == "pid " || s.substr(0, 9) == "await -p ") {
      completion_found_pids = current_console_p->tools_.GetAllPids();
      matches = rl_completion_matches(text, CompletionPidGenerator);
    } else if (s.substr(0, 5) == "name " || s.substr(0, 6) == "await ") {
      completion_found_names = current_console_p->tools_.GetAllProcessNames();
      matches = rl_completion_matches(text, CompletionNameGenerator);
    } else if (s.substr(0, 5) == "view ") {
      completion_found_segment_names =
          current_console_p->memory_accessor_.GetAllSegmentNames();
      matches = rl_completion_matches(text, CompletionSegmentNameGenerator);
    }
  }

  return matches;
}
// Конец заимствования.
}

} // namespace memoryaccessor_console_src

/*!
 \brief Constructor.
 \param [in,out] memory_accessor A reference to an instance of MemoryAccessor
 class. \param [in,out] hex_viewer A reference to an instance of HexViewer
 class. \param [in,out] tools A reference to an instance of Tools struct. \throw
 std::logic_error If an instance of the class have already been created and it
 is a second instance.

 Initializes MemoryAccessor class, HexViewer class and Tools struct references
 by values got as parameters. Throws an exception if an instance of the class
 have already been created. Sets one_instance_created_ to true.
*/
Console::Console(MemoryAccessor &memory_accessor, HexViewer &hex_viewer,
                 Tools &tools) noexcept(false)
    : memory_accessor_(memory_accessor), hex_viewer_(hex_viewer),
      tools_(tools) {
  if (one_instance_created_)
    throw std::logic_error("Only one instance of Console can be created");
  one_instance_created_ = true;
}

/*!
 \brief Destructor.

 Deletes SIGINT handler attach if it is set to
 memoryaccessor_console_src::CtrlC, sets variables current_console_p to nullptr,
 rl_attempted_completion_function to nullptr and one_instance_created_ to false.
*/
Console::~Console() noexcept {
  struct sigaction old_sigact;
  sigaction(SIGINT, nullptr, &old_sigact);

  if (old_sigact.sa_handler == memoryaccessor_console_src::CtrlC)
    tools_.SetSigint(SIG_DFL);

  memoryaccessor_console_src::current_console_p = nullptr;
  rl_attempted_completion_function = nullptr;

  one_instance_created_ = false;
}

/*!
 \brief Print project name and version.

 Print project name and version to stdout.
*/
void Console::PrintNameVer() const noexcept {
  std::cout << kProjectName << " " << kProjectVersion << std::endl;
}

/*!
 \brief Start the console.

 Try to set handler for SIGINT, set
 memoryaccessor_console_src::current_console_p, rl_attempted_completion_function
 and print greeting message to stdout.
*/
void Console::Start() noexcept {
  if (tools_.SetSigint(memoryaccessor_console_src::CtrlC))
    std::cerr
        << "Couldn't assign handler to SIGINT. Ctrl-C will not be working."
        << std::endl;

  memoryaccessor_console_src::current_console_p = this;
  rl_attempted_completion_function = memoryaccessor_console_src::completion;

  PrintNameVer();
  std::cout << "Type \"help\" for help." << std::endl;
}

/*!
 \brief Read and process input from stdin.

 Print prefix of a console, read input and process it (add line to history and
 handle command). If Ctrl-D is pressed, print "Quit" and exit the program with
 the code 0.
*/
void Console::ReadStdin() noexcept {
  if (ctrl_c_pressed)
    ctrl_c_pressed = false;

  char *s = readline((kConsolePrefix + " ").c_str());

  if (!s) { // Ctrl-D
    std::free(s);
    std::cout << "Quit" << std::endl;
    std::exit(0);
  }

  if (s[0] != '\0') {
    add_history(s);
    HandleCommand(std::string(s));
  }

  std::free(s);
}

/*!
 \brief Handle line with command.
 \param [in] line Line containing the command.f

 Parse and execute the given line containing the command.
*/
void Console::HandleCommand(const std::string &line) noexcept {
  if (line.empty())
    return;

  std::vector<std::string> args{ParseCmdline(line)};

  if (!args.size())
    return;

  std::string command_name{args[0]};
  args.erase(args.begin());

  auto command_p = std::find_if(std::begin(kCommands), std::end(kCommands),
                                [command_name](const Command &command) {
                                  return command_name == command.name;
                                });
  if (command_p != std::end(kCommands)) {
    (this->*((*command_p).func))(*command_p, args);
  } else {
    std::cerr << command_name << ": command not found" << std::endl;
  }
}

/*!
 \brief Print description of a command.
 \param [in] command Command object to print description of which.
 \param [in] left How many characters to skip from the start of the terminal
 line. Default is 2. \param [in] middle How many characters is taken to print
 the left side of the description line. If the left side is shorter, whitespaces
 are added. If this parameter is 0, middle is set to the longest left part of
 all lines. Default is 0.

 Print description of a command in a proper format. An offset after the start of
 the line can be made ("left" parameter), also a minimum length of the left side
 can be set ("middle" parameter).
*/
void Console::PrintDescription(const Command &command, uint32_t left,
                               uint32_t middle) const noexcept {
  struct winsize ws;
  ioctl(0, TIOCGWINSZ, &ws);

  if (!middle) {
    for (const std::array<std::string, 2> &line : command.description) {
      if (line[0].length() > middle)
        middle = line[0].length();
    }
  }

  for (const std::array<std::string, 2> &line : command.description) {
    // Заимствование, источник кода:
    // https://unix.stackexchange.com/questions/210325/posix-command-that-moves-cursor-to-specific-position-in-terminal-window
    // Начало заимствования (есть изменения):
    std::cout << "\33\[" + std::to_string(ws.ws_row) + ";" +
                     std::to_string(left + 1) + "H";
    // Конец заимствования.
    std::cout << std::setfill(' ') << std::setw(middle + 3) << std::left
              << line[0] << line[1] << std::endl;
  }
}

/*!
 \brief Print usage message of a command.
 \param [in] command Command object to print description of which.

 Print "Usage: " and a formatted manual of a command with offset from left.
*/
void Console::ShowUsage(const Command &command) const noexcept {
  std::cout << "Usage:";
  PrintDescription(command, 7);
}

/*!
 \brief Print an error message, text of which does not change.
 \param [in] error Type of message to print.

 Print an error message to stderr, text of which does not change. If printing
 the specified message is disabled, do nothing.
*/
void Console::PrintError0Arg(const Error0Arg &error) const noexcept {
  switch (error) {
  case Error0Arg::kPidNotSet:
    std::cerr << "PID is not set. Set it with the command \"pid\"."
              << std::endl;
    break;
  case Error0Arg::kPidNotSetUnexpectably:
    std::cerr << "Seems like PID was not set properly. Try to set it again."
              << std::endl;
    break;
  case Error0Arg::kErrCheckingPid:
    std::cerr << "An error occured while checking if PID exists. "
              << kCheckSudoStr << std::endl;
    break;
  case Error0Arg::kPrintErrCheckingProcess:
    std::cerr << "An error occured while checking if the process exists. "
              << kCheckSudoStr << std::endl;
    break;
  case Error0Arg::kPrintErrOpenMaps:
    std::cerr << "Error in opening /maps file. " << kCheckSudoStr << std::endl;
    break;
  case Error0Arg::kPrintErrParseMaps:
    std::cerr << "Error in parsing /maps file. " << kCheckSudoStr << std::endl;
    break;
  case Error0Arg::kPrintErrOpenMem:
    std::cerr << "Error in opening /mem file. " << kCheckSudoStr << std::endl;
    break;
  case Error0Arg::kPrintSegNotExist:
    if (seg_not_exist_msg_enabled_)
      std::cerr << "Attempt to reach a segment that does not exist."
                << std::endl;
    break;
  case Error0Arg::kPrintSegNoAccess:
    if (seg_no_access_msg_enabled_)
      std::cerr << "Reached segment to which we don't have access."
                << std::endl;
    break;
  }
}

/*!
 \brief Print message that file cannot be opened.
 \param [in] path Given path to the file.

 Print message that a file cannot be opened to stderr.
*/
void Console::PrintFileNotOpened(const std::string &path) const noexcept {
  std::cerr << path << ": could not open file" << std::endl;
}

/*!
 \brief Print message that file failed.
 \param [in] path Given path to the file.

 Print message that a file cannot be read anymore to stderr.
*/
void Console::PrintFileFail(const std::string &path) const noexcept {
  std::cerr << path << ": cannot read more data from file." << std::endl;
}

/*!
 \brief Print information about segment.
 \param [in] segmentInfo SegmentInfo object.

 Print information about one memory segment in a style it is printed in
 /proc/PID/maps.
*/
void Console::PrintSegment(const SegmentInfo &segmentInfo) const noexcept {
  std::ostringstream oss;
  oss << std::hex << segmentInfo.start << '-' << segmentInfo.end << ' '
      << tools_.EncodePermissions(segmentInfo.mode) << ' ' << std::setfill('0')
      << std::setw(8) << std::right << segmentInfo.offset << ' '
      << std::setfill('0') << std::setw(2) << std::right << segmentInfo.major_id
      << ':' << std::setfill('0') << std::setw(2) << std::right
      << segmentInfo.minor_id << ' ' << std::dec << segmentInfo.inode_id << ' ';

  std::cout << std::setfill(' ') << std::setw(73) << std::left << oss.str()
            << segmentInfo.path << '\n';
}

/*!
 \brief Print information about multiple segments.
 \param [in] segment_infos_ std::vector of SegmentInfo.

 Print information about multiple memory segments in a style it is printed in
 /proc/PID/maps, adding count at the beginning of each line.
*/
void Console::PrintSegments(
    const std::vector<SegmentInfo> &segment_infos_) const noexcept {
  size_t segment_infos_size{segment_infos_.size()};

  if (segment_infos_size) {
    uint32_t num_width{
        static_cast<uint32_t>(std::log10(segment_infos_size - 1) + 1)};

    for (uint32_t i{0}; i < segment_infos_size; i++) {
      if (ctrl_c_pressed) {
        ctrl_c_pressed = false;
        break;
      }

      std::cout << std::setfill(' ') << std::setw(num_width + 2) << std::right
                << std::to_string(i) + ". ";
      PrintSegment(segment_infos_[i]);
    }
  }
}

/*!
 \brief Parse line containing a Console command.
 \param [in] line Line with a command.
 \return std::vector of arguments, where 0th argument is command name.

 Parse line containing a Console command with certain rules and return
 std::vector of arguments. The function replaces escape sequences \\a, \\b, \\f,
 \\n, \\r, \\t, \\v to the corresponding ASCII characters, takes into account
 quotes when they are not escaped and separates arguments by spaces when they
 are not escaped or located inside quotes.
*/
std::vector<std::string>
Console::ParseCmdline(std::string line) const noexcept {
  std::vector<std::string> result;
  bool in_quote{false}, in_escape{false}, was_space{false};
  std::string param;

  for (const char &c : line) {
    if (in_escape) {
      char res_c;

      // no use of this except here, so no need of another function
      switch (c) {
      case 'a':
        res_c = '\a';
        break;
      case 'b':
        res_c = '\b';
        break;
      case 'f':
        res_c = '\f';
        break;
      case 'n':
        res_c = '\n';
        break;
      case 'r':
        res_c = '\r';
        break;
      case 't':
        res_c = '\t';
        break;
      case 'v':
        res_c = '\v';
        break;
      default:
        res_c = c;
        break;
      }

      param.push_back(res_c);

      in_escape = false;
      continue;
    }

    if (c == ' ') {
      if (!in_quote) {
        if (!was_space && !param.empty()) {
          result.push_back(param);
          param.clear();
        }
        was_space = true;
        continue;
      }
    } else {
      was_space = false;

      if (c == '\"') {
        in_quote = !in_quote;
        continue;
      } else if (c == '\\') {
        in_escape = true;
        continue;
      }
    }

    param.push_back(c);
  }

  if (!param.empty())
    result.push_back(param);

  return result;
}

/*!
 \brief Get size_t value from std::string representing HEX.
 \param [in] s std::string to process.
 \param [out] result Variable to store the result.
 \return Return code, 0 is success, 1 is failure.

 Get size_t value from std::string representing HEX and store it to the variable
 by the reference result. If there was a failure, print error message to stderr.
*/
uint8_t Console::ParseAddress(const std::string &s,
                              size_t &result) const noexcept {
  std::istringstream iss(s);
  iss >> std::hex >> result;
  if (iss.fail()) {
    std::cerr << "Not an address: " << s << std::endl;
    return 1;
  }
  return 0;
}

/*!
 \brief Use std::stoi function and print messages in case of errors.
 \param [in] s std::string to process.
 \param [out] result Variable to store the result.
 \param [in] name Display name of the variable to use in error messages.
 \return Return code, 0 is success, 1 is "not a number" error, 2 is "too big
 number" error.

 Get int value from std::string and store it to the variable by the reference
 result. If there was a failure, print error message to stderr using display
 name "name".
*/
uint8_t Console::StoiWrapper(const std::string &s, int &result,
                             const std::string &name) const noexcept {
  try {
    result = std::stoi(s);
  } catch (const std::invalid_argument &ex) {
    std::cerr << "Not a(n) " << name << ": " << s << std::endl;
    return 1;
  } catch (const std::out_of_range &ex) {
    std::cerr << "Specified " << name << " is too big: " << s << std::endl;
    return 2;
  }
  return 0;
}

/*!
 \brief Use std::stoull function and print messages in case of errors.
 \param [in] s std::string to process.
 \param [out] result Variable to store the result.
 \param [in] name Display name of the variable to use in error messages.
 \return Return code, 0 is success, 1 is "not a number" error, 2 is "too big
 number" error.

 Get uint64_t value from std::string and store it to the variable by the
 reference result. If there was a failure, print error message to stderr using
 display name "name".
*/
uint8_t Console::StoullWrapper(const std::string &s, uint64_t &result,
                               const std::string &name) const noexcept {
  try {
    result = std::stoull(s);
  } catch (const std::invalid_argument &ex) {
    std::cerr << "Not a(n) " << name << ": " << s << std::endl;
    return 1;
  } catch (const std::out_of_range &ex) {
    std::cerr << "Specified " << name << " is too big: " << s << std::endl;
    return 2;
  }
  return 0;
}

/*!
 \brief Parse /proc/PID/maps and print messages in case of errors.
 \return Return code, 0 is success, 1 is an error and Reset was made.

 Parse /proc/PID/maps and print messages to stderr in case of errors. When an
 error occured, call a function Reset of memory_accessor_.
*/
uint8_t Console::ParseMapsWrapper() const noexcept {
  try {
    try {
      memory_accessor_.ParseMaps();
    } catch (const MemoryAccessor::MapsFileEx &ex) {
      PrintError0Arg(Error0Arg::kPrintErrOpenMaps);
      throw WrapperException(1);
    } catch (const MemoryAccessor::BadMapsEx &ex) {
      PrintError0Arg(Error0Arg::kPrintErrParseMaps);
      throw WrapperException(1);
    } catch (const MemoryAccessor::PidNotSetEx &ex) {
      PrintError0Arg(Error0Arg::kPidNotSetUnexpectably);
      throw WrapperException(1);
    }
  } catch (const WrapperException &ex) {
    memory_accessor_.Reset();
    return ex.return_code;
  }

  return 0;
}

/*!
 \brief Check if PID is set and print message if it is not.
 \return Return code, 0 is success, 1 is an error (PID not set).

 Check if PID is set and print messages to stderr if it is not.
*/
uint8_t Console::CheckPidWrapper() const noexcept {
  try {
    memory_accessor_.CheckPid();
  } catch (const MemoryAccessor::PidNotSetEx &ex) {
    PrintError0Arg(Error0Arg::kPidNotSet);
    return 1;
  }

  return 0;
}

/*!
 \brief Check segment number and print messages in case of errors.
 \param [in] num Number of the segment.
 \return Return code, 0 is success, 1 is an error.

 Check segment number and print messages to stderr in case of errors.
*/
uint8_t Console::CheckSegNumWrapper(const size_t &num) const noexcept {
  try {
    try {
      memory_accessor_.CheckSegNum(num);
    } catch (const MemoryAccessor::PidNotSetEx &ex) {
      PrintError0Arg(Error0Arg::kPidNotSet);
      throw WrapperException(1);
    } catch (const MemoryAccessor::SegmentEx &ex) {
      PrintError0Arg(Error0Arg::kPrintSegNotExist);
      throw WrapperException(1);
    }
  } catch (const WrapperException &ex) {
    return ex.return_code;
  }

  return 0;
}

/*!
 \brief Read segment and print messages in case of errors.
 \param [out] dst Destination to which data will be copied.
 \param [in] num Number of the memory segment starting from 0.
 \param [in] start Offset relative to the start of the segment, default is 0.
 \param [in] amount Number of bytes to capture after the "start" parameter,
 default is SIZE_MAX. If a value is too big, it is set to a maximum appropriate
 value. \return Return code, 0 is success, 1 is a "bad" error (related to PID or
 /proc/PID/mem), 2 is a segment error.

 Read segment and print messages to stderr in case of errors.
*/
uint8_t Console::ReadSegWrapper(char *dst, const size_t &num, size_t start,
                                size_t amount) const noexcept {
  try {
    try {
      memory_accessor_.ReadSegment(dst, num, start, amount);
    } catch (const MemoryAccessor::PidNotSetEx &ex) {
      PrintError0Arg(Error0Arg::kPidNotSet);
      throw WrapperException(1);
    } catch (const MemoryAccessor::MemFileEx &ex) {
      PrintError0Arg(Error0Arg::kPrintErrOpenMem);
      throw WrapperException(1);
    } catch (const MemoryAccessor::SegmentAccessDeniedEx &ex) {
      PrintError0Arg(Error0Arg::kPrintSegNoAccess);
      throw WrapperException(2);
    } catch (const MemoryAccessor::SegmentEx &ex) {
      PrintError0Arg(Error0Arg::kPrintSegNotExist);
      throw WrapperException(2);
    }
  } catch (const WrapperException &ex) {
    return ex.return_code;
  }

  return 0;
}

/*!
 \brief Write data to segment and print messages in case of errors.
 \param [in] src Source from which data will be copied.
 \param [in] num Number of the memory segment starting from 0.
 \param [in] start Offset relative to the start of the segment, default is 0.
 \param [in] amount Number of bytes to capture after the "start" parameter,
 default is SIZE_MAX. If a value is too big, it is set to a maximum appropriate
 value. \return Return code, 0 is success, 1 is a "bad" error (related to PID or
 /proc/PID/mem), 2 is a segment error.

 Write data to segment and print messages to stderr in case of errors.
*/
uint8_t Console::WriteSegWrapper(char *src, const size_t &num, size_t start,
                                 size_t amount) const noexcept {
  try {
    try {
      memory_accessor_.WriteSegment(src, num, start, amount);
    } catch (const MemoryAccessor::PidNotSetEx &ex) {
      PrintError0Arg(Error0Arg::kPidNotSet);
      throw WrapperException(1);
    } catch (const MemoryAccessor::MemFileEx &ex) {
      PrintError0Arg(Error0Arg::kPrintErrOpenMem);
      throw WrapperException(1);
    } catch (const MemoryAccessor::SegmentAccessDeniedEx &ex) {
      PrintError0Arg(Error0Arg::kPrintSegNoAccess);
      throw WrapperException(2);
    } catch (const MemoryAccessor::SegmentEx &ex) {
      PrintError0Arg(Error0Arg::kPrintSegNotExist);
      throw WrapperException(2);
    }
  } catch (const WrapperException &ex) {
    return ex.return_code;
  }

  return 0;
}

/*!
 \brief Read data from /proc/PID/mem and print messages in case of errors.
 \param [out] dst Destination to which data will be copied.
 \param [in] address Address to start from.
 \param [in] amount Number of bytes to read.
 \param [out] done_amount How much data were read (add to existing value).
 \return Return code, 0 is success, 1 is a "bad" error (related to PID or
 /proc/PID/mem), 2 is a segment error.

 Read data from /proc/PID/mem and print messages to stderr in case of errors.
*/
uint8_t Console::ReadWrapper(char *dst, size_t address, size_t amount,
                             size_t &done_amount) const noexcept {
  try {
    try {
      memory_accessor_.Read(dst, address, amount, done_amount);
    } catch (const MemoryAccessor::PidNotSetEx &ex) {
      PrintError0Arg(Error0Arg::kPidNotSet);
      throw WrapperException(1);
    } catch (const MemoryAccessor::MemFileEx &ex) {
      PrintError0Arg(Error0Arg::kPrintErrOpenMem);
      throw WrapperException(1);
    } catch (const MemoryAccessor::SegmentAccessDeniedEx &ex) {
      PrintError0Arg(Error0Arg::kPrintSegNoAccess);
      throw WrapperException(2);
    } catch (const MemoryAccessor::SegmentEx &ex) {
      PrintError0Arg(Error0Arg::kPrintSegNotExist);
      throw WrapperException(2);
    }
  } catch (const WrapperException &ex) {
    return ex.return_code;
  }

  return 0;
}

/*!
 \brief Write data to /proc/PID/mem and print messages in case of errors.
 \param [in] src Source from which data will be copied.
 \param [in] address Address to start from.
 \param [in] amount Number of bytes to write.
 \param [out] done_amount How much data were written (add to existing value).
 \return Return code, 0 is success, 1 is a "bad" error (related to PID or
 /proc/PID/mem), 2 is a segment error.

 Write data to /proc/PID/mem and print messages to stderr in case of errors.
*/
uint8_t Console::WriteWrapper(char *src, size_t address, size_t amount,
                              size_t &done_amount) const noexcept {
  try {
    try {
      memory_accessor_.Write(src, address, amount, done_amount);
    } catch (const MemoryAccessor::PidNotSetEx &ex) {
      PrintError0Arg(Error0Arg::kPidNotSet);
      throw WrapperException(1);
    } catch (const MemoryAccessor::MemFileEx &ex) {
      PrintError0Arg(Error0Arg::kPrintErrOpenMem);
      throw WrapperException(1);
    } catch (const MemoryAccessor::SegmentAccessDeniedEx &ex) {
      PrintError0Arg(Error0Arg::kPrintSegNoAccess);
      throw WrapperException(2);
    } catch (const MemoryAccessor::SegmentEx &ex) {
      PrintError0Arg(Error0Arg::kPrintSegNotExist);
      throw WrapperException(2);
    }
  } catch (const WrapperException &ex) {
    return ex.return_code;
  }

  return 0;
}

/*!
 \brief Dump segment to unique_ptr (related to diff).
 \param [out] mem_dump Destination in which dump will be created.
 \param [in] num Number of the segment.
 \return Return code, 0 is success, 1 is a "bad" error (related to PID or
 /proc/PID/mem).

 Dump segment to unique_ptr variable located outside. If have no access or
 segment does not exist, unique_ptr contains nullptr. Returns 1 in case of a
 "bad" error, otherwise returns 0.
*/
uint8_t Console::DiffReadSeg(std::unique_ptr<char[]> &mem_dump,
                             const size_t &num) noexcept {
  mem_dump =
      std::make_unique<char[]>(memory_accessor_.segment_infos_[num].end -
                               memory_accessor_.segment_infos_[num].start);
  switch (ReadSegWrapper(mem_dump.get(), num)) {
  case 1:
    return 1;
  case 2:
    mem_dump.reset();
  default:
    return 0;
  }
}

/*!
 \brief Compare parts of segments, print differences and replace if possible
 (related to diff). \param [in] old_dump Pointer to the beginning of an old
 segment. \param [in] new_dump Pointer to the beginning of a new segment. \param
 [in] o_offs Offset for an old segment. \param [in] n_offs Offset for a new
 segment. \param [in] amount Amount of data to process. \param [in] start_addr
 Address of the beginning of the compared data in victim process. \param [in]
 length Length of differences to find. \param [in] replacement String to replace
 difference.

 Compare parts of old and new segments specified by their offsets and amount of
 data to process, find differences of specified length and print to stdout, and
 replace found arrays to replacement string in memory, if replacement is not
 empty.
*/
void Console::DiffCompare(const char *old_dump, const char *new_dump,
                          const size_t &o_offs, const size_t &n_offs,
                          size_t amount, size_t start_addr,
                          const size_t &length,
                          const std::string &replacement) noexcept {
  if (old_dump && new_dump) {
    std::array<std::unique_ptr<char[]>, 2> found;
    old_dump += o_offs;
    new_dump += n_offs;
    start_addr -= length;
    std::ostringstream oss;
    size_t done{0};

    while (amount) {
      found =
          tools_.FindDifferencesOfLen(old_dump, new_dump, amount, done, length);
      old_dump += done;
      new_dump += done;
      amount -= done;
      start_addr += done;

      if (found[0] && found[1]) {
        std::cout << "Found:\n";
        hex_viewer_.PrintHex(&std::cout, found[0].get(), length, start_addr,
                             true);
        hex_viewer_.PrintHex(&std::cout, found[1].get(), length, start_addr,
                             true);

        if (!replacement.empty()) {
          oss.clear();
          oss.str("");
          oss << std::hex << start_addr;
          HandleCommand("write " + oss.str() + " " + std::to_string(length) +
                        " " + replacement);
        }
      }
    }
  }
}

/*!
 \brief Go to next old segment (related to diff).
 \param [in,out] i Current number of the old segment in use.
 \param [in] old_segments_amount Amount of old segments.
 \param [out] it An iterator referring to the used old segment in full_dump.
 \param [out] full_dump Vector of dumped segments that is updating.
 \return Return code, 0 is success, 1 means that the end of old segments is
 reached.

 Increment old segment in use: add 1 to its number (i) and erase previous
 segment in full_dump.
*/
uint8_t
Console::DiffOldNext(size_t &i, const size_t &old_segments_amount,
                     std::vector<std::unique_ptr<char[]>>::iterator &it,
                     std::vector<std::unique_ptr<char[]>> &full_dump) noexcept {
  it = full_dump.erase(it);
  i++;
  if (i == old_segments_amount)
    return 1;
  return 0;
}

/*!
 \brief Go to next new segment (related to diff).
 \param [in,out] j Current number of the new segment in use.
 \param [out] it An iterator referring to the used old segment in full_dump.
 \param [out] mem_dump unique_ptr containing dump of current new segment in use.
 \param [out] full_dump Vector of dumped segments that is updating.
 \return Return code, 0 is success, 1 means that the end of new segments is
 reached.

 Increment new segment in use: insert previous new in full_dump before the old
 segment in use, add 1 to its number (j) and create dump of the next new segment
 in mem_dump.
*/
uint8_t
Console::DiffNewNext(size_t &j,
                     std::vector<std::unique_ptr<char[]>>::iterator &it,
                     std::unique_ptr<char[]> &mem_dump,
                     std::vector<std::unique_ptr<char[]>> &full_dump) noexcept {
  full_dump.insert(it, std::move(mem_dump));
  it++;
  j++;
  if (DiffReadSeg(mem_dump, j))
    return 2;
  if (j == memory_accessor_.segment_infos_.size())
    return 1;
  return 0;
}

/*!
 \brief Handle command "help".
 \param [in] parent Related Command object.
 \param [in] args Arguments for the command.

 Print project name, version and description, and then list all descriptions of
 commands.
*/
void Console::CommandHelp(const Command &parent,
                          const std::vector<std::string> &args) noexcept {
  PrintNameVer();
  std::cout << kProjectDescription << std::endl;
  //	std::cout << std::endl;
  std::cout << "Commands:\n" << std::endl;

  uint32_t middle{0};
  for (const Command &command : kCommands) {
    for (const std::array<std::string, 2> &line : command.description) {
      if (line[0].length() > middle)
        middle = line[0].length();
    }
  }

  for (const Command &command : kCommands) {
    PrintDescription(command, 2, middle);
    std::cout << std::endl;
  }
}

/*!
 \brief Handle command "name".
 \param [in] parent Related Command object.
 \param [in] args Arguments for the command.

 Search for PID by process name provided as the first argument and set PID by
 calling command_pid if only 1 PID found, or set PID number pid_num of found
 PIDs if pid_num is specified (starting from 0) as the second argument. Print
 usage otherwise.
*/
void Console::CommandName(const Command &parent,
                          const std::vector<std::string> &args) noexcept {
  if (args.size() < 1) {
    ShowUsage(parent);
    return;
  }

  int pid_num{-1};
  if (args.size() >= 2)
    if (StoiWrapper(args[1], pid_num, "pid number") != 0)
      return;

  std::unordered_set<pid_t> pids{tools_.FindPidsByName(args[0])};
  switch (pids.size()) {
  case 0:
    std::cerr << "No PID found by name: " + args[0] << std::endl;
    break;

  case 1:
    std::cout << "Found PID: " + std::to_string(*(pids.begin())) << std::endl;
    HandleCommand("pid " + std::to_string(*(pids.begin())));
    break;

  default:
    std::cout << "Found PIDs: ";
    for (const pid_t &p : pids)
      std::cout << std::to_string(p) + ' ';
    std::cout << std::endl;
    if (pid_num == -1)
      std::cout << "Set any of them with the command \"pid\"." << std::endl;
    else {
      if (pid_num < 0 || pid_num > pids.size() - 1)
        std::cerr << "Wrong found PID number: " << args[1] << std::endl;
      else {
        auto pid_iterator{pids.begin()};
        std::advance(pid_iterator, pid_num);
        HandleCommand("pid " + std::to_string(*pid_iterator));
      }
    }
    break;
  }
}

/*!
 \brief Handle command "pid".
 \param [in] parent Related Command object.
 \param [in] args Arguments for the command.

 Set PID provided as the first argument and parse /proc/PID/maps. Print usage
 otherwise.
*/
void Console::CommandPid(const Command &parent,
                         const std::vector<std::string> &args) noexcept {
  if (args.size() < 1) {
    ShowUsage(parent);
    return;
  }

  pid_t pid{0};
  if (StoiWrapper(args[0], pid, "PID") != 0)
    return;

  try {
    memory_accessor_.SetPid(pid);
    std::cout << "Set PID: " << args[0] << std::endl;
  } catch (const MemoryAccessor::ErrCheckingPidEx &ex) {
    PrintError0Arg(Error0Arg::kErrCheckingPid);
    return;
  } catch (const MemoryAccessor::PidNotExistEx &ex) {
    std::cerr << "The process with PID " << args[0] << " does not exist."
              << std::endl;
    return;
  }

  std::cout << "Parsing /proc/" << args[0] << "/maps..." << std::endl;
  if (ParseMapsWrapper() != 0)
    return;

  bool any_special_segment_found{false};
  for (const std::pair<std::string, SegmentInfo *> &p :
       memory_accessor_.special_segment_found_) {
    if (std::get<1>(p)) {
      if (!any_special_segment_found) {
        any_special_segment_found = true;
        std::cout << "Found:";
      }
      std::cout << " " << std::get<0>(p);
    }
  }
  if (any_special_segment_found)
    std::cout << std::endl;

  std::cout << "Found "
            << std::to_string(memory_accessor_.segment_infos_.size())
            << (memory_accessor_.segment_infos_.size() == 1 ? " segment"
                                                            : " segments")
            << " in total." << std::endl;
}

/*!
 \brief Handle command "maps".
 \param [in] parent Related Command object.
 \param [in] args Arguments for the command.

 Print to stdout memory segments found by parsing /proc/PID/maps.
*/
void Console::CommandMaps(const Command &parent,
                          const std::vector<std::string> &args) noexcept {
  try {
    memory_accessor_.CheckPid();
    PrintSegments(memory_accessor_.segment_infos_);
  } catch (const MemoryAccessor::PidNotSetEx &ex) {
    PrintError0Arg(Error0Arg::kPidNotSet);
  }
}

/*!
 \brief Handle command "view".
 \param [in] parent Related Command object.
 \param [in] args Arguments for the command.

 Print data of memory segment with name or PID provided as the first argument.
 If there are multiple segments with the same name, print data from the first
 matching. Keys available: "-h" - show hex (if no "-r" specified), "-r" - print
 raw data, "-f file" write output to file "file". Print usage in case of usage
 errors.
*/
void Console::CommandView(const Command &parent,
                          const std::vector<std::string> &args) noexcept {
  bool raw{false}, hex{false};

  std::string file_path, segment;

  uint32_t par_amount{static_cast<uint32_t>(args.size())};
  for (uint32_t par_num{0}; par_num < par_amount; par_num++) {
    if (args[par_num].empty())
      continue;

    if (args[par_num][0] == '-') {
      if (args[par_num].length() == 1)
        continue;

      for (uint32_t ch_num{1}; ch_num < args[par_num].length(); ch_num++) {
        switch (args[par_num][ch_num]) {
        case 'r':
          raw = true;
          break;
        case 'h':
          hex = true;
          break;
        case 'f':
          if (par_num != par_amount - 1 && file_path.empty()) {
            par_num++;
            file_path = args[par_num];
          } else {
            ShowUsage(parent);
            return;
          }
          break;
        }
      }
    } else {
      if (segment.empty())
        segment = args[par_num];
    }
  }

  if (segment.empty()) {
    ShowUsage(parent);
    return;
  }

  if (CheckPidWrapper() != 0)
    return;

  size_t num{0}, segment_infos_size{memory_accessor_.segment_infos_.size()};
  try {
    num = std::stoull(segment);
    if (num >= segment_infos_size) {
      PrintError0Arg(Error0Arg::kPrintSegNotExist);
      return;
    }
  } catch (const std::invalid_argument &ex) {
    size_t i{0};

    for (; i < segment_infos_size; i++) {
      if (memory_accessor_.segment_infos_[i].path == segment) {
        num = i;
        break;
      }
    }

    if (i >= segment_infos_size) {
      PrintError0Arg(Error0Arg::kPrintSegNotExist);
      return;
    }
  } catch (const std::out_of_range &ex) {
    std::cerr << "Specified segment number is too big: " << segment
              << std::endl;
    return;
  }

  std::ostream *stream_p{nullptr};
  std::ofstream file;
  if (!file_path.empty()) {
    file.open(file_path, std::ios::out | std::ios::binary);
    stream_p = &file;
  } else {
    stream_p = &std::cout;
  }

  if (CheckSegNumWrapper(num) != 0)
    return;

  size_t size{memory_accessor_.segment_infos_[num].end -
              memory_accessor_.segment_infos_[num].start},
      done_size{0};
  auto buf{std::make_unique<char[]>(buffer_size_)};
  uint8_t last_wrapper_exit_code{0};

  if (raw) {
    for (; size >= buffer_size_; size -= buffer_size_) {
      if (ctrl_c_pressed) {
        ctrl_c_pressed = false;
        break;
      }

      last_wrapper_exit_code =
          ReadSegWrapper(buf.get(), num, done_size, buffer_size_);
      if (last_wrapper_exit_code != 0)
        break;

      stream_p->write(buf.get(), buffer_size_);
      done_size += buffer_size_;
    }
    if (size && last_wrapper_exit_code == 0) {
      last_wrapper_exit_code = ReadSegWrapper(buf.get(), num, done_size);
      if (last_wrapper_exit_code == 0)
        stream_p->write(buf.get(), buffer_size_);
    }
  } else {
    for (; size >= buffer_size_; size -= buffer_size_) {
      if (ctrl_c_pressed) {
        ctrl_c_pressed = false;
        break;
      }

      last_wrapper_exit_code =
          ReadSegWrapper(buf.get(), num, done_size, buffer_size_);
      if (last_wrapper_exit_code != 0)
        break;

      hex_viewer_.PrintHex(
          stream_p, buf.get(), buffer_size_,
          memory_accessor_.segment_infos_[num].start + done_size, hex);
      done_size += buffer_size_;
    }
    if (size && last_wrapper_exit_code == 0) {
      last_wrapper_exit_code = ReadSegWrapper(buf.get(), num, done_size);
      if (last_wrapper_exit_code == 0)
        hex_viewer_.PrintHex(
            stream_p, buf.get(), buffer_size_,
            memory_accessor_.segment_infos_[num].start + done_size, hex);
    }
  }

  if (raw && stream_p == &std::cout)
    std::cout << std::endl;
}

/*!
 \brief Handle command "read".
 \param [in] parent Related Command object.
 \param [in] args Arguments for the command.

 Read an amount of bytes provided as the 2nd argument starting from an address
 provided as the 1st argument. Keys available: "-h" - show hex (if no "-r"
 specified), "-r" - print raw data, "-f file" write output to file "file". Print
 usage in case of usage errors.
*/
void Console::CommandRead(const Command &parent,
                          const std::vector<std::string> &args) noexcept {
  bool raw{false}, hex{false};

  std::string file_path, addr_str, amount_str;

  uint32_t par_amount{static_cast<uint32_t>(args.size())};
  for (uint32_t par_num{0}; par_num < par_amount; par_num++) {
    if (args[par_num].empty())
      continue;

    if (args[par_num][0] == '-') {
      if (args[par_num].length() == 1)
        continue;

      for (uint32_t ch_num{1}; ch_num < args[par_num].length(); ch_num++) {
        switch (args[par_num][ch_num]) {
        case 'r':
          raw = true;
          break;
        case 'h':
          hex = true;
          break;
        case 'f':
          if (par_num != par_amount - 1 && file_path.empty()) {
            par_num++;
            file_path = args[par_num];
          } else {
            ShowUsage(parent);
            return;
          }
          break;
        }
      }
    } else {
      if (addr_str.empty()) {
        addr_str = args[par_num];
        continue;
      }
      if (amount_str.empty()) {
        amount_str = args[par_num];
        continue;
      }
    }
  }

  if (addr_str.empty() || amount_str.empty()) {
    ShowUsage(parent);
    return;
  }

  size_t address{0};
  if (ParseAddress(addr_str, address))
    return;

  size_t amount{0};
  if (StoullWrapper(amount_str, amount, "amount") != 0)
    return;

  if (CheckPidWrapper() != 0)
    return;

  std::ostream *stream_p{nullptr};
  std::ofstream file;
  if (!file_path.empty()) {
    file.open(file_path, std::ios::out | std::ios::binary);
    stream_p = &file;
  } else {
    stream_p = &std::cout;
  }

  auto buf{std::make_unique<char[]>(buffer_size_)};
  size_t done_amount{0}, temp_done_amount{0};
  uint8_t last_wrapper_exit_code{0};
  if (raw) {
    for (; amount >= buffer_size_; amount -= buffer_size_) {
      if (ctrl_c_pressed) {
        ctrl_c_pressed = false;
        break;
      }

      last_wrapper_exit_code =
          ReadWrapper(buf.get(), address, buffer_size_, temp_done_amount);
      if (last_wrapper_exit_code != 0)
        break;
      stream_p->write(buf.get(), temp_done_amount);
      done_amount += temp_done_amount;
      address += temp_done_amount;
    }
    if (amount && last_wrapper_exit_code == 0) {
      last_wrapper_exit_code =
          ReadWrapper(buf.get(), address, amount, temp_done_amount);
      if (last_wrapper_exit_code == 0) {
        stream_p->write(buf.get(), temp_done_amount);
        done_amount += temp_done_amount;
        address += temp_done_amount;
      }
    }
  } else {
    for (; amount >= buffer_size_; amount -= buffer_size_) {
      if (ctrl_c_pressed) {
        ctrl_c_pressed = false;
        break;
      }

      last_wrapper_exit_code =
          ReadWrapper(buf.get(), address, buffer_size_, temp_done_amount);
      if (last_wrapper_exit_code != 0)
        break;
      hex_viewer_.PrintHex(stream_p, buf.get(), temp_done_amount, address, hex);
      done_amount += temp_done_amount;
      address += temp_done_amount;
    }
    if (amount && last_wrapper_exit_code == 0) {
      last_wrapper_exit_code =
          ReadWrapper(buf.get(), address, amount, temp_done_amount);
      if (last_wrapper_exit_code == 0) {
        hex_viewer_.PrintHex(stream_p, buf.get(), temp_done_amount, address,
                             hex);
        done_amount += temp_done_amount;
        address += temp_done_amount;
      }
    }
  }

  if (last_wrapper_exit_code == 2) {
    if (raw)
      stream_p->write(buf.get(), temp_done_amount);
    else
      hex_viewer_.PrintHex(stream_p, buf.get(), temp_done_amount, address, hex);
    done_amount += temp_done_amount;
  }

  if (raw && stream_p == &std::cout)
    std::cout << std::endl;

  if (last_wrapper_exit_code != 1)
    std::cout << done_amount << " bytes read." << std::endl;
}

/*!
 \brief Handle command "write".
 \param [in] parent Related Command object.
 \param [in] args Arguments for the command.

 Write to memory an amount of bytes provided as the 2nd argument starting from
 an address provided as the 1st argument. Data is taken either from the string
 provided as the 3rd argument or the file specified in type "-f file". Print
 usage in case of usage errors.
*/
void Console::CommandWrite(const Command &parent,
                           const std::vector<std::string> &args) noexcept {
  std::string file_path, addr_str, amount_str;
  const std::string *str_p{nullptr};
  std::ifstream file;

  uint32_t par_amount{static_cast<uint32_t>(args.size())};
  for (uint32_t par_num{0}; par_num < par_amount; par_num++) {
    if (args[par_num].empty())
      continue;

    if (args[par_num][0] == '-') {
      if (args[par_num].length() == 1)
        continue;

      for (uint32_t ch_num{1}; ch_num < args[par_num].length(); ch_num++) {
        if (args[par_num][ch_num] == 'f') {
          if (par_num != par_amount - 1 && file_path.empty()) {
            par_num++;
            file_path = args[par_num];
          } else {
            ShowUsage(parent);
            return;
          }
        }
      }
    } else {
      if (addr_str.empty()) {
        addr_str = args[par_num];
        continue;
      }
      if (amount_str.empty()) {
        amount_str = args[par_num];
        continue;
      }
      if (!str_p) {
        str_p = &args[par_num];
        continue;
      }
    }
  }

  if (addr_str.empty() || amount_str.empty() || (!str_p && file_path.empty())) {
    ShowUsage(parent);
    return;
  }

  size_t address{0};
  if (ParseAddress(addr_str, address))
    return;

  size_t amount{0};
  if (StoullWrapper(amount_str, amount, "amount") != 0)
    return;

  if (str_p && str_p->length() < amount) {
    std::cout << "String length is less than amount, setting amount to length."
              << std::endl;
    amount = str_p->length();
  }

  if (!file_path.empty()) {
    file.open(file_path, std::ios::in | std::ios::binary);
    if (!file.good()) {
      PrintFileNotOpened(file_path);
      return;
    }
  }

  if (CheckPidWrapper() != 0)
    return;

  auto buf{std::make_unique<char[]>(buffer_size_)};
  size_t done_amount{0}, temp_done_amount{0}, gcount{0};
  uint8_t last_wrapper_exit_code{0};
  if (file.is_open()) {
    for (; amount >= buffer_size_;
         amount -= buffer_size_, address += buffer_size_) {
      file.read(buf.get(), buffer_size_);
      if (file.fail()) {
        PrintFileFail(file_path);
        break;
      }
      if ((gcount = file.gcount()) != buffer_size_) {
        last_wrapper_exit_code =
            WriteWrapper(buf.get(), address, gcount, temp_done_amount);
        if (last_wrapper_exit_code == 0)
          done_amount += temp_done_amount;
        break;
      }
      last_wrapper_exit_code =
          WriteWrapper(buf.get(), address, buffer_size_, temp_done_amount);
      if (last_wrapper_exit_code != 0)
        break;
      done_amount += temp_done_amount;
    }
    if (amount && file.good() && last_wrapper_exit_code == 0) {
      file.read(buf.get(), amount);
      if (file.fail())
        PrintFileFail(file_path);
      else {
        if ((gcount = file.gcount()) != buffer_size_)
          last_wrapper_exit_code =
              WriteWrapper(buf.get(), address, gcount, temp_done_amount);
        else
          last_wrapper_exit_code =
              WriteWrapper(buf.get(), address, amount, temp_done_amount);
        if (last_wrapper_exit_code == 0)
          done_amount += temp_done_amount;
      }
    }
  } else {
    for (; amount >= buffer_size_;
         amount -= buffer_size_, address += buffer_size_) {
      str_p->copy(buf.get(), buffer_size_, done_amount);
      last_wrapper_exit_code =
          WriteWrapper(buf.get(), address, buffer_size_, temp_done_amount);
      if (last_wrapper_exit_code != 0)
        break;

      done_amount += temp_done_amount;
    }
    if (amount && last_wrapper_exit_code == 0) {
      str_p->copy(buf.get(), amount, done_amount);
      last_wrapper_exit_code =
          WriteWrapper(buf.get(), address, amount, temp_done_amount);
      if (last_wrapper_exit_code == 0)
        done_amount += temp_done_amount;
    }
  }

  if (last_wrapper_exit_code != 1)
    std::cout << done_amount << " bytes written." << std::endl;
}

/*!
 \brief Handle command "diff".
 \param [in] parent Related Command object.
 \param [in] args Arguments for the command.

 Find differences in memory states by length provided as the 1st argument and
 replace to string provided as the 2nd argument (optional). Print usage in case
 of usage errors.
*/
void Console::CommandDiff(const Command &parent,
                          const std::vector<std::string> &args) noexcept {
  if (args.size() < 1) {
    ShowUsage(parent);
    return;
  }

  size_t length{0};
  if (StoullWrapper(args[0], length, "length") != 0)
    return;

  std::string replacement;
  if (args.size() > 1)
    replacement = args[1];

  uint8_t last_exit_code{0};
  size_t num{0};     // used in for loops
  size_t i{0}, j{0}; // i - old, j - new
  size_t old_segments_amount{memory_accessor_.segment_infos_.size()};
  std::vector<SegmentInfo> old_segment_infos;
  size_t o_offs{0}, n_offs{0}, amount{0};
  std::unique_ptr<char[]> mem_dump;
  std::vector<std::unique_ptr<char[]>> full_dump;
  std::vector<std::unique_ptr<char[]>>::iterator it;

  seg_not_exist_msg_enabled_ = seg_no_access_msg_enabled_ = false;

  // 1st run before the loop: parsing maps and dumping all segments

  if (ParseMapsWrapper() != 0)
    goto diff_return;

  for (num = 0; num < old_segments_amount; num++) {
    if (ctrl_c_pressed) { // Ctrl-C check
      ctrl_c_pressed = false;
      goto diff_return;
    }

    if (DiffReadSeg(mem_dump, num))
      goto diff_return;
    full_dump.push_back(std::move(mem_dump));
  }

  // Have 2 variables (i, j) - representing numbers of old and new segments to
  // examine. When a segment comes to an end, the corresponding number is
  // incremented. Also, only 3 situations exist: in a taken address can be no
  // segments, only one or both. Only the last case is not skipped. Old segments
  // are dumped in full_dump.

  // loop
  for (;;) {
    if (ctrl_c_pressed) { // Ctrl-C check
      ctrl_c_pressed = false;
      goto diff_return;
    }

    old_segments_amount = memory_accessor_.segment_infos_.size();
    old_segment_infos = memory_accessor_.segment_infos_;

    if (ParseMapsWrapper() != 0)
      break;

    // Now have:
    // Maps - old_segment_infos and new memory_accessor_.segment_infos_
    // Segment amounts - old_segments_amount and new
    // memory_accessor_.segment_infos_.size() Old segments dumped - full_dump

    if (!old_segments_amount || !memory_accessor_.segment_infos_.size())
      continue;

    // It makes no sense checking anything below the first (0th) old segment
    it = full_dump.begin();
    i = j = 0;
    o_offs = n_offs = amount = 0;

    // Dump 0th new segment
    if (DiffReadSeg(mem_dump, j))
      goto diff_return;

    // Will dump new segment as early as possible, but replace old one by it in
    // full_dump only when it is fully proceeded
    for (;;) {
      if (ctrl_c_pressed) { // Ctrl-C check
        ctrl_c_pressed = false;
        goto diff_return;
      }

      // Either one segment is fully below the other or they have a "collision"

      // Old segment is below the new -> erase old, i++, continue
      if (old_segment_infos[i].end <=
          memory_accessor_.segment_infos_[j].start) {
        if (DiffOldNext(i, old_segments_amount, it, full_dump))
          break;
        continue;
      }

      // New segment is below the old -> push new, j++, continue
      if (memory_accessor_.segment_infos_[j].end <=
          old_segment_infos[i].start) {
        last_exit_code = DiffNewNext(j, it, mem_dump, full_dump);
        if (last_exit_code == 0)
          continue;
        else if (last_exit_code == 1)
          break;
        else
          goto diff_return;
      }

      // The beginning of "collision" - max start of 2 segments. In other
      // segment adding offset is needed.
      if (memory_accessor_.segment_infos_[j].start <=
          old_segment_infos[i].start) {
        n_offs = old_segment_infos[i].start -
                 memory_accessor_.segment_infos_[j].start;
      } else {
        o_offs = memory_accessor_.segment_infos_[j].start -
                 old_segment_infos[i].start;
      }

      // The end of "collision" - min end of 2 segments.
      // Therefore, amount = (min end) - (related offset).
      // The segment, in which the end is reached, is replaced by a new one.
      if (memory_accessor_.segment_infos_[j].end <= old_segment_infos[i].end) {
        amount = memory_accessor_.segment_infos_[j].end -
                 memory_accessor_.segment_infos_[j].start - n_offs;
        DiffCompare(it->get(), mem_dump.get(), o_offs, n_offs, amount,
                    old_segment_infos[i].start + o_offs, length, replacement);
        last_exit_code = DiffNewNext(j, it, mem_dump, full_dump);
        if (last_exit_code == 0)
          continue;
        else if (last_exit_code == 1)
          break;
        else
          goto diff_return;
        // If ends are the same, both segments are updated
        if (memory_accessor_.segment_infos_[j].end ==
            old_segment_infos[i].end) {
          if (DiffOldNext(i, old_segments_amount, it, full_dump))
            break;
        }
      } else {
        amount = old_segment_infos[i].end - old_segment_infos[i].start - o_offs;
        DiffCompare(it->get(), mem_dump.get(), o_offs, n_offs, amount,
                    old_segment_infos[i].start + o_offs, length, replacement);
        if (DiffOldNext(i, old_segments_amount, it, full_dump))
          break;
      }
    }

    // Proceeding remaining segments
    while (i < old_segments_amount)
      DiffOldNext(i, old_segments_amount, it, full_dump);
    if (j < memory_accessor_.segment_infos_.size())
      for (;;) {
        last_exit_code = DiffNewNext(j, it, mem_dump, full_dump);
        if (last_exit_code == 1)
          break;
        else if (last_exit_code == 2)
          goto diff_return;
      }
  }

diff_return:
  seg_not_exist_msg_enabled_ = seg_no_access_msg_enabled_ = true;
}

/*!
 \brief Handle command "await".
 \param [in] parent Related Command object.
 \param [in] args Arguments for the command.

 Wait for the process with matching name or PID provided as the 1st argument.
 Print usage in case of usage errors.
*/
void Console::CommandAwait(const Command &parent,
                           const std::vector<std::string> &args) noexcept {
  std::string name, pid_str;

  uint32_t par_amount{static_cast<uint32_t>(args.size())};
  for (uint32_t par_num{0}; par_num < par_amount; par_num++) {
    if (args[par_num].empty())
      continue;

    if (args[par_num][0] == '-') {
      if (args[par_num].length() == 1)
        continue;

      for (uint32_t ch_num{1}; ch_num < args[par_num].length(); ch_num++) {
        if (args[par_num][ch_num] == 'p') {
          if (par_num != par_amount - 1 && pid_str.empty()) {
            par_num++;
            pid_str = args[par_num];
          } else {
            ShowUsage(parent); // no pid specified
            return;
          }
        }
      }
    } else {
      if (name.empty()) {
        name = args[par_num];
        continue;
      }
    }
  }

  if (name.empty() && pid_str.empty()) {
    ShowUsage(parent);
    return;
  }

  bool pid_mode{!pid_str.empty()};

  pid_t pid{0};
  if (pid_mode)
    if (StoiWrapper(pid_str, pid, "PID") != 0)
      return;

  if (pid_mode) {
    std::cout << "Awaiting PID: " << pid_str << std::endl;

    for (;;) {
      if (ctrl_c_pressed) {
        ctrl_c_pressed = false;
        break;
      }

      switch (tools_.PidExists(pid)) {
      case 0:
        std::cout << "PID was found: " << pid_str << std::endl;
        return;
      case 2:
        PrintError0Arg(Error0Arg::kErrCheckingPid);
        return;
      }
    }
  } else {
    std::cout << "Awaiting process: " << name << std::endl;

    for (;;) {
      if (ctrl_c_pressed) {
        ctrl_c_pressed = false;
        break;
      }

      switch (tools_.ProcessExists(name)) {
      case 0:
        std::cout << "Process was found: " << name << std::endl;
        return;
      case 2:
        PrintError0Arg(Error0Arg::kPrintErrCheckingProcess);
        return;
      }
    }
  }
}
