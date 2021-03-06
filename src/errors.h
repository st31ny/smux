/// \file errors.h
#ifndef _ERRORS_H_INCLUDED_
#define _ERRORS_H_INCLUDED_

#include <cstring>
#include <stdexcept>
#include <string>

namespace smux_client
{
    /**
     * \brief                   base error class for the smux client application
     */
    class error : public std::runtime_error
    {
        public:
            using std::runtime_error::runtime_error;
    };

    /**
     * \brief                   signals error in the configuration
     */
    class config_error : public error
    {
        public:
            using error::error;
    };

    /**
     * \brief                   signals an error with system resources (e.g., file not found)
     */
    class system_error : public error
    {
        public:
            using error::error;

            /**
             * \brief                   ctor with errno
             * \param errnum_           value of errno
             *
             * The text description will be set to the result of std::strerror(errno).
             */
            explicit system_error(int errnum_)
                : error(std::strerror(errnum_))
                , errnum(errnum_)
            {}

            /**
             * \brief                   ctor with errno and custom message
             * \param errnum_           value of errno
             * \param msg               error message
             */
            system_error(int errnum_, std::string msg)
                : error(std::move(msg))
                , errnum(errnum_)
            {}

            const int errnum = 0;
    };
} // namespace smux_client

#endif
