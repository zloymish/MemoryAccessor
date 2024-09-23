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
 \brief HexViewer header

 A header that contains the definition of HexViewer class.
*/

#ifndef MEMORYACCESSOR_SRC_HEXVIEWER_H_
#define MEMORYACCESSOR_SRC_HEXVIEWER_H_

#include <ostream>

/*!
 \brief A class for printing data in a readable format.

 This class provides an opportunity to print in readable format any array of
 char (bytes) to any ostream, optionally showing the hexadecimal values of
 bytes.
*/
class HexViewer {
public:
  void PrintHex(std::ostream *stream_p, const char *s, size_t size, size_t addr,
                bool show_hex = false) const noexcept;

private:
  uint32_t base_width_{
      8}; //!< 1/4 of minimum width of the output in characters. It is supposed
          //!< that other 3/4 of width are used by the hex values.
};

#endif // MEMORYACCESSOR_SRC_HEXVIEWER_H_
