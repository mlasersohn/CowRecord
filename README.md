(c) 2025 Mark Lasersohn
NDI® is a registered trademark of Vizrt NDI AB

Use and distribute freely, but at your own risk and with attribution. cow_record uses 
OpenCV, FLTK, and the FFMPEG libraries, each of which have their own licenses and terms. 

cow_record
	cow_record with no arguments waits for a new window to open and records that window.

--source=[source]
	Where [source] is the name of a window to be recorded, or:

 --source=launch://[command name]
		Where [command name] is any program that will open a window.
		cow_record will then record the next window that opens, which likely to be that command's window.

 --source=http://[URL]
 --source=https://[URL]
		Where [URL] indicates the location of a video file available on a website.

 --source=ndi://[NDI SPECIFIER]
		Where [NDI SPECIFIER] is a valid NDI video stream available on your network.

 --source=camera://[PATH TO A V4L CAMERA]
		Where [PATH TO A V4L CAMERA] is the file system path to camera device. PATH may also
		point to a video file, rather than a camera, in which case the video file will be played
		and recorded. This may be either a device path, video file, or the camera number.

 --source=desktop://
		Records the entire desktop.

 --source=fullscreen://[n]
		cow_record waits for a fullscreen window to open. If [n] is greater than zero,
		cow_record waits until the [n] instance of a fullscreen window opens and records that window.

 --source=mouse://
		Select a window to record by clicking on it with the mouse.

 --source=rectangle://
		Select an area to record by rectangle selection ("lasso"). After drawing the rectangle,
		confirm the selection by pressing the right mouse button.

 --source=text://[text]
		The source is an ascii text that will appear at --x and --y. See the font and color modifiers.

 --source=timestamp://[format]
		Print a timestamp onto the frame. The format codes are:
			%Y -- a four digit year
			%M -- a two digit month
			%D -- a two digit day
			%h -- a two digit hour
			%m -- a two digit minute
			%s -- a two digit second
			%f -- the frame number
		The format defaults to: "%Y/%M/%D %h:%m:%s (%f)".

 --source=image://[image_file_path]
		The source is an image file.

 --source=background://[r:g:b:a]
		The source is a solid color. Each color component is specified in a range from 0 to 255.

--pip=[source]
	Uses the same syntax as --source, but places the recorded window on top of the window recorded
	by the --source command. All commands subsequent to a --pip command will operate on the source
	specified in the last --pip command.

--monitor
	Provides a window to monitor the output being recorded. It defaults to 1/4 scale.

--monitor_ratio=[n]
	The ratio of the monitor to the actually output size, defaulting to 0.25
	The output size is multiplied by [n] to determine the height and with of
	the monitor window.

--monitor_only
	Only monitor the output in a window. Do not record.

--instance=[n]
	When no source is specified, wait until the [n] instance of a new window is opened, and record
	that window. This is used in the cases where a program opens a series of windows, possibly title windows,
	before the main window is opened.

--repeat
	If the source is --camera:// and the path points at a video file, repeat the playback of that
	file when it reaches the end, as opposed to having the background go black if the primary source
	or transparent, if a --pip.

--x=[n1] --y=[n1]
	The pixel position of the --pip window on top of the main source.

--width=[n1] --height=[n1]
	The pixel width and height of either the main window or the last --pip source

--audio_delay=[n]
	When no audio is being recorded, delay each frame by [n] microseconds to provide correct timing.
	The default is 20,000 microseconds.

--background=[r:g:b:a]
	Set the background color for the last source. Each color component is specified in a range from 0 to 255.
	The default background color is 0:0:0:0.

--foreground=[r:g:b:a]
	Set the foreground color for the last source. Each color component is specified in a range from 0 to 255.
	The default background color is 255:255:255:255.

--start=[time]
	The last source specified will only appear after [time] is recorded. Time is specified in seconds or portions
	hereof.

--stop=[time]
	The last source specified will cease appearing after [time] is recorded. Time is specified in seconds or portions
	hereof.

--wait=[n]
	Wait [n] seconds before recording.

--record_time=[n]
	Record for [n] seconds.

--wait_on_key
	Only start recording when ENTER is pressed.

--select_existing
	Select a window to record from a list of current windows.

--window_names
	Print a list of currently open window names and then exit without recording.

--launch=[PATH TO EXECUTABLE]
	Launch a program. Unlike the --source=launch:// syntax, this does not necessarily
	record the launched program's window.

--format_code=[FOURCC]
	Specify a preferred FOURCC code for your camera to use. This is only useful if your source is a camera.

--scale=[n]
	The scale of the incoming source or --pip.

--fps=[n]
	The frame rate for the produced video file or stream.

--snapshot=[filename]
	At the snapshot interval, save a png of the current frame. The snapshot filename is
	only a prefix for the full filename, which also includes a timestamp. The default is
	"snapshot".

--snapshot_interval=[n]
	Take a snapshot at every [n] seconds.

--font=[font_name]
	A font used by the last "--source=text://" command. The default is "Sans".

--font_size=[font_name]
	A font size used by the last "--source=text://" command. The default is 24.

--bold
	Causes the font selected by "--font=" to be bold.

--italic
	Causes the font selected by "--font=" to be italic.

--record_mouse
	When recording, record the mouse pointer. This only applies to window sources, not cameras.

--video_codec=[CODE]
	The preferred video codec for the produced video file or stream.

--audio_codec=[CODE]
	The preferred audio codec for the produced video file or stream.

--audio_device=[AUDIO DEVICE NAME]
	Use an alternative to the default audio input. This can be specified as an audio device name
	or the audio device index.

--audio_device_list
	List the audio input devices along with their indexes, descriptions, channels, and rates.

--volume=[n]
	Sets the volume for the last audio device specified. [n] should be between 0.0 and 1.0.
	if specifying volume for more than one device, all should add up to no more than 1.0.

--frequency=[n]
	Use an audio frequency other than 44100.

--channels=[n]
	Set the number of audio channels. The default is 2.

--output=[filename]
	The preferred filename for the produced video file. The default is "out.mp4", or:

  --output=ndi://[STREAM_NAME]
		Will stream output to a NDI stream under the specified name.

  --output=rtmp://[URL] or --output=rtmps://[URL]
		Write to a network streaming site like Twitch, YouTube, or Facebook.

Note: The order of --source and --pip commands is important. The first specified, whether it is
a --source or a --pip will always be the primary source, acting as the background and determining
the height and width of the final video file or stream (unless otherwise specified). Commands that
act on a video source, like --scale, will always act on the last video --source or --pip specified
before the command in question. So, "cow_record --source=camera:///dev/video0 --pip=calculator --scale=0.24"
will use the video coming in from video0 as the background. Screen capture from the window with the name
"calculator" will be placed on top of the video at 0.25 scale. Closing all of the sources, will
terminate recording. Closing a primary source will present a black background.
