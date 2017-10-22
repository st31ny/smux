// cnf_argv.cpp
#include <utility> // std::move
#include <unistd.h> // getopt

#include "cnf_argv.h"

using namespace smux_client;

// flA = read part or read/write, flB = write part
static channel_type parse_channel_spec(std::string const& spec, smux_channel& ch, file_def& flA, file_def& flB);
static channel_type parse_file_specs(std::string const& spec, file_def& flA, file_def& flB);
static bool parse_file_spec(std::string const& spec, file_def& fl);

void cnf_argv::parse(int argc, char *const argv[])
{
    // first argument is program name
    if(argc > 0)
        _pgrm_name = argv[0];

    // loop over the rest
    int optres;
    while((optres = getopt(argc, argv, ":dhm:c:")) != -1)
    {
        switch(optres)
        {
            case 'h':
                _help_level += 1;
                break;
            case 'd':
                _debug_level += 1;
                break;
            case 'm':
                {
                    // parse file definition
                    std::unique_ptr<file_def> flA(new file_def), flB(new file_def);
                    auto ct = parse_file_specs(optarg, *flA, *flB);
                    switch(ct)
                    {
                        case channel_type::symmetric:
                            set_master_file(std::move(flA)); break;
                        case channel_type::read_only:
                            set_master_file_in(std::move(flA)); break;
                        case channel_type::write_only:
                            set_master_file_out(std::move(flB)); break;
                        case channel_type::separate:
                            set_master_file_in(std::move(flA));
                            set_master_file_out(std::move(flB));
                            break;
                        default:
                            throw config_error(std::string("unable to parse master file specification: ") + optarg);
                    }
                }
                break;
            case 'c':
                {
                    // parse channel spec
                    std::unique_ptr<file_def> flA(new file_def), flB(new file_def);
                    smux_channel ch;
                    auto ct = parse_channel_spec(optarg, ch, *flA, *flB);
                    switch(ct)
                    {
                        case channel_type::symmetric:
                            set_channel_file(ch, std::move(flA)); break;
                        case channel_type::read_only:
                            set_channel_file_in(ch, std::move(flA)); break;
                        case channel_type::write_only:
                            set_channel_file_out(ch, std::move(flB)); break;
                        case channel_type::separate:
                            set_channel_file_in(ch, std::move(flA));
                            set_channel_file_out(ch, std::move(flB));
                            break;
                        default:
                            throw config_error(std::string("unable to parse channel specification: ") + optarg);
                    }
                }
                break;
            case ':':
                throw config_error(std::string("missing argument for -") + (char)optopt);
            case '?':
            default:
                throw config_error(std::string("unknown option +") + (char)optopt);
        }
    }
}

static channel_type parse_channel_spec(std::string const& spec, smux_channel& ch, file_def& flA, file_def& flB)
{
    std::string const delim("=");
    auto delim_ch = spec.find(delim);
    if(delim_ch == std::string::npos || delim_ch == spec.length())
        return channel_type::none;
    // extract first characters and convert them to a number
    int ch_num;
    try
    {
        ch_num = std::stoi(spec);
    } catch(...)
    {
        return channel_type::none;
    }
    // check range of channel number
    if(ch_num < smux_channel_min || ch_num > smux_channel_max)
        return channel_type::none;
    // when we are here, we can savely convert the channel number
    ch = static_cast<smux_channel>(ch_num);

    // now, parse the file spec, beginning after the first :
    return parse_file_specs(spec.substr(delim_ch + delim.length()), flA, flB);
}

static channel_type parse_file_specs(std::string const& spec, file_def& flA, file_def& flB)
{
    std::string const delim("%");
    auto delim_io = spec.find(delim);

    // symmetric channel
    if(delim_io == std::string::npos)
    {
        if(parse_file_spec(spec, flA))
            return channel_type::symmetric;
        return channel_type::none; // error
    }

    // has read part only?
    if(delim_io == spec.length() - delim.length())
    {
        if(parse_file_spec(spec.substr(0, delim_io), flA))
            return channel_type::read_only;
        return channel_type::none;
    }

    // has write part only
    if(delim_io == 0)
    {
        if(parse_file_spec(spec.substr(delim.length()), flB))
            return channel_type::write_only;
        return channel_type::none;
    }

    // has separate read/write parts
    if(parse_file_spec(spec.substr(0, delim_io), flA) && parse_file_spec(spec.substr(delim_io + delim.length()), flB))
        return channel_type::separate;

    return channel_type::none;
}


static bool parse_file_spec(std::string const& spec, file_def& fl_def)
{
    std::string const delim(":");

    auto delim_arg = spec.find(delim);
    if(delim_arg != std::string::npos && delim_arg + delim.length() < spec.length()) // argument(s)?
        fl_def.arg = spec.substr(delim_arg + delim.length());
    fl_def.type = spec.substr(0, delim_arg);

    if(fl_def.type.empty())
        return false;

    return true;
}
