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


#include <moment/moment_types.h>
#include <moment/rtmp_connection.h>
#include <moment/video_stream.h>
#include <moment/storage.h>
#include <moment/push_protocol.h>
#include <moment/transcoder.h>


namespace Moment {

using namespace M;

// Only one MomentServer object may be initialized during program's lifetime.
// This limitation comes form loadable modules support.
//
class MomentServer : public Object
{
private:
    StateMutex mutex;

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


// __________________________________ Events ___________________________________

public:
    struct Events
    {
        void (*destroy) (void *cb_data);
    };

private:
    Informer_<Events> event_informer;

    static void informDestroy (Events *events,
                               void   *cb_data,
                               void   *inform_data);

    void fireDestroy ();

public:
    Informer_<Events>* getEventInformer ()
    {
        return &event_informer;
    }


// ____________________________ Transcoder backend _____________________________

public:
    struct TranscoderBackend
    {
        Ref<Transcoder> (*newTranscoder) (void *cb_data);
    };

private:
    mt_mutex (mutex) Cb<TranscoderBackend> transcoder_backend;

public:
    void setTranscoderBackend (CbDesc<TranscoderBackend> const &cb)
    {
        mutex.lock ();
        transcoder_backend = cb;
        mutex.unlock ();
    }

    Ref<Transcoder> newTranscoder ()
    {
        mutex.lock ();
        Cb<TranscoderBackend> const cb = transcoder_backend;
        mutex.unlock ();

        Ref<Transcoder> transcoder;
        if (cb)
            cb.call_ret (&transcoder, cb->newTranscoder);

        return transcoder;
    }


// ________________________________ Statistics _________________________________

private:
    static HttpService::HttpHandler const stat_http_handler;

    static Result statHttpRequest (HttpRequest   * mt_nonnull req,
                                   Sender        * mt_nonnull conn_sender,
                                   Memory const  & /* msg_body */,
                                   void         ** mt_nonnull /* ret_msg_data */,
                                   void          *_self);


// ___________________________ Video stream handlers ___________________________

public:
    // TODO Add a wrapper to api.h
    struct VideoStreamHandler
    {
        void (*videoStreamAdded) (VideoStream * mt_nonnull video_stream,
                                  ConstMemory  stream_name,
                                  void        *cb_data);
    };

    class VideoStreamHandlerKey
    {
        friend class MomentServer;

    private:
	GenericInformer::SubscriptionKey sbn_key;
    };

private:
    struct VideoStreamAddedNotification
    {
        Ref<VideoStream> video_stream;
        Ref<String> stream_name;
    };

    Informer_<VideoStreamHandler> video_stream_informer;

    DeferredProcessor::Registration vs_inform_reg;
    DeferredProcessor::Task vs_added_inform_task;

    mt_mutex (mutex) List<VideoStreamAddedNotification> vs_added_notifications;

    static void informVideoStreamAdded (VideoStreamHandler *vs_handler,
                                        void               *cb_data,
                                        void               *inform_data);

#if 0
    void fireVideoStreamAdded (VideoStream * mt_nonnull video_stream,
                               ConstMemory  stream_name);
#endif

    mt_mutex (mutex) void notifyDeferred_VideoStreamAdded (VideoStream * mt_nonnull video_stream,
                                                           ConstMemory  stream_name);

    static bool videoStreamAddedInformTask (void *_self);

public:
    VideoStreamHandlerKey addVideoStreamHandler (CbDesc<VideoStreamHandler> const &vs_handler);

    mt_mutex (mutex) VideoStreamHandlerKey addVideoStreamHandler_unlocked (CbDesc<VideoStreamHandler> const &vs_handler);

    void removeVideoStreamHandler (VideoStreamHandlerKey vs_handler_key);

    mt_mutex (mutex) void removeVideoStreamHandler_unlocked (VideoStreamHandlerKey vs_handler_key);

// _____________________________________________________________________________


public:
    typedef void StartWatchingCallback (VideoStream *video_stream,
                                        void        *cb_data);

    typedef void StartStreamingCallback (Result  result,
                                         void   *cb_data);

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
	    bool (*startWatching) (ConstMemory       stream_name,
                                   IpAddress         client_addr,
                                   CbDesc<StartWatchingCallback> const &cb,
                                   Ref<VideoStream> * mt_nonnull ret_video_stream,
                                   void             *cb_data);

	    bool (*startStreaming) (ConstMemory    stream_name,
                                    IpAddress      client_addr,
                                    VideoStream   * mt_nonnull video_stream,
                                    RecordingMode  rec_mode,
                                    CbDesc<StartStreamingCallback> const &cb,
                                    Result        * mt_nonnull ret_res,
                                    void          *cb_data);
	};

    private:
	mt_mutex (mutex) bool processing_connected_event;
	mt_mutex (mutex) bool disconnected;

	mt_const WeakCodeRef weak_rtmp_conn;
	mt_const RtmpConnection *unsafe_rtmp_conn;

	IpAddress client_addr;

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

//public:
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

public:
    class PageRequestResult
    {
    public:
	enum Value {
	    Success,
	    NotFound,
	    AccessDenied,
	    InternalError
	};
	operator Value () const { return value; }
	PageRequestResult (Value const value) : value (value) {}
	PageRequestResult () {}
    private:
	Value value;
    };

    class PageRequest
    {
    public:
	// If ret.mem() == NULL, then the parameter is not set.
	// If ret.len() == 0, then the parameter has empty value.
	virtual ConstMemory getParameter (ConstMemory name) = 0;

	virtual IpAddress getClientAddress () = 0;

	virtual void addHashVar (ConstMemory name,
				 ConstMemory value) = 0;

	virtual void showSection (ConstMemory name) = 0;

        virtual ~PageRequest () {}
    };

    struct PageRequestHandler
    {
	PageRequestResult (*pageRequest) (PageRequest  *req,
					  ConstMemory   path,
					  ConstMemory   full_path,
					  void         *cb_data);
    };

private:
    class PageRequestHandlerEntry : public Object
    {
	friend class MomentServer;

    private:
	Informer_<PageRequestHandler> event_informer;

	typedef StringHash< Ref<PageRequestHandlerEntry> > PageRequestHandlerHash;
	mt_const PageRequestHandlerHash::EntryKey hash_key;

	mt_mutex (MomentServer::mutex) Count num_handlers;

	static void informPageRequest (PageRequestHandler *handler,
				       void               *cb_data,
				       void               *inform_data);

	PageRequestResult firePageRequest (PageRequest *page_req,
					   ConstMemory  path,
					   ConstMemory  full_path);

    public:
	Informer_<PageRequestHandler>* getEventInformer ()
	{
	    return &event_informer;
	}

	PageRequestHandlerEntry ()
	    : event_informer (this, &mutex),
	      num_handlers (0)
	{
	}
    };

    typedef PageRequestHandlerEntry::PageRequestHandlerHash PageRequestHandlerHash;

private:
    mt_const ServerApp *server_app;
    mt_const PagePool *page_pool;
    mt_const HttpService *http_service;
    mt_const HttpService *admin_http_service;
    mt_const MConfig::Config *config;
    mt_const ServerThreadPool *recorder_thread_pool;
    mt_const Storage *storage;

    mt_const bool publish_all_streams;

    mt_mutex (mutex) PageRequestHandlerHash page_handler_hash;

    mt_mutex (mutex) ClientSessionList client_session_list;

    static MomentServer *instance;

    mt_mutex (mutex) VideoStreamHash video_stream_hash;

    mt_const Ref<VideoStream> mix_video_stream;

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
					    RtmpConnection    * mt_nonnull conn,
					    IpAddress   const &client_addr);

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

    bool startWatching (ClientSession    * mt_nonnull client_session,
                        ConstMemory       stream_name,
                        CbDesc<StartWatchingCallback> const &cb,
                        Ref<VideoStream> * mt_nonnull ret_video_stream);

    bool startStreaming (ClientSession * mt_nonnull client_session,
                         ConstMemory    stream_name,
                         VideoStream   * mt_nonnull video_stream,
                         RecordingMode  rec_mode,
                         CbDesc<StartStreamingCallback> const &cb,
                         Result        * mt_nonnull ret_res);

    struct ClientHandlerKey
    {
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

  // Get/add/remove video streams

    // TODO There's a logical problem here. A stream can only be deleted
    // by the one who created it. This limitation makes little sense.
    // But overcoming it requires more complex synchronization.
    Ref<VideoStream> getVideoStream (ConstMemory path);

    Ref<VideoStream> getVideoStream_unlocked (ConstMemory path);

    VideoStreamKey addVideoStream (VideoStream *video_stream,
				   ConstMemory  path);

private:
    mt_mutex (mutex) void removeVideoStream_unlocked (VideoStreamKey video_stream_key);

public:
    void removeVideoStream (VideoStreamKey video_stream_key);

    Ref<VideoStream> getMixVideoStream ();

  // Serving of static pages (mod_file)

    struct PageRequestHandlerKey {
	PageRequestHandlerEntry *handler_entry;
	GenericInformer::SubscriptionKey sbn_key;
    };

    PageRequestHandlerKey addPageRequestHandler (CbDesc<PageRequestHandler> const &cb,
						 ConstMemory path);

    void removePageRequestHandler (PageRequestHandlerKey handler_key);

    PageRequestResult processPageRequest (PageRequest *page_req,
					  ConstMemory  path);

  // Push protocols

private:
    typedef StringHash< Ref<PushProtocol> > PushProtocolHash;

    mt_mutex (mutex) PushProtocolHash push_protocol_hash;

public:
    void addPushProtocol (ConstMemory   protocol_name,
                          PushProtocol * mt_nonnull push_protocol);

    Ref<PushProtocol> getPushProtocolForUri (ConstMemory uri);

    Ref<PushConnection> createPushConnection (VideoStream *video_stream,
                                              ConstMemory  uri,
                                              ConstMemory  username,
                                              ConstMemory  password);

// _______________________________ Authorization _______________________________

public:
    enum AuthAction
    {
        AuthAction_Watch,
        AuthAction_Stream
    };

    typedef void CheckAuthorizationCallback (bool  authorized,
                                             void *cb_data);

    struct AuthBackend
    {
        bool (*checkAuthorization) (AuthAction   auth_action,
                                    ConstMemory  stream_name,
                                    ConstMemory  auth_key,
                                    IpAddress    client_addr,
                                    CbDesc<CheckAuthorizationCallback> const &cb,
                                    bool        * mt_nonnull ret_authorized,
                                    void        *cb_data);
    };

private:
    mt_mutex (mutex) Cb<AuthBackend> auth_backend;

public:
    void setAuthBackend (CbDesc<AuthBackend> const &auth_backend)
    {
        this->auth_backend = auth_backend;
    }

    bool checkAuthorization (AuthAction   auth_action,
                             ConstMemory  stream_name,
                             ConstMemory  auth_key,
                             IpAddress    client_addr,
                             CbDesc<CheckAuthorizationCallback> const &cb,
                             bool        * mt_nonnull ret_authorized);

// _____________________________________________________________________________


  // Utility

    void dumpStreamList ();

  // Initialization

    mt_locks (mutex) void lock ();

    mt_unlocks (mutex) void unlock ();

    Result init (ServerApp        * mt_nonnull server_app,
		 PagePool         * mt_nonnull page_pool,
		 HttpService      * mt_nonnull http_service,
		 HttpService      * mt_nonnull admin_http_service,
		 MConfig::Config  * mt_nonnull config,
		 ServerThreadPool * mt_nonnull recorder_thread_pool,
		 Storage          * mt_nonnull storage);

    MomentServer ();

    ~MomentServer ();    


// __________________________ Internal public methods __________________________

Result setClientSessionVideoStream (ClientSession *client_session,
                                    VideoStream   *video_stream,
                                    ConstMemory    stream_name);

// _____________________________________________________________________________

};

}


#endif /* __MOMENT__SERVER__H__ */

