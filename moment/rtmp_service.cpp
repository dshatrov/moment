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


#include <moment/rtmp_service.h>


using namespace M;

namespace Moment {

namespace {
LogGroup libMary_logGroup_rtmp_service ("rtmp_service", LogLevel::N);
}

RtmpConnection::Backend RtmpService::rtmp_conn_backend = {
    closeRtmpConn
};

TcpServer::Frontend RtmpService::tcp_server_frontend = {
    accepted
};

// Either 'mutex' must be locked when calling this method, or we must be sure
// that object's state is valid for the current thread
// (ses acceptOneConnection()).
void
RtmpService::destroySession (ClientSession * const session)
{
    logD (rtmp_service, _func, "session: 0x", fmt_hex, (UintPtr) session);

    if (!session->valid) {
	return;
    }
    session->valid = false;

    poll_group->removePollable (session->pollable_key);

    session->deferred_reg.release ();

    // TODO close TCP connection explicitly.

    session_list.remove (session);
    logD (rtmp_service, _func, "session refcount: ", session->getRefCount());
    session->unref ();
}

bool
RtmpService::acceptOneConnection ()
{
    logD (rtmp_service, _func_);

    Ref<ClientSession> session = grab (new ClientSession (timers, page_pool));
    session->valid = true;
    session->weak_rtmp_service = this;
    session->unsafe_rtmp_service = this;

    {
	TcpServer::AcceptResult const res = tcp_server.accept (&session->tcp_conn);
	if (res == TcpServer::AcceptResult::Error) {
	    logE (rtmp_service, _func, "accept() failed: ", exc->toString());
	    return false;
	}

	if (res == TcpServer::AcceptResult::NotAccepted)
	    return false;

	assert (res == TcpServer::AcceptResult::Accepted);
    }

    session->conn_sender.setConnection (&session->tcp_conn);
    session->conn_sender.setDeferredRegistration (&session->deferred_reg);

    session->rtmp_conn.setBackend (Cb<RtmpConnection::Backend> (&rtmp_conn_backend, session, session));
    session->rtmp_conn.setSender (&session->conn_sender);

    session->conn_receiver.setFrontend (session->rtmp_conn.getReceiverFrontend());

    if (!(session->pollable_key = poll_group->addPollable (session->tcp_conn.getPollable(), &session->deferred_reg))) {
	logE (rtmp_service, _func, "PollGroup::addPollable() failed: ", exc->toString());
	return true;
    }

    mutex.lock ();
    session_list.append (session);
    mutex.unlock ();
    session->ref ();
  // The session is fully initialized and should be destroyed with
  // destroySession() when necessary.

    {
	Result res;
	if (!frontend.call_ret (&res, frontend->clientConnected, /*(*/ /* session, */ &session->rtmp_conn /*)*/)
	    || !res)
	{
	    mutex.lock ();
	    destroySession (session);
	    mutex.unlock ();
	}
    }

    logD (rtmp_service, _func, "done");

    return true;
}

void
RtmpService::closeRtmpConn (void * const _session)
{
    logD (rtmp_service, _func, "session 0x", fmt_hex, (UintPtr) _session);

    ClientSession * const session = static_cast <ClientSession*> (_session);

    CodeRef self_ref;
    if (session->weak_rtmp_service.isValid()) {
	self_ref = session->weak_rtmp_service;
	if (!self_ref) {
	    logD_ (_func, "self gone");
	    return;
	}
    }
    RtmpService * const self = session->unsafe_rtmp_service;

    self->mutex.lock ();
    self->destroySession (session);
    self->mutex.unlock ();
}

void
RtmpService::accepted (void * const _self)
{
    RtmpService * const self = static_cast <RtmpService*> (_self);

    for (;;) {
	if (!self->acceptOneConnection ())
	    break;
    }
}

mt_throws Result
RtmpService::init ()
{
    if (!tcp_server.open ())
	return Result::Failure;

    tcp_server.setFrontend (Cb<TcpServer::Frontend> (&tcp_server_frontend, this, getCoderefContainer()));

    return Result::Success;
}

mt_throws Result
RtmpService::bind (IpAddress const &addr)
{
    if (!tcp_server.bind (addr))
	return Result::Failure;

    return Result::Success;
}

mt_throws Result
RtmpService::start ()
{
    if (!tcp_server.listen ())
	return Result::Failure;

    if (!poll_group->addPollable (tcp_server.getPollable(), NULL /* ret_reg */))
	return Result::Failure;

    return Result::Success;
}

RtmpService::~RtmpService ()
{
    mutex.lock ();

    SessionList::iter iter (session_list);
    while (!session_list.iter_done (iter)) {
	ClientSession * const session = session_list.iter_next (iter);
	destroySession (session);
    }

    mutex.unlock ();
}

}

