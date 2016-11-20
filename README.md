Pebble MPD Client
=================

This is an MPD client for the pebble!

![Pebble MPD Client](https://raw.githubusercontent.com/Sorunome/pebble-mpd-client/master/screenshots/basalt.png)

Supported platforms are `aplite`,`basalt`,`chalk`,`diorite` and `emery` so basically every pebble platform available! If you want support for any other platform or found any bug just create an issue.

Installation
------------
After installing via the pebble app store you might need to do some more extra configuration.  
Because of how the pebble API is working you might need to run a seperate python proxy on the server where your MPD server is running or on any other computer in the same network.

You do _NOT_ have to run the python proxy if you apply to one of the following:
* run an older MPD server
* have your pebble connected to IOS

So, how do you run said python proxy?

1. `sudo pip3 install python-mpd2`
2. copy all files from [here](https://github.com/Sorunome/pebble-mpd-client/blob/master/server/) to a directory on your server
3. edit `config.json` with the server IP and (optionally) a password (Password is recommended!)
4. run `python3 pebble_mpd_proxy.py` . It is recommended to run the proxy in e.g. a different screen so that you can close the connection to your server after starting the proxy. For that:
 - `screen -S pebble_mpd_proxy`
 - `python3 pebble_mpd_proxy.py`
 - exiting: hitting `Ctrl+a` and then `d`
 - for resuming: `screen -r pebble_mpd_proxy`

I you are interested as to why such a proxy is needed, head over [here](https://forums.pebble.com/t/pebblekit-js-raw-tcp-sockets/25042).

Usage
-----
What each button always does is shown on the right, you can stop by hitting the center button for a long time.

Donate
------
Do you like this app a lot? Perhaps you'd want to [donate](https://www.sorunome.de/donate) something! 
