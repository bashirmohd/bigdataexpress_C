
==========================================
Set up multiple docker images on head node
==========================================

First of all, please open a new terminal and create the BDE working directory ``/home/bde``:

.. code-block:: console

   $ mkdir -p /home/bde && cd /home/bde

.. _docker-run-mqtt:

Setting up MQTT
===============

BDE uses `Mosquitto`_, an implementation of the `MQTT`_ messaging
protocol, to communicate among different components.

.. _Mosquitto: https://mosquitto.org/
.. _MQTT: https://en.wikipedia.org/wiki/MQTT

Eclipse Mosquitto docker official images and instructions are available at 
https://hub.docker.com/_/eclipse-mosquitto

Run MQTT with Default Configuration
-----------------------------------

Run the default Eclipse Mosquitto docker image in detached mode. 
It will pull the image automatically from Mosquitto docker official registry.

.. code-block:: console

   $ docker run -dit \
      --name mosquitto \
      --net=host \
      -p 1883:1883 \
      -v /mosquitto/data \
      -v /mosquitto/log \
      eclipse-mosquitto

If successful, MQTT broker listens to the port 1883 and messages are exchanged
without encryption.

To check the status of the running MQTT broker, run

.. code-block:: console

   $ docker logs mosquitto


Run MQTT with Encryption
------------------------

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

.. note:: The directory ``mosquitto/ssl`` will be mounted to ``/mosquitto/config/ssl`` 
   inside the container.
   
3. Generate the configuration file ``mosquitto/mosquitto.conf``

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
   cafile /mosquitto/config/ssl/mqtt-ca.pem

   # Path to the PEM encoded server certificate.
   certfile /mosquitto/config/ssl/mqtt-cert.pem

   # Path to the PEM encoded keyfile.
   keyfile /mosquitto/config/ssl/mqtt-key.pem
   
   # Port to use for the default listener.
   port 8883
   
   
4. Launch the MQTT broker container in the detached mode:

.. code-block:: console

   $ docker run -dit \
      --name mosquitto \
      --net=host \
      -p 1883:1883 \
      -p 8883:8883 \
      -v $(pwd)/mosquitto/ssl:/mosquitto/config/ssl \
      -v $(pwd)/mosquitto/mosquitto.conf:/mosquitto/config/mosquitto.conf \
      -v /mosquitto/data \
      -v /mosquitto/log \
      eclipse-mosquitto
      
5. Check status

.. code-block:: console

   $ docker logs mosquitto
      

.. _docker-run-mongodb:

Setting up MongoDB
==================

1. Run MongoDB Server

Run the default `MongoDB`_ docker image. It will pull the image automatically 
from MongoDB docker official registry:

.. _MongoDB: https://docs.docker.com/samples/library/mongo/

.. code-block:: console

   $ docker run -d -p 27017-27019:27017-27019 --name mongodb mongo:3.4
   
2. Create BDE Users in MongoDB

Add user accounts, ``admin`` and ``bde`` in MongoDB. 

A quick approach is to launch a seperate MongoDB client on the same host to add the accounts.

.. code-block:: console
   
   $ docker run -it --network host --rm mongo mongo --host 127.0.0.1
   > use admin
   > db.createUser({user: 'admin', pwd: 'admin', roles:[{role: 'userAdminAnyDatabase', db: 'admin'}]})
   > use bde
   > db.createUser({user: 'bde', pwd: 'bde', roles:[{role: 'readWrite', db: 'bde'}]})

Quit the MongoDB client shell by entering ``Ctrl-C`` after the operations are completed.

.. _docker-run-influxdb:

Setting up InfluxDB
===================

1. Run the default `InfluxDB`_ docker image. It will pull the image automatically 
from InfluxDB docker official registry:

.. _InfluxDB: https://docs.docker.com/samples/library/influxdb/

.. code-block:: console

   $ docker run -d -p 8086:8086 -v influxdb:/var/lib/influxdb  --name influxdb influxdb
   
2. Create a ``bde`` database in influxDB. 

A quick approach is to launch a seperate InfluxDB client on the same host to create the DB.

.. code-block:: console  
 
   $ docker exec -it influxdb influx  
   Connected to http://localhost:8086 version 1.6.4
   InfluxDB shell version: 1.6.4
   > create database bde
   > quit

.. _set-up-bde-server:

Setting up BDE Server
=====================

1. Create the subdirectory ``server`` under the BDE working directory:

.. code-block:: console

   $ mkdir -p server

2. Pull BDE server docker image:

.. code-block:: console

   $ docker pull publicregistry.fnal.gov/bigdata_express/bdeaserver:1.5-xenial 

3. Follow instructions in :ref:`appendix-bde-server-config` to create the bde server 
configuration file ``bdeserver.json``, in JSON format.

.. literalinclude:: bdeserver.json
   :language: json
   :linenos:


4. Test the configuration:

.. code-block:: console

   $ docker run --name bdeserver -ti --rm --network host \
      -v $(pwd):/usr/local/etc \
      publicregistry.fnal.gov/bigdata_express/bdeserver \
      -c /usr/local/etc/bdeserver.json
   -- Running commit b784e7d, branch mqtt, built on Sep 16 2019 at 14:51:40.
   -- Starting BDE Server as a foreground process.
   -- Reading configuration from /usr/local/etc/bdeserver.json.
   -- Logging to console.

   [2019-09-17 21:10:26] WAN configured with:
           auth url:
           base url:

   [2019-09-17 21:10:26] Scheduler uses following configurations:
           SDN enabled:                      true
           SDN RPC timeout time:             10s
           Launching Daemon auth method:     certificate
           Launching Daemon write cmd:       false
           Checksum verification:            false
           Checksum algorithm:               sha1
           Group size:                       21474836480 (20GB)
           Network configurations:           null

   [2019-09-17 21:10:26] [BDEserver] : MQ server @ localhost / localhost : 1883
   [2019-09-17 21:10:26] [BDEserver] : Mongo server @ localhost / localhost : 27017
   [2019-09-17 21:10:26] starting bde server.
   [2019-09-17 21:10:26] using unencrpyted MQTT broker.
   [2019-09-17 21:10:26] mqtt connect error: Connection refused
   [2019-09-17 21:10:26] mqtt connect error: Connection refused
   [2019-09-17 21:10:26] mqtt connect error: Connection refused
   ^C^C
   [2019-09-17 21:10:56] exception catched:
   No suitable servers found: `serverSelectionTimeoutMS` expired: [connection refused calling ismaster on 'localhost:27017']: generic server error

Correct configuration errors if necessary. If successful, entry ``Ctrl-C`` to exit the test.

5. Launch BDE server in the detached mode:

.. code-block:: console

   $ docker run --name bdeserver -dit --network host \
      -v $(pwd)/server:/usr/local/etc/ \
      -v $(pwd)/mosquitto/ssl:/mosquitto/config/ssl \
      publicregistry.fnal.gov/bigdata_express/bdeaserver:1.5-xenial \
      -c /usr/local/etc/bdeserver.json

You can check the log from the docker container:

.. code-block:: console

   $ docker logs bdeserver


.. _set-up-bde-launcher:

Setting up BDE launching agent
==============================

1. Create the subdirectory ``launcher`` under BDE working directory:

.. code-block:: console

   $ mkdir -p launcher

2. Pull BDE launching agent image:

.. code-block:: console

   $ docker pull publicregistry.fnal.gov/bigdata_express/bdeagent:1.5-xenial

3. Create configuration file ``launcher/bdelauncher.json``. See instructions in 
   :ref:`appendix-bde-launcher-config` for details. 

.. literalinclude:: bdelauncher-tls.json
   :language: json
   :linenos:

.. note:: The file ``launcher/bdelauncher.json`` will be mounted
   to ``/usr/local/etc/bdeagent.json`` inside the container.

4. Create mdtmFTP+ configure file ``mdtmconfig.xml``, which will be mounted
to ``/bdework`` inside the container. 

5. Follow instructions in :ref:`head-node-trust-path` to setting up X.509
   security on head node.

.. note:: The directory ``/etc/grid-security`` will be mounted to
   ``/etc/grid-security`` inside the container.

6. Run bde launching agent in frontend:

.. code-block:: console

   $ docker run \
       --name bdelauncher \
       -- it \
       --network host \
       -v /etc/grid-security:/etc/grid-security \
       -v `pwd`/launcher/bdelauncher.json:/usr/local/etc/bdeagent.json \
       -v `pwd`/mosquitto/ssl:/mosquitto/config/ssl \
       -v $(pwd$):/bdework \
       --security-opt seccomp:unconfined \
       --privileged \
       publicregistry.fnal.gov/bigdata_express/bdeagent:1.5-xenial \
       -c /usr/local/etc/bdeagent.json -l bdelauncher.log

.. note:: The folder ``mosquitto/ssl`` will be mounted to ``/mosquitto/config/ssl`` in the container
          as described in :ref:`docker-run-mqtt`. If MQTT enables TLS/SSL secure communiction, 
          BDE launching agent will use the MQTT certificate to connect to the server.

Correct errors if necessary, otherewise, quit the container by entering ``Ctrl-C``.

7. Run bde launching agent in the detached mode:

.. code-block:: console

   $ docker run \
       --name bdelauncher \
       -- dit \
       --network host \
       -v /etc/grid-security:/etc/grid-security \
       -v $(pwd)/launcher/bdelauncher.json:/usr/local/etc/bdeagent.json \
       -v $(pwd)/mosquitto/ssl:/mosquitto/config/ssl \
       -v $(pwd):/bdework \
       --security-opt seccomp:unconfined \
       --privileged \
       publicregistry.fnal.gov/bigdata_express/bdeagent:1.5-xenial \
       -c /usr/local/etc/bdeagent.json -l bdelauncher.log


.. _set-up-portal:

Setting up BDE portal
=====================

1. Create the subdirectories, ``portal/config`` and ``portal/www/ssl``, under the BDE working directory:

.. code-block:: console

   $ mkdir -p portal/config && mkdir -p portal/www/ssl

2. Pull BDE portal docker image:

.. code-block:: console

   $ docker pull publicregistry.fnal.gov/bigdata_express/bdeportal:1.5-xenial

3. Prepare a BDE portal certificate/private key pair, and copy them to ``portal/www/ssl``,
   which will be mounted to ``/srv/www/ssl`` in the container. You can reuse the MQTT 
   certificate/private pair in the directory ``bde/mosquitto/ssl``.

.. note:: This step is not required if you plan to set up a reverse proxy in front of BDE portal.

4. Create BDE web portal configuration file  ``portal/config/local.js`` using the template below:

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

5. Create BDE web portal configuration file ``portal/config/connections.js``.

.. literalinclude:: connections.js
   :language: javascript
   :linenos:

6. Run the bdeportal container in detached mode:

.. code-block:: console

   $ docker run -dit \
      --name bdeportal \
      --network host \
      --mount type=bind,source=$(pwd)/portal/config/local.js,target=/srv/www/bigdata-express-portal/config/local.js \
      --mount type=bind,source=$(pwd)/portal/config/connections.js,target=/srv/www/bigdata-express-portal/config/connections.js \
      --mount type=bind,source=$(pwd)/mosquitto/ssl:/mosquitto/config/ssl \
      --mount type=bind,source=$(pwd)/poral/www/ssl,target=/srv/www/ssl \
      publicregistry.fnal.gov/bigdata_express/bdeportal:1.5-xenial \
      --port 5000

Change ``--port 5000`` to a listening port that you prefer to.

Once the bdeportal container starts successfully, check its status

.. code-block:: console

   $ docker logs bdeportal

and logs from the container should show up

.. code-block:: console

   using encrypted message queue server with server certificate
   info: 
   info:                .-..-.
   info: 
   info:    Sails              <|    .-..-.
   info:    v0.11.5             |\
   info:                       /|.\
   info:                      / || \
   info:                    ,'  |'  \
   info:                 .-'.-==|/_--'
   info:                 `--'-------' 
   info:    __---___--___---___--___---___--___
   info:  ____---___--___---___--___---___--___-__
   info: 
   info: Server lifted in `/srv/www/bigdata-express-portal`
   info: To see your app, visit https://localhost:5000
   info: To shut down Sails, press <CTRL> + C at any time.
   
   debug: --------------------------------------------------------
   debug: :: Thu Oct 10 2019 21:13:17 GMT+0000 (UTC)
   
   debug: Environment : development
   debug: Port        : 5000
   debug: --------------------------------------------------------
   connected

