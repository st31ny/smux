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

    // base class for files with one file descriptor

    class simple_file : public file
    {
        public:
            simple_file(file_mode m)
                : file(m)
                , _fd(-1)
            {
            }

            virtual std::size_t read(void* buf, std::size_t count) override
            {
                auto ret = ::read(_fd, buf, count);
                if(ret < 0)
                    throw system_error(errno);
                if(ret == 0) // eof
                    throw eof();
                return static_cast<std::size_t>(ret);
            }

            virtual std::size_t write(const void* buf, std::size_t count) override
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
                if(_fd >= 0)
                    close(_fd);
                // ignore errors (do not throw in dtors)
            }
        protected:
            int _fd;
    };

    // simple file for reading/writing
    class regular_file : public simple_file
    {
        public:
            regular_file(file_type const&, file_mode m, file_args const& args)
                : simple_file(m)
            {
                int flags = 0;

                // parse arguments
                assert_config(args.size() >= 1, "one argument required");
                assert_config(args.size() <= 2, "only two arguments supported");
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
    };

    // stdin
    class stdin_file : public simple_file
    {
        public:
            stdin_file(file_type const&, file_mode m, file_args const& args)
                : simple_file(m)
            {
                if(m != file_mode::in)
                    throw config_error("you can only read from stdin");
                assert_config(args.size() == 0, "no arguments supported");
                // duplicate the fd as it is closed in the dtor
                _fd = dup(STDIN_FILENO);
                if(_fd == -1)
                    throw system_error(errno);
            }
    };

    // stdout
    class stdout_file : public simple_file
    {
        public:
            stdout_file(file_type const&, file_mode m, file_args const& args)
                : simple_file(m)
            {
                if(m != file_mode::out)
                    throw config_error("you can only write to stdout");
                assert_config(args.size() == 0, "no arguments supported");
                // duplicate the fd here, too
                _fd = dup(STDOUT_FILENO);
                if(_fd == -1)
                    throw system_error(errno);
            }
    };

    // register file types
    static register_file_type<regular_file> regular_file_registrar("file");
    static register_file_type<stdin_file> stdin_file_registrar("stdin");
    static register_file_type<stdout_file> stdout_file_registrar("stdout");


} // smux_client
