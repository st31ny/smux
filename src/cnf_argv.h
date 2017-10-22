/// \file cnf_argv.h
#ifndef _CNF_ARGV_H_
#define _CNF_ARGV_H_

#include <string>

#include "errors.h"
#include "cnf.h"

namespace smux_client
{
    /**
     * \brief                   parser of argc/argv command line options
     */
    class cnf_argv : public cnf
    {
        public:
            /**
             * \brief                   parse a set of argc/argv
             * \param argc
             * \param argv
             * \throw config_error
             */
            void parse(int argc, char *const argv[]);

            /**
             * \brief                   get the program's name as passed in argv
             * \return                  passed name
             */
            std::string const& pgrm_name() const
            {
                return _pgrm_name;
            }

            /**
             * \brief                   get the requested help level
             * \return                  number of -h specified on command line
             */
            unsigned help_level() const
            {
                return _help_level;
            }

            /**
             * \brief                   get the requested debug level
             * \return                  number of -d specified on command line
             */
            unsigned debug_level() const
            {
                return _debug_level;
            }

        protected:
            std::string _pgrm_name;
            unsigned _help_level = 0;
            unsigned _debug_level = 0;
    };

} // namespace smux_client

#endif // ifndef _CNF_ARGV_H_
