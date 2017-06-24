/// \file rt.h
#ifndef _RT_H_INCLUDED_
#define _RT_H_INCLUDED_

#include <memory>
#include <utility>
#include <vector>
#include <unordered_map>

#include <smux.hpp>

#include "file.h"
#include "error.h"

namespace smux_client
{
    /**
     * \brief                   smux client runtime system
     *
     * An object of this classes manages all channels, monitors their associated file descriptors
     * and copies the data to the right places.
     */
    class runtime_system
    {
        public:
            /**
             * \brief                   ctor
             * \param master_in         file to read smux data from
             * \param master_out        file to write smux data to
             *
             * master_in can point to the same object as master_out
             */
            runtime_system(std::shared_ptr<file> master_in, std::shared_ptr<file> master_out)
                : _master_in(master_in)
                , _master_out(master_out)
            {
            }

            /**
             * \brief                   add a new channel with different input and output files
             * \param ch                the associated smux channel
             * \param in                file for reading
             * \param out               file for writing
             */
            void add_channel(smux_channel ch, std::unique_ptr<file> in, std::unique_ptr<file> out)
            {
                auto half_channel_in = std::make_shared<half_channel>(ch, std::move(in));
                auto half_channel_out = std::make_shared<half_channel>(ch, std::move(out));

                auto channel = _channels[ch];
                channel.in = half_channel_in;
                channel.out = half_channel_out;
            }

            /**
             * \brief                   add a new channel with equal input and ouput file
             * \param ch                the associated channel
             * \param io                file for reading and writing
             */
            void add_channel(smux_channel ch, std::unique_ptr<file> io)
            {
                auto half_channel_io = std::make_shared<half_channel>(ch, std::move(io));

                auto channel = _channels[ch];
                channel.in = half_channel_io;
                channel.out = half_channel_io;
            }

            /**
             * \brief                   main function
             *
             * The function begins to monitor all file descriptors of all channels and copies the
             * data to the right places. It is called after the configuration has been parsed and
             * this object is initialized using add_channel(). Usually, it does not return
             * (except in case of error).
             */
            void run();

        private:
            using buffer = std::vector<char>;
            // one half of a channel (in or out)
            struct half_channel
            {
                smux_channel const ch;
                std::unique_ptr<file> const fl;
                file_descriptor_set read_fds, write_fds, except_fds;

                half_channel(smux_channel ch_, std::unique_ptr<file> fl_)
                    : ch(ch_)
                    , fl(std::move(fl_))
                {}
            };

            // two half channels form a channel (in can be equal to out)
            struct channel
            {
                std::shared_ptr<half_channel> in;
                std::shared_ptr<half_channel> out;
                buffer out_buffer; // characters to be written soon
            };

            // map with definitions of all channels
            using channel_map = std::unordered_map<smux_channel, channel>;
            // lookup map to fastly find a half_channel on a select event
            using fd_map = std::unordered_map<file_descriptor, half_channel*>;


            // master file
            std::shared_ptr<file> _master_in;
            std::shared_ptr<file> _master_out;

            // all channels go here
            channel_map _channels;
    };
} // namespace smux_client

#endif
