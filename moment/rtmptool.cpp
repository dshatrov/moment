#include <libmary/types.h>
#include <cmath>

#include <moment/libmoment.h>

#include <mycpp/mycpp.h>


using namespace M;
using namespace Moment;

namespace {

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

    static Result commandMessageCallback (RtmpConnection::MessageInfo * mt_nonnull msg_info,
					  PagePool::PageListHead      * mt_nonnull page_list,
					  Size                         msg_len,
					  AmfEncoding                  amf_encoding,
					  void                        *_self);

    static Result audioMessage (RtmpConnection::MessageInfo * mt_nonnull msg_info,
				PagePool::PageListHead      * mt_nonnull page_list,
				Size                         msg_len,
				void                        *_self);

    static Result videoMessage (RtmpConnection::MessageInfo * mt_nonnull msg_info,
				PagePool::PageListHead      * mt_nonnull page_list,
				Size                         msg_len,
				void                        *_self);

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
    exit (0);
}

Result
RtmpClient::handshakeComplete (void * const _self)
{
    logD_ (_func_);

    RtmpClient * const self = static_cast <RtmpClient*> (_self);

    self->rtmp_conn.sendConnect ();
    self->conn_state = ConnectionState_ConnectSent;

    return Result::Success;
}

Result
RtmpClient::commandMessageCallback (RtmpConnection::MessageInfo * const mt_nonnull msg_info,
				    PagePool::PageListHead      * const mt_nonnull page_list,
				    Size                          const msg_len,
				    AmfEncoding                   const /* amf_encoding */,
				    void                        * const _self)
{
    logD_ (_func, "ts: ", msg_info->timestamp);

    RtmpClient * const self = static_cast <RtmpClient*> (_self);

    PagePool::PageListArray pl_array (page_list->first, msg_len);
    AmfDecoder decoder (AmfEncoding::AMF0, &pl_array, msg_len);

    Byte method_name [256];
    Size method_name_len;
    if (!decoder.decodeString (Memory::forObject (method_name),
			       &method_name_len,
			       NULL /* ret_full_len */))
    {
	logE_ (_func, "could not decode method name");
	return Result::Failure;
    }

    logD_ (_func, "method: ", ConstMemory (method_name, method_name_len));

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

		self->rtmp_conn.sendPlay ("red5StreamDemo");

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
    if (!compare (method, "onMetaData") == 0) {
      // No-op
    } else
    if (!compare (method, "onStatus") == 0) {
      // No-op
    } else {
	logW_ (_func, "unknown method: ", method);
    }

    return Result::Success;
}

Result
RtmpClient::audioMessage (RtmpConnection::MessageInfo * const mt_nonnull /* msg_info */,
			  PagePool::PageListHead      * const mt_nonnull /* page_list */,
			  Size                          const /* msg_len */,
			  void                        * const /* _self */)
{
//    logD_ (_func, "ts: ", msg_info->timestamp);
    return Result::Success;
}

Result
RtmpClient::videoMessage (RtmpConnection::MessageInfo * const mt_nonnull /* msg_info */,
			  PagePool::PageListHead      * const mt_nonnull /* page_list */,
			  Size                          const /* msg_len */,
			  void                        * const _self)
{
//    logD_ (_func, "0x", fmt_hex, (UintPtr) _self, ", ts: ", fmt_def, msg_info->timestamp);

    RtmpClient * const self = static_cast <RtmpClient*> (_self);

    static int debug_counter = 1031;
    ++debug_counter;
    if (debug_counter >= 1031) {
	debug_counter = 0;

	logs->print (ConstMemory::forObject (self->id_char));
	logs->flush ();
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

    poll_group->addPollable (tcp_conn.getPollable());
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
    MyCpp::myCppInit ();
    libMaryInit ();

    PagePool page_pool (4096 /* page_size */, 4096 /* min_pages */);

    ServerApp server_app (NULL /* coderef_container */);
    if (!server_app.init ()) {
	logE_ (_func, "server_app.init() failed: ", exc->toString());
	return Result::Failure;
    }

    IpAddress addr;
    if (!setIpAddress ("localhost:8083", &addr)) {
	logE_ (_func, "setIpAddress() failed");
	return Result::Failure;
    }

    {
	Byte id_char = 'a';
	for (Uint32 i = 0; i < 250; ++i) {
	    logD_ (_func, "Starting client, id_char: ", ConstMemory::forObject (id_char));

	    // Note that RtmpClient objects are never freed.
	    RtmpClient *client = new RtmpClient (NULL /* coderef_container */, id_char);

	    client->init (server_app.getTimers(), &page_pool);
	    client->start (server_app.getPollGroup(), addr);

	    if (id_char == 'z')
		id_char = 'a';
	    else
		++id_char;
	}
    }

    logD_ (_func, "Starting...");
    server_app.run ();
    logD_ (_func, "...Finished");

    return Result::Success;
}

} // namespace {}

int main (void)
{
    if (doTest ())
	return 0;

    return EXIT_FAILURE;
}

