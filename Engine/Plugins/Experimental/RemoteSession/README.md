# RemoteSession
A plugin for Unreal that allows one instance to act as a thin-client (rendering and input) to a second instance

## Setup Info

* Get the BackChannel plugin (https://github.com/andrewgrant/BackChannel)
* Copy RemoteSession and BackChannel to your MyProject/PluginsFolder
* Build
* Start your project with -game or use Play In New Editor window (Play In Viewport is not currently supported)
* Start the RemoteSessionApp on your mobile device and enter the IP address of the machine running the game


## Useful Options

There are a number of options that can be configured by putting one or more of the following in your DefaultEngine.ini under [RemoteSession]

The values shown below are the current defaults. If you do not want to change from the defaults then I recommend not adding the line to your ini file incase they change in the future.

<pre>
[RemoteSession]
; Start a host when the game runs
bAutoHostWithGame=true
; Start a host when PIE is used
bAutoHostWithPIE=true
; Image Quality (1-100)
Quality=70
; Framerate (0-100)
Framerate=30
; Port to listen on
HostPort=2049
; Whether RemoteSession runs in shipping builds
bAllowInShipping=false
</pre>


Framerate and Quality can be adjusted at runtime via the remote.framerate and remote.quality cvars.
