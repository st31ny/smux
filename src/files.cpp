// files.cpp
#include "file.h"
#include "file_factory.h"

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

namespace smux_client
{
    static void assert_config(bool f, char const* msg)
    {
        if(!f)
            throw config_error(msg);
    }

    // simple file for reading/writing
    class simple_file : public file
    {
        public:
            simple_file(file_type const&, file_mode m, file_args const& args)
                : file(m)
            {
                int flags = 0;

                // parse arguments
                assert_config(args.size() < 1, "one argument required");
                assert_config(args.size() > 2, "only two arguments supported");
                switch(m)
                {
                    case file_mode::io:
                        flags = O_RDWR | O_CREAT; break;
                    case file_mode::in:
                        flags = O_RDONLY; break;
                    case file_mode::out:
                        flags = O_WRONLY | O_CREAT; break;
                    default:
                        throw config_error("invalid file mode");
                }
                if(args.size() > 1)
                {
                    assert_config(args[1] == "a" || args[1] == "t", "optional flag value unsupported");
                    if(args[1] == "a")
                        flags |= O_APPEND;
                    else
                    if(args[1] == "t")
                        flags |= O_TRUNC;
                }

                // open file
                _fd = open(args[0].c_str(), flags, 0666);
                if(_fd == -1)
                    throw system_error(errno);
            }

            virtual std::size_t read(void* buf, std::size_t count, fd_type const&) override
            {
                auto ret = ::read(_fd, buf, count);
                if(ret < 0)
                    throw system_error(errno);
                return static_cast<std::size_t>(ret);
            }

            virtual std::size_t write(void* buf, std::size_t count, fd_type const&) override
            {
                auto ret = ::write(_fd, buf, count);
                if(ret < 0)
                    throw system_error(errno);
                return static_cast<std::size_t>(ret);
            }

            virtual void exception(fd_type const&) override
            {
                // TODO
            }

            virtual void select_fds(fd_set_type& read_fds, fd_set_type& write_fds, fd_set_type& except_fds) override
            {
                read_fds.insert(_fd);
                write_fds.insert(_fd);
                (void)except_fds;
            }

            virtual ~simple_file()
            {
                close(_fd);
                // ignore errors (do not throw in dtors)
            }
        protected:
            int _fd;
    };
    static register_file_type<simple_file> simple_file_registrar("file");



} // smux_client
