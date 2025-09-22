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
 \brief ConsoleCommand header

 A header that contains the definition of ConsoleCommand struct.
*/

#ifndef MEMORYACCESSOR_SRC_CONSOLECOMMAND_H_
#define MEMORYACCESSOR_SRC_CONSOLECOMMAND_H_

#include <array>
#include <string>
#include <vector>

#include "console.h"

class Console;

/*!
 \brief A struct that represents command in Console.

 In this struct all the variables related to Console command are stored: name of
 the command, pointer to Console function that handles the command, and
 formattable description of the command.
*/
struct ConsoleCommand {
  using CommandFuncP = void (Console::*)(
      const ConsoleCommand &parent, const std::vector<std::string>
                                 &); //!< Type of pointer to command function.

  std::string name;           //!< Command name.
  CommandFuncP func{nullptr}; //!< Pointer.
  std::vector<std::array<std::string, 2>>
      description; //!< Description of command: lines split by the left and
                   //!< right sides for better formatting.
};

#endif // MEMORYACCESSOR_SRC_CONSOLECOMMAND_H_