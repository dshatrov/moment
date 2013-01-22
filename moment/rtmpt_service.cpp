/*  Moment Video Server - High performance media server
    Copyright (C) 2011-2013 Dmitry Shatrov
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


#include <moment/rtmpt_service.h>


using namespace M;

namespace Moment {

static LogGroup libMary_logGroup_rtmpt_service ("rtmpt_service", LogLevel::D);

RtmptServer::Frontend const RtmptService::rtmpt_server_frontend = {
    clientConnected,
    connectionClosed
};

TcpServer::Frontend const RtmptService::tcp_server_frontend = {
    accepted
};

Result
RtmptService::clientConnected (RtmpConnection  * const mt_nonnull rtmp_conn,
			       IpAddress const &client_addr,
			       void            * const _self)
{
    RtmptService * const self = static_cast <RtmptService*> (_self);

    Result res = Result::Failure;
    if (!self->frontend.call_ret<Result> (&res, self->frontend->clientConnected, /*(*/ rtmp_conn, client_addr /*)*/))
        return Result::Failure;

    if (!res)
        return Result::Failure;

    return Result::Success;
}

void
RtmptService::connectionClosed (void * const _conn_data,
				void * const /* _self */)
{
//    RtmptService * const self = static_cast <RtmptService*> (_self);
    ConnectionData * const conn_data = static_cast <ConnectionData*> (_conn_data);

    logD (rtmpt_service, _func, "closed, conn_data 0x", fmt_hex, (UintPtr) conn_data);

    CodeDepRef<ServerThreadContext> const thread_ctx = conn_data->weak_thread_ctx;

    if (thread_ctx) {
        conn_data->mutex.lock ();
        if (conn_data->pollable_key) {
            thread_ctx->getPollGroup()->removePollable (conn_data->pollable_key);
            conn_data->pollable_key = NULL;
        }
        conn_data->mutex.unlock ();
    }
}

bool
RtmptService::acceptOneConnection ()
{
    Ref<ConnectionData> const conn_data = grab (new (std::nothrow) ConnectionData);
    conn_data->pollable_key = NULL;

    CodeDepRef<ServerThreadContext> const thread_ctx = server_ctx->selectThreadContext ();
    conn_data->weak_thread_ctx = thread_ctx;

    IpAddress client_addr;
    {
	TcpServer::AcceptResult const res = tcp_server.accept (&conn_data->tcp_conn, &client_addr);
	if (res == TcpServer::AcceptResult::Error) {
	    logE_ (_func, exc->toString());
	    return false;
	}

	if (res == TcpServer::AcceptResult::NotAccepted)
	    return false;

	assert (res == TcpServer::AcceptResult::Accepted);
    }

    logD (rtmpt_service, _func, "accepted, conn_data 0x", fmt_hex, (UintPtr) conn_data.ptr());

    Ref<RtmptServer::RtmptConnection> const rtmpt_conn =
            rtmpt_server.addConnection (&conn_data->tcp_conn,
                                        thread_ctx->getDeferredProcessor(),
                                        conn_data /* conn_ref */,
                                        conn_data /* conn_cb_data */,
                                        client_addr);

    conn_data->mutex.lock ();
    PollGroup::PollableKey const pollable_key =
            thread_ctx->getPollGroup()->addPollable (conn_data->tcp_conn.getPollable(),
                                                     true /* activate */);
    conn_data->pollable_key = pollable_key;
    if (!pollable_key) {
	conn_data->mutex.unlock ();
        rtmpt_server.removeConnection (rtmpt_conn);
	logE_ (_func, "PollGroup::addPollable() failed: ", exc->toString ());
	return true;
    }
    conn_data->mutex.unlock ();

    return true;
}

void
RtmptService::accepted (void * const _self)
{
    RtmptService * const self = static_cast <RtmptService*> (_self);

    for (;;) {
	if (!self->acceptOneConnection ())
	    break;
    }
}

mt_const mt_throws Result
RtmptService::init (ServerContext * const mt_nonnull server_ctx,
                    PagePool      * const mt_nonnull page_pool,
                    Time            const session_keepalive_timeout,
		    bool            const no_keepalive_conns,
                    bool            const prechunking_enabled)
{
    this->server_ctx = server_ctx;

    rtmpt_server.init (CbDesc<RtmptServer::Frontend> (&rtmpt_server_frontend, this, getCoderefContainer()),
                       server_ctx->getMainThreadContext()->getTimers(),
                       page_pool,
                       session_keepalive_timeout,
                       // TODO Separate conn_keepalive_timeout
                       session_keepalive_timeout,
                       no_keepalive_conns,
                       prechunking_enabled);

    tcp_server.init (CbDesc<TcpServer::Frontend> (&tcp_server_frontend, this, getCoderefContainer()),
                     server_ctx->getMainThreadContext()->getTimers());

    if (!tcp_server.open ())
	return Result::Failure;

    return Result::Success;
}

mt_throws Result
RtmptService::bind (IpAddress const addr)
{
    if (!tcp_server.bind (addr))
	return Result::Failure;

    return Result::Success;
}

mt_throws Result
RtmptService::start ()
{
    if (!tcp_server.listen ())
	return Result::Failure;

    mutex.lock ();
    assert (!server_pollable_key);
    server_pollable_key =
            server_ctx->getMainThreadContext()->getPollGroup()->addPollable (
                    tcp_server.getPollable());
    if (!server_pollable_key) {
        mutex.unlock ();
	return Result::Failure;
    }
    mutex.unlock ();

    return Result::Success;
}

RtmptService::~RtmptService ()
{
    mutex.lock ();

    if (server_pollable_key) {
        server_ctx->getMainThreadContext()->getPollGroup()->removePollable (server_pollable_key);
        server_pollable_key = NULL;
    }

    // TODO Merge RtmptService and RtmptServer to form a single class "RtmptService".
    //      Then release all active connections here.

    mutex.unlock ();
}

}

