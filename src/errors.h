/// \file errors.h
#ifndef _ERRORS_H_INCLUDED_
#define _ERRORS_H_INCLUDED_

#include <stdexcept>

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
    };
} // namespace smux_client

#endif
