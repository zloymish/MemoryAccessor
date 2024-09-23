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
 \brief ArgvParser source

  A source that contains the realization of ArgvParser class.
*/

#include "argvparser.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "console.h"

/*!
 \brief Parse program arguments.
 \param [in] argc Arguments count given to the program.
 \param [in] argv Array of C strings representing arguments given to the
 program.

 Return if there were 1 argument (program executable), call a key function if a
 key is recognized and handle error otherwise.
*/
void ArgvParser::ParseArgv(const int &argc, char **argv) noexcept {
  if (argc < 2)
    return;
  if (argv[1][0] != '-')
    return;

  std::string key(argv[1]);

  try {
    (this->*kKeys.at(key))(argc, argv);
  } catch (const std::out_of_range &ex) {
    ArgUnkn(key);
  }
}

/*!
 \brief Print Usage of the program.

 Print Usage message: program name, version and description, and description of
 every key properly formatted.
*/
void ArgvParser::Usage() const noexcept {
  console_.PrintNameVer();
  std::cout << console_.kProjectDescription << std::endl;

  std::cout << std::endl;
  std::cout << "Usage: " << console_.kProjectName << " [OPTION]..."
            << std::endl;
  std::cout << std::endl;

  size_t max_left_len{0};
  for (const std::array<std::string, 2> &line : kKeyManuals)
    if (line[0].length() > max_left_len)
      max_left_len = line[0].length();

  for (const std::array<std::string, 2> &line : kKeyManuals)
    std::cout << std::string(2, ' ') << std::setfill(' ')
              << std::setw(max_left_len + 3) << std::left << line[0] << line[1]
              << std::endl;
  std::cout << std::endl;
}

/*!
 \brief Print prefix of the program

 Print program name and semicolon with space ": " to stderr . To be used in
 different error messages.
*/
void ArgvParser::PrintErrPrefix() const noexcept {
  std::cerr << console_.kProjectName << ": ";
}

/*!
 \brief Suggest to use help key.

 Print message with hint to use --help key to stderr.
*/
void ArgvParser::TypeHelp() const noexcept {
  std::cerr << "Use --help to see help about keys." << std::endl;
}

/*!
 \brief Print error message that an argument is required.
 \param [in] key Key with which an error occured.

 Print error message that an argument is required to stderr with program prefix
 and exit with code 1.
*/
void ArgvParser::ArgReq(const std::string &key) const noexcept {
  PrintErrPrefix();
  std::cerr << key << " requires an argument" << std::endl;
  TypeHelp();
  std::exit(1);
}

/*!
 \brief Print error message that key is unknown.
 \param [in] key Key with which an error occured.

 Print error message that key is unknown to stderr with program prefix and exit
 with code 1.
*/
void ArgvParser::ArgUnkn(const std::string &key) const noexcept {
  PrintErrPrefix();
  std::cerr << "unknown key " << key << std::endl;
  TypeHelp();
  std::exit(1);
}

/*!
 \brief Print error message that file does not exist.
 \param [in] path Given path to file.

 Print error message that file does not exist to stderr with program prefix and
 exit with code 1.
*/
void ArgvParser::FileNotEx(const std::string &path) const noexcept {
  PrintErrPrefix();
  std::cerr << path << ": file not exist" << std::endl;
  std::exit(1);
}

/*!
 \brief Print message about error in opening file.
 \param [in] path Given path to file.

 Print message about error in opening file to stderr with program prefix and
 exit with code 1.
*/
void ArgvParser::FileErr(const std::string &path) const noexcept {
  PrintErrPrefix();
  std::cerr << path << ": error opening file" << std::endl;
  std::exit(1);
}

/*!
 \brief Handle key "--help".
 \param [in] argc Arguments count given to the program.
 \param [in] argv Array of C strings representing arguments given to the
 program.

 Print help (call Usage()) and exit with code 0.
*/
void ArgvParser::KeyHelp(const int &argc, char **argv) noexcept {
  Usage();

  std::exit(0);
}

/*!
 \brief Handle key "--command".
 \param [in] argc Arguments count given to the program.
 \param [in] argv Array of C strings representing arguments given to the
 program.

 Run command of Console (error if command not provided) and exit with code 0.
*/
void ArgvParser::KeyCommand(const int &argc, char **argv) noexcept {
  if (argc < 3)
    ArgReq("--command");

  console_.HandleCommand(std::string(argv[2]));

  std::exit(0);
}

/*!
 \brief Handle key "--file".
 \param [in] argc Arguments count given to the program.
 \param [in] argv Array of C strings representing arguments given to the
 program.

 Open file (or handle errors), read and execute commands of Console line by line
 and exit with code 0.
*/
void ArgvParser::KeyFile(const int &argc, char **argv) noexcept {
  if (argc < 3)
    ArgReq("--file");

  std::string path(argv[2]);

  if (!std::filesystem::exists(path))
    FileNotEx(path);

  std::ifstream file;
  file.open(path, std::ios::in);
  if (!file.good())
    FileErr(path);

  std::string line;
  while (std::getline(file, line))
    console_.HandleCommand(line);

  std::exit(0);
}
