.. _prepare-dtns:

Preparing and tuning DTNs
==========================

.. _prepare-data-transfer-folders:

Prepare data transfer folders
-----------------------------

A simple case is to desingate data folders in existing disk/storage 
devices as data transfer folders. 

However, there are cases that you need to install and mount new disk/storage 
devices to set up data transfer folders. For example, a NVME drive ``/dev/nvme0n1`` 
in a DTN will be used for data tranfer. You need to complete the following instructions 
to create a data transfer folder ``/data``:

* Format the storage.
* Create a partition.
* Create a file system.
* Create a mount point.

.. code-block:: console

   $ sudo fdisk /dev/nvme0n1
   $ sudo mkfs -t ext4 /dev/nvme0n1p1
   $ sudo mkdir /data
   $ sudo mount /dev/nvme0n1p1 /data


.. _create-user-accounts:

Create user accounts and set permissions
----------------------------------------

Create user accounts in each DTN, and set appropriate permissions to grant 
access to data transfer folders. A BDE user can access a particular DTN only
when the user's X.509 certificate is mapped to a local user account in the DTN.

In addition, it is recommend to create a user account ``bde`` to run BDE 
software on each DTN. 

.. _tcp-tuning:

TCP tuning
----------

Linux does a good job of auto-tuning TCP buffers for the typical
cases.  Still, default maximum TCP buffer sizes may be too small.
Here are some example ``/etc/sysctl.conf`` commands for different
types of hosts, taken from ESNet's `Linux Tuning`_ site.

.. _Linux Tuning: https://fasterdata.es.net/host-tuning/linux/

For a host with a 10G NIC, optimized for network paths up to 100ms
RTT, and for friendlyness to single and parallel stream tools, add
this to ``/etc/sysctl.conf``:

.. code-block:: ini

   # allow testing with buffers up to 64MB
   net.core.rmem_max = 67108864
   net.core.wmem_max = 67108864
   # increase Linux autotuning TCP buffer limit to 32MB
   net.ipv4.tcp_rmem = 4096 87380 33554432
   net.ipv4.tcp_wmem = 4096 65536 33554432
   # recommended default congestion control is htcp
   net.ipv4.tcp_congestion_control=htcp
   # recommended for hosts with jumbo frames enabled
   net.ipv4.tcp_mtu_probing=1
   # recommended for CentOS7+/Debian8+ hosts
   net.core.default_qdisc = fq

For a host with a 10G NIC optimized for network paths up to 200ms RTT,
and for friendliness to single and parallel stream tools, or a 40G NIC
up on paths up to 50ms RTT, add the below to ``/etc/sysctl.conf``:

.. code-block:: ini

   # allow testing with buffers up to 128MB
   net.core.rmem_max = 134217728
   net.core.wmem_max = 134217728
   # increase Linux autotuning TCP buffer limit to 64MB
   net.ipv4.tcp_rmem = 4096 87380 67108864
   net.ipv4.tcp_wmem = 4096 65536 67108864
   # recommended default congestion control is htcp
   net.ipv4.tcp_congestion_control=htcp
   # recommended for hosts with jumbo frames enabled
   net.ipv4.tcp_mtu_probing=1
   # recommended for CentOS7+/Debian8+ hosts
   net.core.default_qdisc = fq

.. _raise-resource-limits:

Raise resource limits
---------------------

The default maximum for open file descriptors may not be sufficient
for BDE.  You will have to raise the limits.

If BDE Agent and mdtmftp+ server are configured to run as a root
process, raise file descriptors for root user by editing
``/etc/security/limits.conf``:

.. code-block:: ini

   root soft nofile 4096
   root hard nofile 10240

If BDE Agent and mdtmftp+ server are configured to run as a non-root
(say, ``bde`` user) process, raise file descriptors for ``bde`` user
by editing ``/etc/security/limits.conf``:

.. code-block:: ini

   bde soft nofile 4096
   bde hard nofile 10240

The user will have to log out and log back in for this to take effect.


 

