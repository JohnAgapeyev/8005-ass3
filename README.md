# 8005-ass3
This application forwards data back and forth between two machines as a port-forwarder.

# Compilation and Use
Requires:
* glibc 2.5 or later
* GNU Make
* Linux 2.6.17 or later
* GCC 4.9 or later
```bash
make
./8005-ass3.elf
```
That's all it takes!

# Configuration
The application looks for a file named forward.conf in the current directory.
This is hardcoded, and is not configurable.
If the file is not found, an error will be thrown.

The file must contain rules in the following format; one per line:
```
[input port],[output address],[output port]
```
Fields are comma-seperated.
The output port is the only optional part of the forwarding rule
If the output port is not specified, it will default to the input port

Example rules:
* `22,192.168.0.1,2200`
* `80,192.168.0.1`
* `1337,192.168.0.1, 1337`
