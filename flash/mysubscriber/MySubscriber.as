package {

import flash.display.Sprite;
import flash.display.StageScaleMode;
import flash.display.StageAlign;
import flash.media.Video;
import flash.net.NetConnection;
import flash.net.NetStream;
import flash.net.ObjectEncoding;
import flash.events.Event;
import flash.events.NetStatusEvent;

//[SWF(width='640', height='480')]
[SWF(backgroundColor=0)]
public class MySubscriber extends Sprite
{
    public function MySubscriber()
    {
	stage.scaleMode = StageScaleMode.NO_SCALE;
	stage.align = StageAlign.TOP_LEFT;

	var video : Video = new Video();
//	video.width = 640;
//	video.height = 480;
//	video.width = 32;
//	video.height = 32;
	video.smoothing = true;
//	addChild (video);

	var conn : NetConnection = new NetConnection();
	conn.objectEncoding = ObjectEncoding.AMF0;
	conn.connect ("rtmp://10.0.0.3:8083");

	conn.addEventListener (NetStatusEvent.NET_STATUS,
	    function (event : NetStatusEvent) : void {
		trace ("--- CONN STATUS");
		trace (event.info.code);

		if (event.info.code == "NetConnection.Connect.Success") {
		    var stream : NetStream = new NetStream (conn);
		    stream.bufferTime = 5;
		    stream.client = new MyStreamClient();

		    video.attachNetStream (stream);

		    stream.addEventListener (NetStatusEvent.NET_STATUS,
			function (event : NetStatusEvent) : void {
			    trace ("--- STREAM STATUS");
			    trace (event.info.code);

			    if (event.info.code == "NetStream.Buffer.Full") {
//				video.width = video.videoWidth;
//				video.height = video.videoHeight;

				video.width = stage.stageWidth;
				video.height = stage.stageWidth * (video.videoHeight / video.videoWidth);

				video.x = 0;
				video.y = (stage.stageHeight - video.height) / 2;

				addChild (video);

//				stage.stageWidth = video.width / 2;
//				stage.stageHeight = video.height;

				stage.addEventListener ("resize",
				    function (event : Event) : void {
					video.width = stage.stageWidth;
					video.height = stage.stageWidth * (video.videoHeight / video.videoWidth);

					video.x = 0;
					video.y = (stage.stageHeight - video.height) / 2;
				    });
			    }
			});

		    video.addEventListener (NetStatusEvent.NET_STATUS,
			function (event : NetStatusEvent) : void {
			    trace ("--- VIDEO STATUS");
			    trace (event.info.code);
			});

		    stream.play ("red5StreamDemo");
		}
	    });
    }
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

