package {

import flash.display.Sprite;
import flash.media.Camera;
import flash.media.Microphone;
import flash.media.Video;
import flash.media.SoundCodec;
import flash.media.H264VideoStreamSettings;
import flash.media.H264Level;
import flash.media.H264Profile;
import flash.net.NetConnection;
import flash.net.NetStream;
import flash.net.ObjectEncoding;
import flash.events.NetStatusEvent;
import flash.utils.setInterval;
import flash.utils.clearInterval;

[SWF(backgroundColor=0)]
public class Publisher extends Sprite
{
    private var server_uri : String;
    private var stream_name : String;

    private var conn : NetConnection;
    private var stream : NetStream;
    private var video : Video;

    private var camera : Camera;
    private var microphone : Microphone;

    private var reconnect_interval : uint;
    private var reconnect_timer : uint;

    public function Publisher ()
    {
        reconnect_interval = 3000;

        server_uri  = loaderInfo.parameters ["uri"];
        stream_name = loaderInfo.parameters ["stream"];

        video = new Video();
        video.width  = 640;
        video.height = 480;
        addChild (video);

//        camera = null;
        /**/
        camera = Camera.getCamera();
        if (camera) {
            camera.setMode (320, 240, 30);
            camera.setQuality (200000, 80);
//            camera.setQuality (500000, 80);
            video.attachCamera (camera);
        }
        /**/

//        microphone = Microphone.getEnhancedMicrophone();
        microphone = Microphone.getMicrophone();
        /**/
        if (microphone) {
            microphone.codec = SoundCodec.SPEEX;
            microphone.framesPerPacket = 1;
            microphone.enableVAD = true;
            microphone.setSilenceLevel (0, 2000);
            microphone.setUseEchoSuppression (true);
            microphone.setLoopBack (false);
            microphone.gain = 50;
        }
        /**/

        doConnect ();
    }

    private function doDisconnect () : void
    {
//        if (stream)
//            stream.removeEventListener (NetStatusEvent.NET_STATUS, onStreamNetStatus);

        if (conn) {
            conn.removeEventListener (NetStatusEvent.NET_STATUS, onConnNetStatus);
            conn.close ();
            conn = null;
        }
    }

    private function doConnect () : void
    {
        if (reconnect_timer) {
            clearInterval (reconnect_timer);
            reconnect_timer = 0;
        }

        doDisconnect ();

        conn = new NetConnection();
        conn.objectEncoding = ObjectEncoding.AMF0;

        conn.addEventListener (NetStatusEvent.NET_STATUS, onConnNetStatus);
        conn.connect (server_uri);

        reconnect_timer = setInterval (reconnectTick, reconnect_interval);
    }

    private function onConnNetStatus (event : NetStatusEvent) : void
    {
        if (event.info.code == "NetConnection.Connect.Success") {
            if (reconnect_timer) {
                clearInterval (reconnect_timer);
                reconnect_timer = 0;
            }

            stream = new NetStream (conn);
            stream.bufferTime = 0.0;

            video.attachCamera (camera);

//            stream.addEventListener (NetStatusEvent.NET_STATUS, onStreamNetStatus);

            {
                var avc_opts : H264VideoStreamSettings = new H264VideoStreamSettings ();
                avc_opts.setProfileLevel (H264Profile.BASELINE, H264Level.LEVEL_3_1);
// This has no effect                avc_opts.setQuality (1000000, 100);
                stream.videoStreamSettings = avc_opts;
            }

            stream.publish (stream_name);

            stream.attachCamera (camera);
            stream.attachAudio (microphone);
        } else
        if (event.info.code == "NetConnection.Connect.Closed") {
            if (!reconnect_timer) {
                if (reconnect_interval == 0) {
                    doReconnect ();
                    return;
                }

                reconnect_timer = setInterval (reconnectTick, reconnect_interval);
            }
        }
    }

    private function reconnectTick () : void
    {
        doReconnect ();
    }

    private function doReconnect () : void
    {
        doConnect ();
    }
}

}

