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
 \brief ArgvParser header

 A header that contains the definition of ArgvParser class.
*/

#ifndef MEMORYACCESSOR_SRC_ARGVPARSER_H_
#define MEMORYACCESSOR_SRC_ARGVPARSER_H_

#include <array>
#include <map>
#include <string>
#include <vector>

#include "console.h"

/*!
 \brief A class to parse arguments from the command line

 This class is used to parse arguments given from the command line. It uses argc
 and argv variables that should be got as the arguments from the function main.
*/
class ArgvParser {
public:
  /*!
   \brief Constructor.
   \param [in,out] console An reference to an instance of Console class.

   Initializes Console class reference by value got as a parameter.
  */
  explicit ArgvParser(Console &console) noexcept : console_(console) {}

  void ParseArgv(const int &argc, char **argv) noexcept;

private:
  using KeyFuncP = void (ArgvParser::*)(
      const int &argc, char **argv); //!< Type of pointer to key function.

  void Usage() const noexcept;
  void PrintErrPrefix() const noexcept;
  void TypeHelp() const noexcept;
  void ArgReq(const std::string &key) const noexcept;
  void ArgUnkn(const std::string &key) const noexcept;
  void FileNotEx(const std::string &path) const noexcept;
  void FileErr(const std::string &path) const noexcept;

  void KeyHelp(const int &argc, char **argv) noexcept;
  void KeyCommand(const int &argc, char **argv) noexcept;
  void KeyFile(const int &argc, char **argv) noexcept;

  Console &console_; //!< A reference to a Console class instance.
  const std::vector<std::array<std::string, 2>> kKeyManuals{
      {"--help", "show help"},
      {"--command COMMAND", "do command"},
      {"--file FILE", "do commands from file"},
  }; //!< Manuals for the available keys, split by the left and right sides for
     //!< better formatting.
  const std::map<std::string, KeyFuncP> kKeys{
      {"--help", &ArgvParser::KeyHelp},
      {"--command", &ArgvParser::KeyCommand},
      {"--file", &ArgvParser::KeyFile},
  }; //!< Available keys matched with pointers to key functions.
};

#endif // MEMORYACCESSOR_SRC_ARGVPARSER_H_
