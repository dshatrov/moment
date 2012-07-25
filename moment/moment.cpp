/*  Moment Video Server - High performance media server
    Copyright (C) 2011 Dmitry Shatrov
    e-mail: shatrov@gmail.com

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include <libmary/types.h>

#include <cstdio>

#ifndef LIBMARY_PLATFORM_WIN32
#include <unistd.h>
#endif
#include <errno.h>

#include <moment/libmoment.h>

#include <mycpp/mycpp.h>
#include <mycpp/cmdline.h>


using namespace M;
using namespace Moment;

namespace {

struct Options {
    bool help;
    bool daemonize;
    Ref<String> config_filename;
    Ref<String> log_filename;
    LogLevel loglevel;
    Uint64 exit_after;

    Options ()
	: help (false),
	  daemonize (false),
	  loglevel (LogLevel::Info),
	  exit_after ((Uint64) -1)
    {
    }
};

const Count default__page_pool__min_pages     = 512;
const Time  default__http__keepalive_timeout  =  60;

Options options;
MConfig::Config config;

PagePool page_pool (4096 /* page_size */, default__page_pool__min_pages);

ServerApp server_app (NULL /* coderef_container */);
HttpService http_service (NULL /* coderef_container */);

HttpService separate_admin_http_service (NULL /* coderef_container */);
HttpService *admin_http_service_ptr = &separate_admin_http_service;

FixedThreadPool recorder_thread_pool (NULL /* coderef_container */);

LocalStorage storage;

MomentServer moment_server;

mt_mutex (mutex) Timers::TimerKey exit_timer_key = NULL;
Mutex mutex;

static void
printUsage ()
{
    outs->print ("Usage: moment [options]\n"
		  "Options:\n"
		  "  -c --config <config_file>  Configuration file to use (default: /opt/moment/moment.conf)\n"
		  "  -l --log <log_file>        Log file to use (default: /var/log/moment.log)\n"
		  "  --loglevel <loglevel>      Loglevel, one of A/D/I/W/E/H/F/N (default: I, \"Info\")\n"
#ifndef PLATFORM_WIN32
		  "  -d --daemonize             Daemonize (run in the background as a daemon).\n"
#endif
		  "  --exit-after <number>      Exit after specified timeout in seconds.\n"
		  "  -h --help                  Show this help message.\n");
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
cmdline_daemonize (char const * /* short_name */,
		   char const * /* long_name */,
		   char const * /* value */,
		   void       * /* opt_data */,
		   void       * /* cb_data */)
{
    options.daemonize = true;
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

static bool
cmdline_log (char const * /* short_name */,
	     char const * /* long_name */,
	     char const *value,
	     void       * /* opt_data */,
	     void       * /* cb_data */)
{
    options.log_filename = grab (new String (value));
    return true;
}

static bool
cmdline_loglevel (char const * /* short_name */,
		  char const * /* long_name */,
		  char const *value,
		  void       * /* opt_data */,
		  void       * /* cb_data */)
{
    ConstMemory const value_mem = ConstMemory (value, value ? strlen (value) : 0);
    if (!LogLevel::fromString (value_mem, &options.loglevel)) {
	logE_ (_func, "Invalid loglevel name \"", value_mem, "\", using \"Info\"");
	options.loglevel = LogLevel::Info;
    }

    return true;
}

static bool
cmdline_exit_after (char const * /* short_nmae */,
		    char const * /* long_name */,
		    char const *value,
		    void       * /* opt_data */,
		    void       * /* cb_data */)
{
    if (!strToUint64_safe (value, &options.exit_after)) {
	logE_ (_func, "Invalid value \"", value, "\" "
	       "for --exit-after (number expected): ", exc->toString());
	exit (EXIT_FAILURE);
    }

    logD_ (_func, "options.exit_after: ", options.exit_after);
    return true;
}

static void exitTimerTick (void * const /* cb_data */)
{
    logI_ (_func, "Exit timer expired (", options.exit_after, " seconds)");
    mutex.lock ();
    server_app.getServerContext()->getTimers()->deleteTimer (exit_timer_key);
    mutex.unlock ();

//    exit (0);
    server_app.stop ();
}

}

int main (int argc, char **argv)
{
    int ret_res = 0;

    MyCpp::myCppInit ();
    libMaryInit ();

    {
	unsigned const num_opts = 6;
	MyCpp::CmdlineOption opts [num_opts];

	opts [0].short_name = "h";
	opts [0].long_name  = "help";
	opts [0].with_value = false;
	opts [0].opt_data   = NULL;
	opts [0].opt_callback = cmdline_help;

	opts [1].short_name = "d";
	opts [1].long_name  = "daemonize";
	opts [1].with_value = false;
	opts [1].opt_data   = NULL;
	opts [1].opt_callback = cmdline_daemonize;

	opts [2].short_name = "c";
	opts [2].long_name  = "config";
	opts [2].with_value = true;
	opts [2].opt_data   = NULL;
	opts [2].opt_callback = cmdline_config;

	opts [3].short_name = "l";
	opts [3].long_name  = "log";
	opts [3].with_value = true;
	opts [3].opt_data   = NULL;
	opts [3].opt_callback = cmdline_log;

	opts [4].short_name = "NULL";
	opts [4].long_name = "loglevel";
	opts [4].with_value = true;
	opts [4].opt_data = NULL;
	opts [4].opt_callback = cmdline_loglevel;

	opts [5].short_name = NULL;
	opts [5].long_name = "exit-after";
	opts [5].with_value = true;
	opts [5].opt_data = NULL;
	opts [5].opt_callback = cmdline_exit_after;

	MyCpp::ArrayIterator<MyCpp::CmdlineOption> opts_iter (opts, num_opts);
	MyCpp::parseCmdline (&argc, &argv, opts_iter, NULL /* callback */, NULL /* callbackData */);
    }

    if (options.help) {
        setGlobalLogLevel (LogLevel::Failure);
	printUsage ();
	return 0;
    }

// Unnecessary   getDefaultLogGroup()->setLogLevel (options.loglevel);
    setGlobalLogLevel (options.loglevel);

    if (options.daemonize) {
#ifdef PLATFORM_WIN32
        logW_ (_func, "Daemonization is not supported on Windows");
#else
	logI_ (_func, "Daemonizing. Server log is at /var/log/moment.log");
	int const res = daemon (1 /* nochdir */, 0 /* noclose */);
	if (res == -1)
	    logE_ (_func, "daemon() failed: ", errnoString (errno));
	else
	if (res != 0)
	    logE_ (_func, "Unexpected return value from daemon(): ", res);
#endif
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

    {
	ConstMemory const log_filename = options.log_filename ?
						 options.log_filename->mem() :
						 ConstMemory ("/var/log/moment.log");
	// We never deallocate 'log_file' after log file is opened successfully.
	NativeFile * const log_file = new NativeFile ();
	assert (log_file);
	if (!log_file->open (log_filename,
			     File::OpenFlags::Create | File::OpenFlags::Append,
			     File::AccessMode::WriteOnly))
	{
	    logE_ (_func, "Could not open log file \"", log_filename, "\": ", exc->toString());
	    delete log_file;
	} else {
	    logI_ (_func, "Log file is at ", log_filename);
	    // We never deallocate 'logs'
	    delete logs;
	    logs = new BufferedOutputStream (log_file, 4096);
	    assert (logs);
	}
    }

    logLock ();
    config.dump (logs);
    logUnlock ();

    if (!server_app.init ()) {
	logE_ (_func, "server_app.init() failed: ", exc->toString());
	return EXIT_FAILURE;
    }

    {
	Uint64 min_pages = default__page_pool__min_pages;
	{
	    ConstMemory const opt_name ("moment/min_pages");
	    ConstMemory const min_pages_mem = config.getString (opt_name);
	    if (min_pages_mem.len()) {
		if (!strToUint64_safe (min_pages_mem, &min_pages)) {
		    logE_ (_func, "Bad option value \"", min_pages_mem, "\" "
			   "for ", opt_name, " (number expected): ", exc->toString());
		    logE_ (_func, "Using default value of ", min_pages, " for ", opt_name);
		}
	    }
	}

//	logD_ (_func, "min_pages: ", min_pages);
	page_pool.setMinPages (min_pages);
    }

    {
	Uint64 num_threads = 0;
	{
	    ConstMemory const opt_name = "moment/num_threads";
	    if (!config.getUint64_default (opt_name, &num_threads, num_threads)) {
		logE_ (_func, "Bad value for config option ", opt_name);
		return EXIT_FAILURE;
	    }

	    logI_ (_func, opt_name, ": ", num_threads);
	}

	server_app.setNumThreads (num_threads);
    }

    {
	Uint64 num_file_threads = 0;
	{
	    ConstMemory const opt_name = "moment/num_file_threads";
	    if (!config.getUint64_default (opt_name, &num_file_threads, num_file_threads)) {
		logE_ (_func, "Bad value for config option ", opt_name);
		return EXIT_FAILURE;
	    }

	    logI_ (_func, opt_name, ": ", num_file_threads);
	}

	recorder_thread_pool.setNumThreads (num_file_threads);
    }

    Uint64 http_keepalive_timeout = default__http__keepalive_timeout;
    {
	ConstMemory const opt_name ("http/keepalive_timeout");
	ConstMemory const http_keepalive_timeout_mem = config.getString (opt_name);
	if (http_keepalive_timeout_mem.len()) {
	    if (!strToUint64_safe (http_keepalive_timeout_mem, &http_keepalive_timeout)) {
		logE_ (_func, "Bad option value \"", http_keepalive_timeout_mem, "\" "
		       "for ", opt_name, " (number expected): ", exc->toString());
		logE_ (_func, "Using default value of ", http_keepalive_timeout, " for ", opt_name);
	    }
	}
    }

    bool no_keepalive_conns = false;
    {
	ConstMemory const opt_name ("http/no_keepalive_conns");
	MConfig::Config::BooleanValue const enable = config.getBoolean (opt_name);
	if (enable == MConfig::Config::Boolean_Invalid) {
	    logE_ (_func, "Invalid value for ", opt_name, ": ", config.getString (opt_name));
	    return EXIT_FAILURE;
	}

	if (enable == MConfig::Config::Boolean_True)
	    no_keepalive_conns = true;

	logI_ (_func, opt_name, ": ", no_keepalive_conns);
    }

    ConstMemory http_bind;
    {
	if (!http_service.init (server_app.getServerContext()->getMainPollGroup(),
				server_app.getServerContext()->getTimers(),
				&page_pool,
				http_keepalive_timeout,
				no_keepalive_conns))
	{
	    logE_ (_func, "http_service.init() failed: ", exc->toString());
	    return EXIT_FAILURE;
	}

	do {
	    ConstMemory const opt_name = "http/http_bind";
	    http_bind = config.getString_default (opt_name, ":8080");
	    logI_ (_func, opt_name, ": ", http_bind);
	    if (http_bind.isNull()) {
		logI_ (_func, "HTTP service is not bound to any port "
		       "and won't accept any connections. "
		       "Set \"", opt_name, "\" option to bind the service.");
		break;
	    }

	    IpAddress addr;
	    if (!setIpAddress_default (http_bind,
				       ConstMemory() /* default_host */,
				       8080          /* default_port */,
				       true          /* allow_any_host */,
				       &addr))
	    {
		logE_ (_func, "setIpAddress_default() failed (http)");
		return EXIT_FAILURE;
	    }

	    if (!http_service.bind (addr)) {
		logE_ (_func, "http_service.bind() failed (http): ", exc->toString());
		break;
	    }

	    if (!http_service.start ()) {
		logE_ (_func, "http_service.start() failed (http): ", exc->toString());
		return EXIT_FAILURE;
	    }
	} while (0);
    }

    {
	do {
	    ConstMemory const opt_name = "http/admin_bind";
	    ConstMemory const admin_bind = config.getString_default (opt_name, ":8082");
	    logI_ (_func, opt_name, ": ", admin_bind);
	    if (admin_bind.isNull()) {
		logI_ (_func, "HTTP-Admin service is not bound to any port "
		       "and won't accept any connections. "
		       "Set \"", opt_name, "\" option to bind the service.");
		break;
	    }

	    if (equal (admin_bind, http_bind)) {
		admin_http_service_ptr = &http_service;
		break;
	    }

	    if (!separate_admin_http_service.init (server_app.getServerContext()->getMainPollGroup(),
						   server_app.getServerContext()->getTimers(),
						   &page_pool,
						   http_keepalive_timeout,
						   no_keepalive_conns))
	    {
		logE_ (_func, "admin_http_service.init() failed: ", exc->toString());
		return EXIT_FAILURE;
	    }

	    IpAddress addr;
	    if (!setIpAddress_default (admin_bind,
				       ConstMemory() /* default_host */,
				       8082          /* default_port */,
				       true          /* allow_any_host */,
				       &addr))
	    {
		logE_ (_func, "setIpAddress_default() failed (admin)");
		return EXIT_FAILURE;
	    }

	    if (!separate_admin_http_service.bind (addr)) {
		logE_ (_func, "http_service.bind() failed (admin): ", exc->toString());
		break;
	    }

	    if (!separate_admin_http_service.start ()) {
		logE_ (_func, "http_service.start() failed (admin): ", exc->toString());
		return EXIT_FAILURE;
	    }
	} while (0);
    }

    recorder_thread_pool.setMainThreadContext (server_app.getMainThreadContext());
    if (!recorder_thread_pool.spawn ()) {
	logE_ (_func, "recorder_thread_pool.spawn() failed");
	return EXIT_FAILURE;
    }

    if (!moment_server.init (&server_app,
			     &page_pool,
			     &http_service,
			     admin_http_service_ptr,
			     &config,
			     &recorder_thread_pool,
			     &storage))
    {
	logE_ (_func, "moment_server.init() failed: ", exc->toString());
	ret_res = EXIT_FAILURE;
	goto _stop_recorder;
    }

    if (options.exit_after != (Uint64) -1) {
	logI_ (_func, "options.exit_after: ", options.exit_after);
	mutex.lock ();
	exit_timer_key = server_app.getServerContext()->getTimers()->addTimer (
                exitTimerTick,
                NULL /* cb_data */,
                NULL /* coderef_container */,
                options.exit_after,
                false /* periodical */);
	mutex.unlock ();
    }

    if (!server_app.run ()) {
	logE_ (_func, "server_app.run() failed: ", exc->toString());
	ret_res = EXIT_FAILURE;
	goto _stop_recorder;
    }

    logI_ (_func, "done");

_stop_recorder:
    recorder_thread_pool.stop ();

    return ret_res;
}

