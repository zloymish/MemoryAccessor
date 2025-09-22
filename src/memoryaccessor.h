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
 \brief MemoryAccessor header

 A header that contains the definitions of MemoryAccessor class itself and its
 exceptions.
*/

#ifndef MEMORYACCESSOR_SRC_MEMORYACCESSOR_H_
#define MEMORYACCESSOR_SRC_MEMORYACCESSOR_H_

#include <sys/types.h>

#include <cstdint>
#include <fstream>
#include <map>
#include <string>
#include <unordered_set>
#include <vector>

#include "processapi.h"
#include "segmentinfo.h"

/*!
 \brief A class to perform the main operations with memory

 This class is dedicated to make all the work of reading and writing from/to
 /proc/PID/mem. Firstly, PID needs to be set. Then the instance parses
 /proc/PID/maps to get information about memory segments. If everything is
 correct, on the found segments r/w operations can be performed. Moving, copying
 and creating more than 1 instance of this class is prohibited.
*/
class MemoryAccessor {
public:
  /*!
   \brief Error code enumeration used in methods.

   Enumeration that describes possible error codes that methods of MemoryAccessor can return.
  */
  enum class ErrorCode {
    kNoError = 0, //!< Finished successfully
    kErrCheckingPid, //!< An error occurred while checking if PID exists
    kPidNotExistErr, //!< PID does not exist
    kPidNotSetErr, //!< PID is not set
    kMemFileErr, //!< Error in opening current /proc/PID/mem
    kMapsFileErr, //!< Error in opening current /proc/PID/maps
    kBadMapsErr, //!< Error in parsing /proc/PID/maps
    kSegmentNotExistErr, //!< The segment of memory does not exist
    kSegmentAccessDeniedErr, //!< Access to the segment of memory is denied
    kAddressNotInSegmentErr, //!< Address does not belong to any segment
  };
  
  explicit MemoryAccessor(ProcessApi *process_api) noexcept(false);

  /*!
   \brief Copy constructor (deleted).
   \param [in] origin MemoryAccessor instance to copy from.

   Create a new object by copying an old one. Prohibited.
  */
  MemoryAccessor(const MemoryAccessor &origin) = delete;

  /*!
   \brief Move constructor (deleted).
   \param [in] origin Moved MemoryAccessor object.

   Create a new object by moving an old one. Prohibited.
  */
  MemoryAccessor(MemoryAccessor &&origin) = delete;

  /*!
   \brief Copy-assignment operator (deleted).
   \param [in] origin MemoryAccessor instance to copy from.

   Assign an object by copying other object. Prohibited.
  */
  MemoryAccessor &operator=(const MemoryAccessor &origin) = delete;

  /*!
   \brief Move-assignment operator (deleted).
   \param [in] origin Moved MemoryAccessor object.

   Assign an object by moving other object. Prohibited.
  */
  MemoryAccessor &operator=(MemoryAccessor &&origin) = delete;

  ~MemoryAccessor() noexcept;

  ErrorCode GetPid(pid_t& pid) const noexcept;
  ErrorCode SetPid(const pid_t &pid) noexcept;
  ErrorCode CheckPid() const noexcept;
  ErrorCode ParseMaps() noexcept;
  std::unordered_set<std::string> GetAllSegmentNames() const noexcept;
  ErrorCode AddressInSegment(const size_t &address, size_t &num) const noexcept;
  ErrorCode CheckSegNum(const size_t &num) const noexcept;
  void ResetSegments() noexcept;
  void Reset() noexcept;
  ErrorCode ReadSegment(char *dst, const size_t &num, size_t &done_amount, size_t start = 0,
                     size_t amount = SIZE_MAX) noexcept;
  ErrorCode WriteSegment(const char *src, const size_t &num, size_t &done_amount, size_t start = 0,
                      size_t amount = SIZE_MAX) noexcept;
  ErrorCode Read(char *dst, size_t address, size_t amount,
            size_t &done_amount) noexcept;
  ErrorCode Write(const char *src, size_t address, size_t amount,
             size_t &done_amount) noexcept;

  ProcessApi *process_api_{nullptr}; //!< A pointer to a ProcessApi class instance

  std::map<std::string, SegmentInfo *>
      special_segment_found_; //!< "Special" segment infos found (segments,
                              //!< which name starts with '['). Contains pairs
                              //!< that consist of a segment name and a pointer
                              //!< to SegmentInfo.
  std::vector<SegmentInfo>
      segment_infos_; //!< SegmentInfo objects got as a result of parsing
                      //!< /proc/PID/maps.
private:
  ErrorCode OpenMem() noexcept;
  ErrorCode CheckMem() noexcept;
  ErrorCode CheckSegBoundaries(const size_t &num, const size_t &start,
                          size_t &amount) const noexcept;
  ErrorCode PrepareMemSegment(const size_t &num, const size_t &start,
                         size_t &amount) noexcept;

  static bool one_instance_created_; //!< A static variable that is true when
                                     //!< one instance of class exists.

  std::fstream mem_; //!< File stream that represents /proc/PID/mem.
  
  // Be careful! Call mem_.close() before the associated process dies, otherwise it will hang the program.

  pid_t pid_{0}; //!< Current PID in use. Value doesn't matter if pid_set is
                 //!< false. It is not meant to write to this variable directly,
                 //!< set_pid function is dedicated for this purpose.
  bool pid_set_{
      false}; //!< This variable shows if PID was set and is ready to be used.
};

#endif // MEMORYACCESSOR_SRC_MEMORYACCESSOR_H_
