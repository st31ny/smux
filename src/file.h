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
     * \brief                   read/write mode for files
     */
    enum class file_mode : int
    {
        in  = 4, ///< open file for reading
        out = 2, ///< open file for writing
        io  = 6, ///< open file for reading and writing
    };

    /**
     * \brief                   base file interface
     * \param FD                file descriptor type
     * \param FD_SET            type for sets of file descriptors
     */
    template<class FD, class FD_SET = basic_file_descriptor_set<FD>>
    class basic_file
    {
        public:
            using fd_type = FD;
            using fd_set_type = FD_SET;

            /**
             * \brief                   ctor
             * \param mode              file mode
             */
            basic_file(file_mode mode)
                : _mode(mode)
            {}

            /**
             * \brief                   read from the file
             * \param[out] buf          buffer to write the read data to
             * \param count             size of the buffer
             * \param fd                file descriptor that signaled a read event
             * \return                  actual number of read bytes (0 in case of EOF)
             * \throw                   system_error
             *
             * Note: count can be 0. See select_lists() for details.
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
             *
             * Note: The file descriptors in write_fds are only hooked up if there is data to be written.
             * However, file descriptors in read_fds are always monitored, but read() might be called with
             * count == 0 if a file is used for output only.
             */
            virtual void select_fds(fd_set_type& read_fds, fd_set_type& write_fds, fd_set_type& except_fds) = 0;

            /**
             * \brief                   get the file mode
             * \return                  file mode
             */
            file_mode mode() const
            {
                return _mode;
            }

            /**
             * \brief                   dtor
             */
            virtual ~basic_file()
            {
            }

        protected:
            file_mode _mode;
    };

    using file = basic_file<file_descriptor, file_descriptor_set>;


} // namespace smux_client

#endif // ifndef _FILE_H_INCLUDED_
