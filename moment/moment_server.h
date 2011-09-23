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


#ifndef __MOMENT__SERVER__H__
#define __MOMENT__SERVER__H__


#include <libmary/libmary.h>
#include <mconfig/mconfig.h>


#include <moment/rtmp_connection.h>
#include <moment/video_stream.h>
#include <moment/storage.h>


namespace Moment {

using namespace M;

// Only one MomentServer object may be initialized during program's lifetime.
// This limitation comes form loadable modules support.
class MomentServer
{
private:
    class VideoStreamEntry
    {
    public:
	Ref<VideoStream> video_stream;

	VideoStreamEntry (VideoStream * const video_stream)
	    : video_stream (video_stream)
	{
	}
    };

    typedef StringHash<VideoStreamEntry> VideoStreamHash;

public:
    class VideoStreamKey
    {
	friend class MomentServer;
    private:
	VideoStreamHash::EntryKey entry_key;
	VideoStreamKey (VideoStreamHash::EntryKey entry_key) : entry_key (entry_key) {}
    public:
	operator bool () const { return entry_key; }
	VideoStreamKey () {}

	// Methods for C API binding.
	void *getAsVoidPtr () const { return entry_key.getAsVoidPtr(); }
	static VideoStreamKey fromVoidPtr (void *ptr) {
		return VideoStreamKey (VideoStreamHash::EntryKey::fromVoidPtr (ptr)); }
    };

#if 0
    // TODO Unused
    struct VideoStreamHandler
    {
	Result (*videoStreamOpened) (VideoStream * mt_nonnull video_stream);
    };
#endif

    class ClientSessionList_name;

    class ClientSession : public Object,
			  public IntrusiveListElement<ClientSessionList_name>
    {
	friend class MomentServer;

    public:
	struct Events
	{
	    void (*rtmpCommandMessage) (RtmpConnection       * mt_nonnull conn,
//					Uint32                msg_stream_id,
					VideoStream::Message * mt_nonnull msg,
					ConstMemory const    &method_name,
					AmfDecoder           * mt_nonnull amf_decoder,
					void                 *cb_data);

	    void (*clientDisconnected) (void *cb_data);
	};

	struct Backend
	{
	    Ref<VideoStream> (*startWatching) (ConstMemory  stream_name,
					       void        *cb_data);

	    Ref<VideoStream> (*startStreaming) (ConstMemory  stream_name,
						void        *cb_data);
	};

    private:
	mt_mutex (mutex) bool processing_connected_event;
	mt_mutex (mutex) bool disconnected;

	mt_const WeakCodeRef weak_rtmp_conn;
	mt_const RtmpConnection *unsafe_rtmp_conn;

	Informer_<Events> event_informer;
	Cb<Backend> backend;

	// TODO synchronization - ?
	VideoStreamKey video_stream_key;

	static void informRtmpCommandMessage (Events *events,
					      void   *cb_data,
					      void   *inform_data);

	static void informClientDisconnected (Events *events,
					      void   *cb_data,
					      void   *inform_data);

    public:
	Informer_<Events>* getEventInformer ()
	{
	    return &event_informer;
	}

	bool isConnected_subscribe (CbDesc<Events> const &cb);

	// Should be called by clientConnected handlers only.
	void setBackend (CbDesc<Backend> const &cb);

    private:
	void fireRtmpCommandMessage (RtmpConnection       * mt_nonnull conn,
				     VideoStream::Message * mt_nonnull msg,
				     ConstMemory const    &method_name,
				     AmfDecoder           * mt_nonnull amf_decoder);

	void clientDisconnected ();

	ClientSession ();

    public:
	RtmpConnection* getRtmpConnection (CodeRef * const mt_nonnull ret_code_ref)
	{
	    *ret_code_ref = weak_rtmp_conn;
	    if (*ret_code_ref)
		return unsafe_rtmp_conn;

	    return NULL;
	}

	~ClientSession ();
    };

    typedef IntrusiveList<ClientSession, ClientSessionList_name> ClientSessionList;

    struct ClientHandler
    {
	void (*clientConnected) (ClientSession     *client_session,
				 ConstMemory const &app_name,
				 ConstMemory const &full_app_name,
				 void              *cb_data);
    };

private:
    class Namespace;

public:
    class ClientEntry : public Object
    {
	friend class MomentServer;

    private:
	Namespace *parent_nsp;
	StringHash< Ref<ClientEntry> >::EntryKey client_entry_key;

	Informer_<ClientHandler> event_informer;

	static void informClientConnected (ClientHandler *client_handler,
					   void          *cb_data,
					   void          *inform_data);

	void fireClientConnected (ClientSession     *client_session,
				  ConstMemory const &app_name,
				  ConstMemory const &full_app_name);

    public:
	Informer_<ClientHandler>* getEventInformer ()
	{
	    return &event_informer;
	}

	ClientEntry ()
	    : event_informer (this, &mutex)
	{
	}
    };

private:
    mt_const ServerApp *server_app;
    mt_const PagePool *page_pool;
    mt_const HttpService *http_service;
    mt_const HttpService *admin_http_service;
    mt_const MConfig::Config *config;
    mt_const ServerThreadPool *recorder_thread_pool;
    mt_const Storage *storage;

    mt_mutex (mutex) ClientSessionList client_session_list;

    static MomentServer *instance;

    mt_mutex (mutex) VideoStreamHash video_stream_hash;

    class Namespace : public BasicReferenced
    {
    public:
	// We use Ref<Namespace> because 'Namespace' is an incomplete type here
	// (language limitation).
	typedef StringHash< Ref<Namespace> > NamespaceHash;
	// We use Ref<ClientEntry> because of ClientEntry::event_informer.
	typedef StringHash< Ref<ClientEntry> > ClientEntryHash;

	Namespace *parent_nsp;
	NamespaceHash::EntryKey namespace_hash_key;

	NamespaceHash namespace_hash;
	ClientEntryHash client_entry_hash;

	Namespace ()
	    : parent_nsp (NULL)
	{
	}
    };

    mt_mutex (mutex) Namespace root_namespace;

    StateMutex mutex;

    mt_mutex (mutex) ClientEntry* getClientEntry_rec (ConstMemory  path,
						      ConstMemory * mt_nonnull ret_path_tail,
						      Namespace   * mt_nonnull nsp);

    mt_mutex (mutex) ClientEntry* getClientEntry (ConstMemory  path,
						  ConstMemory * mt_nonnull ret_path_tail,
						  Namespace   * mt_nonnull nsp);

    mt_throws Result loadModules ();

public:
  // Getting pointers to common objects

    ServerApp* getServerApp ();

    PagePool* getPagePool ();

    HttpService* getHttpService ();

    HttpService* getAdminHttpService ();

    MConfig::Config* getConfig ();

    ServerThreadPool* getRecorderThreadPool ();

    Storage* getStorage ();

    static MomentServer* getInstance ();

  // Client events

    Ref<ClientSession> rtmpClientConnected (ConstMemory const &path,
					    RtmpConnection    * mt_nonnull conn);

    void clientDisconnected (ClientSession * mt_nonnull client_session);

    void rtmpCommandMessage (ClientSession        * const mt_nonnull client_session,
			     RtmpConnection       * const mt_nonnull conn,
			     VideoStream::Message * const mt_nonnull msg,
			     ConstMemory const    &method_name,
			     AmfDecoder           * const mt_nonnull amf_decoder)
    {
	client_session->fireRtmpCommandMessage (conn, msg, method_name, amf_decoder);
    }

    void disconnect (ClientSession * mt_nonnull client_session);

    Ref<VideoStream> startWatching (ClientSession * mt_nonnull client_session,
				    ConstMemory    stream_name);

    Ref<VideoStream> startStreaming (ClientSession * mt_nonnull client_session,
				     ConstMemory    stream_name);

    struct ClientHandlerKey {
	ClientEntry *client_entry;
	GenericInformer::SubscriptionKey sbn_key;
    };

private:
    mt_mutex (mutex) ClientHandlerKey addClientHandler_rec (CbDesc<ClientHandler> const &cb,
							    ConstMemory const           &path,
							    Namespace                   *nsp);

public:
    ClientHandlerKey addClientHandler (CbDesc<ClientHandler> const &cb,
				       ConstMemory           const &path);

    void removeClientHandler (ClientHandlerKey client_handler_key);

  // Video stream handlers

#if 0
// Unused
    void addVideoStreamHandler (Cb<VideoStreamHandler> const &cb,
				ConstMemory const &path_prefix);
#endif

  // Get/add/remove video streams

    // TODO There's a logical problem here. A stream can only be deleted
    // by the one who created it. This limitation makes little sense.
    // But overcoming it requires more complex synchronization.
    Ref<VideoStream> getVideoStream (ConstMemory const &path);

    VideoStreamKey addVideoStream (VideoStream * const video_stream,
				   ConstMemory const &path);

private:
    mt_mutex (mutex) void removeVideoStream_unlocked (VideoStreamKey video_stream_key);

public:
    void removeVideoStream (VideoStreamKey video_stream_key);

  // Initialization

    Result init (ServerApp        * mt_nonnull server_app,
		 PagePool         * mt_nonnull page_pool,
		 HttpService      * mt_nonnull http_service,
		 HttpService      * mt_nonnull admin_http_service,
		 MConfig::Config  * mt_nonnull config,
		 ServerThreadPool * mt_nonnull recorder_thread_pool,
		 Storage          * mt_nonnull storage);

    MomentServer ();

    ~MomentServer ();    
};

}


#endif /* __MOMENT__SERVER__H__ */

