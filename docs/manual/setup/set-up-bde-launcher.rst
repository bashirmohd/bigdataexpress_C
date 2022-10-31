.. _set-up-bde-launcher:

Setting up BDE Launching Agent
==============================

On **head node**, ``bdeagent`` is configured as ``launch agent``
through a `launch agent configuration file`.

Installing Launching Agent
--------------------------

Assume that ``bdeagent`` has been built from
sources.  If not, please refer to :ref:`build-bde`.

Copy ``build/agent/bdeagent`` from the build directory to
``/usr/local/bin``:

.. code-block:: console

  $ sudo cp build/agent/bdeagent /usr/local/bin

If ``bdeagent`` was built on a different host, please copy
``bdesagent`` from the build host to ``/usr/local/bin`` at **head
node**.


Installing mdtmftp+ client
--------------------------

Please refer to :ref:`build-mdtmftp` on how to build and install
mdtmftp+ client.

.. note:: Unlike ``bdeagent`` binary (which is statically linked), you
          cannot simply copy ``mdtm-ftp-client`` binary from the build
          host to the head node.  You will need to install it from
          sources.

Below we will assume that mdtmftp+ client is installed in
``/usr/local/mdtmftp+/current/bin/mdtm-ftp-client``.


Configuring Launching Agent
---------------------------

A `launch agent configuration` file describes:

* MQTT and MongoDB services that Launching Agent will use,
* location of the mdtmftp+ client binary, and
* parameters to use when running mdtmftp+ client.

First, create a `launch agent configuration` file
``/usr/local/etc/bdelauncher.json``:

.. literalinclude:: bdelauncher.json
   :language: json
   :linenos:

Then, test the configuration:

.. code-block:: console

  $ /usr/local/bin/bdeagent -c /usr/local/etc/bdelauncher.json

This command runs the Launching Agent in the foreground. If everything is correctly configured, the Agent should be able to connect to MQTT
and MongoDB, and to successfully launch mdtmftp+ client.

.. code-block:: console

  $ /usr/local/bin/ps -ef|grep mdtm

If succeed, quit the Agent with ``Ctrl-C``, and proceed to the next step.

Setting up Launching Agent as a systemd service
-----------------------------------------------

First, create ``/lib/systemd/system/bdelauncher.service`` with the following lines:

.. code-block:: ini

  [Unit]
  Description=Bigdata Express Launcher Agent
  After=network.target

  [Service]
  Type=simple
  ExecStart=/usr/local/bin/bdeagent -c /usr/local/etc/bdelauncher.json -l /tmp/bdelauncher.log
  User=bde
  TimeoutStopSec=10
  KillMode=mixed
  Restart=always
  UMask=0077

  [Install]
  WantedBy=multi-user.target

BDE Launch Agent does not require root privileges to run. It is
recommended to set ``User`` to a non-root user ``bde``.

Then, run BDE Launching Agent as a systemd service:

.. code-block:: console

  $ sudo systemctl daemon-reload
  $ sudo systemctl enable bdelauncher.service
  $ sudo systemctl start bdelauncher.service


In operation, BDE Launch Agent records events to log file:

* ``/tmp/bdelauncher.log``.

You can view BDE Server log for status messages, warnings, and errors:

.. code-block:: console

  $ tail -f /tmp/bdelauncher.log

On normal execution, you will see log statements without errors, like
below:

.. code-block:: console

   [2019-08-23 15:24:27] [Main] Starting BDE agent...
   [2019-08-23 15:24:27] [AgentManager] Bootstrapping Launcher module.
   [2019-08-23 15:24:27] [AgentManager] bootstrapping done.
   [2019-08-23 15:24:27] [AgentManager] using CA from /etc/ssl/bde-ca.pem for encrypted MQTT broker.
   [2019-08-23 15:24:27] [MQTT] Connected to broker (cid: rpc-server-rpc/A2CE-537B-794B-6E21)
   [2019-08-23 15:24:27] [AgentManager] initialized RPC server
   [2019-08-23 15:24:27] [MQTT] Connected to broker (cid: rpc-client-149C-FBC5-B6BD-9E98)
   [2019-08-23 15:24:27] [AgentManager] initialized RPC client
   [2019-08-23 15:24:27] [AgentManager] agent module launcher-agent  init...
   [2019-08-23 15:24:27] [Launcher] MQTT: using CA certificate at /etc/ssl/bde-ca.pem.
   [2019-08-23 15:24:27] [Launcher/ReportListener] Using CA certificate at /etc/ssl/bde-ca.pem for rpc/mdtm-report-launcher.
   [2019-08-23 15:24:27] [MQTT] Connected to broker (cid: rpc-server-rpc/mdtm-report-launcher)
   [2019-08-23 15:24:27] [Launcher] Listening on rpc/mdtm-report-launcher
   [2019-08-23 15:24:27] [Launcher] Running /usr/local/mdtmftp+/current/bin/mdtm-ftp-client
   [2019-08-23 15:24:27] [AgentManager] agent module launcher-agent  registration...
   [2019-08-23 15:24:27] Running "/usr/local/mdtmftp+/current/bin/mdtm-ftp-client -vb -p 8 -msgq head-node.example.net:8883:mdtm-listen-launcher:mdtm-report-launcher:2:/etc/ssl/bde-ca.pem" (pid: 19555) (env: )
   [2019-08-23 15:24:27] [MQTT] Connected to broker (cid: agent-A2CE-537B-794B-6E21)
   [2019-08-23 15:24:27] [Main] BDE agent has been started.
   [2019-08-23 15:24:27] [AgentManager] connected to mqtt server
