// rt.cpp
#include <errno.h>
#include <iostream>

#include "rt.h"

using namespace smux_client;

void runtime_system::run()
{
    buffer buf;

    // master files
    half_channel* master_in = _master.in.get();
    half_channel* master_out = _master.out.get();
    if(master_in)
    {
        _smux.set_read_fn([master_in](void* buf, size_t count) { return master_in->fl->read(buf, count); });
    } else
    {
        // no master_in
        _smux.set_read_fn([](void*, size_t) { return 0; });
        std::clog << "Warning: no master read file: cannot receive data" << std::endl;
    }
    if(master_out)
    {
        _smux.set_write_fn([master_out](const void* buf, size_t count) { return master_out->fl->write(buf, count); });
    } else
    {
        // no master_out
        _smux.set_write_fn([](const void*, size_t count) { return count; });
        std::clog << "Warning: no master write file: cannot transmit data" << std::endl;
    }

    // initially fill _fm and _fs
    for(auto& channel : _channels)
    {
        _update_fds(channel.second);
    }
    // hook up the master
    _update_fds(_master);

    // main loop
    std::clog << "entering main loop" << std::endl;
    while(true)
    {
        fd_sets _fs_tmp = _fs;
        int nfds = std::max({_fs.read.fd_max, _fs.write.fd_max, _fs.except.fd_max}) + 1;
        std::clog << "calling select()..." << std::endl;
        int select_result = select(nfds, &_fs_tmp.read.fs, &_fs_tmp.write.fs, &_fs_tmp.except.fs, nullptr);
        std::clog << "called select()=" << select_result << std::endl;
        if(select_result < 0)
            throw system_error(errno);

        for(file_descriptor fd = 0; fd < nfds; ++fd)
        {
            // find our half channel associated with fd
            auto _fm_it = _fm.find(fd);
            if(_fm_it == _fm.end())
                continue;
            auto& hc = *_fm_it->second;

            // read event
            if(_fs_tmp.read.is_set(fd) && hc.fl->read_event(fd))
            {
                if(&hc == master_in)
                {
                    std::clog << "master read event" << std::endl;

                    std::size_t ret = 0;
                    smux_channel ch;
                    try
                    {
                        if(_smux.read() < 0)
                            throw system_error("reading into smux buffer failed");

                        std::clog << "_smux.read() done" << std::endl;

                        buf.resize(RECEIVE_BUFFER_SIZE);
                        ret = _smux.recv(&ch, buf.data(), buf.size());
                    } catch(file::eof&)
                    {
                        std::clog << "eof on master file" << std::endl;
                        // avoid further reading
                        master_in->fds.mask_read = true;
                    }

                    // forward data to the correct output
                    if(ret > 0)
                    {
                        if(_channels.count(ch))
                        {
                            auto& hc_out = _channels[ch].out;
                            if(hc_out)
                            {
                                buf.resize(ret); // remember correct size
                                hc_out->out_buffer = std::move(buf);
                                hc_out->fds.mask_write = false;
                                _update_fds(*hc_out);
                                std::clog << "received data for channel " << static_cast<int>(ch) << std::endl;
                            }
                        } else // channel not existing
                        {
                            std::clog << "igonring data for channel " << static_cast<int>(ch) << std::endl;
                        }
                    }
                } else
                {
                    // a channel is ready to be read
                    std::clog << "read event on channel " << static_cast<int>(hc.ch) << ", fd=" << fd << std::endl;
                    buf.resize(RECEIVE_BUFFER_SIZE);
                    try
                    {
                        std::size_t ret = hc.fl->read(buf.data(), buf.size());
                        if(ret > 0)
                        {
                            // forward data to smux
                            smux::ostream out(_smux, hc.ch);
                            out.write(buf.data(), ret);
                            out.flush();
                        }
                    } catch(file::eof&)
                    {
                        std::clog << "eof on channel " << static_cast<int>(hc.ch) << std::endl;
                        // avoid further reading
                        hc.fds.mask_read = true;
                    }

                }

                // give file a chance to update its fd sets
                _update_fds(hc);
            }

            // write event
            if(_fs_tmp.write.is_set(fd) && hc.fl->write_event(fd))
            {
                if(&hc == master_out)
                {
                    std::clog << "master write event" << std::endl;
                    // master is written directly as it is the bottleneck anyway
                } else
                {
                    // a channel is ready to be written
                    std::clog << "write event on channel " << static_cast<int>(hc.ch) << ", fd=" << fd << std::endl;
                    if(hc.out_buffer.size() > 0)
                    {
                        std::size_t ret = hc.fl->write(hc.out_buffer.data(), hc.out_buffer.size());
                        auto begin = hc.out_buffer.begin();
                        hc.out_buffer.erase(begin, begin + ret);
                    }
                    // buffer empty now? => mask fds
                    if(hc.out_buffer.size() == 0)
                    {
                        hc.fds.mask_write = true;
                    }
                }
            }

            // exception
            if(_fs_tmp.except.is_set(fd))
            {
                std::clog << "except event on " << fd << std::endl;

                // call exception handler
                hc.fl->exception_event(fd);
            }

            // give the file a chance to update its fd sets
            _update_fds(hc);
        }
    }
}

void runtime_system::_update_fds(half_channel& hc)
{
    file_descriptor_set read_fds, write_fds, except_fds;

    hc.fl->select_fds(read_fds, write_fds, except_fds);


    // remove all old file descriptors
    for(auto const& fds : {hc.fds.read, hc.fds.write, hc.fds.except})
    {
        for(auto const& fd : fds)
        {
            _fm.erase(fd);

            // we assume that a fd uniquely belongs to a single file
            _fs.read.clear(fd);
            _fs.write.clear(fd);
            _fs.except.clear(fd);
        }
    }

    // add the new definitions
    for(auto const& fd : read_fds)
    {
        _fm[fd] = &hc;
        if(!hc.fds.mask_read)
            _fs.read.set(fd);
    }
    for(auto const& fd : write_fds)
    {
        _fm[fd] = &hc;
        if(!hc.fds.mask_write)
            _fs.write.set(fd);
    }
    for(auto const& fd : except_fds)
    {
        _fm[fd] = &hc;
        if(!hc.fds.mask_except)
            _fs.except.set(fd);
    }


    // finally, remember the new sets
    hc.fds.read = std::move(read_fds);
    hc.fds.write = std::move(write_fds);
    hc.fds.except = std::move(except_fds);
}
