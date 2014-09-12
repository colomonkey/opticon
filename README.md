# Opticon Server Metering for OpenStack

Opticon is the CloudVPS successor to its N2 monitoring service: A way of
keeping tabs on what your servers are doing for purposes of monitoring,
performance analysis and incident forensics. Opticon is designed to be
used in conjunction with the OpenStack Compute service, but can be
operated independently.

Like its predecessor, the Opticon server agent uses *push notification*
to send updates to the central collector, allowing important performance
data to keep flowing even under adversial conditions, where traditional
pollers tend to fail.

## Components

Opticon consists of three main program components. Monitored servers run
*opticon-agent*. On the other side, *opticon-collector* and
*opticon-api* handle the collection of data and interaction with users
respectively.

The source code is organized into smaller modules, some of which are
shared between multiple programs:

+-----------------------+-----------------------+
| libopticon            | Data structures for   |
|                       | keeping track of      |
|                       | hosts and meters, as  |
|                       | well as encoding and  |
|                       | decoding them for     |
|                       | transport or storage. |
+-----------------------+-----------------------+
| libopticondb          | Access to the         |
|                       | timestamp-indexed     |
|                       | opticon database that |
|                       | tracks meter samples. |
+-----------------------+-----------------------+
| libsvc                | Infrastructure for    |
|                       | building a system     |
|                       | service: Process      |
|                       | control, data         |
|                       | transport, threading, |
|                       | and logging.          |
+-----------------------+-----------------------+


