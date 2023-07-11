# XToys E-Stim Controller

A utility for stereostim users who play Virtual Succubus to intercept the calls to XToys and convert them into stroke signals output to your stereostim.  It will also handle the E-Stim scene in FemDomination.  It can also be used as a standalone utility for stereostim play.

## Use

You should start xtoysestimcontroller from the command line.  If you're running in Windows you will need to start the process with Administrator access, otherwise the process will not have access to the audio hardware.  You can add ```-?``` to the command line to see all available options.  At a minimum, you will need to specify the audio device to output to with ```-d [devid]```.  You can see the list of available audio output devices with ```-list```.  If you want to run the FemDomination scene, you can pass ```-u``` on the command line to listen for the UDP packets from FemDomination.

When xtoysestimcontroller is running, you can open a browser to http://localhost:8082 (or the IP and port you've specified on the command line).

In the web interface, there will be a master volume and a drop down to select the currently playing estim mode.  If you change either of these, they will only take effect when you click the Update button.

Below that are the modes used for Virtual Succubus.  Hovering over the row heading will explain which Virtual Succubus event will trigger that mode.  Note that some modes are not currently used by Virtual Succubus (most of the 2nd modes).  FemDomination only uses the Stroke 2 mode.

Each mode has several options available.

- **Stroke Type**	Frequency will adjust the phase difference between the channels to simulate a stroke.  Amplitude will adjust the amplitude of both channels to simulate a stroke.
- **Frequencies**	A comma separated list of carrier frequencies to use.  It can be one or more (up to 10) frequencies that will be combined.
- **SPM**		Strokes per minute.
- **Min/Max Pos**	0-100.  The minimum and maximum stroke position.  For the frequency stroke type, this will be the minimum and maximum phase difference (mapped to 0 to 90 degrees) between the channels.  For the amplitude stroke type, this will be the minimum and maximum volume multiplier (also multiplied by master volume and mode volume multipliers)
- **Volume**		0-100.  Volume multiplier for this mode.
- **Balance**		Amplitude balance between left and right channels.
- **Amp Shift**	-180 to 180.  For amplitude stroke types, this will shift the peak of the amplitude on each channel.  Negative values will shift towards the left channel peak occuring first, and positive values will shift towards the right channel peak occuring first.
- **Phase Shift**	-180 to 180.  For frequency or amplitude stroke types, this will shift the phase of base carrier frequencies between each channels.  Negative values will shift to the left channel and positive values will shift to the right channel occuring first.

SPM and min and max positions may be overridden by Virtual Succubus based on the current event.  The counted stroke modes are special and won't perform the stroke until a stroke signal is sent from Virtual Succubus.  You should use another mode to test stroke settings you want to use and then apply those settings to the counted stroke modes.

After changing values for any of the modes, you must click the Update button under that section to save the changes.  The mode settings will be saved in a file and used the next time you start xtoysestimcontroller.

Below the Virtual Succubus modes are generic user modes you can use to experiment or play around with.

To intercept the web calls to XToys, you will need to setup a proxy.  [mitmproxy](https://mitmproxy.org/) works well for this purpose.
```mitmproxy --mode transparent --map-remote "|http(s)?\://.*xtoys.app|http://localhost:8082"```
That will intercept all web calls to xtoys.app and forward them to the local xtoysestimcontroller instance.  You may need to change the port or IP if you've specified them on the command line when starting xtoysestimcontroller.

After you have xtoysestimcontroller running and the modes configured, and the proxy set up to intercept the web calls, you can open Virtual Succubus and set up the Bluetooth toys section.  You can turn on the rhythm, teasing, punishment options in the Virtual Succubus settings.  There are buttons to test some of the modes available, and you can make sure they're working.  After everything is tested you can enjoy your estim session.

## Compiling
A CMakeLists.txt is provided to use CMake to compile an executable.  The portaudio library is required.
