// cnf_argv.cpp
#include <utility> // std::move

#include "cnf_argv.h"

using namespace smux_client;

/**
 * \brief                   parse specification of a file
 * \param spec              textual spec
 * \param[out] fl_def       file definiton
 * \return                  true if successful
 */
static bool parse_file_spec(std::string const& spec, cnf::file_def& fl_def);

/**
 * \brief                   parse a channel specification (channel id, channel type, file spec)
 * \param spec              channel spec
 * \param[out] ch           channel number
 * \param[out] fl_def       file definition
 * \return                  true if successful
 */
static bool parse_channel_spec(std::string const& spec, smux_channel& ch, cnf::file_def& fl_def);

void cnf_argv::parse(int argc, char const* argv[])
{
    // first argument is program name
    if(argc > 0)
        _pgrm_name = argv[0];

    // loop over the rest
    for(int i = 1; i < argc; ++i)
    {
        if(argv[i][0] == '-') // option?
        {
            if(argv[i][1] == '-')
            {
                if(argv[i][2] == 0) // litaral --
                {
                    // stop option processing
                    // positional arguments never start with -, so just ignore it
                    continue;
                } else
                {
                    // long option
                    std::string opt(argv[i] + 2);

                    // switch all available options
                    if(opt == "master") // definiton of master file
                    {
                        // get the file specification
                        if(i + 1 >= argc)
                            throw config_error("--master requieres an argument");
                        std::string arg(argv[++i]);
                        // parse file definition
                        std::unique_ptr<cnf::file_def> fl_def(new cnf::file_def);
                        if(!parse_file_spec(arg, *fl_def))
                            throw config_error(std::string("unable to parse master file specification: ") + arg);
                        // add master file spec
                        if(fl_def->mode == file_mode::io)
                            set_master_file(std::move(fl_def));
                        else if(fl_def->mode == file_mode::in)
                            set_master_file_in(std::move(fl_def));
                        else
                            set_master_file_out(std::move(fl_def));
                    } else
                        throw config_error("unknown option --" + opt);
                }
            } else
            {
                // short option
                std::string opt(argv[i] + 1);
                for(auto& o : opt)
                {
                    switch(o)
                    {
                        case 'h':
                            _help_level++;
                            break;
                        case 'd':
                            _debug_level++;
                            break;
                        default:
                            throw config_error(std::string("unknown option -") + o);
                    }
                }
            }
        } else // positional argument == channel definiton
        {
            std::string arg(argv[i]);
            std::unique_ptr<cnf::file_def> fl_def(new cnf::file_def);
            smux_channel ch;
            if(!parse_channel_spec(arg, ch, *fl_def))
                throw config_error(std::string("unable to parse channel specification: ") + arg);
            // finally, add the spec to the channel list
            if(fl_def->mode == file_mode::io)
                set_channel_file(ch, std::move(fl_def));
            else if(fl_def->mode == file_mode::in)
                set_channel_file_in(ch, std::move(fl_def));
            else
                set_channel_file_out(ch, std::move(fl_def));
        }
    }
}

static bool parse_channel_spec(std::string const& spec, smux_channel& ch, cnf::file_def& fl_def)
{
    // string up to the first : contains the channel number
    auto delim = spec.find(':');
    if(delim == std::string::npos || delim == spec.length())
        return false;
    // extract first characters and convert them to a number
    int ch_num;
    try
    {
        ch_num = std::stoi(spec);
    } catch(...)
    {
        return false;
    }
    // check range of channel number
    if(ch_num < smux_channel_min || ch_num > smux_channel_max)
        return false;
    // when we are here, we can savely convert the channel number
    ch = static_cast<smux_channel>(ch_num);

    // now, parse the file spec, beginning after the first :
    return parse_file_spec(spec.substr(delim + 1), fl_def);
}

static bool parse_file_spec(std::string const& spec, cnf::file_def& fl_def)
{
    std::string const delim(":");

    // up to the first : we can find the file mode (or nothing for io)
    auto delim_mode = spec.find(delim);
    if(delim_mode == 0)
        fl_def.mode = file_mode::io;
    else if(delim_mode == 1)
    {
        if(spec[0] == 'i')
            fl_def.mode = file_mode::in;
        else if(spec[0] == 'o')
            fl_def.mode = file_mode::out;
        else return false;
    } else
        return false; // delim can only be 0 or 1

    // next, we expect the file type (between the first : and the second :)
    auto delim_type = spec.find(delim, delim_mode + delim.length());
    if(delim_type == std::string::npos)
        return false;
    fl_def.type = spec.substr(delim_mode + delim.length(), delim_type - delim_mode - delim.length());

    // finally, split arguments at the remaining :
    auto arg_start = delim_type + delim.length();
    auto arg_end = spec.find(delim, arg_start);
    while(arg_end != std::string::npos)
    {
        fl_def.args.emplace_back(spec.substr(arg_start, arg_end - arg_start));
        arg_start = arg_end + delim.length();
        arg_end = spec.find(delim, arg_start);
    }
    fl_def.args.emplace_back(spec.substr(arg_start)); // save the last argument

    return true;
}
