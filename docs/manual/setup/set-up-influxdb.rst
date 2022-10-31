.. _set-up-influxdb:

Setting up InfluxDB
===================

InfluxDB maintains its own package repositories.  Please refer to
`InfluxDB documantation
<https://docs.influxdata.com/influxdb/v1.7/introduction/installation/>`_
on how to install InfluxDB from its official repositories.


Enable and start InfluxDB service
---------------------------------

Once InfluxDB has been installed, enable and start the service:

.. code-block:: console

  $ sudo systemctl enable influxdb
  $ sudo systemctl start influxdb


Set up InfluxDB BDE database
----------------------------

Create a ``bde`` database in InfluxDB:

.. code-block:: console

  $ influx
  Connected to http://localhost:8086 version 1.6.4
  InfluxDB shell version: 1.6.4
  > create database bde
  > quit

InfluxDB disk usage tends to grow after prolonged use.  In order to
prevent that, you will need to change the default retention policy:

.. code-block:: console

  $ influx
  Connected to http://localhost:8086 version 1.6.4
  InfluxDB shell version: 1.6.4
  > show retention policies on bde
  name    duration shardGroupDuration replicaN default
  ----    -------- ------------------ -------- -------
  autogen 0s       168h0m0s           1        true
  > alter retention policy "autogen" on "bde" duration 3w shard duration 30m default;
  > show retention policies on bde
  name    duration shardGroupDuration replicaN default
  ----    -------- ------------------ -------- -------
  autogen 504h0m0s 1h0m0s             1        true
  > quit


.. _Enabling HTTPS with InfluxDB: https://docs.influxdata.com/influxdb/v1.7/administration/https_setup/
