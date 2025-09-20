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
 \brief SegmentInfo source

  A source that contains the realization of SegmentInfo class.
*/

#include "segmentinfo.h"

#include <cstdint>
#include <string>

/*!
 \brief Get permissions stored to mode_ field from std::string.
 \param [in] permissions Permissions stored as std::string, for example, "rwxp".
 Some additional characters after are not prohibited. 
 \return Exit code, 0 is success, 1 is failure.

 Store permissions of a memory segment from std::string to mode_ field. In case of 
 an error permissions are not stored and 1 is returned.
*/
uint8_t
SegmentInfo::DecodePermissions(const std::string &permissions) noexcept {
  if (kModesLength > permissions.length())
    return 1;

  uint8_t mode{0}, i{0};

  for (; i < kModesLength - 1; i++) {
    if (permissions[i] == kModes[i])
      mode |= 1 << (kModesLength - 1 - i);
    else if (permissions[i] != '-')
      return 1;
  }

  if (permissions[i] == kModes[i])
    mode |= 1 << (kModesLength - 1 - i);
  else if (permissions[i] != 'p')
    return 1;

  mode_ = mode;
  return 0;
}

/*!
 \brief Get permissions stored as std::string processed from the mode_ field.
 \return Permissions stored as std::string, for example, "rwxp".

 Process permissions of a memory segment from the mode_ field to std::string.
*/
std::string SegmentInfo::EncodePermissions() const noexcept {
  std::string permissions;
  for (uint8_t i{0}; i < kModesLength - 1; i++) {
    if (mode_ & (1 << (kModesLength - 1 - i)))
      permissions.push_back(kModes[i]);
    else
      permissions.push_back('-');
  }
  if (mode_ & 1)
    permissions.push_back('s');
  else
    permissions.push_back('p');
  return permissions;
}