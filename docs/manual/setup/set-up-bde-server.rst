.. _set-up-bde-server:

Setting up BDE Server
=====================


Installing BDE Server
---------------------

Assume that the BDE Server binary has been built from
sources.  If not, please refer to :ref:`build-bde`.

Copy ``build/server/bdeserver`` from the build directory to
``/usr/local/bin``:

.. code-block:: console

  $ sudo cp build/server/bdeserver /usr/local/bin

If the BDE Server binary was built on a different host, please copy ``bdeserver`` from the build host to ``/usr/local/bin`` at **head node**.


Configuring BDE Server
----------------------

First, create a BDE server configuration file ``/usr/local/etc/bdeserver.json``:

.. literalinclude:: bdeserver.json
   :language: json
   :linenos:

See :ref:`appendix-bde-server-config` for description of items in BDE
Server configuration file.

Then, test the configuration:

.. code-block:: console

  $ /usr/local/bin/bdeserver -c /usr/local/etc/bdeserver.json

If BDE Server is launched successfully in the foreground, quit it with
``Ctrl-C``, and proceed to the next step.


Set up BDE Server as a systemd service
--------------------------------------

First, create ``/lib/system/systemd/bdeserver.service`` with the following lines:

.. code-block:: ini

  [Unit]
  Description=Bigdata Express Server/Scheduler
  After=network.target

  [Service]
  Type=forking
  PIDFile=/var/lib/bdeserver/run/bdeserver.pid
  ExecStart=/usr/local/bin/bdeserver -d -c /usr/local/etc/bdeserver.json
  User=bde
  TimeoutStopSec=10
  KillMode=mixed
  Restart=always

  [Install]
  WantedBy=multi-user.target

BDE Server does not require root privileges to run. It is recommended to set ``User`` to a non-root user ``bde``.

In operation, BDE Server records events in log files:

* ``/var/lib/bdeserver/run/bdeserver.pid`` to write the process ID.
* ``/var/lib/bdeserver/log/bdeagent.log`` for logging.

User ``bde`` needs write permission on these directories. Please create these directories, with appropriate permissions configured:

.. code-block:: console

  $ sudo mkdir -p /var/lib/bdeserver/run /var/lib/bdeserver/log
  $ sudo chown -R $USER: /var/lib/bdeserver

Then, run BDE Server as a systemd service:

.. code-block:: console

  $ sudo systemctl daemon-reload
  $ sudo systemctl enable bdeserver.service
  $ sudo systemctl start bdeserver.service

You can view BDE Server log for status messages, warnings, and errors:

.. code-block:: console

  $ tail -f /var/lib/bdeserver/log/bdeagent.log

Upon successful startup of BDE Server, you should see log messages
like:

.. code-block:: console

   [2019-07-17 11:57:51] WAN configured with:
           auth url:
           base url:

   [2019-07-17 11:57:51] Scheduler uses following configurations:
           SDN enabled:                      false
           SDN RPC timeout time:             10s
           Launching Daemon auth method:     certificate
           Launching Daemon write cmd:       false
           Checksum verification:            true
           Checksum algorithm:               sha1
           Group size:                       21474836480 (20GB)
           Network configurations:           {
           "wan_static" :
           {
                   "paths" :
                   [
                           {
                                   "circuits" :
                                   [
                                           {
                                                   "bandwidth" : 10000,
                                                   "dtns" : [],
                                                   "local_vlan_id" : 2039,
                                                   "remote_vlan_id" : 2039
                                           }
                                   ],
                                   "remote_site" : "Some-Remote-Site"
                           }
                   ]
           }
   }

   [2019-07-17 11:57:51] [BDEserver] : MQ server @ localhost / localhost : 1883
   [2019-07-17 11:57:51] [BDEserver] : Mongo server @ localhost / localhost : 27017
   [2019-07-17 11:57:51] starting bde server.
   [2019-07-17 11:57:51] using unencrpyted MQTT broker.
   [2019-07-17 11:57:51] [MQTT] Connected to broker (cid: rpc-client-44FB-C85E-CC4A-AD0E)
   [2019-07-17 11:57:51] [MQTT] Connected to broker (cid: rpc-server-rpc/bdeserver)
   [2019-07-17 11:57:51] [MQTT] Connected to broker (cid: bde-45CB-00FD-DDCB-A321)
