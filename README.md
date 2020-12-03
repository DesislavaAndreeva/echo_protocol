# echo_protocol
Implementation of TCP/UDP based echo service

# General info

Echo service is a very useful debugging and measurement tool. It simply sends back to the originating source any data it receives. 
It is defined in RFC 862. A host may connect to the server using the Transmission Control Protocol (TCP) or the User Datagram Protocol (UDP) 
on port number 7 (Default echo port). The server sends back an identical copy of the data it received and measures the round-trip time.

Note: On UNIX-like operating systems an echo server is built into the inetd family of daemon. It is usually not enabled by default. If it is
enabled use another port for testing this application (change ECHO_PORT_DEFAULT);

# Description

# Details

### The aplication provides:

 * Very simple command line interface (CLI) with the following commads:

```
 [desia@localhost echo_protocol]$ ./echocli
== echocli 2020-12-02T16:40:40Z Exporting config ...

echocli

Usage: echocli [command]

Commands:
  echo-server  Start echo server (TCP and UDP)
  echo-test    Start echo client
  compile      Compile the application
  help         Help
```

 * TCP and UDP echo servers;
 * TCP and UDP echo clients;
 
# Compile

```
./echocli compile
```
 
 # Run 
 
 ## Server
 
As explained above, the default echo port is 7. In order to be able to bind to port numbers lower than 1024, you should run the script as a privileged user.
If you change ECHO_PORT_DEFAULT to a number number larger than 1024 you can run the scipt as unprivileged user.

```
sudo ./echocli echo-server <tcp-max-connections>
```

Example way to check if the server is listening:

```
[desia@localhost ~]$ sudo lsof -i:7
[sudo] password for desia:
COMMAND    PID USER   FD   TYPE     DEVICE SIZE/OFF NODE NAME
echo    143943 root    3u  IPv4 2277710204      0t0  TCP *:echo (LISTEN)
echo    143943 root    4u  IPv4 2277710205      0t0  UDP *:echo
```

 ## Client
```
./echocli echo-test ip <A.B.C.D> <tcp|udp> echo-message <message> wait-time <time-miliseconds>"
```

# TODO 

Add more commands. Example: command to automise the server's state checking.
