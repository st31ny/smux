/// \file file.h
#ifndef _FILE_H_INCLUDED_
#define _FILE_H_INCLUDED_

#include <cstddef>
#include <unordered_set>
#include <utility>

#include "errors.h"

namespace smux_client
{
    /**
     * \brief                   type for file descriptors
     */
    using file_descriptor = int;

    /**
     * \brief                   type for sets of file descriptors
     */
    template<class FD>
    using basic_file_descriptor_set = std::unordered_set<FD>;
    using file_descriptor_set = basic_file_descriptor_set<file_descriptor>;

    /**
     * \brief                   base file interface
     * \param FD                file descriptor type
     * \param FD_SET            type for sets of file descriptors
     *
     * Main purpose is abstraction of the select() and read()/write() interface. When select
     * signals any hooked up event, the appropriate handler is called, followed by, if required,
     * read() or write().
     */
    template<class FD, class FD_SET = basic_file_descriptor_set<FD>>
    class basic_file
    {
        public:
            /// class to signal an eof condition
            class eof {};

            using fd_type = FD;
            using fd_set_type = FD_SET;

            /**
             * \brief                   handle read event
             * \param fd                file descriptor that signaled a read event
             * \return                  true if actual data can be read
             * \throw                   system_error
             */
            virtual bool read_event(fd_type const& fd) = 0;

            /**
             * \brief                   handle write event
             * \param fd                file descriptor that signaled a write event
             * \return                  true if actual data can be written
             * \throw                   system_error
             */
            virtual bool write_event(fd_type const& fd) = 0;

            /**
             * \brief                   handle exception
             * \param fd                file descriptor that signaled an exception event
             * \throw                   system_error
             *
             * This function is called when a file descriptor signaled an exception.
             */
            virtual void exception_event(fd_type const& fd) = 0;

            /**
             * \brief                   register file descriptors associated with this file
             * \param[out] read_fds     file descriptors to monitor for reading
             * \param[out] write_fds    file descriptors to monitor for writing
             * \param[out] except_fds   file descriptors to monitor for exceptions
             *
             * When one of the events occurs, the appropriate handler is called. Then, this
             * function is called again in order to re-build the (potentially different) list
             * of file descriptors.
             *
             * Attention: If two files share a file descriptor, only one handler is called.
             */
            virtual void select_fds(fd_set_type& read_fds, fd_set_type& write_fds, fd_set_type& except_fds) = 0;

            /**
             * \brief                   read from the file
             * \param[out] buf          buffer to write the read data to
             * \param count             size of the buffer
             * \return                  actual number of read bytes
             * \throw                   system_error
             * \throw                   eof
             */
            virtual std::size_t read(void* buf, std::size_t count) = 0;

            /**
             * \brief                   write to the file
             * \param buf               buffer to write
             * \param count             size of buf
             * \return                  actual number of written bytes
             * \throw                   system_error
             */
            virtual std::size_t write(const void* buf, std::size_t count) = 0;


            /**
             * \brief                   dtor
             */
            virtual ~basic_file()
            {
            }
    };

    using file = basic_file<file_descriptor, file_descriptor_set>;


} // namespace smux_client

#endif // ifndef _FILE_H_INCLUDED_
