# echo_protocol
Implementation of TCP/UDP based echo service

# General info

Echo service is a very useful debugging and measurement tool. An echo service simply sends back to the originating source any data it
receives. One echo service is defined as a connection based application on TCP. Another echo service is defined as a datagram based application on UDP.
Default port number to listen is 7.

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
```
./echocli echo-server <tcp-max-connections>
```

 ## Client
```
./echocli echo-test ip <A.B.C.D> <tcp|udp> echo-message <message> wait-time <time-miliseconds>"
```
 
