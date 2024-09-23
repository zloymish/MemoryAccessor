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
 \brief Main file

 Main file of the project.
*/

#include "main.h"

#include <string>

#include "argvparser.h"
#include "console.h"
#include "hexviewer.h"
#include "memoryaccessor.h"
#include "tools.h"

/*!
 \brief Main function.
 \param [in] argc Arguments count.
 \param [in] argv Array of C strings representing arguments.
 \return Exit code of the program.

 Creates instances of classes, sets equal buffer_size, handles arguments, starts
 the console and makes it read stdin infinitely.
*/
int main(int argc, char **argv) {
  constexpr size_t kBufferSize{0x1000};

  Tools tools;
  tools.SetBufferSize(kBufferSize);
  MemoryAccessor memory_accessor(tools);
  HexViewer hex_viewer;
  Console console(memory_accessor, hex_viewer, tools);
  console.SetBufferSize(kBufferSize);

  ArgvParser argv_parser(console);
  argv_parser.ParseArgv(argc, argv);

  console.Start();

  for (;;)
    console.ReadStdin();

  return 0;
}
