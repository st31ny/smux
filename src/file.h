/// \file file.h
#ifndef _FILE_H_INCLUDED_
#define _FILE_H_INCLUDED_

#include <cstddef>
#include <vector>
#include <utility>

#include "errors.h"

namespace smux_client
{
    /**
     * \brief                   class for file descriptors
     * \param T                 raw (platform specific) file descriptor type
     */
    template<class T>
    class basic_file_descriptor
    {
        public:
            using raw_fd_type = T;

            /**
             * \brief                   ctor
             * \param fd                raw file descriptor
             */
            explicit basic_file_descriptor(raw_fd_type fd)
                : _fd(std::move(fd))
            {
            }

            /**
             * \brief                   get raw file descriptor
             * \return                  raw file descriptor
             */
            raw_fd_type const& raw() const
            {
                return _fd;
            }

        private:
            raw_fd_type _fd;
    };

    template<class FD>
    using basic_file_descriptor_list = std::vector<FD>;

    /**
     * \brief                   base class for files
     * \param FD                file descriptor type
     *
     * Objects of derived classes manage resources for reading from and writing to files. A single
     */
    template<class FD, class FD_LIST = basic_file_descriptor_list<FD>>
    class basic_file
    {
        public:
            using fd_type = FD;
            using fd_list_type = FD_LIST;

            /**
             * \brief                   read from the file
             * \param[out] buf          buffer to write the read data to
             * \param count             size of the buffer
             * \param fd                file descriptor that signaled a read event
             * \return                  actual number of read bytes
             * \throw                   system_error
             */
            virtual std::size_t read(void* buf, std::size_t count, fd_type const& fd) = 0;

            /**
             * \brief                   write to the file
             * \param buf               buffer to write
             * \param count             size of buf
             * \param fd                file descriptor that signaled a write event
             * \return                  actual number of written bytes
             * \throw                   system_error
             */
            virtual std::size_t write(void* buf, std::size_t count, fd_type const& fd) = 0;

            /**
             * \brief                   handle exception
             * \param fd                file descriptor that signaled an exception event
             * \return                  actual number of written bytes
             * \throw                   system_error
             *
             * This function is called when a file descriptor signaled an exception.
             */
            virtual void exception(fd_type const& fd) = 0;

            /**
             * \brief                   register file descriptors associated with this file
             * \param[out] read_fds     file descriptors to monitor for reading
             * \param[out] write_fds    file descriptors to monitor for writing
             * \param[out] except_fds   file descriptors to monitor for exceptions
             *
             * When one of the events occurs, the appropriate handler is called. Then, this
             * function is called again in order to re-build the (potentially different) list
             * of file descriptors.
             */
            virtual void select_lists(fd_list_type& read_fds, fd_list_type& write_fds, fd_list_type& except_fds) = 0;

            /**
             * \brief                   check if errors have occurred
             * \return                  true if file is in failed state
             *
             * When a file is in a failed state, no attempt to read from or write to it should be made.
             */
            virtual bool failed() const noexcept = 0;

            /**
             * \brief                   check if file has reached eof
             * \return                  true if file has reached its end of file
             */
            virtual bool eof() const noexcept = 0;

            /**
             * \brief                   check if file has neither failed nor is at its eof
             * \return                  true if file is good for further operations
             */
            bool good() const noexcept
            {
                return !(failed() || eof());
            }

            /**
             * \brief                   dtor
             */
            virtual ~basic_file()
            {
            }
    };

    using file_descriptor = basic_file_descriptor<int>;
    using file_descriptor_list = basic_file_descriptor_list<file_descriptor>;
    using file = basic_file<file_descriptor, file_descriptor_list>;

} // namespace smux_client

#endif // ifndef _FILE_H_INCLUDED_
