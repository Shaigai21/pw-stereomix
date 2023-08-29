# pw-stereomix

Made this so I can screenshare with sound in Discord.

Script will automatically connect [PipeWire](https://pipewire.org) nodes from default_node_names array or filename to the discord (by default; can be changed) node. 

## Building

0. You have to have [PipeWire](https://pipewire.org) installed on your system
1. Run `make` in the repo folder 

## Usage

After building, run `./pw_stereomix [OPTIONS]` 

The `DST_NODE_NAME` argument for "-n" option is a string that contains the name of PipeWire node to which other nodes should be connected.
The `SRC_FILE` argument for "-f" option is a path to the text file that contains the list of PipeWire node names that you want to connect to the `DST_NODE_NAME` (WEBRTC VoiceEngine, by default)

You can run the program with the "-h" option for additional help.
You can also create a shortcut for this script, so it will automatically start and end at your command.

## Authors
 - **[Mastah](https://github.com/Shaigai21)** 
 - **[/dev/cat](https://github.com/7dev7cat)**



