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
RtmptService::connectionClosed (void * const _pollable_key,
				void * const _self)
{
    RtmptService * const self = static_cast <RtmptService*> (_self);
    PollGroup::PollableKey const pollable_key = static_cast <PollGroup::PollableKey> (_pollable_key);

    self->poll_group->removePollable (pollable_key);
}

bool
RtmptService::acceptOneConnection ()
{
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
    }

    PollGroup::PollableKey pollable_key = poll_group->addPollable (tcp_conn->getPollable());
    if (!pollable_key) {
	delete tcp_conn;
	logE_ (_func, "PollGroup::addPollable() failed: ", exc->toString ());
	return true;
    }

    rtmpt_server.addConnection (tcp_conn, tcp_conn /* dep_code_referenced */, pollable_key /* conn_cb_data */);

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

mt_throws Result
RtmptService::init ()
{
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

    if (!poll_group->addPollable (tcp_server.getPollable()))
	return Result::Failure;

    return Result::Success;
}

// TODO Remove tcp_server pollable from poll group in destructor.

}

