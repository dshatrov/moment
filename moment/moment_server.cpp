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


#include <moment/moment_server.h>


namespace Moment {

using namespace M;

static LogGroup libMary_logGroup_session ("MomentServer.session", LogLevel::I);

MomentServer* MomentServer::instance = NULL;


// __________________________________ Events ___________________________________

void
MomentServer::informDestroy (Events * const events,
                             void   * const cb_data,
                             void   * const /* inform_data */)
{
    events->destroy (cb_data);
}

void
MomentServer::fireDestroy ()
{
    event_informer.informAll (informDestroy, NULL /* inform_cb_data */);
}


// ___________________________ Video stream handlers ___________________________

namespace {
    class InformVideoStreamAdded_Data
    {
    public:
        VideoStream * const video_stream;
        ConstMemory   const stream_name;

        InformVideoStreamAdded_Data (VideoStream * const video_stream,
                                     ConstMemory   const stream_name)
            : video_stream (video_stream),
              stream_name (stream_name)
        {
        }
    };
}

void
MomentServer::informVideoStreamAdded (VideoStreamHandler * const vs_handler,
                                      void               * const cb_data,
                                      void               * const _inform_data)
{
    InformVideoStreamAdded_Data * const inform_data =
            static_cast <InformVideoStreamAdded_Data*> (_inform_data);
    vs_handler->videoStreamAdded (inform_data->video_stream,
                                  inform_data->stream_name,
                                  cb_data);
}

#if 0
void
MomentServer::fireVideoStreamAdded (VideoStream * const mt_nonnull video_stream,
                                    ConstMemory   const stream_name)
{
    InformVideoStreamAdded_Data inform_data (video_stream, stream_name);
    video_stream_informer.informAll (informVideoStreamAdded, &inform_data);
}
#endif

mt_mutex (mutex) void
MomentServer::notifyDeferred_VideoStreamAdded (VideoStream * const mt_nonnull video_stream,
                                               ConstMemory  const stream_name)
{
    VideoStreamAddedNotification * const notification = &vs_added_notifications.appendEmpty ()->data;
    notification->video_stream = video_stream;
    notification->stream_name = grab (new String (stream_name));

    vs_inform_reg.scheduleTask (&vs_added_inform_task, false /* permanent */);
}

bool
MomentServer::videoStreamAddedInformTask (void * const _self)
{
    MomentServer * const self = static_cast <MomentServer*> (_self);

    logD_ (_func_);

    self->mutex.lock ();

    while (!self->vs_added_notifications.isEmpty()) {
        VideoStreamAddedNotification * const notification = &self->vs_added_notifications.getFirst();

        Ref<VideoStream> video_stream;
        video_stream.setNoRef ((VideoStream*) notification->video_stream);
        notification->video_stream.setNoUnref ((VideoStream*) NULL);

        Ref<String> stream_name;
        stream_name.setNoRef ((String*) notification->stream_name);
        notification->stream_name.setNoUnref ((String*) NULL);

        self->vs_added_notifications.remove (self->vs_added_notifications.getFirstElement());

        InformVideoStreamAdded_Data inform_data (video_stream, stream_name->mem());
        mt_unlocks_locks (self->mutex) self->video_stream_informer.informAll_unlocked (informVideoStreamAdded, &inform_data);
    }

    self->mutex.unlock ();

    return false /* Do not reschedule */;
}

MomentServer::VideoStreamHandlerKey
MomentServer::addVideoStreamHandler (CbDesc<VideoStreamHandler> const &vs_handler)
{
    VideoStreamHandlerKey vs_handler_key;
    vs_handler_key.sbn_key = video_stream_informer.subscribe (vs_handler);
    return vs_handler_key;
}

void
MomentServer::removeVideoStreamHandler (VideoStreamHandlerKey vs_handler_key)
{
    video_stream_informer.unsubscribe (vs_handler_key.sbn_key);
}

// _____________________________________________________________________________


namespace {
    class InformRtmpCommandMessage_Data {
    public:
	RtmpConnection       * const conn;
	VideoStream::Message * const msg;
	ConstMemory const    &method_name;
	AmfDecoder           * const amf_decoder;

	InformRtmpCommandMessage_Data (RtmpConnection       * const conn,
				       VideoStream::Message * const msg,
				       ConstMemory const    &method_name,
				       AmfDecoder           * const amf_decoder)
	    : conn (conn),
	      msg (msg),
	      method_name (method_name),
	      amf_decoder (amf_decoder)
	{
	}
    };
}

void
MomentServer::ClientSession::informRtmpCommandMessage (Events * const events,
						       void   * const cb_data,
						       void   * const _inform_data)
{
    InformRtmpCommandMessage_Data * const inform_data =
	    static_cast <InformRtmpCommandMessage_Data*> (_inform_data);
    events->rtmpCommandMessage (inform_data->conn,
				inform_data->msg,
				inform_data->method_name,
				inform_data->amf_decoder,
				cb_data);
}

void
MomentServer::ClientSession::informClientDisconnected (Events * const events,
						       void   * const cb_data,
						       void   * const /* _inform_data */)
{
    events->clientDisconnected (cb_data);
}

bool
MomentServer::ClientSession::isConnected_subscribe (CbDesc<Events> const &cb)
{
    mutex.lock ();
    if (disconnected) {
	mutex.unlock ();
	return false;
    }
    event_informer.subscribe_unlocked (cb);
    mutex.unlock ();
    return true;
}

void
MomentServer::ClientSession::setBackend (CbDesc<Backend> const &cb)
{
    backend = cb;
}

void
MomentServer::ClientSession::fireRtmpCommandMessage (RtmpConnection       * const mt_nonnull conn,
						     VideoStream::Message * const mt_nonnull msg,
						     ConstMemory const    &method_name,
						     AmfDecoder           * const mt_nonnull amf_decoder)
{
    InformRtmpCommandMessage_Data inform_data (conn, msg, method_name, amf_decoder);
    event_informer.informAll (informRtmpCommandMessage, &inform_data);
}

void
MomentServer::ClientSession::clientDisconnected ()
{
    mutex.lock ();
    disconnected = true;
    // To avoid races, we defer invocation of "client disconnected" callbacks
    // until all "client connected" callbacks are called.
    if (!processing_connected_event)
	event_informer.informAll_unlocked (informClientDisconnected, NULL /* inform_data */);
    mutex.unlock ();
}

MomentServer::ClientSession::ClientSession ()
    : disconnected (false),
      event_informer (this, &mutex)
{
    logD (session, _func, "0x", fmt_hex, (UintPtr) this);
}

MomentServer::ClientSession::~ClientSession ()
{
    logD (session, _func, "0x", fmt_hex, (UintPtr) this);
}

namespace {
    class InformClientConnected_Data {
    public:
	MomentServer::ClientSession * const client_session;
	ConstMemory const &app_name;
	ConstMemory const &full_app_name;

	InformClientConnected_Data (MomentServer::ClientSession * const client_session,
				    ConstMemory const &app_name,
				    ConstMemory const &full_app_name)
	    : client_session (client_session),
	      app_name (app_name),
	      full_app_name (full_app_name)
	{
	}
    };
}

void
MomentServer::ClientEntry::informClientConnected (ClientHandler * const client_handler,
						  void          * const cb_data,
						  void          * const _inform_data)
{
    InformClientConnected_Data * const inform_data =
	    static_cast <InformClientConnected_Data*> (_inform_data);
    client_handler->clientConnected (inform_data->client_session,
				     inform_data->app_name,
				     inform_data->full_app_name,
				     cb_data);
}

void
MomentServer::ClientEntry::fireClientConnected (ClientSession     * const client_session,
						ConstMemory const &app_name,
						ConstMemory const &full_app_name)
{
    InformClientConnected_Data inform_data (client_session, app_name, full_app_name);
    event_informer.informAll (informClientConnected, &inform_data);
}

// @ret_path_tail should contain original path.
mt_mutex (mutex) MomentServer::ClientEntry*
MomentServer::getClientEntry_rec (ConstMemory   const path,
				  ConstMemory * const mt_nonnull ret_path_tail,
				  Namespace   * const mt_nonnull nsp)
{
    Byte const *delim = (Byte const *) memchr (path.mem(), '/', path.len());
    ConstMemory next_name;
    if (delim) {
	next_name = path.region (0, delim - path.mem());
	Namespace::NamespaceHash::EntryKey const next_nsp_key = nsp->namespace_hash.lookup (next_name);
	if (next_nsp_key) {
	    Namespace * const next_nsp = next_nsp_key.getData();
	    ClientEntry * const client_entry = getClientEntry_rec (path.region (delim - path.mem() + 1),
								   ret_path_tail,
								   next_nsp);
	    if (client_entry)
		return client_entry;
	}
    } else {
	next_name = path;
    }

    Namespace::ClientEntryHash::EntryKey const client_entry_key = nsp->client_entry_hash.lookup (next_name);
    if (!client_entry_key)
	return NULL;

    *ret_path_tail = (*ret_path_tail).region ((next_name.mem() + next_name.len()) - (*ret_path_tail).mem());
    return client_entry_key.getData();
}

mt_mutex (mutex) MomentServer::ClientEntry*
MomentServer::getClientEntry (ConstMemory  path,
			      ConstMemory * const mt_nonnull ret_path_tail,
			      Namespace   * const mt_nonnull nsp)
{
    if (path.len() > 0 && path.mem() [0] == '/')
	path = path.region (1);

    *ret_path_tail = path;
    return getClientEntry_rec (path, ret_path_tail, nsp);
}

mt_throws Result
MomentServer::loadModules ()
{
    ConstMemory module_path = config->getString ("moment/module_path");
    if (module_path.len() == 0)
	module_path = LIBMOMENT_PREFIX "/moment-1.0";

    logD_ (_func, "module_path: ", module_path);

    Ref<Vfs> const vfs = Vfs::createDefaultLocalVfs (module_path);
    if (!vfs)
	return Result::Failure;

    Ref<Vfs::Directory> const dir = vfs->openDirectory (ConstMemory());
    if (!dir)
	return Result::Failure;

    StringHash<EmptyBase> loaded_names;

    Ref<String> const mod_gst_name = makeString (module_path, "/libmoment-gst-1.0");

    for (;;) {
	Ref<String> dir_entry;
	if (!dir->getNextEntry (dir_entry))
	    return Result::Failure;
	if (!dir_entry)
	    break;

	Ref<String> const stat_path = makeString (module_path, "/", dir_entry->mem());
	ConstMemory const entry_name = stat_path->mem();

	Ref<Vfs::FileStat> const stat_data = vfs->stat (dir_entry->mem());
	if (!stat_data) {
	    logE_ (_func, "Could not stat ", stat_path);
	    continue;
	}

	// TODO Find rightmost slash, then skip one dot.
	ConstMemory module_name = entry_name;
	{
	    void *dot_ptr = memchr ((void*) entry_name.mem(), '.', entry_name.len());
	    // XXX Dirty.
	    // Skipping the first dot (belongs to "moment-1.0" substring).
	    if (dot_ptr)
		dot_ptr = memchr ((void*) ((Byte const *) dot_ptr + 1), '.', entry_name.len() - ((Byte const *) dot_ptr - entry_name.mem()) - 1);
	    // Skipping the second dot (-1.0 in library version).
	    if (dot_ptr)
		dot_ptr = memchr ((void*) ((Byte const *) dot_ptr + 1), '.', entry_name.len() - ((Byte const *) dot_ptr - entry_name.mem()) - 1);
#ifdef LIBMARY_PLATFORM_WIN32
// TEST: skipping the third dot.
// TODO The dots should be skipped from the end of the string!
	    if (dot_ptr)
		dot_ptr = memchr ((void*) ((Byte const *) dot_ptr + 1), '.', entry_name.len() - ((Byte const *) dot_ptr - entry_name.mem()) - 1);
#endif

	    if (dot_ptr)
		module_name = entry_name.region (0, (Byte const *) dot_ptr - entry_name.mem());
	}

        if (equal (module_name, mod_gst_name->mem()))
            continue;

	if (stat_data->file_type == Vfs::FileType::RegularFile &&
	    !loaded_names.lookup (module_name))
	{
	    loaded_names.add (module_name, EmptyBase());

	    logD_ (_func, "loading module ", module_name);

	    if (!loadModule (module_name))
		logE_ (_func, "Could not load module ", module_name, ": ", exc->toString());
	}
    }

    {
      // Loading mod_gst first, so that it deinitializes first.
      // We count on the fact that M::Informer prepends new subscribers
      // to the beginning of subscriber list, which is hacky, because
      // M::Informer has no explicit guarantees for that.
      //
      // This is important for proper deinitialization. Ideally, the order
      // of module deinitialization should not matter.
      // The process of deinitialization needs extra though.

	assert (!loaded_names.lookup (mod_gst_name->mem()));
        loaded_names.add (mod_gst_name->mem(), EmptyBase());

        logD_ (_func, "loading module (forced last) ", mod_gst_name);
        if (!loadModule (mod_gst_name->mem()))
            logE_ (_func, "Could not load module ", mod_gst_name, ": ", exc->toString());
    }

#ifdef LIBMARY_PLATFORM_WIN32
    {
        if (!loadModule ("C:/MinGW/msys/1.0/opt/moment/lib/bin/libmoment-file-1.0-0.dll"))
            logE_ (_func, "Could not load mod_file (win32)");

        if (!loadModule ("C:/MinGW/msys/1.0/opt/moment/lib/bin/libmoment-gst-1.0-0.dll"))
            logE_ (_func, "Could not load mod_gst (win32)");

        if (!loadModule ("C:/MinGW/msys/1.0/opt/moment/lib/bin/libmoment-rtmp-1.0-0.dll"))
            logE_ (_func, "Could not load mod_rtmp (win32)");

        if (!loadModule ("C:/MinGW/msys/1.0/opt/moment/lib/bin/libmoment-mychat-1.0-0.dll"))
            logE_ (_func, "Could not load mychat module (win32)");

        if (!loadModule ("C:/MinGW/msys/1.0/opt/moment/lib/bin/libmoment-test-1.0-0.dll"))
            logE_ (_func, "Could not load mychat module (win32)");

        if (!loadModule ("C:/MinGW/msys/1.0/opt/moment/lib/bin/libmoment-lectorium-1.0-0.dll"))
            logE_ (_func, "Could not load lectorium (win32)");
    }
#endif

    return Result::Success;
}

ServerApp*
MomentServer::getServerApp ()
{
    return server_app;
}

PagePool*
MomentServer::getPagePool ()
{
    return page_pool;
}

HttpService*
MomentServer::getHttpService ()
{
    return http_service;
}

HttpService*
MomentServer::getAdminHttpService ()
{
    return admin_http_service;
}

MConfig::Config* 
MomentServer::getConfig ()
{
    return config;
}

ServerThreadPool*
MomentServer::getRecorderThreadPool ()
{
    return recorder_thread_pool;
}

Storage*
MomentServer::getStorage ()
{
    return storage;
}

MomentServer*
MomentServer::getInstance ()
{
    return instance;
}

Ref<MomentServer::ClientSession>
MomentServer::rtmpClientConnected (ConstMemory const &path,
				   RtmpConnection    * const mt_nonnull conn,
				   IpAddress   const &client_addr)
{
    logD (session, _func_);

    ConstMemory path_tail;

    mutex.lock ();
    Ref<ClientEntry> const client_entry = getClientEntry (path, &path_tail, &root_namespace);

    Ref<ClientSession> const client_session = grab (new ClientSession);
    client_session->weak_rtmp_conn = conn;
    client_session->unsafe_rtmp_conn = conn;
    client_session->client_addr = client_addr;
    client_session->processing_connected_event = true;

    client_session_list.append (client_session);
    client_session->ref ();
    mutex.unlock ();

    logD (session, _func, "client_session refcount before: ", client_session->getRefCount());
    if (client_entry)
	client_entry->fireClientConnected (client_session, path_tail, path);

    client_session->mutex.lock ();
    client_session->processing_connected_event = false;
    if (client_session->disconnected) {
	client_session->event_informer.informAll_unlocked (client_session->informClientDisconnected, NULL /* inform_data */);
	client_session->mutex.unlock ();
	return NULL;
    }
    client_session->mutex.unlock ();

    logD (session, _func, "client_session refcount after: ", client_session->getRefCount());
    return client_session;
}

void
MomentServer::clientDisconnected (ClientSession * const mt_nonnull client_session)
{
    logD (session, _func, "client_session refcount before: ", client_session->getRefCount());

    client_session->clientDisconnected ();

    mutex.lock ();
    if (client_session->video_stream_key)
	removeVideoStream_unlocked (client_session->video_stream_key);

    client_session_list.remove (client_session);
    mutex.unlock ();

    logD (session, _func, "client_session refcount after: ", client_session->getRefCount());
    client_session->unref ();
}

void
MomentServer::disconnect (ClientSession * const mt_nonnull client_session)
{
    CodeRef rtmp_conn_ref;
    RtmpConnection * const rtmp_conn = client_session->getRtmpConnection (&rtmp_conn_ref);
    if (!rtmp_conn)
	return;

    rtmp_conn->closeAfterFlush ();
}

Ref<VideoStream>
MomentServer::startWatching (ClientSession * const mt_nonnull client_session,
			     ConstMemory     const stream_name)
{
    Ref<VideoStream> video_stream;

    if (client_session->backend
	&& client_session->backend->startWatching)
    {
	logD (session, _func, "calling backend->startWatching()");
	if (!client_session->backend.call_ret< Ref<VideoStream> > (&video_stream,
								   client_session->backend->startWatching,
								   /* ( */ stream_name /* ) */))
	{
	    goto _not_found;
	}

	goto _return;
    }

    logD (session, _func, "default path");

    video_stream = getVideoStream (stream_name);
    if (!video_stream)
	goto _not_found;

_return:
    logA_ ("moment OK ", client_session->client_addr, " watch ", stream_name);
    return video_stream;

_not_found:
    logA_ ("moment NOT_FOUND ", client_session->client_addr, " watch ", stream_name);
    return NULL;
}

Ref<VideoStream>
MomentServer::startStreaming (ClientSession * const mt_nonnull client_session,
			      ConstMemory     const stream_name,
			      RecordingMode   const rec_mode)
{
    Ref<VideoStream> video_stream;

  {
    if (client_session->backend
	&& client_session->backend->startStreaming)
    {
	logD (session, _func, "calling backend->startStreaming()");
	if (!client_session->backend.call_ret< Ref<VideoStream> > (&video_stream,
								   client_session->backend->startStreaming,
								   /* ( */ stream_name, rec_mode /* ) */))
	{
	    goto _denied;
	}

	goto _return;
    }

    logD (session, _func, "default path");

    if (!publish_all_streams)
	goto _denied;

    video_stream = grab (new VideoStream);
    VideoStreamKey const video_stream_key = addVideoStream (video_stream, stream_name);

    client_session->mutex.lock ();
    if (client_session->disconnected) {
	client_session->mutex.unlock ();
	removeVideoStream (video_stream_key);
	goto _denied;
    }
    client_session->video_stream_key = video_stream_key;
    client_session->mutex.unlock ();
  }

_return:
    logA_ ("moment OK ", client_session->client_addr, " stream ", stream_name);
    return video_stream;

_denied:
    logA_ ("moment DENIED ", client_session->client_addr, " stream ", stream_name);
    return NULL;
}

mt_mutex (mutex) MomentServer::ClientHandlerKey
MomentServer::addClientHandler_rec (CbDesc<ClientHandler> const &cb,
				    ConstMemory const           &path_,
				    Namespace                   * const nsp)
{
  // This code is pretty much like M::HttpService::addHttpHandler_rec()

    ConstMemory path = path_;
    if (path.len() > 0 && path.mem() [0] == '/')
	path = path.region (1);

    Byte const *delim = (Byte const *) memchr (path.mem(), '/', path.len());
    if (!delim) {
	ClientEntry *client_entry;
	{
	    Namespace::ClientEntryHash::EntryKey client_entry_key = nsp->client_entry_hash.lookup (path);
	    if (!client_entry_key) {
		Ref<ClientEntry> new_entry = grab (new ClientEntry);
		new_entry->parent_nsp = nsp;
		new_entry->client_entry_key = nsp->client_entry_hash.add (path, new_entry);
		client_entry = new_entry;
	    } else {
		client_entry = client_entry_key.getData();
	    }
	}

	ClientHandlerKey client_handler_key;
	client_handler_key.client_entry = client_entry;
	client_handler_key.sbn_key = client_entry->event_informer.subscribe (cb);
	return client_handler_key;
    }

    ConstMemory const next_nsp_name = path.region (0, delim - path.mem());
    Namespace::NamespaceHash::EntryKey const next_nsp_key =
	    nsp->namespace_hash.lookup (next_nsp_name);
    Namespace *next_nsp;
    if (next_nsp_key) {
	next_nsp = next_nsp_key.getData();
    } else {
	Ref<Namespace> const new_nsp = grab (new Namespace);
	new_nsp->parent_nsp = nsp;
	new_nsp->namespace_hash_key = nsp->namespace_hash.add (next_nsp_name, new_nsp);
	next_nsp = new_nsp;
    }

    return addClientHandler_rec (cb, path.region (delim - path.mem() + 1), next_nsp);
}

MomentServer::ClientHandlerKey
MomentServer::addClientHandler (CbDesc<ClientHandler> const &cb,
				ConstMemory           const &path)
{
    mutex.lock ();
    ClientHandlerKey const client_handler_key = addClientHandler_rec (cb, path, &root_namespace);
    mutex.unlock ();
    return client_handler_key;
}

void
MomentServer::removeClientHandler (ClientHandlerKey const client_handler_key)
{
    mutex.lock ();

    Ref<ClientEntry> const client_entry = client_handler_key.client_entry;

    bool remove_client_entry = false;
    client_entry->mutex.lock ();
//#error Initially, this should have been unsubscribe_unlocked()
    client_entry->event_informer.unsubscribe (client_handler_key.sbn_key);
//#error Informer's mutex is not held here!
//#warning This condition looks strange.
    if (!client_entry->event_informer.gotSubscriptions_unlocked ()) {
	remove_client_entry = true;
    }
    client_entry->mutex.unlock ();

    if (remove_client_entry) {
	client_entry->parent_nsp->client_entry_hash.remove (client_entry->client_entry_key);

	{
	    Namespace *nsp = client_entry->parent_nsp;
	    while (nsp && nsp->parent_nsp) {
		if (nsp->client_entry_hash.isEmpty() &&
		    nsp->namespace_hash.isEmpty())
		{
		    Namespace * const tmp_nsp = nsp;
		    nsp = nsp->parent_nsp;
		    nsp->namespace_hash.remove (tmp_nsp->namespace_hash_key);
		}
	    }
	}
    }

    mutex.unlock ();
}

Ref<VideoStream>
MomentServer::getVideoStream (ConstMemory const &path)
{
  StateMutexLock l (&mutex);

    VideoStreamHash::EntryKey const entry = video_stream_hash.lookup (path);
    if (!entry)
	return NULL;

    return entry.getData().video_stream;
}

MomentServer::VideoStreamKey
MomentServer::addVideoStream (VideoStream * const  video_stream,
			      ConstMemory   const &path)
{
  StateMutexLock l (&mutex);

    MomentServer::VideoStreamKey const vs_key = video_stream_hash.add (path, video_stream);
    notifyDeferred_VideoStreamAdded (video_stream, path);
    return vs_key;
}

mt_mutex (mutex) void
MomentServer::removeVideoStream_unlocked (VideoStreamKey const video_stream_key)
{
    video_stream_hash.remove (video_stream_key.entry_key);
}

void
MomentServer::removeVideoStream (VideoStreamKey const video_stream_key)
{
    mutex.lock ();
// TODO Is deferred notification about stream removal necessary?
    removeVideoStream_unlocked (video_stream_key);
    mutex.unlock ();
}

Ref<VideoStream>
MomentServer::getMixVideoStream ()
{
    return mix_video_stream;
}

namespace {
    class InformPageRequest_Data {
    public:
	MomentServer::PageRequest * const page_req;
	ConstMemory const path;
	ConstMemory const full_path;

	MomentServer::PageRequestResult result;

	InformPageRequest_Data (MomentServer::PageRequest * const page_req,
				ConstMemory const path,
				ConstMemory const full_path)
	    : page_req (page_req),
	      path (path),
	      full_path (full_path),
	      result (MomentServer::PageRequestResult::Success)
	{
	}
    };
}

void
MomentServer::PageRequestHandlerEntry::informPageRequest (PageRequestHandler * const handler,
							  void               * const cb_data,
							  void               * const _inform_data)
{
    InformPageRequest_Data * const inform_data =
	    static_cast <InformPageRequest_Data*> (_inform_data);

    PageRequestResult const res = handler->pageRequest (inform_data->page_req,
							inform_data->path,
							inform_data->full_path,
							cb_data);
    if (inform_data->result == PageRequestResult::Success)
	inform_data->result = res;
}

MomentServer::PageRequestResult
MomentServer::PageRequestHandlerEntry::firePageRequest (PageRequest * const page_req,
							ConstMemory   const path,
							ConstMemory   const full_path)
{
    InformPageRequest_Data inform_data (page_req, path, full_path);
    event_informer.informAll (informPageRequest, &inform_data);
    return inform_data.result;
}

MomentServer::PageRequestHandlerKey
MomentServer::addPageRequestHandler (CbDesc<PageRequestHandler> const &cb,
				     ConstMemory path)
{
    PageRequestHandlerEntry *handler_entry;
    GenericInformer::SubscriptionKey sbn_key;

    mutex.lock ();

    PageRequestHandlerHash::EntryKey const hash_key = page_handler_hash.lookup (path);
    if (hash_key) {
	handler_entry = hash_key.getData();
    } else {
	handler_entry = new PageRequestHandlerEntry;
	handler_entry->hash_key = page_handler_hash.add (path, handler_entry);
    }

    sbn_key = handler_entry->event_informer.subscribe (cb);

    ++handler_entry->num_handlers;

    mutex.unlock ();

    PageRequestHandlerKey handler_key;
    handler_key.handler_entry = handler_entry;
    handler_key.sbn_key = sbn_key;

    return handler_key;
}

void
MomentServer::removePageRequestHandler (PageRequestHandlerKey handler_key)
{
    PageRequestHandlerEntry * const handler_entry = handler_key.handler_entry;

    mutex.lock ();

    handler_entry->event_informer.unsubscribe (handler_key.sbn_key);
    --handler_entry->num_handlers;
    if (handler_entry->num_handlers == 0)
	page_handler_hash.remove (handler_entry->hash_key);

    mutex.unlock ();
}

MomentServer::PageRequestResult
MomentServer::processPageRequest (PageRequest * const page_req,
				  ConstMemory   const path)
{
    mutex.lock ();

    PageRequestHandlerHash::EntryKey const hash_key = page_handler_hash.lookup (path);
    if (!hash_key) {
	mutex.unlock ();
	return PageRequestResult::Success;
    }

    Ref<PageRequestHandlerEntry> const handler = hash_key.getData();

    mutex.unlock ();

    PageRequestResult const res = handler->firePageRequest (page_req,
							    path,
							    path /* full_path */);

    return res;
}

void
MomentServer::addPushProtocol (ConstMemory    const protocol_name,
                               PushProtocol * const mt_nonnull push_protocol)
{
    mutex.lock ();
    push_protocol_hash.add (protocol_name, push_protocol);
    mutex.unlock ();
}

Ref<PushProtocol>
MomentServer::getPushProtocolForUri (ConstMemory const uri)
{
    ConstMemory protocol_name;
    {
        Count i = 0;
        for (Count const i_end = uri.len(); i < i_end; ++i) {
            if (uri.mem() [i] == ':')
                break;
        }

        protocol_name = uri.region (0, i);
    }

    Ref<PushProtocol> push_protocol;
    {
        mutex.lock ();

        PushProtocolHash::EntryKey const push_protocol_key = push_protocol_hash.lookup (protocol_name);
        if (push_protocol_key)
            push_protocol = push_protocol_key.getData();

        mutex.unlock ();
    }

    if (!push_protocol) {
        logE_ (_func, "Push protocol not found: ", protocol_name);
        return NULL;
    }

    return push_protocol;
}

Ref<PushConnection>
MomentServer::createPushConnection (VideoStream * const video_stream,
                                    ConstMemory   const uri,
                                    ConstMemory   const username,
                                    ConstMemory   const password)
{
    Ref<PushProtocol> const push_protocol = getPushProtocolForUri (uri);
    if (!push_protocol)
        return NULL;

    return push_protocol->connect (video_stream, uri, username, password);
}

void
MomentServer::dumpStreamList ()
{
    log__ (_func_);

    {
      StateMutexLock l (&mutex);

	VideoStreamHash::iter iter (video_stream_hash);
	while (!video_stream_hash.iter_done (iter)) {
	    VideoStreamHash::EntryKey const entry = video_stream_hash.iter_next (iter);
	    log__ (_func, "    ", entry.getKey());
	}
    }

    log__ (_func_, "done");
}

Result
MomentServer::init (ServerApp        * const mt_nonnull server_app,
		    PagePool         * const mt_nonnull page_pool,
		    HttpService      * const mt_nonnull http_service,
		    HttpService      * const mt_nonnull admin_http_service,
		    MConfig::Config  * const mt_nonnull config,
		    ServerThreadPool * const mt_nonnull recorder_thread_pool,
		    Storage          * const mt_nonnull storage)
{
    this->server_app = server_app;
    this->page_pool = page_pool;
    this->http_service = http_service;
    this->admin_http_service = admin_http_service;
    this->config = config;
    this->recorder_thread_pool = recorder_thread_pool;
    this->storage = storage;

    mix_video_stream = grab (new VideoStream);

    vs_inform_reg.setDeferredProcessor (server_app->getMainThreadContext()->getDeferredProcessor());

    {
	ConstMemory const opt_name = "moment/publish_all";
	MConfig::BooleanValue const value = config->getBoolean (opt_name);
	if (value == MConfig::Boolean_Invalid) {
	    logE_ (_func, "Invalid value for ", opt_name, ": ", config->getString (opt_name),
		   ", assuming \"", publish_all_streams, "\"");
	} else {
	    if (value == MConfig::Boolean_False)
		publish_all_streams = false;
	    else
		publish_all_streams = true;

	    logD_ (_func, opt_name, ": ", publish_all_streams);
	}
    }

    if (!loadModules ())
	logE_ (_func, "Could not load modules");

    return Result::Success;
}

MomentServer::MomentServer ()
    : event_informer (NULL /* coderef_container */, &mutex),
      video_stream_informer (NULL /* coderef_container */, &mutex),
      server_app (NULL),
      page_pool (NULL),
      http_service (NULL),
      config (NULL),
      recorder_thread_pool (NULL),
      storage (NULL),
      publish_all_streams (true)
{
    instance = this;

    vs_added_inform_task.cb = CbDesc<DeferredProcessor::TaskCallback> (
            videoStreamAddedInformTask, this /* cb_data */, NULL /* coderef_container */);
}

MomentServer::~MomentServer ()
{
    logH_ (_func_);

    mutex.lock ();
    mutex.unlock ();

    vs_inform_reg.release ();

    fireDestroy ();

    if (server_app)
        server_app->release ();
}

}

