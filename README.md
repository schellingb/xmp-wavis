# xmp-wavis
This plugin for [XMPlay](https://www.un4seen.com/xmplay.html)
loads and displays Winamp visualization plugins.

## Download
You can find a binary download under the
[Releases page](https://github.com/schellingb/xmp-wavis/releases/latest).

## Screenshot
![Screenshot](https://raw.githubusercontent.com/schellingb/xmp-wavis/master/README.png)

## Usage
When xmp-wavis.dll is copied somewhere under the XMPlay directory,
it can be found under the 'Options and stuff' screen on the DSP page.

An added VIS wrapper will first have no plugin loaded. To load a plugin,
open the configuration of the wrapper plugin. It will list all loadable
Winamp plugins that can be found under the directory of XMPlay.
To load a plugin outside of the XMPlay directory, use the '...' button.

You can add multiple 'Winamp Vis Wrapper' DSP plugins and run them at
the same time as long as you don't load the same Winamp plugin DLL twice.

To configure the wrapped Winamp visualization plugin, click the 'Configure'
button in the option screen.

The plugin also registers an XMPlay shortcut 'Winamp Visualization on/off',
which toggles the running state of all plugins that have been set up.

## Notes
 - To load the same Winamp Visualization plugin multiple times,
   copy the DLL to force a new loaded instance of it.

## License
xmp-wavis available under the [Public Domain](https://www.unlicense.org).
