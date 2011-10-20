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


#include <moment/rtmpt_service.h>


using namespace M;

namespace Moment {

RtmptServer::Frontend RtmptService::rtmpt_server_frontend = {
    clientConnected,
    connectionClosed
};

TcpServer::Frontend RtmptService::tcp_server_frontend = {
    accepted
};

Result
RtmptService::clientConnected (RtmpConnection * const mt_nonnull rtmp_conn,
			       void           * const _self)
{
    RtmptService * const self = static_cast <RtmptService*> (_self);

    Result res;
    if (!self->frontend.call_ret<Result> (&res, self->frontend->clientConnected, /*(*/ rtmp_conn /*)*/)
	|| !res)
    {
	return Result::Failure;
    }

    return Result::Success;
}

void
RtmptService::connectionClosed (void * const _conn_data,
				void * const _self)
{
    RtmptService * const self = static_cast <RtmptService*> (_self);
    ConnectionData * const conn_data = static_cast <ConnectionData*> (_conn_data);

    conn_data->mutex.lock ();

    if (conn_data->pollable_key)
	self->poll_group->removePollable (conn_data->pollable_key);

    conn_data->mutex.unlock ();
}

bool
RtmptService::acceptOneConnection ()
{
    logD_ (_func_);

    TcpConnection * const tcp_conn = new TcpConnection (NULL /* coderef_container */);
    assert (tcp_conn);
    {
	TcpServer::AcceptResult const res = tcp_server.accept (tcp_conn);
	if (res == TcpServer::AcceptResult::Error) {
	    delete tcp_conn;
	    logE_ (_func, exc->toString());
	    return false;
	}

	if (res == TcpServer::AcceptResult::NotAccepted) {
	    delete tcp_conn;
	    return false;
	}

	assert (res == TcpServer::AcceptResult::Accepted);
	logD_ (_func, "Connection accepted");
    }

    Ref<ConnectionData> const conn_data = grab (new ConnectionData);

    rtmpt_server.addConnection (tcp_conn,
				tcp_conn  /* dep_code_referenced */,
				conn_data /* conn_cb_data */,
				conn_data /* ref_data */);

    conn_data->mutex.lock ();
    PollGroup::PollableKey const pollable_key = poll_group->addPollable (tcp_conn->getPollable(), NULL /* ret_reg */);
    conn_data->pollable_key = pollable_key;
    if (!pollable_key) {
	conn_data->mutex.unlock ();
	// TODO FIXME Remove the connection from rtmpt_server.

	delete tcp_conn;
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

    logD_ (_func_);

    for (;;) {
	if (!self->acceptOneConnection ())
	    break;
    }
}

mt_throws Result
RtmptService::init (Time const session_keepalive_timeout,
		    bool const no_keepalive_conns)
{
    rtmpt_server.init (session_keepalive_timeout, no_keepalive_conns);

    if (!tcp_server.open ())
	return Result::Failure;

    rtmpt_server.setFrontend (Cb<RtmptServer::Frontend> (&rtmpt_server_frontend, this, getCoderefContainer()));

    tcp_server.setFrontend (Cb<TcpServer::Frontend> (&tcp_server_frontend, this, getCoderefContainer()));

    return Result::Success;
}

mt_throws Result
RtmptService::bind (IpAddress const &addr)
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

    if (!poll_group->addPollable (tcp_server.getPollable(), NULL /* ret_reg */))
	return Result::Failure;

    return Result::Success;
}

// TODO Remove tcp_server pollable from poll group in destructor.

}

