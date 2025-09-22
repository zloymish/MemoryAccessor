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

#include "processapi.h"
#include "segmentinfo.h"

bool MemoryAccessor::one_instance_created_{false};

/*!
 \brief Constructor.
 \param [in,out] process_api A pointer to an instance of ProcessApi class.
 \throw std::logic_error If an instance of the class have already been created
 and it is a second instance.

 Initializes ProcessApi class pointer by value got as an parameter. Throws an
 exception if an instance of the class have already been created. Sets
 one_instance_created_ to true.
*/
MemoryAccessor::MemoryAccessor(ProcessApi *process_api) noexcept(false) : process_api_(process_api) {
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
 \param [out] pid Current PID assigned to an instance if it is set.
 \return MemoryAccessor::ErrorCode, kPidNotSetErr if pid_set_ is false, kNoError otherwise.

 Get PID if it is set and return the correspondong MemoryAccessor::ErrorCode.
*/
MemoryAccessor::ErrorCode MemoryAccessor::GetPid(pid_t& pid) const noexcept {
  ErrorCode err{ErrorCode::kNoError};
  
  err = CheckPid();
  if (err == ErrorCode::kNoError)
    pid = pid_;
  return err;
}

/*!
 \brief Set PID.
 \param [in] pid PID value to set.
 \return MemoryAccessor::ErrorCode, kErrCheckingPid if an error while checking if PID exists occured, kPidNotExistErr if PID provided is not exist, kNoError otherwise.

 Reset all objects of an instance that are related to PID and set a new PID.
*/
MemoryAccessor::ErrorCode MemoryAccessor::SetPid(const pid_t &pid) noexcept {
  switch (process_api_->PidExists(pid)) {
  case 0:
    break;
  case 1:
    return ErrorCode::kPidNotExistErr;
    break;
  case 2:
  default:
    return ErrorCode::kErrCheckingPid;
    break;
  }

  Reset();
  pid_ = pid;
  pid_set_ = true;
  
  return ErrorCode::kNoError;
}

/*!
 \brief Check if PID is set.
 \return MemoryAccessor::ErrorCode, kPidNotSetErr if pid_set_ is false, kNoError otherwise.

 Check if pid_set_ is true, and, if it is not, returns the correspondong MemoryAccessor::ErrorCode.
*/
MemoryAccessor::ErrorCode MemoryAccessor::CheckPid() const noexcept {
  if (!pid_set_)
    return ErrorCode::kPidNotSetErr;
  return ErrorCode::kNoError;
}

/*!
 \brief Parse maps file.
 \return MemoryAccessor::ErrorCode, kBadMapsErr if an error in parsing /proc/PID/maps file occured, kMapsFileErr if an error in opening /proc/PID/maps file occured, kPidNotSetErr if PID is not set, kNoError otherwise.

 Open and parse /proc/PID/maps file saving data in segment_infos_ and
 special_segment_found_.
*/
MemoryAccessor::ErrorCode MemoryAccessor::ParseMaps() noexcept {
  ErrorCode err{ErrorCode::kNoError};
  
  err = CheckPid();
  if (err != ErrorCode::kNoError)
    return err;

  std::ifstream maps;
  maps.open("/proc/" + std::to_string(pid_) + "/maps", std::ios::in);

  if (!maps.good()) {
    maps.close();
    return ErrorCode::kMapsFileErr;
  }

  ResetSegments();

  std::string line;
  SegmentInfo segmentInfo;
  char trash{0}; // skip 1 char
  std::string permissions;

  while (std::getline(maps, line)) {
    std::istringstream iss(line);

    iss >> std::hex >> segmentInfo.start_ >> trash >> segmentInfo.end_ >>
        permissions >> segmentInfo.offset_ >> segmentInfo.major_id_ >> trash >>
        segmentInfo.minor_id_ >> std::dec >> segmentInfo.inode_id_;

    segmentInfo.DecodePermissions(permissions);

    if (iss.fail() || iss.bad() || segmentInfo.mode_ == 255) {
      ResetSegments();
      return ErrorCode::kBadMapsErr;
    }

    segmentInfo.DecodePermissions(permissions);

    do {
      iss >> trash;
    } while (trash == ' ');

    if (trash != ' ' && !iss.eof()) {
      iss.unget();
      std::getline(iss, segmentInfo.path_);
    }

    segment_infos_.push_back(segmentInfo);

    if (segmentInfo.path_[0] == '[') {
      special_segment_found_[segmentInfo.path_] = &segment_infos_.back();
    }
  }
  
  return err;
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
    if (!segmentInfo.path_.empty())
      result.insert(segmentInfo.path_);
  }

  return result;
}

/*!
 \brief Find out which segment an address belongs to.
 \param [in] address Address to process.
 \param [out] num Number of the segment.
 \return MemoryAccessor::ErrorCode, kAddressNotInSegmentErr if the given address does not belong to any
 segment, kNoError otherwise.

 Find out which memory segment an address belongs to and return the number of
 the segment.
*/
MemoryAccessor::ErrorCode MemoryAccessor::AddressInSegment(const size_t &address, size_t &num) const noexcept {
  size_t segment_infos_size{segment_infos_.size()};

  for (size_t i{0}; i < segment_infos_size; i++)
    if (segment_infos_[i].end_ > address) {
      num = i;
      return ErrorCode::kNoError;
    }

  return ErrorCode::kAddressNotInSegmentErr;
}

/*!
 \brief Check if a segment with the given number exists.
 \param [in] num Number of the memory segment starting from 0.
 \return MemoryAccessor::ErrorCode, kPidNotSetErr if PID is not set, kSegmentNotExistErr if the segment does not exist, kNoError otherwise.

 Check if a memory segment with the given number exists. This function also
 checks if PID is set.
*/
MemoryAccessor::ErrorCode MemoryAccessor::CheckSegNum(const size_t &num) const noexcept {
  ErrorCode err{ErrorCode::kNoError};
  
  err = CheckPid();
  if (err != ErrorCode::kNoError)
    return err;
  
  if (num >= segment_infos_.size())
    err = ErrorCode::kSegmentNotExistErr;
  
  return err;
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
 \param [out] dst Destination to which data will be copied. Needs to be a valid array of the specified amount.
 \param [in] num Number of the memory segment starting from 0.
 \param [out] done_amount How much data were read.
 \param [in] start Offset relative to the start of the segment, default is 0.
 \param [in] amount Number of bytes to capture after the "start" parameter,
 default is SIZE_MAX. If a value is too big, it is set to a maximum appropriate
 value. 
 \return MemoryAccessor::ErrorCode, kPidNotSetErr if PID is not set, kSegmentNotExistErr if segment with number "num" does not exist, kAddressNotInSegmentErr if the value of parameter "start" represents address on/after the end of the segment, kSegmentAccessDeniedErr if access to the segment is denied by an operating system, kMemFileErr If an error in opening /proc/PID/mem file occured, kNoError otherwise.

 Read full memory segment or a part of it to a destination "dst" if it is possible.
*/
MemoryAccessor::ErrorCode MemoryAccessor::ReadSegment(char *dst, const size_t &num, size_t &done_amount, size_t start, size_t amount) noexcept {
  ErrorCode err{ErrorCode::kNoError};
  
  err = PrepareMemSegment(num, start, amount);
  if (err != ErrorCode::kNoError)
    return err;
  
  mem_.read(dst, amount);
  if (!mem_.good())
    return ErrorCode::kSegmentAccessDeniedErr;
  
  done_amount += amount;
  
  return err;
}

/*!
 \brief Write data to memory segment.
 \param [in] src Source from which data will be copied. Needs to be a valid array of the specified amount.
 \param [in] num Number of the memory segment starting from 0.
 \param [out] done_amount How much data were read.
 \param [in] start Offset relative to the start of the segment, default is 0.
 \param [in] amount Number of bytes to capture after the "start" parameter,
 default is SIZE_MAX. If a value is too big, it is set to a maximum appropriate
 value. 
 \return MemoryAccessor::ErrorCode, kPidNotSetErr if PID is not set, kSegmentNotExistErr if segment with number "num" does not exist, kAddressNotInSegmentErr if the value of parameter "start" represents address on/after the end of the segment, kSegmentAccessDeniedErr if access to the segment is
 denied by an operating system, kMemFileErr If an error in opening
 /proc/PID/mem file occured, kNoError otherwise.

 Write data to memory segment from a source "src" if it is possible.
*/
MemoryAccessor::ErrorCode MemoryAccessor::WriteSegment(const char *src, const size_t &num, size_t &done_amount, size_t start, size_t amount) noexcept {
  ErrorCode err{ErrorCode::kNoError};
  
  err = PrepareMemSegment(num, start, amount);
  if (err != ErrorCode::kNoError)
    return err;
  
  mem_.write(src, amount);
  mem_.seekg(0); // if the access is denied, it doesn't block at first, but
                 // blocks after the next seekg operation.
  
  if (!mem_.good())
    return ErrorCode::kSegmentAccessDeniedErr;
  
  done_amount += amount;
  
  return err;
}

/*!
 \brief Read data from /proc/PID/mem.
 \param [out] dst Destination to which data will be copied. Needs to be a valid array of the specified amount.
 \param [in] address Address to start from.
 \param [in] amount Number of bytes to read.
 \param [out] done_amount How much data were read.
 \return MemoryAccessor::ErrorCode, kPidNotSetErr if PID is not set, kAddressNotInSegmentErr if an address reached that does not belong to any segment, kSegmentAccessDeniedErr if access to the segment is denied by an operating system, kMemFileErr If an error in opening /proc/PID/mem file occured, kNoError otherwise.
 
 Read data from /proc/PID/mem to a destination "dst", modifying done_amount by
 how many bytes were read.
*/
MemoryAccessor::ErrorCode MemoryAccessor::Read(char *dst, size_t address, size_t amount,
                          size_t &done_amount) noexcept {
  ErrorCode err{ErrorCode::kNoError};
  
  err = CheckPid();
  if (err != ErrorCode::kNoError)
    return err;

  size_t cur_segment_num{0}, segment_infos_size{segment_infos_.size()}, ret_size{0};
  err = AddressInSegment(address, cur_segment_num);
  if (err != ErrorCode::kNoError)
    return err;
  
  done_amount = 0;

  address -= segment_infos_[cur_segment_num].start_;

  err = ReadSegment(dst, cur_segment_num, ret_size, address, amount);
  if (err != ErrorCode::kNoError)
    return err;
  
  amount -= ret_size;
  done_amount += ret_size;
  
  cur_segment_num++;
  for (; amount; cur_segment_num++) {
    ret_size = 0;
    
    if (segment_infos_[cur_segment_num - 1].end_ !=
        segment_infos_[cur_segment_num].start_)
      return ErrorCode::kAddressNotInSegmentErr;
    
    err = ReadSegment(dst + done_amount, cur_segment_num, ret_size, 0, amount);
    if (err != ErrorCode::kNoError)
      return err;
    
    amount -= ret_size;
    done_amount += ret_size;
  }
  
  return err;
}

/*!
 \brief Write data to /proc/PID/mem.
 \param [in] src Source from which data will be copied. Needs to be a valid array of the specified amount.
 \param [in] address Address to start from.
 \param [in] amount Number of bytes to write.
 \param [out] done_amount How much data were written.
 \return MemoryAccessor::ErrorCode, kPidNotSetErr if PID is not set, kAddressNotInSegmentErr if an address reached that does not belong to any segment, kSegmentAccessDeniedErr if access to the segment is denied by an operating system, kMemFileErr If an error in opening /proc/PID/mem file occured, kNoError otherwise.
 
 \throw AddressNotInSegmentEx If an address reached that does not belong to any
 segment. \throw MemFileEx If an error in opening /proc/PID/mem file occured.
 \throw PidNotSetEx If PID is not set.
 \throw SegmentAccessDeniedEx If a segment is reached, access to which is denied
 by an operating system. \throw SegmentNotExistEx Must not be thrown normally,
 but appears in called methods.

 Write data to /proc/PID/mem from a source "src", modifying done_amount by how
 many bytes were written.
*/
MemoryAccessor::ErrorCode MemoryAccessor::Write(const char *src, size_t address, size_t amount,
                           size_t &done_amount) noexcept {
  ErrorCode err{ErrorCode::kNoError};
  
  err = CheckPid();
  if (err != ErrorCode::kNoError)
    return err;

  size_t cur_segment_num{0}, segment_infos_size{segment_infos_.size()}, ret_size{0};
  err = AddressInSegment(address, cur_segment_num);
  if (err != ErrorCode::kNoError)
    return err;
  
  done_amount = 0;

  address -= segment_infos_[cur_segment_num].start_;
  
  err = WriteSegment(src, cur_segment_num, ret_size, address, amount);
  if (err != ErrorCode::kNoError)
    return err;
  
  amount -= ret_size;
  done_amount += ret_size;

  cur_segment_num++;
  for (; amount; cur_segment_num++) {
    ret_size = 0;
    
    if (segment_infos_[cur_segment_num - 1].end_ !=
        segment_infos_[cur_segment_num].start_)
      return ErrorCode::kAddressNotInSegmentErr;
    
    err = WriteSegment(src + done_amount, cur_segment_num, ret_size, 0, amount);
    if (err != ErrorCode::kNoError)
      return err;
    
    amount -= ret_size;
    done_amount += ret_size;
  }
  
  return err;
}

/*!
 \brief Open /proc/PID/mem file.
 \return MemoryAccessor::ErrorCode, kPidNotSetErr if PID is not set, kMemFileErr if an error in opening file occured, kNoError otherwise.

 Open or re-open /proc/PID/mem file as std::fstream. After opening, there is a
 check if the std::fstream object is good.
*/
MemoryAccessor::ErrorCode MemoryAccessor::OpenMem() noexcept {
  ErrorCode err{ErrorCode::kNoError};
  
  err = CheckPid();
  if (err != ErrorCode::kNoError)
    return err;
  
  mem_.close();
  //	mem_.clear();
  mem_.open("/proc/" + std::to_string(pid_) + "/mem",
            std::ios::in | std::ios::out | std::ios::binary);

  if (!mem_.good())
    return ErrorCode::kMemFileErr;
  
  return err;
}

/*!
 \brief Make sure that /proc/PID/mem is opened.
 \return MemoryAccessor::ErrorCode, kPidNotSetErr if PID is not set, kMemFileErr if an error in opening file occured, kNoError otherwise.

 Check if the std::fstream object representing /proc/PID/mem is open() and good(),
 and open /proc/PID/mem otherwise.
*/
MemoryAccessor::ErrorCode MemoryAccessor::CheckMem() noexcept {
  if (!mem_.is_open() || !mem_.good()) {
    return OpenMem();
  }
  
  return ErrorCode::kNoError;
}

/*!
 \brief Check if the given interval is located inside the segment.
 \param [in] num Number of the memory segment starting from 0.
 \param [in] start Offset relative to the start of the segment.
 \param [in,out] amount Number of bytes to capture after the "start" parameter.
 If a value is too big, it is set to a maximum appropriate value. 
 \return MemoryAccessor::ErrorCode, kPidNotSetErr if PID is not set, kSegmentNotExistErr if the segment with number "num" does not exist, kAddressNotInSegmentErr if the value of parameter "start" represents address on/after the end of the segment, kNoError otherwise.

 Check if the given boundaries are located inside the memory segment with the
 given number.
*/
MemoryAccessor::ErrorCode MemoryAccessor::CheckSegBoundaries(const size_t &num, const size_t &start,
                                        size_t &amount) const noexcept {
  ErrorCode err{ErrorCode::kNoError};
  
  err = CheckSegNum(num);
  if (err != ErrorCode::kNoError)
    return err;
  
  size_t seg_size{segment_infos_[num].end_ - segment_infos_[num].start_};

  if (start >= seg_size)
    return ErrorCode::kAddressNotInSegmentErr;
  
  if (amount > seg_size || start + amount > seg_size)
    amount = seg_size - start;
  
  return err;
}

/*!
 \brief Make preparations to perform operations with a memory segment.
 \param [in] num Number of the memory segment starting from 0.
 \param [in] start Offset relative to the start of the segment.
 \param [in,out] amount Number of bytes to capture after the "start" parameter.
 If a value is too big, it is set to a maximum appropriate value. 
 \return MemoryAccessor::ErrorCode, kPidNotSetErr if PID is not set, kSegmentNotExistErr if segment with number "num" does not exist, kAddressNotInSegmentErr if the value of parameter "start" represents address on/after the end of the segment, kMemFileErr If an error in opening
 /proc/PID/mem file occured, kNoError otherwise.

 Prepare: check the /proc/PID/mem std::fstream, given segment number and
 boundaries, seek to the needed position in /proc/PID/mem std::fstream.
*/
MemoryAccessor::ErrorCode MemoryAccessor::PrepareMemSegment(const size_t &num, const size_t &start,
                                       size_t &amount) noexcept {
  ErrorCode err{ErrorCode::kNoError};
  
  err = CheckPid();
  if (err != ErrorCode::kNoError)
    return err;
  
  err = CheckMem();
  if (err != ErrorCode::kNoError)
    return err;
  
  err = CheckSegBoundaries(num, start, amount);
  if (err != ErrorCode::kNoError)
    return err;
  
  mem_.seekg(segment_infos_[num].start_ + start);
  
  return err;
}
