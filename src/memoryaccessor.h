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
#include <exception>
#include <fstream>
#include <map>
#include <string>
#include <unordered_set>
#include <vector>

#include "segmentinfo.h"
#include "tools.h"

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
   \brief Ex: Base exception

   All other exceptions are inherited from this.
  */
  class BaseException : public std::exception {
    /*!
     \brief "what" function of the exception.
     \return C-string descripting the exception.

     Prints message to stdout when the exception is thrown.
    */
    virtual const char *what() const noexcept override {
      return "Base exception";
    }
  };

  /*!
   \brief Ex: Exception related to PID

   All exceptions related to PID are inherited from this.
  */
  class PidEx : public BaseException {
    /*!
     \brief "what" function of the exception.
     \return C-string descripting the exception.

     Prints message to stdout when the exception is thrown.
    */
    virtual const char *what() const noexcept override {
      return "PID exception";
    }
  };

  /*!
   \brief Ex: Could not check if PID exists

   This exception is thrown when checking of PID has failed (e.g., because of
   permissions).
  */
  class ErrCheckingPidEx : public PidEx {
    /*!
     \brief "what" function of the exception.
     \return C-string descripting the exception.

     Prints message to stdout when the exception is thrown.
    */
    virtual const char *what() const noexcept override {
      return "An error occurred while checking if PID exists";
    }
  };

  /*!
   \brief Ex: PID does not exist

   This exception is thrown when it is discovered that the preferrable PID does
   not exist.
  */
  class PidNotExistEx : public PidEx {
    /*!
     \brief "what" function of the exception.
     \return C-string descripting the exception.

     Prints message to stdout when the exception is thrown.
    */
    virtual const char *what() const noexcept override {
      return "PID not exist";
    }
  };

  /*!
   \brief Ex: PID is not set

   This exception is thrown when PID is not set and there is an attempt to
   perform some operation that requires PID use.
  */
  class PidNotSetEx : public PidEx {
    /*!
     \brief "what" function of the exception.
     \return C-string descripting the exception.

     Prints message to stdout when the exception is thrown.
    */
    virtual const char *what() const noexcept override { return "PID not set"; }
  };

  /*!
   \brief Ex: Exception related to files

   All exceptions related to operations with files are inherited from this.
  */
  class FileEx : public BaseException {
    /*!
     \brief "what" function of the exception.
     \return C-string descripting the exception.

     Prints message to stdout when the exception is thrown.
    */
    virtual const char *what() const noexcept override {
      return "Error in opening file";
    }
  };

  /*!
   \brief Ex: Operation with /proc/PID/mem failed

   This exception is thrown when there is an error in opening or reopening
   /proc/PID/mem.
  */
  class MemFileEx : public FileEx {
    /*!
     \brief "what" function of the exception.
     \return C-string descripting the exception.

     Prints message to stdout when the exception is thrown.
    */
    virtual const char *what() const noexcept override {
      return "Error in opening current /proc/PID/mem";
    }
  };

  /*!
   \brief Ex: Opening /proc/PID/maps failed

   This exception is thrown when there is an error in opening /proc/PID/maps.
  */
  class MapsFileEx : public FileEx {
    /*!
     \brief "what" function of the exception.
     \return C-string descripting the exception.

     Prints message to stdout when the exception is thrown.
    */
    virtual const char *what() const noexcept override {
      return "Error in opening current /proc/PID/maps";
    }
  };

  /*!
   \brief Ex: Parsing /proc/PID/maps failed

   This exception is thrown when there is an error in parsing /proc/PID/maps.
  */
  class BadMapsEx : public BaseException {
    /*!
     \brief "what" function of the exception.
     \return C-string descripting the exception.

     Prints message to stdout when the exception is thrown.
    */
    virtual const char *what() const noexcept override {
      return "Error in parsing /proc/PID/maps";
    }
  };

  /*!
   \brief Ex: Exception related to memory segments

   All exceptions related to operations with memory segments are inherited from
   this.
  */
  class SegmentEx : public BaseException {
    /*!
     \brief "what" function of the exception.
     \return C-string descripting the exception.

     Prints message to stdout when the exception is thrown.
    */
    virtual const char *what() const noexcept override {
      return "Segment exception";
    }
  };

  /*!
   \brief Ex: Segment of memory does not exist

   This exception is thrown when a memory segment is requested which does not
   exist (e.g., a number of segment requested that is too big).
  */
  class SegmentNotExistEx : public SegmentEx {
    /*!
     \brief "what" function of the exception.
     \return C-string descripting the exception.

     Prints message to stdout when the exception is thrown.
    */
    virtual const char *what() const noexcept override {
      return "The segment of memory does not exist";
    }
  };

  /*!
   \brief Ex: Access to the segment of memory denied

   This exception is thrown when the OS denied the access to a desirable
   segment.
  */
  class SegmentAccessDeniedEx : public SegmentEx {
    /*!
     \brief "what" function of the exception.
     \return C-string descripting the exception.

     Prints message to stdout when the exception is thrown.
    */
    virtual const char *what() const noexcept override {
      return "Access to the segment of memory denied";
    }
  };

  /*!
   \brief Ex: Address does not belong to any segment

   This exception is thrown when there is an attempt to use an address that is
   not located in any segment of memory.
  */
  class AddressNotInSegmentEx : public SegmentEx {
    /*!
     \brief "what" function of the exception.
     \return C-string descripting the exception.

     Prints message to stdout when the exception is thrown.
    */
    virtual const char *what() const noexcept override {
      return "Address does not belong to any segment";
    }
  };

  explicit MemoryAccessor(Tools &tools) noexcept(false);

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

  pid_t GetPid() const noexcept(false);
  void SetPid(const pid_t &pid) noexcept(false);
  void CheckPid() const noexcept(false);
  void ParseMaps() noexcept(false);
  std::unordered_set<std::string> GetAllSegmentNames() const noexcept;
  size_t AddressInSegment(const size_t &address) const noexcept(false);
  void CheckSegNum(const size_t &num) const noexcept(false);
  void ResetSegments() noexcept;
  void Reset() noexcept;
  size_t ReadSegment(char *dst, const size_t &num, size_t start = 0,
                     size_t amount = SIZE_MAX) noexcept(false);
  size_t WriteSegment(const char *src, const size_t &num, size_t start = 0,
                      size_t amount = SIZE_MAX) noexcept(false);
  void Read(char *dst, size_t address, size_t amount,
            size_t &done_amount) noexcept(false);
  void Write(const char *src, size_t address, size_t amount,
             size_t &done_amount) noexcept(false);

  Tools &tools_; //!< A reference to a Tools class instance

  std::map<std::string, SegmentInfo *>
      special_segment_found_; //!< "Special" segment infos found (segments,
                              //!< which name starts with '['). Contains pairs
                              //!< that consist of a segment name and a pointer
                              //!< to SegmentInfo.
  std::vector<SegmentInfo>
      segment_infos_; //!< SegmentInfo objects got as a result of parsing
                      //!< /proc/PID/maps.
private:
  void OpenMem() noexcept(false);
  void CheckMem() noexcept(false);
  void CheckSegBoundaries(const size_t &num, const size_t &start,
                          size_t &amount) const noexcept(false);
  void PrepareMemSegment(const size_t &num, const size_t &start,
                         size_t &amount) noexcept(false);

  static bool one_instance_created_; //!< A static variable that is true when
                                     //!< one instance of class exists.

  std::fstream mem_; //!< File stream that represents /proc/PID/mem.

  pid_t pid_{0}; //!< Current PID in use. Value doesn't matter if pid_set is
                 //!< false. It is not meant to write to this variable directly,
                 //!< set_pid function is dedicated for this purpose.
  bool pid_set_{
      false}; //!< This variable shows if PID was set and is ready to be used.
};

#endif // MEMORYACCESSOR_SRC_MEMORYACCESSOR_H_
