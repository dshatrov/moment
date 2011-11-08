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

MomentServer* MomentServer::instance = NULL;

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
    logD_ (_func, "0x", fmt_hex, (UintPtr) this);
}

MomentServer::ClientSession::~ClientSession ()
{
    logD_ (_func, "0x", fmt_hex, (UintPtr) this);
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

    logD_ (_func, "MODULE PATH: ", module_path);

    Ref<Vfs> const vfs = Vfs::createDefaultLocalVfs (module_path);
    if (!vfs)
	return Result::Failure;

    Ref<Vfs::Directory> const dir = vfs->openDirectory (ConstMemory());
    if (!dir)
	return Result::Failure;

    StringHash<EmptyBase> loaded_names;
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

	    if (dot_ptr)
		module_name = entry_name.region (0, (Byte const *) dot_ptr - entry_name.mem());
	}

	if (stat_data->file_type == Vfs::FileType::RegularFile &&
	    !loaded_names.lookup (module_name))
	{
	    loaded_names.add (module_name, EmptyBase());

	    logD_ (_func, "LOADING MODULE ", module_name);

	    if (!loadModule (module_name))
		logE_ (_func, "Could not load module ", module_name, ": ", exc->toString());
	}
    }

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
				   RtmpConnection    * const mt_nonnull conn)
{
    logD_ (_func_);

    ConstMemory path_tail;

    mutex.lock ();
    Ref<ClientEntry> const client_entry = getClientEntry (path, &path_tail, &root_namespace);
#if 0
// TODO 'free_access' config option.
    if (!client_entry) {
    }
#endif

    Ref<ClientSession> const client_session = grab (new ClientSession);
    client_session->weak_rtmp_conn = conn;
    client_session->unsafe_rtmp_conn = conn;
    client_session->processing_connected_event = true;

    client_session_list.append (client_session);
    client_session->ref ();
    mutex.unlock ();

    logD_ (_func, "client_session refcount before: ", client_session->getRefCount());
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

    logD_ (_func, "client_session refcount after: ", client_session->getRefCount());
    return client_session;
}

void
MomentServer::clientDisconnected (ClientSession * const mt_nonnull client_session)
{
    logD_ (_func, "client_session refcount before: ", client_session->getRefCount());

    client_session->clientDisconnected ();

    mutex.lock ();
    if (client_session->video_stream_key)
	removeVideoStream_unlocked (client_session->video_stream_key);

    client_session_list.remove (client_session);
    mutex.unlock ();

    logD_ (_func, "client_session refcount after: ", client_session->getRefCount());
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
			     ConstMemory const stream_name)
{
    if (client_session->backend
	&& client_session->backend->startWatching)
    {
	logD_ (_func, "calling backend->startWatching()");
	Ref<VideoStream> video_stream;
	if (!client_session->backend.call_ret< Ref<VideoStream> > (&video_stream,
								   client_session->backend->startWatching,
								   /* ( */ stream_name /* ) */))
	{
	    return NULL;
	}

	return video_stream;
    }

    logD_ (_func, "default path");

    return getVideoStream (stream_name);
}

Ref<VideoStream>
MomentServer::startStreaming (ClientSession * const mt_nonnull client_session,
			      ConstMemory const stream_name)
{
    if (client_session->backend
	&& client_session->backend->startStreaming)
    {
	logD_ (_func, "calling backend->startStreaming()");
	Ref<VideoStream> video_stream;
	if (!client_session->backend.call_ret< Ref<VideoStream> > (&video_stream,
								   client_session->backend->startStreaming,
								   /* ( */ stream_name /* ) */))
	{
	    return NULL;
	}

	return video_stream;
    }

    logD_ (_func, "default path");

    if (!publish_all_streams)
	return NULL;

    Ref<VideoStream> const video_stream = grab (new VideoStream);
    VideoStreamKey const video_stream_key = addVideoStream (video_stream, stream_name);

    client_session->mutex.lock ();
    if (client_session->disconnected) {
	client_session->mutex.unlock ();
	removeVideoStream (video_stream_key);
	return NULL;
    }
    client_session->video_stream_key = video_stream_key;
    client_session->mutex.unlock ();

    return video_stream;
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
    client_entry->event_informer.unsubscribe (client_handler_key.sbn_key);
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

#if 0
// Unused
void
MomentServer::addVideoStreamHandler (Cb<VideoStreamHandler> const &cb,
				     ConstMemory const &path_prefix)
{
  // TODO
}
#endif

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
    return video_stream_hash.add (path, video_stream);
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
    removeVideoStream_unlocked (video_stream_key);
    mutex.unlock ();
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

    {
	ConstMemory const opt_name = "moment/publish_all";
	MConfig::Config::BooleanValue const value = config->getBoolean (opt_name);
	if (value == MConfig::Config::Boolean_Invalid) {
	    logE_ (_func, "Invalid value for ", opt_name, ": ", config->getString (opt_name),
		   ", assuming \"", publish_all_streams, "\"");
	} else {
	    if (value == MConfig::Config::Boolean_False)
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
    : server_app (NULL),
      page_pool (NULL),
      http_service (NULL),
      config (NULL),
      recorder_thread_pool (NULL),
      storage (NULL),
      publish_all_streams (true)
{
    instance = this;
}

MomentServer::~MomentServer ()
{
    mutex.lock ();
    mutex.unlock ();
}

}

