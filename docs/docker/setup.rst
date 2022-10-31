.. _set-up-bde-services:

=======================
Setting up BDE services
=======================

.. figure:: images/bde-two-dtns.png

   **Figure 1 BigData Express deployment diagram - a logical overview**


At **Head Node**, it is recommended to install and run:

1. BDE web portal.

2. BDE Server, as ``bdeserver`` systemd service.

3. BDE Launching Agent, as ``bdelauncher`` systemd service.

4. BDE Launching Agent will launch and manage an mdtmftp+ client
   instance (``mdtm-ftp-client`` binary).  The client initiates and
   monitors third-party data transfer between mdtmftp+ server
   instances running on DTNs.

5. An `MQTT`_ message bus service.  BDE uses Eclipse `Mosquitto`_.

6. A `MongoDB`_ database, as persistent store for web portal user
   registrations, task states, etc.

7. An `InfluxDB`_ database, for storing time-series data such as
   network and disk I/O rates.

At **DTN**, it is recommended to install and run:

8. BDE DTN Agent, as ``bdeagent`` systemd service.

9. BDE Data Transfer Engine, mdtmftp+ server (``mdtm-ftp-server``
   binary).

10. `Telegraf`_ agent, which will collect metrics from the DTN, and
    report them to InfluxDB.

.. _MQTT: https://mqtt.org
.. _Mosquitto: https://mosquitto.org/
.. _MongoDB: https://www.mongodb.com/
.. _InfluxDB: https://www.influxdata.com/
.. _Telegraf: https://www.influxdata.com/time-series-platform/telegraf/

At **Network Control Node (optional)**, it is recommended to install and run:

11. AmoebaNet and ONOS.

Following sections discuss the software setup for head node and
DTN(s).

.. toctree::

   setup/set-up-checklist.rst
   setup/set-up-docker.rst
   setup/set-up-bde-registry.rst
   setup/set-up-head-node.rst
   setup/set-up-dtn.rst
   setup/set-up-test-gsissh.rst
   setup/set-up-firewall.rst
   setup/bde-config-files.rst
   setup/mdtm-config-files.rst
   