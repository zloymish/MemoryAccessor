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
 \brief MemoryAccessor source

  A source that contains the realization of MemoryAccessor class.
*/

#include "memoryaccessor.h"

#include <sys/types.h>

#include <cstdint>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

#include "segmentinfo.h"
#include "tools.h"

bool MemoryAccessor::one_instance_created_{false};

/*!
 \brief Constructor.
 \param [in,out] tools A reference to an instance of Tools struct.
 \throw std::logic_error If an instance of the class have already been created
 and it is a second instance.

 Initializes Tools struct reference by value got as an parameter. Throws an
 exception if an instance of the class have already been created. Sets
 one_instance_created_ to true.
*/
MemoryAccessor::MemoryAccessor(Tools &tools) noexcept(false) : tools_(tools) {
  if (one_instance_created_)
    throw std::logic_error(
        "Only one instance of MemoryAccessor can be created");
  one_instance_created_ = true;
}

/*!
 \brief Destructor.

 Sets one_instance_created_ to false.
*/
MemoryAccessor::~MemoryAccessor() noexcept { one_instance_created_ = false; }

/*!
 \brief Get PID.
 \return Current PID assigned to an instance.
 \throw PidNotSetEx If PID is not set.

 Get PID if it is set and throw an exception otherwise.
*/
pid_t MemoryAccessor::GetPid() const noexcept(false) {
  CheckPid();
  return pid_;
}

/*!
 \brief Set PID.
 \param [in] pid PID value to set.
 \throw ErrCheckingPidEx If an error while checking if PID exists occured.
 \throw PidNotExistEx If PID provided is not exist.

 Reset all objects of an instance that are related to PID and set new PID.
*/
void MemoryAccessor::SetPid(const pid_t &pid) noexcept(false) {
  switch (tools_.PidExists(pid)) {
  case 0:
    break;
  case 1:
    throw PidNotExistEx();
  case 2:
  default:
    throw ErrCheckingPidEx();
  }

  Reset();
  pid_ = pid;
  pid_set_ = true;
}

/*!
 \brief Check if PID is set.
 \throw PidNotSetEx If pid_set_ is false.

 Check if pid_set_ is true, and, if it is not, throws an exception.
*/
void MemoryAccessor::CheckPid() const noexcept(false) {
  if (!pid_set_)
    throw PidNotSetEx();
}

/*!
 \brief Parse maps file.
 \throw BadMapsEx If an error in parsing /proc/PID/maps file occured.
 \throw MapsFileEx If an error in opening /proc/PID/maps file occured.
 \throw PidNotSetEx If PID is not set.

 Open and parse /proc/PID/maps file saving data in segment_infos_ and
 special_segment_found_.
*/
void MemoryAccessor::ParseMaps() noexcept(false) {
  CheckPid();

  std::ifstream maps;
  maps.open("/proc/" + std::to_string(pid_) + "/maps", std::ios::in);

  if (!maps.good()) {
    maps.close();
    throw MapsFileEx();
  }

  ResetSegments();

  std::string line;
  SegmentInfo segmentInfo;
  char trash{0}; // skip 1 char
  std::string permissions;

  while (std::getline(maps, line)) {
    std::istringstream iss(line);

    iss >> std::hex >> segmentInfo.start >> trash >> segmentInfo.end >>
        permissions >> segmentInfo.offset >> segmentInfo.major_id >> trash >>
        segmentInfo.minor_id >> std::dec >> segmentInfo.inode_id;

    segmentInfo.mode = tools_.DecodePermissions(permissions);

    if (iss.fail() || iss.bad() || segmentInfo.mode == 255) {
      ResetSegments();
      throw BadMapsEx();
    }

    segmentInfo.mode = tools_.DecodePermissions(permissions);

    do {
      iss >> trash;
    } while (trash == ' ');

    if (trash != ' ' && !iss.eof()) {
      iss.unget();
      std::getline(iss, segmentInfo.path);
    }

    segment_infos_.push_back(segmentInfo);

    if (segmentInfo.path[0] == '[') {
      special_segment_found_[segmentInfo.path] = &segment_infos_.back();
    }
  }
}

/*!
 \brief Get all segment names.
 \return std::unordered_set with all segment names in std::string type.

 Get all segment names that are currently stored.
*/
std::unordered_set<std::string>
MemoryAccessor::GetAllSegmentNames() const noexcept {
  std::unordered_set<std::string> result;

  for (const SegmentInfo &segmentInfo : segment_infos_) {
    if (!segmentInfo.path.empty())
      result.insert(segmentInfo.path);
  }

  return result;
}

/*!
 \brief Find out which segment an address belongs to.
 \param [in] address Address to process.
 \return Number of the segment.
 \throw AddressNotInSegmentEx If the given address does not belong to any
 segment.

 Find out which memory segment an address belongs to and return the number of
 the segment.
*/
size_t MemoryAccessor::AddressInSegment(const size_t &address) const
    noexcept(false) {
  size_t segment_infos_size{segment_infos_.size()};

  for (size_t i{0}; i < segment_infos_size; i++)
    if (segment_infos_[i].end > address)
      return i;

  throw AddressNotInSegmentEx();
}

/*!
 \brief Check if a segment with the given number exists.
 \param [in] num Number of the memory segment starting from 0.
 \throw PidNotSetEx If PID is not set.
 \throw SegmentNotExistEx If the segment does not exist.

 Check if a memory segment with the given number exists. The function also
 checks if PID is set.
*/
void MemoryAccessor::CheckSegNum(const size_t &num) const noexcept(false) {
  CheckPid();

  if (num >= segment_infos_.size())
    throw SegmentNotExistEx();
}

/*!
 \brief Delete all data related to found segments.

 Clear/delete all variables which conatin information about found segments.
*/
void MemoryAccessor::ResetSegments() noexcept {
  segment_infos_.clear();
  special_segment_found_.clear();
}

/*!
 \brief Reset all data related to PID.

 Clear/delete all variables which are related to current PID, including setting
 pid_set_ to false.
*/
void MemoryAccessor::Reset() noexcept {
  pid_set_ = false;
  ResetSegments();
  if (mem_.is_open())
    mem_.close();
}

/*!
 \brief Read full memory segment or a part of it.
 \param [out] dst Destination to which data will be copied.
 \param [in] num Number of the memory segment starting from 0.
 \param [in] start Offset relative to the start of the segment, default is 0.
 \param [in] amount Number of bytes to capture after the "start" parameter,
 default is SIZE_MAX. If a value is too big, it is set to a maximum appropriate
 value. \return Amount of bytes read. \throw AddressNotInSegmentEx If the value
 of parameter "start" represents address on/after the end of the segment. \throw
 MemFileEx If an error in opening /proc/PID/mem file occured. \throw PidNotSetEx
 If PID is not set. \throw SegmentAccessDeniedEx If access to the segment is
 denied by an operating system. \throw SegmentNotExistEx If a segment with a
 number "num" does not exist.

 Read full memory segment or a part of it to a destination "dst" and return how
 many bytes were read.
*/
size_t MemoryAccessor::ReadSegment(char *dst, const size_t &num, size_t start,
                                   size_t amount) noexcept(false) {
  PrepareMemSegment(num, start, amount);
  mem_.read(dst, amount);
  if (!mem_.good())
    throw SegmentAccessDeniedEx();
  return amount;
}

/*!
 \brief Write data to memory segment.
 \param [in] src Source from which data will be copied.
 \param [in] num Number of the memory segment starting from 0.
 \param [in] start Offset relative to the start of the segment, default is 0.
 \param [in] amount Number of bytes to capture after the "start" parameter,
 default is SIZE_MAX. If a value is too big, it is set to a maximum appropriate
 value. \return Amount of bytes written. \throw AddressNotInSegmentEx If the
 value of parameter "start" represents address on/after the end of the segment.
 \throw MemFileEx If an error in opening /proc/PID/mem file occured.
 \throw PidNotSetEx If PID is not set.
 \throw SegmentAccessDeniedEx If access to the segment is denied by an operating
 system. \throw SegmentNotExistEx If a segment with a number "num" does not
 exist.

 Write data to memory segment from a source "src" and return how many bytes were
 written.
*/
size_t MemoryAccessor::WriteSegment(const char *src, const size_t &num,
                                    size_t start,
                                    size_t amount) noexcept(false) {
  PrepareMemSegment(num, start, amount);
  mem_.write(src, amount);
  mem_.seekg(0); // if the access is denied, it doesn't block at first, but
                 // blocks after the next seekg operation.
  if (!mem_.good())
    throw SegmentAccessDeniedEx();
  return amount;
}

/*!
 \brief Read data from /proc/PID/mem.
 \param [out] dst Destination to which data will be copied.
 \param [in] address Address to start from.
 \param [in] amount Number of bytes to read.
 \param [out] done_amount How much data were read.
 \throw AddressNotInSegmentEx If an address reached that does not belong to any
 segment. \throw MemFileEx If an error in opening /proc/PID/mem file occured.
 \throw PidNotSetEx If PID is not set.
 \throw SegmentAccessDeniedEx If a segment is reached, access to which is denied
 by an operating system. \throw SegmentNotExistEx Must not be thrown normally,
 but appears in called methods.

 Read data from /proc/PID/mem to a destination "dst", modifying done_amount by
 how many bytes were read.
*/
void MemoryAccessor::Read(char *dst, size_t address, size_t amount,
                          size_t &done_amount) noexcept(false) {
  CheckPid();

  size_t cur_segment_num{AddressInSegment(address)},
      segment_infos_size{segment_infos_.size()}, ret_size{0};
  done_amount = 0;

  address -= segment_infos_[cur_segment_num].start;

  ret_size = ReadSegment(dst, cur_segment_num, address, amount);
  amount -= ret_size;
  done_amount += ret_size;

  cur_segment_num++;
  for (; amount; cur_segment_num++) {
    //		if (cur_segment_num == segment_infos_size)
    //			throw AddressNotInSegmentEx();
    if (segment_infos_[cur_segment_num - 1].end !=
        segment_infos_[cur_segment_num].start)
      throw AddressNotInSegmentEx();
    ret_size = ReadSegment(dst + done_amount, cur_segment_num, 0, amount);
    amount -= ret_size;
    done_amount += ret_size;
  }
}

/*!
 \brief Write data to /proc/PID/mem.
 \param [in] src Source from which data will be copied.
 \param [in] address Address to start from.
 \param [in] amount Number of bytes to write.
 \param [out] done_amount How much data were written.
 \throw AddressNotInSegmentEx If an address reached that does not belong to any
 segment. \throw MemFileEx If an error in opening /proc/PID/mem file occured.
 \throw PidNotSetEx If PID is not set.
 \throw SegmentAccessDeniedEx If a segment is reached, access to which is denied
 by an operating system. \throw SegmentNotExistEx Must not be thrown normally,
 but appears in called methods.

 Write data to /proc/PID/mem from a source "src", modifying done_amount by how
 many bytes were written.
*/
void MemoryAccessor::Write(const char *src, size_t address, size_t amount,
                           size_t &done_amount) noexcept(false) {
  CheckPid();

  size_t cur_segment_num{AddressInSegment(address)},
      segment_infos_size{segment_infos_.size()}, ret_size{0};
  done_amount = 0;

  address -= segment_infos_[cur_segment_num].start;

  ret_size = WriteSegment(src, cur_segment_num, address, amount);
  amount -= ret_size;
  done_amount += ret_size;

  cur_segment_num++;
  for (; amount; cur_segment_num++) {
    //		if (cur_segment_num == segment_infos_size)
    //			throw AddressNotInSegmentEx();
    if (segment_infos_[cur_segment_num - 1].end !=
        segment_infos_[cur_segment_num].start)
      throw AddressNotInSegmentEx();
    ret_size = WriteSegment(src + done_amount, cur_segment_num, 0, amount);
    amount -= ret_size;
    done_amount += ret_size;
  }
}

/*!
 \brief Open /proc/PID/mem file.
 \throw MemFileEx If an error in opening file occured.
 \throw PidNotSetEx If PID is not set.

 Open or re-open /proc/PID/mem file as std::fstream. After opening, there is a
 check if the std::fstream object is good.
*/
void MemoryAccessor::OpenMem() noexcept(false) {
  CheckPid();

  mem_.close();
  //	mem_.clear();
  mem_.open("/proc/" + std::to_string(pid_) + "/mem",
            std::ios::in | std::ios::out | std::ios::binary);

  if (!mem_.good())
    throw MemFileEx();
}

/*!
 \brief Make sure that /proc/PID/mem is opened.
 \throw MemFileEx If an error in opening file occured.
 \throw PidNotSetEx If PID is not set.

 Check if the std::fstream object representing /proc/PID/mem is open and good,
 and open /proc/PID/mem otherwise.
*/
void MemoryAccessor::CheckMem() noexcept(false) {
  if (!mem_.is_open() || !mem_.good()) {
    OpenMem();
  }
}

/*!
 \brief Check if the given interval is located inside the segment.
 \param [in] num Number of the memory segment starting from 0.
 \param [in] start Offset relative to the start of the segment.
 \param [in,out] amount Number of bytes to capture after the "start" parameter.
 If a value is too big, it is set to a maximum appropriate value. \throw
 AddressNotInSegmentEx If the value of parameter "start" represents address
 on/after the end of the segment. \throw SegmentNotExistEx If segment with
 number "num" does not exist.

 Check if the given boundaries are located inside the memory segment with the
 given number.
*/
void MemoryAccessor::CheckSegBoundaries(const size_t &num, const size_t &start,
                                        size_t &amount) const noexcept(false) {
  CheckSegNum(num);

  size_t seg_size{segment_infos_[num].end - segment_infos_[num].start};

  if (start >= seg_size)
    throw AddressNotInSegmentEx();

  if (amount > seg_size || start + amount > seg_size)
    amount = seg_size - start;
}

/*!
 \brief Make preparations to perform operations with a memory segment.
 \param [in] num Number of the memory segment starting from 0.
 \param [in] start Offset relative to the start of the segment.
 \param [in,out] amount Number of bytes to capture after the "start" parameter.
 If a value is too big, it is set to a maximum appropriate value. \throw
 AddressNotInSegmentEx If the value of parameter "start" represents address
 on/after the end of the segment. \throw MemFileEx If an error in opening
 /proc/PID/mem file occured. \throw PidNotSetEx If PID is not set. \throw
 SegmentNotExistEx If segment with number "num" does not exist.

 Prepare: check the /proc/PID/mem std::fstream, given segment number and
 boundaries, seek to the needed position in /proc/PID/mem std::fstream.
*/
void MemoryAccessor::PrepareMemSegment(const size_t &num, const size_t &start,
                                       size_t &amount) noexcept(false) {
  CheckPid();
  CheckMem();
  CheckSegBoundaries(num, start, amount);
  mem_.seekg(segment_infos_[num].start + start);
}
