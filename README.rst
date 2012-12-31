Release: 20121231

Fragpool is a memory management infrastructure designed to support
stream-to-packet layer interfaces in memory constrained devices.

The use case is an embedded system which passes data between a
stream-oriented interface such as a UART and a packet-oriented interface
such as HDLC.  The expectation is that the final length of a packet is not
known at the point where stream reception starts.  Consequently a system is
obliged to allocate a large buffer.  Once the packet is complete, the data
must be passed to another layer, and the space is not available for new
packets that are received while previous packets are being processed.

Fragpool features:

* Designed for use in hardware/first-level interrupt handlers (upper half);

* Ability to hand a completed buffer to another thread while continuing to
  received data in a new buffer;

* API supports request, resize, and release of buffers;

* Configurable pool size and number of active buffers per pool;

* Pool configuration can be done at compile-time or at runtime;


Please see the `documentation`_, `issue tracker`_, and
`homepage`_ on github.  Get a copy using git::

 git clone git://github.com/pabigot/fragpool.git

or by downloading the master branch via: https://github.com/pabigot/fragpool/tarball/master

Copyright 2012, Peter A. Bigot, and licensed under `BSD-3-Clause`_.

.. _documentation: http://pabigot.github.com/fragpool/
.. _issue tracker: http://github.com/pabigot/fragpool/issues
.. _homepage: http://github.com/pabigot/fragpool
.. _BSD-3-Clause: http://www.opensource.org/licenses/BSD-3-Clause
.. _MSPGCC: http://sourceforge.net/projects/mspgcc/
