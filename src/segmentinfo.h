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
 \brief SegmentInfo header

 A header that contains the definition of SegmentInfo struct.
*/

#ifndef MEMORYACCESSOR_SRC_SEGMENTINFO_H_
#define MEMORYACCESSOR_SRC_SEGMENTINFO_H_

#include <cstdint>
#include <string>

/*!
 \brief A struct to store the information of a memory segment

 This struct stores the information of one memory segment in a way it is given
 in /proc/PID/maps file. It uses its own style of storing the permissions
 though.
*/
struct SegmentInfo {
  size_t start; //!< Start address
  size_t
      end; //!< End address (first address that does not belong to the segment)
  size_t offset; //!< Offset from the start of the file if it is a file mapping
                 //!< (e.g., executable from ROM)

  uint8_t mode{0}; //!< Permissions, are stored as 0000rwx(p/s), where p is 0, s
                   //!< is 1 (private - shared)

  uint32_t major_id; //!< Major ID
  uint32_t minor_id; //!< Minor ID

  size_t inode_id; //!< Inode ID if it is a file mapping (e.g., executable from
                   //!< ROM)

  std::string path; //!< Path or name
};

#endif // MEMORYACCESSOR_SRC_SEGMENTINFO_H_
