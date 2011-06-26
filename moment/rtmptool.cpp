#include <libmary/types.h>
#include <cmath>

#include <moment/libmoment.h>

#include <mycpp/mycpp.h>


using namespace M;
using namespace Moment;

namespace {

void connected (Exception *exc_,
	        void      *_rtmp_conn);

void closeRtmpConn (void *cb_data);

RtmpConnection::Backend rtmp_conn_backend = {
    closeRtmpConn
};

enum ConnectionState {
    ConnectionState_Connect,
    ConnectionState_ConnectSent,
    ConnectionState_CreateStreamSent,
    ConnectionState_PlaySent,
    ConnectionState_Streaming
};
ConnectionState conn_state = ConnectionState_Connect;

Uint32 glob_stream_id = 0;

// TODO connect -> createStream -> play

Result handshakeComplete (void *cb_data);

Result commandMessageCallback (RtmpConnection::MessageInfo * mt_nonnull msg_info,
			       PagePool::PageListHead      * mt_nonnull page_list,
			       Size                         msg_len,
			       AmfEncoding                  amf_encoding,
			       void                        *cb_data);

Result audioMessage (RtmpConnection::MessageInfo * mt_nonnull msg_info,
		     PagePool::PageListHead      * mt_nonnull page_list,
		     Size                         msg_len,
		     void                        *cb_data);

Result videoMessage (RtmpConnection::MessageInfo * mt_nonnull msg_info,
		     PagePool::PageListHead      * mt_nonnull page_list,
		     Size                         msg_len,
		     void                        *cb_data);

void closed (Exception *exc_,
	     void      *cb_data);

RtmpConnection::Frontend const rtmp_conn_frontend = {
    handshakeComplete,
    commandMessageCallback,
    audioMessage,
    videoMessage,
    closed
};

Result doTest (void)
{
    MyCpp::myCppInit ();
    libMaryInit ();

    PagePool page_pool (4096 /* page_size */, 128 /* min_pages */);

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

    RtmpConnection rtmp_conn (NULL /* coderef_container */, server_app.getTimers(), &page_pool);

    TcpConnection tcp_conn (NULL /* coderef_container */);

    DeferredConnectionSender conn_sender (NULL /* coderef_container */);
    conn_sender.setConnection (&tcp_conn);

    ConnectionReceiver conn_receiver (NULL /* coderef_container */, &tcp_conn);
    conn_receiver.setConnection (&tcp_conn);
    conn_receiver.setFrontend (rtmp_conn.getReceiverFrontend());

    rtmp_conn.setBackend (Cb<RtmpConnection::Backend> (&rtmp_conn_backend, NULL /* cb_data */, NULL /* coderef_container */));
    rtmp_conn.setFrontend (Cb<RtmpConnection::Frontend> (&rtmp_conn_frontend, &rtmp_conn, NULL /* coderef_container */));
    rtmp_conn.setSender (&conn_sender);

    TcpConnection::Frontend const tcp_conn_frontend = {
	connected
    };
    tcp_conn.setFrontend (Cb<TcpConnection::Frontend> (&tcp_conn_frontend, &rtmp_conn, NULL /* coderef_container */));

    if (!tcp_conn.connect (addr)) {
	logE_ (_func, "tcp_conn.connect() failed: ", exc->toString());
	return Result::Failure;
    }

    server_app.getPollGroup()->addPollable (tcp_conn.getPollable());

    logD_ (_func, "Starting...");
    server_app.run ();
    logD_ (_func, "...Finished");

    return Result::Success;
}

void connected (Exception * const exc_,
		void      * const _rtmp_conn)
{
    if (exc_)
	exit (EXIT_FAILURE);

    logI_ (_func, "Connected successfully");

    RtmpConnection * const rtmp_conn = static_cast <RtmpConnection*> (_rtmp_conn);
    rtmp_conn->startClient ();
}

void closeRtmpConn (void * const /* cb_data */)
{
    logI_ (_func, "Connection closed");
    exit (0);
}

Result handshakeComplete (void * const _rtmp_conn)
{
    logD_ (_func_);

    RtmpConnection * const rtmp_conn = static_cast <RtmpConnection*> (_rtmp_conn);
    rtmp_conn->sendConnect ();
    conn_state = ConnectionState_ConnectSent;
    return Result::Success;
}

Result commandMessageCallback (RtmpConnection::MessageInfo * const mt_nonnull msg_info,
			       PagePool::PageListHead      * const mt_nonnull page_list,
			       Size                          const msg_len,
			       AmfEncoding                   const amf_encoding,
			       void                        * const _rtmp_conn)
{
    logD_ (_func, "ts: ", msg_info->timestamp);

    RtmpConnection * const rtmp_conn = static_cast <RtmpConnection*> (_rtmp_conn);

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
	switch (conn_state) {
	    case ConnectionState_ConnectSent: {
		rtmp_conn->sendCreateStream ();
		conn_state = ConnectionState_CreateStreamSent;
	    } break;
	    case ConnectionState_CreateStreamSent: {
		double stream_id;
		if (!decoder.decodeNumber (&stream_id)) {
		    logE_ (_func, "Could not decode stream_id");
		    return Result::Failure;
		}

		glob_stream_id = lround (stream_id);

		rtmp_conn->sendPlay ("red5StreamDemo");

		conn_state = ConnectionState_Streaming;
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
	switch (conn_state) {
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

Result audioMessage (RtmpConnection::MessageInfo * const mt_nonnull msg_info,
		     PagePool::PageListHead      * const mt_nonnull page_list,
		     Size                          const msg_len,
		     void                        * const /* cb_data */)
{
//    logD_ (_func, "ts: ", msg_info->timestamp);
    return Result::Success;
}

Result videoMessage (RtmpConnection::MessageInfo * const mt_nonnull msg_info,
		     PagePool::PageListHead      * const mt_nonnull page_list,
		     Size                          const msg_len,
		     void                        * const /* cb_data */)
{
//    logD_ (_func, "ts: ", msg_info->timestamp);
    logs->print (".");
    logs->flush ();
    return Result::Success;
}

void closed (Exception * const exc_,
	     void      * const /* cb_data */)
{
    logD_ (_func_);
}

} // namespace {}

int main (void)
{
    if (doTest ())
	return 0;

    return EXIT_FAILURE;
}

