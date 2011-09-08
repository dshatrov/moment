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

RtmpService::ClientSession::~ClientSession ()
{
//    logD_ (_func_);
}

mt_mutex (mutex) void
RtmpService::destroySession (ClientSession * const session)
{
    logD (rtmp_service, _func, "session: 0x", fmt_hex, (UintPtr) session);

    if (!session->valid) {
	logD (rtmp_service, _func, "invalid session");
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

// TEST
//    session->traceReferences ();

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
    // TEST (uncomment)
//    session->conn_sender.setDeferredRegistration (&session->deferred_reg);

    session->rtmp_conn.setBackend (Cb<RtmpConnection::Backend> (&rtmp_conn_backend, session, session));
    session->rtmp_conn.setSender (&session->conn_sender);

    session->conn_receiver.setFrontend (session->rtmp_conn.getReceiverFrontend());

    {
	Result res;
	if (!frontend.call_ret (&res, frontend->clientConnected, /*(*/ /* session, */ &session->rtmp_conn /*)*/)
	    || !res)
	{
	    return true;
	}
    }

    mutex.lock ();
    session->pollable_key = poll_group->addPollable (session->tcp_conn.getPollable(),
						     &session->deferred_reg);
    if (!session->pollable_key) {
	mutex.unlock ();

	// TODO FIXME Call clientDisconnected()

	logE (rtmp_service, _func, "PollGroup::addPollable() failed: ", exc->toString());
	return true;
    }

    session_list.append (session);
    mutex.unlock ();
    session->ref ();
  // The session is fully initialized and should be destroyed with
  // destroySession() when necessary.

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

