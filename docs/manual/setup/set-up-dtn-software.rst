
.. _set-up-dtn-software:

===============================
Setting up BDE Software on DTNs
===============================

.. _set-up-dtn-mdtm-inst:

Installing mdtmftp+ server
--------------------------

Assume that the mdtmftp+ server binary, along with the libraries and
othe ancilliary programs, have been built and installed. If not,
please refer to :ref:`build-mdtmftp`.

.. note:: It is not recommended to simply copy  ``mdtm-ftp-client`` binary
          from the build host to the head node, because there are
          several dynamically linked libraries and other ancillary
          files needs to be installed.  We recommend that you install
          mdtmftp+ from sources.

Assume that the mdtmftp+ server is installed at the path
``/usr/local/mdtmftp+/current/sbin/mdtm-ftp-server``.

.. _set-up-dtn-mdtm-cfg:

Setting up mdtmftp+ server
--------------------------

A mdtmftp+ server needs two configuration files to function correctly:
``/etc/mdtm/mdtmconfig.xml`` and ``/etc/mdtm/server.conf``.

* ``/etc/mdtm/mdtmconfig.xml`` configures a mdtmftp server's
  MDTM-related parameters. Please refer to `mdtmFTP Administrator
  Manual <https://mdtm.fnal.gov/downloads/manual/index.html>`_ for
  details.

Here is an example ``/etc/mdtm/mdtmconfig.xml``:

.. code-block:: xml

  <?xml version="1.0" standalone="no" ?>
  <Topology>
    <Device type="Block" numa="0">sda</Device>
    <Device type="Network" numa="0">eth0</Device>
  </Topology>
  <Online>
    <Device>sda</Device>
    <Device>eth0</Device>
  </Online>
  <Threads threads="2">
    <Device type="Block" threads="1">sda</Device>
    <Device type="Network" threads="1">eth0</Device>
  </Threads>
  <File segment="2G">
  </File>

* ``/etc/mdtm/server.conf`` configures a mdtmftp server's operation
  parameters. Please refer to `mdtmFTP Administrator Manual
  <https://mdtm.fnal.gov/downloads/manual/index.html>`_ for details.

Here is an example ``/etc/mdtm/server.conf``:

.. code-block:: properties

  blocksize 8388608
  direct 0
  splice 0

In normal operation, BDE typically uses X.509 certificates to
authenticate users and hosts, which requries extra certificate-related
configurations . However, for testing and debugging purpose, you can
also set up a password file for each mdtmftp+ server so that you can
run data transfer tests using the simple user name and password
authentication before certificate-related configurations are
completed.

Use ``globus-gridftp-password`` to generate a password file:

.. code-block:: console

  $ /usr/local/mdtmftp+/current/sbin/globus-gridftp-password > /etc/mdtm/password

Once the necessary configuration files are in place, test the mdtmftp+
server:

.. code-block:: console

  $ /usr/local/mdtmftp+/current/sbin/mdtm-ftp-server \
       -p 5001 \
       -password-file /etc/mdtm/password \
       -c /etc/mdtm/server.conf \
       -log-level all

If ``mdtm-ftp-server`` runs well in the foreground, quit it with
Ctrl-C, and proceed to the next step.


.. _set-up-dtn-bde-inst:

Installing BDE Agent
--------------------

Assume the BDE Agent binary ``bdeagent`` has been built from
sources. If not, please refer to :ref:`build-bde`.

First, copy the ``bdeagent`` binary to ``/usr/local/sbin``:

.. code-block:: console

  $ sudo mv build/agent/bdeagent /usr/local/bin

If BDE Agent was built on a different host, you can use ``scp`` to
copy ``bdeagent`` from the build host to ``/usr/local/sbin`` at each
**DTN**


.. _set-up-dtn-bde-cfg:

Setting up BDE Agent
--------------------

First, create a `dtn agent configuration` file
``/usr/local/etc/bdeagent.json``:

.. literalinclude:: bdeagent.json
   :language: json

The above configuration should be mostly self-explanatory.  It
specifies, among other things:

* The MQTT broker, MongoDB, and InfluxDB instances.
* NICs for control plane and data plane, respectively.
* Data transfer folders. 
* Location of mdtmftp+ server binary, and its parameters.
* Bootstrap and command handling options.

Then, test the configuration:

.. code-block:: console

  $ sudo /usr/local/sbin/bdeagent -c /usr/local/etc/bdeagent.json

This command runs the BDE Agent in the foreground.  If there are no
configuration errors, the Agent should be able to connect to MQTT and
MongoDB, and successfully launch mdtmftp+ server.  BDE Agent will
print messages like below on its output.

.. code-block:: console

    ___ ___  ___     _                _
   | _ )   \| __|   /_\  __ _ ___ _ _| |_
   | _ \ |) | _|   / _ \/ _` / -_) ' \  _|
   |___/___/|___| /_/ \_\__, \___|_||_\__|
                        |___/
       https://bigdataexpress.fnal.gov

   -- Running commit a55dd74, branch mqtt, built on Jul 16 2019 at 16:50:04.
   -- Starting BDE Agent as a foreground process.
   -- Reading configuration from /usr/local/etc/bdeagent.json.
   -- Logging to console.

   [2019-08-23 16:30:23] [Main] Starting BDE agent 1.0.0.0...
   [2019-08-23 16:30:23] [Main] Running commit a55dd74: Add more JSON test files for expand_and_group_v2 commands
   [2019-08-23 16:30:23] [AgentManager] Bootstrapping DTN module.
   [2019-08-23 16:30:23] [DTN Agent] Will use management_interface (eno1) IPv4 address for mdtmftp+ control channel.
   [2019-08-23 16:30:23] [DTN Agent] Translated "ip-of(&management_interface)" to 162.244.229.51.
   [2019-08-23 16:30:23] [DTN Agent] Will use data_interface[0] (enp175s0f0.2039) IPv4 address for mdtmftp+ data channel.
   [2019-08-23 16:30:23] [DTN Agent] Translated "ip-of(&data_interfaces[0])" to 10.250.39.53.
   [2019-08-23 16:30:23] [DTN Agent] Setting use_hostname_for_control=true
   [2019-08-23 16:30:23] [DTN Agent] Checksum will use up to 256 threads for files larger than 1073741824 bytes.
   [2019-08-23 16:30:23] [DTN Agent] Bootstrapping with ID: 24:6e:96:c8:90:c6, name: dtn1.example.net
   [2019-08-23 16:30:23] [DTN Agent] Created iozone work directory for /data/disk7 at /data/disk7/bde-iozone/work-m9w6P6.
   [2019-08-23 16:30:23] [DTN Agent] Bringing up LocalStorageAgent for device /dev/nvme7n1
   [2019-08-23 16:30:23] [AgentManager] bootstrapping done.
   [2019-08-23 16:30:23] [AgentManager] using unencrpyted MQTT broker.
   [2019-08-23 16:30:23] [MQTT] Connected to broker (cid: rpc-server-rpc/6359-1771-238E-0CE8)
   [2019-08-23 16:30:23] [AgentManager] initialized RPC server
   [2019-08-23 16:30:23] [MQTT] Connected to broker (cid: rpc-client-6B9A-A967-7750-F13D)
   [2019-08-23 16:30:23] [AgentManager] initialized RPC client
   [2019-08-23 16:30:23] [AgentManager] agent module 24:6e:96:c8:90:c6  init...
   [2019-08-23 16:30:23] [AgentManager] agent module 24:6e:96:c8:90:c6  registration...
   [2019-08-23 16:30:23] [DTN Agent] Registering with hostname "dtn1.example.net" for control plane
   [2019-08-23 16:30:23] [Subprocess] Logging subprocess output (pid 12873, stdout):
   [2019-08-23 16:30:23] Running "/usr/local/mdtmftp+/current/sbin/mdtm-ftp-server -control-interface 162.244.229.51 -data-interface 10.250.39.53 -p 5001 -password-file /etc/mdtm/password -c /etc/mdtm/server.conf -l /tmp/mdtmftp.log -log-level all" (pid: 12873) (env: GLOBUS_TCP_PORT_RANGE=32000,34640;GLOBUS_TCP_SOURCE_RANGE=54000,64000)

   [2019-08-23 16:30:23] [DTN Agent] Setting hostname property "dtn1.example.net" for control plane
   [2019-08-23 16:30:23] [AgentManager] agent module 24:6e:96:c8:90:c6|/dev/nvme7n1  init...
   [2019-08-23 16:30:23] [LocalStorageAgent] Will try to report disk I/O stats for /dev/nvme7n1 to time series database.
   [2019-08-23 16:30:23] [AgentManager] agent module 24:6e:96:c8:90:c6|/dev/nvme7n1  registration...
   [2019-08-23 16:30:23] [MQTT] Connected to broker (cid: agent-6359-1771-238E-0CE8)
   [2019-08-23 16:30:23] [Main] BDE agent has been started. Press Ctrl-C to quit.
   [2019-08-23 16:30:23] [LocalStorageAgent] Fatal error: iozone did not return results! Please check if iozone executable is configured correctly!
   [2019-08-23 16:30:23] [AgentManager] connected to mqtt server
   [2019-08-23 16:30:23] [Subprocess] Logging subprocess output (pid 12873, stderr):
   libmdtm: mdtmconfig load mdtmconfig.xml : Failed to open file. Try the system path.

   [2019-08-23 16:30:24] [Subprocess] Logging subprocess output (pid 12873, stderr):
   mdtm_schedule_threads: device=nvme7n1	cpu=0

   [2019-08-23 16:30:24] [Subprocess] Logging subprocess output (pid 12873, stderr):
   mdtm_schedule_threads: device=nvme7n1	cpu=2
   mdtm_schedule_threads: device=enp175s0f0.2039	cpu=4

   [2019-08-23 16:30:24] [Subprocess] Logging subprocess output (pid 12873, stderr):
   mdtm_schedule_threads: device=enp175s0f0.2039	cpu=6

If that worked, quit the Agent with ``Ctrl-C``, and proceed to the
next step.


.. _set-up-dtn-bde-svc:

Set up BDE Agent as a systemd service
-------------------------------------

First, create ``/lib/systemd/system/bdeagent.service`` with the
following lines:

.. code-block:: ini

  [Unit]
  Description=Bigdata Express Agent
  After=network.target

  [Service]
  Type=simple
  ExecStart=/usr/local/sbin/bdeagent -c /usr/local/etc/bdeagent.json -l /tmp/bdeagent.log
  TimeoutStopSec=10
  KillMode=mixed
  Restart=always

  [Install]
  WantedBy=multi-user.target

Then, install the service:

.. code-block:: console

  $ sudo systemctl daemon-reload
  $ sudo systemctl enable bdeagent.service
  $ sudo systemctl start bdeagent.service

In operation, BDE DTN Agent records events to log file
``/tmp/bdeagent.log``. You can view the log for status messages,
warnings, and errors:

.. code-block:: console

  $ tail -f /tmp/bdeagent.log

The logs should look similar to the log messages BDE Agent printed on
the console when running in the foreground.

In operation, mdtmftp+ server records events to log file
``/tmp/mdtmftp.log``. You can view the log for status messages,
warnings, and errors:

.. code-block:: console

  $ tail -f /tmp/mdtmftp.log

.. note:: The above configuration will run BDE Agent and mdtmftp+
          server as root. However, this is not strictly
          necessary. Neither programs require all privileges available
          to the root user. They can run with a subset of the
          privileges.


Running BDE Agent without root privileges
-----------------------------------------

You can use Linux `capabilities`_ to run BDE Agent and mdtmftp+ server as
non-root processes, with just the privileges they need.

.. _capabilities: http://man7.org/linux/man-pages/man7/capabilities.7.html

BDE Agent needs ``CAP_NET_ADMIN`` capability, so that it can set up
routes and ARP tables if necessary:

.. code-block:: console

   $ sudo setcap cap_net_admin+ep /usr/local/sbin/bdeagent

BDE Agent also needs to be able to write to the file
``/etc/grid-security/grid-mapfile``, so that it can add/remove X.509
mappings of certificate subjects and local users:

.. code-block:: console

   $ sudo setfacl -m u:bde:rw /etc/grid-security/grid-mapfile

BDE Agent will need write access to the data folder(s) you are using:

.. code-block:: console

   $ sudo chown -R bde: /data1 /data2

A different set of capabilities should be available to mdtmftp+
server:

* ``CAP_SYS_NICE``, to bind threads to cores.
* ``CAP_IPC_LOCK``, to lock memory.
* ``CAP_SYS_RESOURCE``, to increase the capacity of a pipe for mdtmFTP
  splice.
* ``CAP_SYS_ADMIN``, to increase the number of open files for mdtmFTP
  splice.

.. code-block:: console

   $ sudo setcap \
       cap_sys_nice,cap_ipc_lock,cap_sys_admin,cap_sys_resource+ep \
       /usr/local/mdtmftp+/current/sbin/mdtm-ftp-server

Once the above capabilities and file system permissions are set up,
you can add a ``User=bde`` line to ``/lib/systemd/system/bdeagent.service``:

.. code-block:: ini

  [Unit]
  Description=Bigdata Express Agent
  After=network.target

  [Service]
  Type=simple
  ExecStart=/usr/local/sbin/bdeagent -c /usr/local/etc/bdeagent.json -l /tmp/bdeagent.log
  User=bde
  TimeoutStopSec=10
  KillMode=mixed
  Restart=always

  [Install]
  WantedBy=multi-user.target

Now reload and restart ``bdeagent`` service:

.. code-block:: console

  $ sudo systemctl daemon-reload
  $ sudo systemctl restart bdeagent.service

Watch the log files ``/tmp/bdeagent.log`` for any permission-related
errors:

.. code-block:: console

  $ tail -f /tmp/bdeagent.log
  $ tail -f /tmp/mdtmftp.log


.. _set-up-dtn-telegraph:

Setting up telegraph
--------------------

BDE makes use of `Telegraph`_ agent to collect and report DTN status
and statistics, which include CPU loads, network activiteis, storage
usage, and memory usage. These statistics will be sent to InfluxDB for
storage, and can be visualized in web portal.

.. _Telegraph: https://www.influxdata.com/time-series-platform/telegraf/

Get a current build from `InfluxDB download site`_.

.. _InfluxDB download site: https://portal.influxdata.com/downloads/

Then, install telegraph on RHEL derivatives:

.. code-block:: console

  $ wget https://dl.influxdata.com/telegraf/releases/telegraf-1.11.3-1.x86_64.rpm
  $ sudo yum localinstall telegraf-1.11.3-1.x86_64.rpm

Or on Ubuntu/Debian:

.. code-block:: console

  $ wget https://dl.influxdata.com/telegraf/releases/telegraf_1.11.3-1_amd64.deb
  $ sudo dpkg -i telegraf_1.11.3-1_amd64.deb

Next, edit ``/etc/telegraph/telegraph.conf`` so that Telegraph can
report DTN statistics and status to the InfluxDB instance on **head
node**.

.. code-block:: ini

  [[outputs.influxdb]]
    urls = ["http://head-node.example.net:8086"] # required
    database = "telegraf" # required

Replace ``head-node.example.net`` with head node's DNS name or IPv4
address.

Finally, enable and start the service:

.. code-block:: console

  $ sudo systemctl enable telegraph.service
  $ sudo systemctl start telegraph.service


