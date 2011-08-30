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


#include <libmary/types.h>
#include <cmath>

#include <moment/libmoment.h>

#include <mycpp/mycpp.h>
#include <mycpp/cmdline.h>


using namespace M;
using namespace Moment;

namespace {

LogGroup libMary_logGroup_time ("rtmptool_time", LogLevel::I);

class Options
{
public:
    bool help;
    Uint32 num_clients;

    bool got_server_addr;
    IpAddress server_addr;

    bool nonfatal_errors;

    Ref<String> app_name;
    Ref<String> channel;
    Ref<String> out_file;

    Uint32 report_interval;

    Options ()
	: help (false),
	  num_clients (1),
	  got_server_addr (false),
	  nonfatal_errors (false),
	  app_name (grab (new String ("oflaDemo"))),
	  channel (grab (new String ("red5StreamDemo"))),
	  report_interval (0)
    {
    }
};

mt_const Options options;

class RtmpClient : public DependentCodeReferenced
{
private:
    enum ConnectionState {
	ConnectionState_Connect,
	ConnectionState_ConnectSent,
	ConnectionState_CreateStreamSent,
	ConnectionState_PlaySent,
	ConnectionState_Streaming
    };

    mt_const Byte id_char;

    RtmpConnection rtmp_conn;
    TcpConnection tcp_conn;
    DeferredConnectionSender conn_sender;
    ConnectionReceiver conn_receiver;

    ConnectionState conn_state;

    Uint32 glob_stream_id;

    static TcpConnection::Frontend const tcp_conn_frontend;

    static void connected (Exception *exc_,
			   void      *_rtmp_conn);

    static RtmpConnection::Backend const rtmp_conn_backend;

    static void closeRtmpConn (void *cb_data);

    static RtmpConnection::Frontend const rtmp_conn_frontend;

    static Result handshakeComplete (void *cb_data);

    static Result commandMessageCallback (VideoStream::Message * mt_nonnull msg,
					  Uint32                msg_stream_id,
					  AmfEncoding           amf_encoding,
					  void                 *_self);

    static Result audioMessage (VideoStream::AudioMessage * mt_nonnull msg,
				void                      *_self);

    static Result videoMessage (VideoStream::VideoMessage * mt_nonnull msg,
				void                      *_self);

    static void closed (Exception *exc_,
			void      *_self);

public:
    Result start (PollGroup       *poll_group,
		  IpAddress const &addr);

    void init (Timers   *timers,
	       PagePool *page_pool);

    RtmpClient (Object *coderef_container,
		Byte    id_char);
};

TcpConnection::Frontend const RtmpClient::tcp_conn_frontend = {
    connected
};

RtmpConnection::Backend const RtmpClient::rtmp_conn_backend = {
    closeRtmpConn
};

RtmpConnection::Frontend const RtmpClient::rtmp_conn_frontend = {
    handshakeComplete,
    commandMessageCallback,
    audioMessage,
    videoMessage,
    NULL /* sendStateChanged */,
    closed
};

void
RtmpClient::connected (Exception * const exc_,
		       void      * const _rtmp_conn)
{
    if (exc_)
	exit (EXIT_FAILURE);

    logI_ (_func, "Connected successfully");

    RtmpConnection * const rtmp_conn = static_cast <RtmpConnection*> (_rtmp_conn);
    rtmp_conn->startClient ();
}

void
RtmpClient::closeRtmpConn (void * const /* cb_data */)
{
    logI_ (_func, "Connection closed");
    if (!options.nonfatal_errors)
	exit (0);
}

Result
RtmpClient::handshakeComplete (void * const _self)
{
    logD_ (_func_);

    RtmpClient * const self = static_cast <RtmpClient*> (_self);

    self->rtmp_conn.sendConnect (options.app_name->mem());
    self->conn_state = ConnectionState_ConnectSent;

    return Result::Success;
}

Result
RtmpClient::commandMessageCallback (VideoStream::Message   * const mt_nonnull msg,
				    Uint32                   const /* msg_stream_id */,
				    AmfEncoding              const /* amf_encoding */,
				    void                   * const _self)
{
    logD (time, _func, "ts:0x", fmt_hex, msg->timestamp);

    RtmpClient * const self = static_cast <RtmpClient*> (_self);

    if (msg->msg_len == 0)
	return Result::Success;

    PagePool::PageListArray pl_array (msg->page_list.first, msg->msg_len);
    AmfDecoder decoder (AmfEncoding::AMF0, &pl_array, msg->msg_len);

    Byte method_name [256];
    Size method_name_len;
    if (!decoder.decodeString (Memory::forObject (method_name),
			       &method_name_len,
			       NULL /* ret_full_len */))
    {
	logE_ (_func, "could not decode method name");
	return Result::Failure;
    }

    logD (time, _func, "method: ", ConstMemory (method_name, method_name_len));

    ConstMemory method (method_name, method_name_len);
    if (!compare (method, "_result")) {
	switch (self->conn_state) {
	    case ConnectionState_ConnectSent: {
		self->rtmp_conn.sendCreateStream ();
		self->conn_state = ConnectionState_CreateStreamSent;
	    } break;
	    case ConnectionState_CreateStreamSent: {
		double stream_id;
		if (!decoder.decodeNumber (&stream_id)) {
		    logE_ (_func, "Could not decode stream_id");
		    return Result::Failure;
		}

		self->glob_stream_id = lround (stream_id);

		self->rtmp_conn.sendPlay (options.channel->mem());

		self->conn_state = ConnectionState_Streaming;
	    } break;
	    case ConnectionState_PlaySent: {
		// Unused
	    } break;
	    default:
	      // Ignoring.
		;
	}
    } else
    if (!compare (method, "_error")) {
	switch (self->conn_state) {
	    case ConnectionState_ConnectSent:
	    case ConnectionState_CreateStreamSent:
	    case ConnectionState_PlaySent: {
		logE_ (_func, "_error received, returning Failure");
		return Result::Failure;
	    } break;
	    default:
	      // Ignoring.
		;
	}
    } else
    if (!compare (method, "onMetaData")) {
      // No-op

#if 0
	{
	    PagePool::Page * const page = msg->page_list->first;
	    if (page)
		hexdump (logs, page->mem());
	}
#endif
    } else
    if (!compare (method, "onStatus")) {
      // No-op

#if 0
	{
	    PagePool::Page * const page = msg->page_list->first;
	    if (page)
		hexdump (logs, page->mem());
	}
#endif
    } else {
	logW_ (_func, "unknown method: ", method);

#if 0
	{
	    PagePool::Page * const page = msg->page_list->first;
	    if (page)
		hexdump (logs, page->mem());
	}
#endif
    }

    return Result::Success;
}

Result
RtmpClient::audioMessage (VideoStream::AudioMessage * const mt_nonnull msg,
			  void                      * const /* _self */)
{
    logD (time, _func, "ts: 0x", fmt_hex, msg->timestamp, " ",
	  msg->codec_id, " ", msg->frame_type);
    return Result::Success;
}

Result
RtmpClient::videoMessage (VideoStream::VideoMessage * const mt_nonnull msg,
			  void                      * const _self)
{
    logD (time, _func, "ts: 0x", fmt_hex, msg->timestamp, " ",
	  msg->codec_id, " ", msg->frame_type);

    RtmpClient * const self = static_cast <RtmpClient*> (_self);

    if (options.report_interval) {
	static Uint32 debug_counter = options.report_interval;
	++debug_counter;
	if (debug_counter >= options.report_interval) {
	    debug_counter = 0;

	    logs->print (ConstMemory::forObject (self->id_char));
	    logs->flush ();
	}
    }

    return Result::Success;
}

void
RtmpClient::closed (Exception * const exc_,
		    void      * const /* _self */)
{
    if (exc_)
	logD_ (_func, exc_->toString());
    else
	logD_ (_func_);
}

Result
RtmpClient::start (PollGroup * const  poll_group,
		   IpAddress   const &addr)
{
    if (!tcp_conn.connect (addr)) {
	logE_ (_func, "tcp_conn.connect() failed: ", exc->toString());
	return Result::Failure;
    }

    poll_group->addPollable (tcp_conn.getPollable(), NULL /* ret_reg */);
    return Result::Success;
}

void
RtmpClient::init (Timers   * const timers,
		  PagePool * const page_pool)
{
    rtmp_conn.init (timers, page_pool);
}

RtmpClient::RtmpClient (Object * const coderef_container,
			Byte     const id_char)
    : DependentCodeReferenced (coderef_container),
      id_char (id_char),
      rtmp_conn     (coderef_container),
      tcp_conn      (coderef_container),
      conn_sender   (coderef_container),
      conn_receiver (coderef_container),
      conn_state (ConnectionState_Connect),
      glob_stream_id (0)
{
    conn_sender.setConnection (&tcp_conn);

    conn_receiver.setConnection (&tcp_conn);
    conn_receiver.setFrontend (rtmp_conn.getReceiverFrontend());

    rtmp_conn.setBackend (Cb<RtmpConnection::Backend> (&rtmp_conn_backend, NULL /* cb_data */, NULL /* coderef_container */));
    rtmp_conn.setFrontend (Cb<RtmpConnection::Frontend> (&rtmp_conn_frontend, this, getCoderefContainer()));
    rtmp_conn.setSender (&conn_sender);

    tcp_conn.setFrontend (Cb<TcpConnection::Frontend> (&tcp_conn_frontend, &rtmp_conn, rtmp_conn.getCoderefContainer()));
}

Result doTest (void)
{
    PagePool page_pool (4096 /* page_size */, 4096 /* min_pages */);

    ServerApp server_app (NULL /* coderef_container */);
    if (!server_app.init ()) {
	logE_ (_func, "server_app.init() failed: ", exc->toString());
	return Result::Failure;
    }

    IpAddress addr = options.server_addr;
    if (!options.got_server_addr) {
	if (!setIpAddress ("localhost:1935", &addr)) {
	    logE_ (_func, "setIpAddress() failed");
	    return Result::Failure;
	}
    }

    {
	Byte id_char = 'a';
	// TODO "Slow start" option.
	for (Uint32 i = 0; i < options.num_clients; ++i) {
	    logD_ (_func, "Starting client, id_char: ", ConstMemory::forObject (id_char));

	    // Note that RtmpClient objects are never freed.
	    RtmpClient * const client = new RtmpClient (NULL /* coderef_container */, id_char);

	    client->init (server_app.getTimers(), &page_pool);
	    if (!client->start (server_app.getPollGroup(), addr)) {
		logE_ (_func, "client->start() failed");
		return Result::Failure;
	    }

	    if (id_char == 'z')
		id_char = 'a';
	    else
		++id_char;
	}
    }

    logI_ (_func, "Starting...");
    server_app.run ();
    logI_ (_func, "...Finished");

    return Result::Success;
}

static void
printUsage ()
{
    outs->print ("Usage: rtmptool [options]\n"
		 "Options:\n"
		 "  -n --num-clients <number>      Simulate N simultaneous clients (default: 1)\n"
		 "  -s --server-addr <address>     Server address, IP:PORT (default: localhost:1935)\n"
		 "  -a --app <string>              Application name (default: oflaDemo)\n"
		 "  -c --channel <string>          Name of the channel to subscribe to (default: red5StreamDemo)\n"
		 "  -r --report-interval <number>  Interval between video frame reports (default: 0, no reports)\n"
		 "  --nonfatal-errors              Do not exit on the first error.\n"
		 "  -h --help                      Show this help message.\n"
//		 "  -o --out-file - Output file name.\n"
		 );
    outs->flush ();
}

bool cmdline_help (char const * /* short_name */,
		   char const * /* long_name */,
		   char const * /* value */,
		   void       * /* opt_data */,
		   void       * /* cb_data */)
{
    options.help = true;
    return true;
}

bool cmdline_num_clients (char const * /* short_name */,
			  char const * /* long_name */,
			  char const *value,
			  void       * /* opt_data */,
			  void       * /* cb_data */)
{
    if (!strToUint32_safe (value, &options.num_clients)) {
	logE_ (_func, "Invalid value \"", value, "\" "
	       "for --num-clients (number expected): ", exc->toString());
	exit (EXIT_FAILURE);
    }
    return true;
}

bool cmdline_server_addr (char const * /* short_name */,
			  char const * /* long_name */,
			  char const *value,
			  void       * /* opt_data */,
			  void       * /* cb_data */)
{
    if (!setIpAddress_default (ConstMemory (value, strlen (value)),
			       "localhost",
			       1935  /* default_port */,
			       false /* allow_any_host */,
			       &options.server_addr))
    {
	logE_ (_func, "Invalid value \"", value, "\" "
	       "for --server-addr (IP:PORT expected)");
	exit (EXIT_FAILURE);
    }
    options.got_server_addr = true;
    return true;
}

bool cmdline_app (char const * /* short_name */,
		  char const * /* long_name */,
		  char const *value,
		  void       * /* opt_data */,
		  void       * /* cb_data */)
{
    options.app_name = grab (new String (value));
    return true;
}

bool cmdline_channel (char const * /* short_name */,
		      char const * /* long_name */,
		      char const *value,
		      void       * /* opt_data */,
		      void       * /* cb_data */)
{
    options.channel = grab (new String (value));
    return true;
}

bool cmdline_out_file (char const * /* short_name */,
		       char const * /* long_name */,
		       char const *value,
		       void       * /* opt_data */,
		       void       * /* cb_data */)
{
    options.out_file = grab (new String (value));
    return true;
}

bool cmdline_report_interval (char const * /* short_name */,
			      char const * /* long_name */,
			      char const *value,
			      void       * /* opt_data */,
			      void       * /* cb_data */)
{
    if (!strToUint32_safe (value, &options.report_interval)) {
	logE_ (_func, "Invalid value \"", value, "\" "
	       "for --report-interval (number expected): ", exc->toString());
	exit (EXIT_FAILURE);
    }
    return true;
}

bool cmdline_nonfatal_errors (char const * /* short_name */,
			      char const * /* long_name */,
			      char const * /* value */,
			      void       * /* opt_data */,
			      void       * /* cb_data */)
{
    options.nonfatal_errors = true;
    return true;
}

} // namespace {}

int main (int argc, char **argv)
{
    MyCpp::myCppInit ();
    libMaryInit ();

    {
	unsigned const num_opts = 8;
	MyCpp::CmdlineOption opts [num_opts];

	opts [0].short_name = "h";
	opts [0].long_name  = "help";
	opts [0].with_value = false;
	opts [0].opt_data   = NULL;
	opts [0].opt_callback = cmdline_help;

	opts [1].short_name = "n";
	opts [1].long_name  = "num-clients";
	opts [1].with_value = true;
	opts [1].opt_data   = NULL;
	opts [1].opt_callback = cmdline_num_clients;

	opts [2].short_name = "s";
	opts [2].long_name  = "server-addr";
	opts [2].with_value = true;
	opts [2].opt_data   = NULL;
	opts [2].opt_callback = cmdline_server_addr;

	opts [3].short_name = "a";
	opts [3].long_name  = "app";
	opts [3].with_value = true;
	opts [3].opt_data   = NULL;
	opts [3].opt_callback = cmdline_app;

	opts [4].short_name = "c";
	opts [4].long_name  = "channel";
	opts [4].with_value = true;
	opts [4].opt_data   = NULL;
	opts [4].opt_callback = cmdline_channel;

	opts [5].short_name = "o";
	opts [5].long_name  = "out-file";
	opts [5].with_value = true;
	opts [5].opt_data   = NULL;
	opts [5].opt_callback = cmdline_out_file;

	opts [6].short_name = "r";
	opts [6].long_name  = "report-interval";
	opts [6].with_value = true;
	opts [6].opt_data   = NULL;
	opts [6].opt_callback = cmdline_report_interval;

	opts [7].short_name = NULL;
	opts [7].long_name  = "nonfatal-errors";
	opts [7].with_value = false;
	opts [7].opt_data   = NULL;
	opts [7].opt_callback = cmdline_nonfatal_errors;

	MyCpp::ArrayIterator<MyCpp::CmdlineOption> opts_iter (opts, num_opts);
	MyCpp::parseCmdline (&argc, &argv, opts_iter, NULL /* callback */, NULL /* callback_data */);
    }

    if (options.help) {
	printUsage ();
	return 0;
    }

    if (doTest ())
	return 0;

    return EXIT_FAILURE;
}

