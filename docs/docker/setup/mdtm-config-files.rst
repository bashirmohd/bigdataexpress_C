.. _appendix-mdtm-config:

==================================
Appendix: MDTM configuration files
==================================

This section discuss various configuration options
available to MDTM module.

.. _appendix-mdtm-config-format:

MDTM configuration file locations
=================================

The latest MDTM configuration file is named as ``mdtmconfig.json``. Its location could be set
in the following ways with descending priority.

1. The path set in the ``mdtmftp`` command line option ``"--config-mdtm-path"``
2. The working directory of ``mdtmftp``
3. The system directory ``"/etc/mdtm/"``

If ``mdtmconfig.json`` is not found in any path described above, the older version of 
configuration file ``mdtmconfig.xml`` will be searched. If both of the configuration
files do not exist in the predefined paths, mdtmftp would exist with an error message.

MDTM configuration file format
==============================

The below shows the typical format of ``mdtmconfig.json``.

.. literalinclude:: mdtmconfig.json
   :language: json
   :linenos:

.. _appendix-mdtm-config-details:

MDTM configuration file segments
================================

The MDTM configuration file contains a number of segments as described below.

topology
--------

The MDTM scans the system devices to generate the topology tree, the key data structure
for operations thereafter. Sometimes, there might be some unrecognized devices that might 
be missing from the topology tree and unknown to MDTM.  To guarantee important devices
are contained in the topology tree, please use segment ``"topology"`` to explicitly identify 
those devices. The entry of topology segment is defined as

.. code-block:: console

   "topology": [
      {
         "type": string,
         "name": string,
         "numa": integer
      },
      ...
   ]
   
* The value of ``"type"`` is either ``"network"`` or ``"block"``. 
* The value of ``"name"`` is the device name in the system, e.g. ``"eth0"`` or ``"sda"``. 
* The value of ``"numa"`` is the index of NUMA node the device is affinited with. Tools like ``"lstopo"`` could be used to find the NUMA index.


online
------

The ``"topology"`` segment only shows devices in the system but not its working state. For example, 
the network interface might be down during a period of time. The ``"online"`` segment lists all the
devices which are in the working states and can be used by the MDTM for data transferring jobs.

.. code-block:: console

   "online": [
        string,
        string,
        ...
   ]
   
threads
-------
 
The MDTM module creates multiple I/O threads for each I/O device. The ``"threads"`` contains the number of 
threads for each device.   

.. code-block:: console

   "threads": [
     {
        "type": string,
        "name": string,
        "threads": integer
     },
           ...
   
* The ``"type"`` may has values of ``"network"`` or ``"block"``. 
* The ``"name"`` has the device name. 
* The ``"threads"`` is an integer that is the assigned number of I/O threads.

filesegment
-----------

In order to transfer files efficiently, MDTM designs a mechanism that splits a large file into 
multiple segments and then processed in parallel. But that segmentation could create processing overhead 
for small files. The ``"filesegment"`` set the threshold for the segmentation so that only files with size 
above that threshold would be segmented before transferring.

.. code-block:: console

   "filesegment": string
   
* The value of the ``"filesegment``" could have unit ``"M"`` or ``"G"`` to indicate Mega Bytes or Giga Bytes.

manually_configured_cpus (optional)
-----------------------------------

The MDTM is designed to have the scheduling capability to automatically find the optimal CPU cores 
to run I/O threads. However, sometimes users would like to manually assign CPU cores to those I/O threads. 
This segment of ``"manually_configured_cpus"`` gives the users the way to place I/O threads by themselves.

.. code-block:: console

   "manually_configured_cpus" : {
         "storage" : [int, int, ...],
         "network" : [int, int, ...]
   }

The arrays following ``"storage"`` and ``"network"`` contains the index of CPU cores to which the
device I/O threads would allocated.

server
------

The segment of ``"server"`` contains a group of parameters important to tune the performance. Actually it 
follows the format of ``"server.conf"``.

.. code-block:: console

   "server" : [
         "blocksize int",
         "direct int",
         "splice int" 
   ]
   
* ``"blocksize"`` defines the size of memory block in the data transfer. 
* ``"direct"`` and ``"splice"`` are the flags to turn on or off the directIO or zero copy.

serverpath
----------

This specifies the path to a file used internally by the MDTM module. Make sure the path is able to be read 
and write by the MDTM module.

.. code-block:: console

   "serverpath" : string   

   