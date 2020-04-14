# lsyslog

lsyslog - the lunix syslog - receives rfc5424 or rfc3164 syslog messages over
tcp or udp and pushes the logs into NATS.


## The syslog protocol

* There is always only one message per packet.
* There is usually a '\r' carriage return at the end of a syslog, but that's
  not required.
* There can be newlines and carriage returns within a syslog message, although
  most syslog clients split them up into multiple syslog packets


### Some example messages

```
<159>1 2020-04-14T15:13:39.513875+00:00 myhostname my-process 184 - - a long message including lots\nof\rdata
<13>Apr 14 18:14:52 my-host tag: message
<13>Apr 14 18:14:52 my-host app[103]: message
<13>Apr 14 18:14:52 my-host message
```
