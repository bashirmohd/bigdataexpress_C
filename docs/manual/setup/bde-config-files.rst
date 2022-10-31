.. _appendix-bde-config:

=================================
Appendix: BDE configuration files
=================================

This section discuss various configuration options
available to BDE software.


.. _appendix-bde-server-config:

BDE Server configuration file
=============================

The BDE Server configuration file, typically installed in
``/etc/bdeserver.json`` or ``/usr/local/etc/bdeserver.json``.

* ``agents``, BDE agent configurations

  * ``ld``, Launcher Agent parameters

    * ``authentication`` specifies the authentication schema. It can
      be either ``cert`` or ``password``.  Production systems must use
      ``cert``; ``password`` is useful for testing.

    * ``auth_username`` and ``auth_password`` specify username and
      password. They are relevant only when auth schema is set to
      ``password``.

* ``checksum_enable`` enables checksum verification for data transfer.

  * ``checksum_algorithm`` specifies checksum algorithm.  The
    available algorithms are ``sha1``, ``sha``, ``mdc2``,
    ``ripemd160``, ``sha224``, ``sha256``, ``sha384``, ``sha512``,
    ``md4``, and ``md5``.

  * ``group_size`` specifies a single data tranfer job's maximum
    size. A large data set will be split into multiple jobs.  Default
    is 20GB.

* ``network``, Local site network configurations

  * ``lan``, local area network (SDN) related configurations

    * ``type`` is the type of the SDN agent. Only ``AmoebaNet`` is
      supported for the moment.

    * ``dtns`` lists the DTNs whose data interfaces are managed by the
      local AmoebaNet.

  * ``wan_static`` specifies the static (OSCAR) WAN paths setup for this
    BDE site.

  * ``wan_dynamic`` specifies the dynamic (SENSE) WAN support and the
    service termination points (STPs) for the site.

    * ``sense`` specifies the URL and credentials for accessing the SENSE
      services.

    * ``stps`` lists all the service termination points (STPs) accessible
      from the local BDE service, along with the ``local_vlan_constraints``
      and the connected ``dtns``.

* ``mq_server``, MQTT parameters

  * ``host`` specifies the MQTT server IP address.

  * ``port`` specifies the MQTT server port number.  If TLS/SSL is
    disabled, the default port is ``1883``.  If TLS/SSL is enabled,
    the default port is ``8883``.

  * ``cacert`` specifies the file containing the MQTT server CA
    certificate when TLS/SSL is enabled.

* ``store``, MongoDB parameters

  * ``host`` specifies the MongoDB server IP address.

  * ``port`` specifies the MongoDB server port number.  The default
    port is ``27017``.

  * ``db`` specifies the MongoDB database that BDE uses.  The default
    database is ``bde``


An example BDE Server configuration file follows:

.. literalinclude:: bdeserver.json
   :language: json
   :linenos:


.. _appendix-bde-launcher-config:

BDE Launching Agent configuration file
======================================

BDE Launching Agent configuration will contain:

* ``agent``

  * ``mq_server``, MQTT parameters

    * ``host`` specifies the MQTT server IP address.

    * ``port`` specifies the MQTT server port number.  If TLS/SSL is
      disabled, the default port is ``1883``.  If TLS/SSL is enabled,
      the default port is ``8883``.

    * ``cacert`` specifies the file containing the MQTT server CA
      certificate when TLS/SSL is enabled.

  * ``store``, MongoDB parameters

    * ``host`` specifies the MongoDB server IP address.

    * ``port`` specifies the MongoDB server port number.  The default
      port is ``27017``.

    * ``db`` specifies the MongoDB database that BDE uses.  The
      default database is ``bde``

  * ``tsdb``, InfluxDB parameters

    * ``host`` specifies the InfluxDB server IP address.

    * ``port`` specifies the InfluxDB server port number.  The default
      port is ``8086``.

    * ``db`` specifies the InfluxDB database that BDE uses.  The
      default database is ``bde``

* ``modules``

  * ``m1``, module 1 parameters

    * ``type`` specifies the agent type - ``Launcher``.

    * ``name`` specifies the agent name.

    * ``id`` specifies the agent id.

    * ``subprocess``

      * ``type`` specifies the process type - ``mdtm-ftp-client``

      * ``path`` specifies the path to mdtmFTP client.

      * ``args`` specifies the arguments passed to mdtmFTP client.


An example BDE launching agent configuration file follows:

.. literalinclude:: bdelauncher.json
   :language: json
   :linenos:


.. _appendix-bde-agent-config:

BDE Agent configuration
=======================

BDE Agent configuration will contain:

* Addresses of the MQTT broker, MongoDB server, and InfluxDB server.

* DTN specific parameters, such as:

  * data folders to use,
  * control and data interfaces,
  * flags that can affect command handlers,

* mdtmftp+ server related configuration, such as:

  * location of mdtmftp+ server binary,
  * parameters to mdtmftp+ server subprocess, and
  * environment variables to set for mdtmftp+ server subprocess.

BDE Agent configuration files are installed in DTNs, usually in
``/usr/local/etc/bdeagent.json``, and they will contain JSON keys and
values like shown below:

.. literalinclude:: config-appendix/bdeagent.json
   :language: json
   :linenos:

Here are some notes on what these means:

* ``ignore_route_cmds`` is optional, and its default value is
  ``false``.  If this is set to ``true``, BDE Agent will not do
  anything when it receives route manipulation commands ("add route"
  and "delete route") from BDE Server.

* ``ignore_arp_cmds`` is optional, and its default value is ``false``.
  If this is set to true, DTN Agent will not do anything on ARP
  manipulation ("add ARP entry" or "delete ARP entry") commands.

* ``use_hostname_for_control`` is optional, and its default value is
  ``false``.

  If this option is set to ``true``, BDE Agent will use the DTN's
  hostname rather than management interface's IPv4 address to register
  itself with BDE Server.  This will cause BDE Server to use hostname
  in "``launch``" commands issued to mdtmftp+ client.

  It may be necessary to set this to ``true`` in some cases where GSI
  validation of host certificates fail due to a mismatch between
  certificate's subject and our use of IPv4 address rather than
  hostname.

* ``use_expand_v2_parent_list`` is optional, and its default value is
  ``false``.

  This is a backward compatibility option, to work with some old
  versions of mdtmftp+ server.  If this option is set to ``true``,
  responses to ``dtn_expand_and_group_v2`` command will contain a
  "parent group", as an indicator that the destination directory
  should be created prior to the actual data transfer.

* ``iozone.enable`` is optional, and its default value is ``true``.
  If set to ``false``, this will disable iozone work folder creation
  within data folders.  This could be useful if data folders are
  read-only.

* ``checksum.threads`` specifies number of threads to use when
  computing file checksums.  Default is 256.

* ``checksums.file_size_threshold`` indicates when to use threads for
  checksum computations.  BDE Agent will use threads for files larger
  than the threshold, and it is set in bytes.  Default in 1000000000.
