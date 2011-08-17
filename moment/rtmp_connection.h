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


#ifndef __LIBMOMENT__RTMP_CONNECTION__H__
#define __LIBMOMENT__RTMP_CONNECTION__H__


#include <libmary/libmary.h>
#include <moment/amf_encoder.h>
#include <moment/amf_decoder.h>
#include <moment/video_stream.h>


struct iovec;

namespace Moment {

using namespace M;

class RtmpConnection_OutputQueue_name;

class RtmpConnection : public DependentCodeReferenced,
		       public IntrusiveListElement<RtmpConnection_OutputQueue_name>
{
public:
    enum {
	DefaultDataChunkStreamId  = 3,
	DefaultAudioChunkStreamId = 4,
	DefaultVideoChunkStreamId = 5
    };

    enum {
	MinChunkSize = 128,
	MaxChunkSize = 65536,
	PrechunkSize = 65536,
//	PrechunkSize = 128,
	DefaultChunkSize = 128
    };

    class MessageInfo
    {
    public:
	Uint32 msg_stream_id;
	Uint32 timestamp;

	Uint32 prechunk_size;

	// TODO Put all information about a message here,
	//      including page_list, msg_len and amf_encoding.
	// TODO Rename to class Message.
    };

    typedef Result (*CommandMessageCallback) (MessageInfo            * mt_nonnull msg_info,
					      PagePool               * mt_nonnull page_pool,
					      PagePool::PageListHead * mt_nonnull page_list,
					      Size                     msg_len,
					      AmfEncoding              amf_encoding,
					      void                   * cb_data);

    struct Frontend
    {
	Result (*handshakeComplete) (void *cb_data);

	CommandMessageCallback commandMessage;

	Result (*audioMessage) (VideoStream::AudioMessageInfo * mt_nonnull msg_info,
			        PagePool                      * mt_nonnull page_pool,
				PagePool::PageListHead        * mt_nonnull page_list,
				Size                           msg_len,
				Size                           msg_offset,
				void                          *cb_data);

	Result (*videoMessage) (VideoStream::VideoMessageInfo * mt_nonnull msg_info,
				PagePool                      * mt_nonnull page_pool,
				PagePool::PageListHead        * mt_nonnull page_list,
				Size                           msg_len,
				Size                           msg_offset,
				void                          *cb_data);

	void (*sendStateChanged) (Sender::SendState send_state,
				  void *cb_data);

	void (*closed) (Exception *exc_ mt_exc_kind ((IoException, InternalException)),
			void *cb_data);
    };

    struct Backend
    {
	void (*close) (void *cb_data);
    };

#if 0
    struct Backend
    {
//	void (*peekInput) (Memory * mt_nonnull mem,
//			   void *cb_data);

//	void (*get_client_addr) ();
    };
#endif

private:
    typedef IntrusiveList<RtmpConnection, RtmpConnection_OutputQueue_name> OutputQueue;

    enum {
	//  3 bytes - chunk basic header;
	// 11 bytes - chunk message header (type 0);
	//  4 bytes - extended timestamp.
	//  4 bytes - extended timestamp;
	//  3 bytes - fix chunk basic header;
	//  7 bytes - fix chunk message header (type 1).
	// TODO More descriptive name
	MaxHeaderLen = 28
    };

    class ReceiveState
    {
    public:
	enum Value {
	    Invalid,

	    ClientWaitS0,
	    ClientWaitS1,
	    ClientWaitS2,

	    ServerWaitC0,
	    ServerWaitC1,
	    ServerWaitC2,

	    BasicHeader,

	    ChunkHeader_Type0,
	    ChunkHeader_Type1,
	    ChunkHeader_Type2,
	    ChunkHeader_Type3,

	    ExtendedTimestamp,
	    ChunkData
	};
	operator Value () const { return value; }
	ReceiveState (Value const value) : value (value) {}
	ReceiveState () {}
    private:
	Value value;
    };

    class CsIdFormat
    {
    public:
	enum Value {
	    Unknown,
	    OneByte,
	    TwoBytes_First,
	    TwoBytes_Second
	};
	operator Value () const { return value; }
	CsIdFormat (Value const value) : value (value) {}
	CsIdFormat () {}
    private:
	Value value;
    };

public:
    class RtmpMessageType
    {
    public:
	enum Value {
	    SetChunkSize      =  1,
	    Abort             =  2,
	    Ack               =  3,
	    UserControl       =  4,
	    WindowAckSize     =  5,
	    SetPeerBandwidth  =  6,
	    AudioMessage      =  8,
	    VideoMessage      =  9,
	    Data_AMF3         = 15,
	    Data_AMF0         = 18,
	    SharedObject_AMF3 = 16,
	    SharedObject_AMF0 = 19,
	    Command_AMF3      = 17,
	    Command_AMF0      = 20,
	    Aggregate         = 22
	};
	operator Value () const { return value; }
	RtmpMessageType (Value const value) : value (value) {}
	RtmpMessageType () {}
    private:
	Value value;
    };

    enum {
	CommandMessageStreamId = 0
    };

    enum {
	DefaultMessageStreamId = 1
    };

private:
    class UserControlMessageType
    {
    public:
	enum Value {
	    StreamBegin      = 0,
	    StreamEof        = 1,
	    StreamDry        = 2,
	    SetBufferLength  = 3,
	    StreamIsRecorded = 4,
	    PingRequest      = 6,
	    PingResponse     = 7
	};
	operator Value () const { return value; }
	UserControlMessageType (Value const value) : value (value) {}
	UserControlMessageType () {}
    private:
	Value value;
    };

    enum {
	Type0_HeaderLen = 11,
	Type1_HeaderLen =  7,
	Type2_HeaderLen =  3,
	Type3_HeaderLen =  0
    };

public:
    class PrechunkContext
    {
	friend class RtmpConnection;

    private:
	Size prechunk_offset;

    public:
	void reset ()
	{
	    prechunk_offset = 0;
	}

	PrechunkContext ()
	    : prechunk_offset (0)
	{
	}
    };

    class ChunkStream : public BasicReferenced
    {
	friend class RtmpConnection;

    private:
	Uint32 chunk_stream_id;

	// Incoming message accumulator.
	PagePool::PageListHead page_list;

	PrechunkContext in_prechunk_ctx;

	Size in_msg_offset;

	Uint32 in_msg_timestamp;
	Uint32 in_msg_timestamp_delta;
	Uint32 in_msg_len;
	Uint32 in_msg_type_id;
	Uint32 in_msg_stream_id;
	bool   in_header_valid;

	Uint32 out_msg_timestamp;
	Uint32 out_msg_timestamp_delta;
	Uint32 out_msg_len;
	Uint32 out_msg_type_id;
	Uint32 out_msg_stream_id;
	bool   out_header_valid;

    public:
	Uint32 getChunkStreamId () const
	{
	    return chunk_stream_id;
	}
    };

private:
    Timers *timers;
    PagePool *page_pool;

    Sender *sender;

    // Prechunking is always enabled currently.
    bool prechunking_enabled;

    Cb<Frontend> frontend;
    Cb<Backend> backend;

    bool is_closed;

    Timers::TimerKey ping_send_timer;
    bool ping_reply_received;

    Size in_chunk_size;
    Size out_chunk_size;

    Size out_got_first_timestamp;
    Uint32 out_first_timestamp;

    bool extended_timestamp_is_delta;
    bool ignore_extended_timestamp;

    bool processing_input;
    bool block_input;

    typedef AvlTree< Ref<ChunkStream>,
		     MemberExtractor< ChunkStream,
				      Uint32 const,
				      &ChunkStream::chunk_stream_id >,
		     DirectComparator<Uint32> >
	    ChunkStreamTree;

    ChunkStreamTree chunk_stream_tree;

  // Receiving state

    Uint32 remote_wack_size;

    Size recv_needed_len; // TODO Get rid of
    Size total_received;
    Size last_ack;

    Uint16 cs_id;
    CsIdFormat cs_id__fmt;
    Size chunk_offset;

    ChunkStream *recv_chunk_stream;

    Byte fmt;

    ReceiveState conn_state;

  // Sending state

    Uint32 local_wack_size;

    static bool timestampGreater (Uint32 const left,
				  Uint32 const right)
    {
	Uint32 delta;
	if (right >= left)
	    delta = right - left;
	else
	    delta = (0xffffffff - left) + right + 1;

	return delta < 0x80000000 ? 1 : 0;
    }

    Receiver::ProcessInputResult doProcessInput (ConstMemory const &mem,
						 Size * mt_nonnull ret_accepted);

public:
    class MessageDesc;

private:
    Uint32 mangleOutTimestamp (Uint32 timestamp);

    Size fillMessageHeader (MessageDesc const * mt_nonnull mdesc,
			    ChunkStream       * mt_nonnull chunk_stream,
			    Byte              * mt_nonnull header_buf,
			    Uint32             timestamp,
			    Uint32             prechunk_size);

    // TODO rename to resetChunk()
    void resetPacket ();

    void resetMessage (ChunkStream * mt_nonnull chunk_stream);

public:
    mt_const ChunkStream *control_chunk_stream;
    mt_const ChunkStream *data_chunk_stream;

    ChunkStream* getChunkStream (Uint32 chunk_stream_id,
				 bool create);

    static void fillPrechunkedPages (PrechunkContext        *prechunk_ctx,
				     ConstMemory const      &mem,
				     PagePool               *page_pool,
				     PagePool::PageListHead *page_list,
				     Uint32                  chunk_stream_id,
				     Uint32                  msg_timestamp,
				     bool                    first_chunk);

  // Send methods.

    // Message description for sending.
    class MessageDesc
    {
    public:
	Uint32 timestamp;
	Uint32 msg_type_id;
	Uint32 msg_stream_id;
	Size msg_len;
	// Chunk stream header compression.
	bool cs_hdr_comp;

	// TODO ChunkStream *chunk_stream;
    };

    // @prechunk_size - If 0, then message data is not prechunked.
    void sendMessage (MessageDesc  const * mt_nonnull mdesc,
		      ChunkStream        * mt_nonnull chunk_stream,
		      ConstMemory  const &mem,
		      Uint32              prechunk_size);

    // TODO 'page_pool' parameter is needed.
    // @prechunk_size - If 0, then message data is not prechunked.
    void sendMessagePages (MessageDesc const      * mt_nonnull mdesc,
			   ChunkStream            * mt_nonnull chunk_stream,
			   PagePool::PageListHead * mt_nonnull page_list,
			   Size                    msg_offset,
			   Uint32                  prechunk_size,
			   bool                    take_ownership = false);

    void sendRawPages (PagePool::Page *first_page,
		       Size msg_offset);

  // Send utility methods.

    void sendSetChunkSize (Uint32 chunk_size);

    void sendAck (Uint32 seq);

    void sendWindowAckSize (Uint32 wack_size);

    void sendSetPeerBandwidth (Uint32 wack_size,
			       Byte   limit_type);

    void sendUserControl_StreamBegin (Uint32 msg_stream_id);

    void sendUserControl_SetBufferLength (Uint32 msg_stream_id,
					  Uint32 buffer_len);

    void sendUserControl_StreamIsRecorded (Uint32 msg_stream_id);

    void sendUserControl_PingRequest ();

    void sendUserControl_PingResponse (Uint32 timestamp);

    void sendCommandMessage_AMF0 (Uint32 msg_stream_Id,
				  ConstMemory const &mem);

  // Extra send utility methods.

    void sendConnect (ConstMemory const &app_name);

    void sendCreateStream ();

    void sendPlay (ConstMemory const &stream_name);

  // ______

    void closeAfterFlush ();

    void close ();

    // Useful for controlling RtmpConnection's state from the backend.
    void close_noBackendCb ();

private:
  // Ping timer

    void beginPings ();

    static void pingTimerTick (void *_self);

  // ___

    Result processMessage (ChunkStream *chunk_stream);

    Result callCommandMessage (ChunkStream *chunk_stream,
			       AmfEncoding amf_encoding);

    Result processUserControlMessage (ChunkStream *chunk_stream);

  mt_iface (Sender::Frontend)

    static Sender::Frontend const sender_frontend;

    static void senderStateChanged (Sender::SendState  send_state,
				    void              *_self);

    static void senderClosed (Exception *exc_,
			      void      *_self);

  mt_iface (Receiver::Frontend)

    static Receiver::Frontend const receiver_frontend;

    static Receiver::ProcessInputResult processInput (Memory const &mem,
						      Size * mt_nonnull ret_accepted,
						      void *_self);

    static void processEof (void *_self);

    static void processError (Exception *exc_,
			      void      *_self);

  mt_iface_end()

public:
    // TODO setReceiver
    Cb<Receiver::Frontend> getReceiverFrontend ()
    {
	return Cb<Receiver::Frontend> (&receiver_frontend, this, getCoderefContainer());
    }

    void setFrontend (Cb<Frontend> const &frontend)
    {
	this->frontend = frontend;
    }

    void setBackend (Cb<Backend> const &backend)
    {
	this->backend = backend;
    }

    void setSender (Sender * const sender)
    {
	this->sender = sender;
	sender->setFrontend (Cb<Sender::Frontend> (&sender_frontend, this, getCoderefContainer()));
    }

    void startClient ();

    void startServer ();

  // TODO doConnect(), doCreateStream(), etc. belong to RtmpServer.

    Result doConnect (MessageInfo * mt_nonnull msg_info);

    Result doCreateStream (MessageInfo * mt_nonnull msg_info,
			   AmfDecoder  * mt_nonnull amf_decoder);

    Result doReleaseStream (MessageInfo * mt_nonnull msg_info,
			    AmfDecoder  * mt_nonnull amf_decoder);

    Result doCloseStream (MessageInfo * mt_nonnull msg_info,
			  AmfDecoder  * mt_nonnull amf_decoder);

    Result doDeleteStream (MessageInfo * mt_nonnull msg_info,
			   AmfDecoder  * mt_nonnull amf_decoder);

    Result fireVideoMessage (VideoStream::VideoMessageInfo * mt_nonnull video_msg_info,
			     PagePool                      * mt_nonnull page_pool,
			     PagePool::PageListHead        * mt_nonnull page_list,
			     Size                           msg_len,
			     Size                           msg_offset);

    // Deprecated constructor.
    RtmpConnection (Object   *coderef_container,
		    Timers   * mt_nonnull timers,
		    PagePool * mt_nonnull page_pool);

    void init (Timers   * mt_nonnull timers,
	       PagePool * mt_nonnull page_pool);

    RtmpConnection (Object *coderef_container);

    ~RtmpConnection ();
};

}


#endif /* __LIBMOMENT__RTMP_CONNECTION__H__ */

