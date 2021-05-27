# eegviewer
data viewer for the OpenEEG firmware v2 protocol

this is a work in progress utility to generate a graph from
the output of `eegreader`, an OpenEEG v2 reader.

this is written for the OpenBSD operating system.

    - make && make install
    - doas eegreader | eegviewer

this will create an X window within which a real-time graph
of the OpenEEG values will be drawn.
