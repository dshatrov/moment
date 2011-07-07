package {

import flash.external.ExternalInterface;
//import flash.display.DisplayObject;
import flash.display.Sprite;
import flash.display.StageScaleMode;
import flash.display.StageAlign;
import flash.display.Loader;
import flash.media.Video;
import flash.net.NetConnection;
import flash.net.NetStream;
import flash.net.ObjectEncoding;
import flash.net.URLRequest;
import flash.events.Event;
import flash.events.NetStatusEvent;
import flash.events.MouseEvent;
import flash.utils.setInterval;
import flash.utils.clearInterval;

//[SWF(width='640', height='480')]
[SWF(backgroundColor=0)]
public class MySubscriber extends Sprite
{
    private function setSource (uri : String, stream_name : String) : void {
	trace ("--- setSource(): " + uri + ", " + stream_name);
	setChannel (uri, stream_name);
    }

    private var conn : NetConnection;
    private var video : Video;

    private var playlist_button : LoadedElement;
    private var fullscreen_button : LoadedElement;

    private var splash : LoadedElement;

    private var msg_connecting : LoadedElement;
    private var msg_buffering  : LoadedElement;
    private var msg_error      : LoadedElement;

    // Stage width and height stored to workaround incorrect stage size bug
    // reported by flash.
    private var stage_width : int;
    private var stage_height : int;

    private var channel_uri : String;
    private var stream_name : String;

    private var reconnect_interval : uint;
    private var reconnect_timer : uint;

    private var hide_buttons_timer : uint;

    private function doResize () : void
    {
        stage_width  = stage.stageWidth;
        stage_height = stage.stageHeight;

        trace ("--- doResize(): " + stage_width + ", " + stage_height);

	repositionMessages ();
        repositionButtons ();
        repositionSplash ();
        repositionVideo ();
    }

    private function repositionSplash () : void
    {
        splash.obj.x = (stage_width - splash.obj.width) / 2;
        splash.obj.y = (stage_height - splash.obj.height) / 2;
    }

    private function repositionVideo () : void
    {
	video.width = stage_width;
	video.height = stage_width * (video.videoHeight / video.videoWidth);
	video.x = 0;
	video.y = (stage_height - video.height) / 2;
    }

    private function repositionButtons () : void
    {
	playlist_button.obj.x = stage.stageWidth  - playlist_button.obj.width - 20;
	playlist_button.obj.y = stage.stageHeight - playlist_button.obj.height - 20;

        if (stage.displayState == "fullScreen")
          fullscreen_button.obj.x = stage.stageWidth - fullscreen_button.obj.width - 20;
        else
          fullscreen_button.obj.x = stage.stageWidth - fullscreen_button.obj.width - 90;

	fullscreen_button.obj.y = stage.stageHeight - fullscreen_button.obj.height - 20;
    }

    private function repositionMessage (msg : Loader) : void
    {
	msg.x = (stage_width - msg.width) / 2;
	msg.y = (stage_height - msg.height) * (4.0 / 5.0);
    }

    private function repositionMessages () : void
    {
	repositionMessage (msg_connecting.obj);
	repositionMessage (msg_buffering.obj);
	repositionMessage (msg_error.obj);
    }

    private function loaderComplete (loader : Loader) : Boolean
    {
        if (loader.contentLoaderInfo
            && loader.contentLoaderInfo.bytesTotal > 0
            && loader.contentLoaderInfo.bytesTotal == loader.contentLoaderInfo.bytesLoaded)
        {
            return true;
        }

        return false;
    }

    private function doLoaderLoadComplete (loaded_element : LoadedElement) : void
    {
	repositionMessages ();
        repositionButtons ();
        repositionSplash ();

	loaded_element.allowVisible ();
    }

    private function loaderLoadCompleteHandler (loaded_element : LoadedElement) : Function
    {
	return function (event : Event) : void {
	    doLoaderLoadComplete (loaded_element);
	};
    }

    private function showSplash () : void
    {
        video.visible = false;
        splash.setVisible (true);
    }

    private function showVideo () : void
    {
	splash.setVisible (false);
	video.visible = true;
    }

    private function showConnecting () : void
    {
	msg_connecting.setVisible (true);
	msg_buffering.setVisible (false);
	msg_error.setVisible (false);
    }

    private function showBuffering () : void
    {
	msg_connecting.setVisible (false);
	msg_buffering.setVisible (true);
	msg_error.setVisible (false);
    }

    private function showError () : void
    {
	msg_connecting.setVisible (false);
	msg_buffering.setVisible (false);
	msg_error.setVisible (true);
    }

    private function hideMessages () : void
    {
	msg_connecting.setVisible (false);
	msg_buffering.setVisible (false);
	msg_error.setVisible (false);
    }

    private function onStreamNetStatus (event : NetStatusEvent) : void
    {
	trace ("--- STREAM STATUS");
	trace (event.info.code);

	if (event.info.code == "NetStream.Buffer.Full") {
	    reconnect_interval = 0;
	    video.removeEventListener (Event.ENTER_FRAME, onEnterFrame);

	    repositionVideo ();
	    showVideo ();
	    hideMessages ();
	}
    }

    private function onEnterFrame (event : Event) : void
    {
	reconnect_interval = 0;
	repositionVideo ();
	showVideo ();
    }

    private function doReconnect () : void
    {
	doSetChannel (channel_uri, stream_name, true /* reconnect */);
    }

    private function reconnectTick () : void
    {
	trace ("--- reconnectTick");

	if (reconnect_interval < 60000)
	    reconnect_interval *= 2;

	doReconnect ();
    }

    private function onConnNetStatus (event : NetStatusEvent) : void
    {
	trace ("--- CONN STATUS");
	trace (event.info.code);

	if (event.info.code == "NetConnection.Connect.Success") {
	    if (reconnect_timer) {
		clearInterval (reconnect_timer);
		reconnect_timer = 0;
	    }

	    showBuffering ();

	    var stream : NetStream = new NetStream (conn);
	    stream.bufferTime = 5;
	    stream.client = new MyStreamClient();

	    video.attachNetStream (stream);

	    stream.addEventListener (NetStatusEvent.NET_STATUS, onStreamNetStatus);

	    video.addEventListener (NetStatusEvent.NET_STATUS,
		function (event : NetStatusEvent) : void {
		    trace ("--- VIDEO STATUS");
		    trace (event.info.code);
		}
	    );

	    video.addEventListener (Event.ENTER_FRAME, onEnterFrame);

	    stream.play (stream_name);
	} else
	if (event.info.code == "NetConnection.Connect.Closed") {
	    video.removeEventListener (Event.ENTER_FRAME, onEnterFrame);
	    splash.setVisible (false);
	    showError ();

	    if (!reconnect_timer) {
		if (reconnect_interval == 0) {
		    reconnect_interval = 2000;
		    doReconnect ();
		    return;
		}

		reconnect_timer = setInterval (reconnectTick, reconnect_interval);
	    }
	}
    }

    public function setChannel (uri : String, stream_name_ : String) : void
    {
	doSetChannel (uri, stream_name_, false /* reconnect */);
    }

    public function doSetChannel (uri : String, stream_name_ : String, reconnect : Boolean) : void
    {
	trace ("--- doSetChannel: " + uri);

	showConnecting ();

	if (!reconnect)
	    reconnect_interval = 0;

	if (reconnect_timer) {
	    clearInterval (reconnect_timer);
	}
	reconnect_timer = setInterval (reconnectTick, 3000);

	channel_uri = uri;
	stream_name = stream_name_;

	if (conn != null) {
	    conn.removeEventListener (NetStatusEvent.NET_STATUS, onConnNetStatus);
	    conn.close ();
	}

	conn = new NetConnection();
	conn.objectEncoding = ObjectEncoding.AMF0;
	conn.connect (uri);

	trace ("--- connecting...");

	conn.addEventListener (NetStatusEvent.NET_STATUS, onConnNetStatus);
    }

    private function toggleFullscreen (event : MouseEvent) : void
    {
        trace ("--- toggleFullscreen");
        if (stage.displayState == "fullScreen") {
          stage.displayState = "normal";
          playlist_button.setVisible (true);
        } else {
          playlist_button.setVisible (false);
          stage.displayState = "fullScreen";
        }
    }

    private function togglePlaylist (event : MouseEvent) : void
    {
        trace ("--- togglePlaylist");
	ExternalInterface.call ("togglePlaylist");
    }

    private function hideButtonsTick () : void
    {
	playlist_button.setVisible (false);
	fullscreen_button.setVisible (false);
    }

    private function onMouseMove (event : MouseEvent) : void
    {
//        trace ("--- onMouseMove, " + event.localX + ", " + event.localY + " (" + stage_width + ", " + stage_height + "), " + (stage_width - (event.target.x + event.localX)) + ", " + (stage_height - (event.target.y + event.localY)) + " | " + event.target.scaleX + ", " + event.target.scaleY);

	if (hide_buttons_timer) {
	    clearInterval (hide_buttons_timer);
	    hide_buttons_timer = 0;
	}

        if (stage_width - (event.target.x + event.localX) > 400 ||
            stage_height - (event.target.y + event.localY) > 300)
        {
	    playlist_button.setVisible (false);
	    fullscreen_button.setVisible (false);
        } else {
	    hide_buttons_timer = setInterval (hideButtonsTick, 5000);

	    if (stage.displayState != "fullScreen")
	      playlist_button.setVisible (true);

	    fullscreen_button.setVisible (true);
        }
    }

    private function createLoadedElement (img_url  : String,
					  visible_ : Boolean) : LoadedElement
    {
	var loaded_element : LoadedElement;
	var loader : Loader;

	loader = new Loader ();

        loaded_element = new LoadedElement (visible_);
	loaded_element.obj = loader;

        loader.load (new URLRequest (img_url));
        loader.visible = false;

        addChild (loaded_element.obj);

        if (loader.contentLoaderInfo)
	    loader.contentLoaderInfo.addEventListener (Event.COMPLETE, loaderLoadCompleteHandler (loaded_element));
        if (loaderComplete (loader))
            doLoaderLoadComplete (loaded_element);

	return loaded_element;
    }

    public function MySubscriber()
    {
	reconnect_interval = 0;
	reconnect_timer = 0;

	hide_buttons_timer = setInterval (hideButtonsTick, 5000);

	stage.scaleMode = StageScaleMode.NO_SCALE;
	stage.align = StageAlign.TOP_LEFT;

        stage_width = stage.stageWidth;
        stage_height = stage.stageHeight;

	splash = createLoadedElement ("splash.png", true /* visible */);

	video = new Video();
	video.width = 320;
	video.height = 240;
	video.smoothing = true;
        video.visible = false;

	addChild (video);

	trace ("--- ExternalInterface.available: " + ExternalInterface.available);
	ExternalInterface.addCallback ("setSource", setSource);

        /*
        playlist_button = new Sprite();
        playlist_button.graphics.lineStyle (3, 0x00ffaa);
        playlist_button.graphics.beginFill (0xaa00ff);
        playlist_button.graphics.drawRect (0, 0, 50, 50);
        addChild (playlist_button);
        */

	playlist_button = createLoadedElement ("playlist.png", true /* visible */);
	playlist_button.obj.addEventListener (MouseEvent.CLICK, togglePlaylist);

        fullscreen_button = createLoadedElement ("fullscreen.png", true /* visible */);
	fullscreen_button.obj.addEventListener (MouseEvent.CLICK, toggleFullscreen);

	msg_connecting = createLoadedElement ("connecting.png", false /* visible */);
	msg_buffering  = createLoadedElement ("buffering.png",  false /* visible */);
	msg_error      = createLoadedElement ("error.png",      false /* visible */);

	doResize ();

        stage.addEventListener ("resize",
            function (event : Event) : void {
              doResize ();
            }
        );

        stage.addEventListener ("mouseMove", onMouseMove);

//	setChannel ("abc");
    }
}

}

internal class LoadedElement
{
    private var visible_allowed : Boolean;
    private var visible : Boolean;

    public var obj : flash.display.Loader;

    public function applyVisible () : void
    {
	obj.visible = visible;
    }

    public function allowVisible () : void
    {
	visible_allowed = true;
	applyVisible ();
    }

    public function setVisible (visible_ : Boolean) : void
    {
	visible = visible_;
	if (visible_allowed)
	    applyVisible ();
    }

    public function LoadedElement (visible_ : Boolean)
    {
	visible = visible_;
	visible_allowed = false;
    }
}

internal class MyStreamClient
{
    public function onMetaData (info : Object) : void
    {
	trace ("metadata: duration=" + info.duration + " width=" + info.width + " height=" + info.height + " framerate=" + info.framerate);
    }

    public function onCuePoint (info : Object) : void
    {
	trace ("cuepoint: time=" + info.time + " name=" + info.name + " type=" + info.type);
    }
}

