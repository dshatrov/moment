package {

import flash.display.Sprite;
import flash.media.Camera;
import flash.media.Microphone;
import flash.media.Video;
import flash.net.NetConnection;
import flash.net.NetStream;
import flash.net.ObjectEncoding;
import flash.events.NetStatusEvent;

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

    public function Publisher ()
    {
        server_uri  = loaderInfo.parameters ["uri"];
        stream_name = loaderInfo.parameters ["stream"];

        video = new Video();
        video.width  = 640;
        video.height = 480;
        addChild (video);

        camera = Camera.getCamera();
        if (camera) {
            video.attachCamera (camera);
        }

//        microphone = Microphone.getEnhancedMicrophone();
        microphone = Microphone.getMicrophone();
        if (microphone) {
            microphone.setSilenceLevel (0, 2000);
            microphone.setUseEchoSuppression (true);
            microphone.setLoopBack (false);
            microphone.gain = 50;
        }

        conn = new NetConnection();
        conn.objectEncoding = ObjectEncoding.AMF0;

        conn.addEventListener (NetStatusEvent.NET_STATUS, onConnNetStatus);
        conn.connect (server_uri);
    }

    private function onConnNetStatus (event : NetStatusEvent) : void
    {
        if (event.info.code == "NetConnection.Connect.Success") {
            stream = new NetStream (conn);
            stream.bufferTime = 0.0;

            video.attachNetStream (stream);

//            stream.addEventListener (NetStatusEvent.NET_STATUS, onStreamNetStatus);

            stream.publish (stream_name);

            stream.attachCamera (camera);
            stream.attachAudio (microphone);
        } else
        if (event.info.code == "NetConnection.Connect.Closed") {
          // TODO reconnect
        }
    }
}

}

