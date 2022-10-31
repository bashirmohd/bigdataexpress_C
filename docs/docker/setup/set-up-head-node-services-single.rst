
=======================================
Set up single docker image on head node
=======================================

We provid a docker image containing all head node services:

* mosquitto broker
* mongodb
* influxdb
* bdeportal
* bdeserver
* bdelauncher
* mdtmftp+

To run head node services with that single docker image, please open a new terminal and create the BDE working directory ``/home/bde``:

.. code-block:: console

   $ mkdir -p /home/bde && cd /home/bde
   
and create a subdirectory to keep log files:

.. code-block:: console

   $ mkdir -p logs

.. _docker-run-mqtt:

Setting up MQTT
===============

BDE uses `Mosquitto`_, an implementation of the `MQTT`_ messaging
protocol, to communicate among different components.

MQTT with Default Configuration
-------------------------------

Set up to run the Eclipse Mosquitto broker with default configuration. 

1. Create subdirectories under the BDE working directory:

.. code-block:: console

   $ mkdir -p mosquitto
   
2. Create the configuration file ``mosquitto/config/mosquitto.conf`` (optional)

   If this file is not created, the mosquitto broker uses default values for configuration.
   
   This file defines all parameters to be used by the mosquitto broker inside the container.
   
.. note::  The file ``mosquitto/config/mosquitto.conf`` will be mounted to 
           ``/mosquitto/config/mosquitto.conf`` inside the container.


MQTT with Encryption
--------------------

The default configuration does not support encrypting messages. 
You can configure SSL/TLS support for mosquitto for encrypted 
network connections and authentication.

1. Create subdirectories under the BDE working directory:

.. code-block:: console

   $ mkdir -p mosquitto && mkdir -p mosquitto/ssl

2. Prepare and place MQTT server certificate files to ``mosquitto/ssl``

   You can either request a MQTT server certificate from a CA, or use openssl to 
   generate a self-signed server certificate: 

   * MQTT server CA certificate ``mqtt-ca.pem``.
   * MQTT server certificate ``mqtt-cert.pem``.
   * MQTT server private key ``mqtt-key.pem``.  

.. note:: The directory ``mosquitto/ssl`` will be mounted to ``/mosquitto/ssl`` 
   inside the container.
   
3. Create the configuration file ``mosquitto/mosquitto.conf``

   This file specifies certification-related paths and listening port to be
   used by the mosquitto broker inside the container:

   * The path to MQTT server CA certificate file.
   * The path to MQTT server private key file
   * The path to MQTT server certificate file
   * Port to use for the default listener
   
.. note::  The file ``mosquitto/mosquitto.conf`` will be mounted to 
           ``/mosquitto/config/mosquitto.conf`` inside the container.

An example MQTT server configurtion file follows: 

.. code-block:: ini

   # cafile defines a file containing the CA certificates.
   cafile /mosquitto/ssl/mqtt-ca.pem

   # Path to the PEM encoded server certificate.
   certfile /mosquitto/ssl/mqtt-cert.pem

   # Path to the PEM encoded keyfile.
   keyfile /mosquitto/ssl/mqtt-key.pem
   
   # Port to use for the default listener.
   port 8883      

.. _docker-run-mongodb:

Setting up MongoDB
==================

1. Create subdirectories under the BDE working directory:

.. code-block:: console

   $ mkdir -p mongod
   
2. Create the configuration file ``mongod/mongod.conf`` (optional)

   This file is optional. Without it, the mongodb server uses default configurarations.
   
   You can create this file to meet specifc configurations.
   
.. note::  The file ``mosquitto/mosquitto.conf`` will be mounted to 
           ``/etc/mongod.conf`` inside the container.

.. _docker-run-influxdb:

Setting up InfluxDB
===================

1. Create subdirectories under the BDE working directory:

.. code-block:: console

   $ mkdir -p influxdb
   
2. Create the configuration file ``influxdb/influxdb.conf`` (optional)

   This file is optional. Without it, the influxdb server uses default configurations.
   
   You can create this file to meet specific configurations.
   
.. note::  The file ``influxdb/influxdb.conf`` will be mounted to 
           ``/etc/influxdb/influxdb.conf`` inside the container.

.. _set-up-bde-server:

Setting up BDE Server
=====================

1. Create the subdirectory ``server`` under the BDE working directory:

.. code-block:: console

   $ mkdir -p server

2. Follow instructions in :ref:`appendix-bde-server-config` to create the bde server 
configuration file ``bdeserver.json``, in JSON format.

.. literalinclude:: bdeserver.json
   :language: json
   :linenos:

.. note::  The file ``server/bdeserver.conf`` will be mounted to 
           ``/usr/local/etc/bdeserver.json`` inside the container.


.. _set-up-bde-launcher:

Setting up BDE launching agent
==============================

1. Create the subdirectory ``launcher`` under BDE working directory:

.. code-block:: console

   $ mkdir -p launcher

2. Create configuration file ``launcher/bdelauncher.json``. See instructions in 
   :ref:`appendix-bde-launcher-config` for details. 

.. literalinclude:: bdelauncher-tls.json
   :language: json
   :linenos:

.. note:: The file ``launcher/bdelauncher.json`` will be mounted
   to ``/usr/local/etc/bdeagent.json`` inside the container.

3. Create mdtmFTP+ configure file ``mdtmconfig.xml``, which will be mounted
to ``/bdework`` inside the container. 

4. Follow instructions in :ref:`head-node-trust-path` to setting up X.509
   security on head node.

.. note:: The directory ``/etc/grid-security`` will be mounted to
   ``/etc/grid-security`` inside the container.

.. _set-up-portal:

Setting up BDE portal
=====================

1. Create the subdirectories, ``portal/config`` and ``portal/www/ssl``, under the BDE working directory:

.. code-block:: console

   $ mkdir -p portal/config && mkdir -p portal/www/ssl

2. Prepare a BDE portal certificate/private key pair, and copy them to ``portal/www/ssl``,
   which will be mounted to ``/srv/www/ssl`` in the container. You can reuse the MQTT 
   certificate/private pair in the directory ``bde/mosquitto/ssl``.

.. note:: This step is not required if you plan to set up a reverse proxy in front of BDE portal.

3. Create BDE web portal configuration file  ``portal/config/local.js`` using the template below:

.. literalinclude:: local.js
   :language: javascript
   :linenos:

*(a) SSL/TLS configuration.* By default, BDE portal ships with a self-signed TLS/SSL certificate. 
If you plan to set up BDE portal without a reverse proxy in the front, we recommend configuring an SSL certificate, issued by a well-trusted CA, for the portal.
Otherwise, web browser will give warnings about untrusted website or certificate when visiting the portal.

   * Replace ``key`` and ``cert`` with the new cert/key pair.

*(b) Site configuration.* It is necessary to edit the file to provide:

   * A name for the local site.
   * The MQTT server's host and port.
   * A certificate file for the MQTT server if TLS/SSL is enabled.
          If no TLS/SSL is configured, omit the ``mq_ca`` line.
   * A network interface's name for the control channel.
   * A client ID, secret, and callback URL for CILogon.

4. Create BDE web portal configuration file ``portal/config/connections.js``.

.. literalinclude:: connections.js
   :language: javascript
   :linenos:

Run BDE head node services
==========================

Now it is time to start the head node services.

1. Pull BDE head node service docker image:

.. code-block:: console

   $ docker pull publicregistry.fnal.gov/bigdata_express/bdeheadnode:1.5-xenial

2. Run the container:

.. code-block:: console

   $ docker run --name bdeheadnode \
           -it --rm \
           -u root \
           --net=host \
           --cap-add=ALL --cap-add=IPC_LOCK --cap-add=SYS_NICE --cap-add=SYS_ADMIN --cap-add=NET_ADMIN --security-opt seccomp:unconfined --privileged \
           -v /etc/grid-security:/etc/grid-security \
           -v `pwd`/mosquitto/ssl:/mosquitto/ssl \
           -v `pwd`/portal/www/ssl:/bdework/srv/www/ssl \
           -v `pwd`/mosquitto/config:/mosquitto/config \
           -v `pwd`/mongod/mongod.conf:/etc/mongod.conf \
           -v `pwd`/influxdb/influxdb.conf:/etc/influxdb/influxdb.conf \
           -v `pwd`/launcher/launcher.json:/usr/local/etc/bdeagent.json \
           -v `pwd`/server/bdeserver.json:/usr/local/etc/bdeserver.json \
           -v `pwd`/portal/config:/bdework/srv/www/bigdata-express-portal/config \
           -v `pwd`/logs:/bdework/logs \
           publicregistry.fnal.gov/bigdata_express/bdeheadnode:1.5-xenial

The output should be like below,

.. code-block:: console

   Starting mosquitto broker ...
   Starting mongodb server and initializing with admin & bde databases ...
   about to fork child process, waiting until server is ready for connections.
   forked process: 10
   child process started successfully, parent exiting
   MongoDB shell version v3.4.23
   connecting to: mongodb://127.0.0.1:27017
   MongoDB server version: 3.4.23
   Successfully added user: {
           "user" : "admin",
           "roles" : [
                   {
                           "role" : "userAdminAnyDatabase",
                           "db" : "admin"
                   }
           ]
   }
   MongoDB shell version v3.4.23
   connecting to: mongodb://127.0.0.1:27017
   MongoDB server version: 3.4.23
   Successfully added user: {
           "user" : "bde",
           "roles" : [
                   {
                           "role" : "readWrite",
                           "db" : "bde"
                   }
           ]
   }
   Starting influxdb server and initializing with bde ...
   Starting bde web portal ... 
   Starting bdelauncher ...
   
    ___ ___  ___     _                _    
   | _ )   \| __|   /_\  __ _ ___ _ _| |_  
   | _ \ |) | _|   / _ \/ _` / -_) ' \  _| 
   |___/___/|___| /_/ \_\__, \___|_||_\__| 
                        |___/              
       https://bigdataexpress.fnal.gov     
   
   -- Running commit 58e2702, branch mqtt, built on Dec 11 2019 at 21:20:59.
   -- Starting BDE Agent as a background process.
   -- Reading configuration from /usr/local/etc/bdeagent.json.
   -- Logging to logs/bdelauncher.log.
   
   -- Daemonizing...
   
   Starting bdeserver ...
   -- Running commit 58e2702, branch mqtt, built on Dec 11 2019 at 21:20:59.
   -- Starting BDE Server as a background process.
   -- Reading configuration from /usr/local/etc/bdeserver.json.
   -- Logging to logs/bdeserver.log.
   
   -- Daemonizing...



