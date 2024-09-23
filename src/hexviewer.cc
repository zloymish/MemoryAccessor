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
 \brief HexViewer source

  A source that contains the realization of HexViewer class.
*/

#include "hexviewer.h"

#include <sys/ioctl.h>

#include <algorithm>
#include <bit> // std::bit_width
#include <cctype>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <ostream>

/*!
 \brief Print data in readable format.
 \param [in,out] stream_p std::ostream to which output should be printed.
 \param [in] s Array of char with data to be processed.
 \param [in] size Size of the array of char.
 \param [in] addr Start address to print to output.
 \param [in] show_hex To show hex values of bytes or not.

 Print data line be line replacing non-printable characters by '.'. If show_hex
 is true, hex values are shown. Also, address provided in parameter "address" is
 printed in hex with offset in each line. The width of each line is base_width_
 multiplied by 2 in power as much as screen width can contain when show_hex is
 true, otherwise width is additionally multiplied by 4.
*/
void HexViewer::PrintHex(std::ostream *stream_p, const char *s, size_t size,
                         size_t addr, bool show_hex) const noexcept {
  if (!size)
    return;

  struct winsize ws;
  ioctl(0, TIOCGWINSZ, &ws);

  uint32_t width{show_hex ? base_width_ : base_width_ * 4};

  if (ws.ws_col - 14 / (base_width_ * 4))
    width <<= std::bit_width((ws.ws_col - 14) / (base_width_ * 4)) - 1;

  auto s1 =
      std::make_unique<char[]>(width + 1); // line to print, last char is \0
  std::fill(s1.get(), s1.get() + width + 1, '\0');
  *stream_p << std::hex;

  uint32_t may_be_cut_width{width};
  bool size_smaller_than_width{false};
  for (; size; size -= width) {
    // address
    *stream_p << std::nouppercase << addr << "  "; // separator

    size_smaller_than_width = size < width;

    if (size_smaller_than_width) {
      std::fill(s1.get(), s1.get() + width, '\0');
      may_be_cut_width = size;
    }
    std::memcpy(s1.get(), s, may_be_cut_width);

    // HEX + replace
    for (uint32_t c_num{0}; c_num < may_be_cut_width; c_num++) {
      if (show_hex)
        *stream_p << std::uppercase << std::setfill('0') << std::setw(2)
                  << std::right
                  << static_cast<uint32_t>(static_cast<uint8_t>(s1[c_num]))
                  << ' ';

      if (!std::isprint(s1[c_num]))
        s1[c_num] = '.';
    }

    if (show_hex) {
      *stream_p << ' ';
      if (size_smaller_than_width) {
        *stream_p << std::string((width - size) * 3, ' ');
      }
    }

    // line
    *stream_p << s1.get() << std::endl;

    if (size_smaller_than_width)
      break;

    addr += width;
    s += width;
  }

  *stream_p << std::nouppercase << std::dec;
}
