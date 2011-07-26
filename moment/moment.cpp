/*  Moment Video Server - High performance media server
    Copyright (C) 2011 Dmitry Shatrov

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/


#include <moment/libmoment.h>

#include <mycpp/mycpp.h>
#include <mycpp/cmdline.h>


using namespace M;
using namespace Moment;

namespace {

struct Options {
    bool help;
    Ref<String> config_filename;

    Options ()
	: help (false)
    {
    }
};

const Count default__page_pool__min_pages    = 512;
const Time  default__http__keepalive_timeout =  60;

Options options;
MConfig::Config config;

PagePool page_pool (4096 /* page_size */, default__page_pool__min_pages);

ServerApp server_app (NULL /* coderef_container */);
HttpService http_service (NULL /* coderef_container */);

MomentServer moment_server;

static void
printUsage ()
{
    outs->print ("Usage: videochatd [options]\n"
		  "Options:\n"
		  "  -c --config <config_file> - Configuration file to use (default: /opt/moment/moment.conf)\n");
    outs->flush ();
}

static bool
cmdline_help (char const * /* short_name */,
	      char const * /* long_name */,
	      char const * /* value */,
	      void       * /* opt_data */,
	      void       * /* cb_data */)
{
    options.help = true;
    return true;
}

static bool
cmdline_config (char const * /* short_name */,
		char const * /* long_name */,
		char const *value,
		void       * /* opt_data */,
		void       * /* cb_data */)
{
    options.config_filename = grab (new String (value));
    return true;
}

}

int main (int argc, char **argv)
{
    MyCpp::myCppInit ();
    libMaryInit ();

    {
	unsigned const num_opts = 2;
	MyCpp::CmdlineOption opts [num_opts];

	opts [0].short_name = "h";
	opts [0].long_name  = "help";
	opts [0].with_value = false;
	opts [0].opt_data   = NULL;
	opts [0].opt_callback = cmdline_help;

	opts [1].short_name = "c";
	opts [1].long_name  = "config";
	opts [1].with_value = true;
	opts [1].opt_data   = NULL;
	opts [1].opt_callback = cmdline_config;

	MyCpp::ArrayIterator<MyCpp::CmdlineOption> opts_iter (opts, num_opts);
	MyCpp::parseCmdline (&argc, &argv, opts_iter, NULL /* callback */, NULL /* callbackData */);
    }

    if (options.help) {
	printUsage ();
	return 0;
    }

    {
	ConstMemory const config_filename = options.config_filename ?
						    options.config_filename->mem() :
						    ConstMemory ("/opt/moment/moment.conf");
	if (!MConfig::parseConfig (config_filename, &config)) {
	    logE_ (_func, "Failed to parse config file ", config_filename);
	    return Result::Failure;
	}
    }
    config.dump (logs);

    if (!server_app.init ()) {
	logE_ (_func, "server_app.init() failed: ", exc->toString());
	return EXIT_FAILURE;
    }

    {
	Uint64 min_pages = default__page_pool__min_pages;
	{
	    ConstMemory opt_name ("page_pool/min_pages");
	    ConstMemory min_pages_mem = config.getString (opt_name);
	    if (min_pages_mem.len()) {
		if (!strToUint64_safe (min_pages_mem, &min_pages)) {
		    logE_ (_func, "Bad option value \"", min_pages_mem, "\" "
			   "for ", opt_name, " (number expected): ", exc->toString());
		    logE_ (_func, "Using default value of ", min_pages, " for ", opt_name);
		}
	    }
	}

	page_pool.setMinPages (min_pages);
    }

    {
	Uint64 http_keepalive_timeout = default__http__keepalive_timeout;
	{
	    ConstMemory opt_name ("http/keepalive_timeout");
	    ConstMemory http_keepalive_timeout_mem = config.getString (opt_name);
	    if (http_keepalive_timeout_mem.len()) {
		if (!strToUint64_safe (http_keepalive_timeout_mem, &http_keepalive_timeout)) {
		    logE_ (_func, "Bad option value \"", http_keepalive_timeout_mem, "\" "
			   "for ", opt_name, " (number expected): ", exc->toString());
		    logE_ (_func, "Using default value of ", http_keepalive_timeout, " for ", opt_name);
		}
	    }
	}

	if (!http_service.init (server_app.getPollGroup(), server_app.getTimers(), &page_pool, http_keepalive_timeout)) {
	    logE_ (_func, "http_service.init() failed: ", exc->toString());
	    return EXIT_FAILURE;
	}

	do {
	    ConstMemory const opt_name = "http/http_bind";
	    ConstMemory const http_bind = config.getString_default (opt_name, ":8080");
	    logD_ (_func, opt_name, ": ", http_bind);
	    if (!http_bind.isNull ()) {
		IpAddress addr;
		if (!setIpAddress (http_bind, &addr)) {
		    logE_ (_func, "setIpAddress() failed (http)");
		    return EXIT_FAILURE;
		}

		if (!http_service.bind (addr)) {
		    logE_ (_func, "http_service.bind() faled: ", exc->toString());
		    break;
		}

		if (!http_service.start ()) {
		    logE_ (_func, "http_service.start() failed: ", exc->toString());
		    return EXIT_FAILURE;
		}
	    } else {
		logI_ (_func, "HTTP service is not bound to any port "
		       "and won't accept any connections. "
		       "Set \"", opt_name, "\" option to bind the service.");
	    }
	} while (0);
    }

    logD_ (_func, "CREATING MOMENT SERVER");
    if (!moment_server.init (&server_app, &page_pool, &http_service, &config)) {
	logE_ (_func, "moment_server.init() failed: ", exc->toString());
	return EXIT_FAILURE;
    }

    if (!server_app.run ()) {
	logE_ (_func, "server_app.run() failed: ", exc->toString());
	return EXIT_FAILURE;
    }

    logD_ (_func, "DONE");

    return 0;
}

