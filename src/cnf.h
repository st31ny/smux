/// \file cnf.h
#ifndef _CNF_H_INCLUDED_
#define _CNF_H_INCLUDED_

#include <map>
#include <memory>

#include <smux.hpp> // smux_channel

#include "file_factory.h"

namespace smux_client
{
    /**
     * \brief                   configuration type of a channel
     */
    enum class channel_type
    {
        none, ///< channel does not exist
        symmetric, ///< one file is used for input and output
        separate, ///< one file is used for input and another for output
        read_only, ///< channel is only used for reading
        write_only, ///< channel is only used for writing
    };

    /**
     * \brief                   representation of application configuration
     *
     * Subclasses provide actual parsers for command line, ...
     */
    class cnf
    {
        public:
            /**
             * \brief                   parameters for a file
             */
            struct file_def
            {
                file_type type; ///< file type
                file_mode mode; ///< open mode of the file
                file_args args; ///< arguments for file creation
            };

            /**
             * \brief                   definition of a channel
             */
            struct channel
            {
                channel_type type = channel_type::none; ///< type of this channel

                std::unique_ptr<file_def> io; ///< used if type is symmetric
                std::unique_ptr<file_def> in; ///< used if type is seperate or read_only
                std::unique_ptr<file_def> out; ///< used if type is seperate or write_only
            };

            /**
             * \brief                   map of channel numbers to their definitions
             */
            using channel_map = std::map<smux_channel, channel>;

            /**
             * \brief                   get the master file definition
             * \return                  channel representing the master file
             */
            channel const& master() const
            {
                return _master_file;
            }

            /**
             * \brief                   get the channel map
             * \return                  channel map
             */
            channel_map const& channels() const
            {
                return _channels;
            }

        protected:
            /**
             * \brief                   define the master file symmetrically
             * \param fl                file definition for input/output
             */
            void set_master_file(std::unique_ptr<file_def> fl);

            /**
             * \brief                   define the master input file
             * \param fl                file definition for input
             */
            void set_master_file_in(std::unique_ptr<file_def> fl);

            /**
             * \brief                   define the master output file
             * \param fl                file definition for output
             */
            void set_master_file_out(std::unique_ptr<file_def> fl);

            /**
             * \brief                   reset the definition of the master file
             */
            void reset_master();

            /**
             * \brief                   define a channel symetrically
             * \param ch                channel number
             * \param fl                file definition for input/output
             */
            void set_channel_file(smux_channel ch, std::unique_ptr<file_def> fl);

            /**
             * \brief                   define a channel's input file
             * \param ch                channel number
             * \param fl                file definition for input
             */
            void set_channel_file_in(smux_channel ch, std::unique_ptr<file_def> fl);

            /**
             * \brief                   define a channel's output file
             * \param ch                channel number
             * \param fl                file definition for output
             */
            void set_channel_file_out(smux_channel ch, std::unique_ptr<file_def> fl);

            /**
             * \brief                   reset the configuration of a channel
             * \param ch                channel number
             */
            void reset_channel(smux_channel ch);

        private:
            channel_map _channels;
            channel _master_file;
    };
} // namespace smux_client

#endif // ifndef _CNF_H_INCLUDED_
