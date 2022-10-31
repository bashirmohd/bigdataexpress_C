
.. _set-up-docker-dtn:

==================================
Setting up BDE docker image on DTN
==================================

1. Create a working directory.

Please open a new terminal and create the BDE working directory ``/home/bde``:

.. code-block:: console

   $ mkdir -p /home/bde && cd /home/bde


2. Pull BDE DTN docker image:

.. code-block:: console

   $ docker pull publicregistry.fnal.gov/bigdata_express/bdeagent:1.5-xenial

3. If MQTT enalbes TLS/SSL for secure communiciaton, copy the MQTT server certificate
   file to the working directory. 

.. note:: DTN agent will use the MQTT server certificate to connect to the server.

4. Create a BDE Agent configuration file ``bdeagent.json`` in your working directory
   using the configuration file from :ref:`appendix-bde-agent-config` as a template.

An example bde agent configuratio file follows:

.. literalinclude:: bdeagent.json
   :language: json
..   :linenos:


5. Create a `Telegraf`_ configuration file ``telegraf.json`` in your working directory.
   
.. _Telegraf: https://www.influxdata.com/time-series-platform/telegraf/

.. code-block:: ini

  [[outputs.influxdb]]
    urls = ["http://head-node.example.net:8086"] # required
    database = "telegraf" # required

Replace ``head-node.example.net`` with head node's DNS name or IPv4
address.

6. Create the enviroment variable `HAVE_TELEGRAF` to set up the `Telegraf`_ service.

.. code-block:: ini

  export HAVE_TELEGRAF="yes"
  

7. Follow `mdtmFTP`_ manuals to set up mdtmFTP+ server configuration files
   ``mdtmconfig.xml`` and ``server.conf`` in the working directory.

.. _mdtmFTP: https://mdtm.fnal.gov/downloads/manual/index.html

8. Create user accounts and set permissions

In standard environment, when a user transfer files between DTNs, he must have an account
on each system.

In docker environment, two sets of user account will be involved, `container user account`
and `host user account`. When a user is created into a container, this user may not
be known for host machine. In this case, if a host volume is mounted into this container,
there may be permission denied issues.

To avoid such permission denied issues, we recommend adopting an 1-to-1 mapping policy bewteen
container and host:

* for each user created in container, set a dedicated uid
* for each group created in container, set a dedicated gid
* on host, create a ``docker`` user with those dedicated uid/gid, and manage permission  

Here is an example on how to set up an acocunt ``bdeuser1`` and group ``bdegroup`` 
in container and in host.

First, launch ``bdeagent:1.5-xenial`` docker image interactively.

.. code-block:: console

   $ sudo docker run -it \
       --name create_account \
       publicregistry.fnal.gov/bigdata_express/bdeagent:1.5-xenial \
       /bin/bash

Next, create user account ``bdeuser1`` within the container. 

.. code-block:: console

   $ groupadd -g 4001 bdegroup
   $ useradd -u 4001 -d /home/bdeuser1 --create-home --shell /bin/bash bdeuser1
   $ usermod -g bdegroup bdeuser1
     
Then, exit from the container, save the container to a new image following `Docker`_ instructions.

.. _Docker: https://docs.docker.com/engine/reference/commandline/commit/

.. code-block:: console

   $ sudo docker commit create_account bdeagent:updated
   
Afterward, create user ``bdeuser1`` with dedicated uid/gid in the host.

.. code-block:: console

   $ groupadd -g 4001 bdegroup
   $ useradd -u 4001 -d /home/bdeuser1 --create-home --shell /bin/bash bdeuser1 
   $ usermod -g bdegroup bdeuser1

Finally, set appropriate permission for ``bdeuser1`` to access data transfer folders in the host.

9. Test BDE Agent configruation files with the newly generated image. 
You can run the bdeagent container as root.

The following folders in the host will be mounted in the container:

* ``/home/bde/`` in the host --> ``/bdework`` in the container.
* ``/etc/grid-security`` in the host -->  ``/etc/grid-security`` in the container.
*  ``/data`` in the host -->  ``/data`` in the container.
* ``$pwd/telegraf.conf`` in the host --> ``/etc/telegraf/telegraf.conf``

.. code-block:: console

   $ sudo docker run --rm -u root \
       --name dtnagent \
       -e HAVE_TELEGRAF \
       -v /etc/grid-security:/etc/grid-security \
       -v $pwd/bdeagent.json:/usr/local/etc/bdeagent.json \
       -v /usr/local/etc/bde:/usr/local/etc/bde \
       -v $pwd:/bdework \
       -v /data:/data \
       -v $pwd/telegraf.conf:/etc/telegraf/telegraf.conf \
       --net=host \
       --security-opt seccomp:unconfined \
       --privileged \
       bdeagent:updated \
       -c /usr/local/etc/bdeagent.json -l bde.log

Once the container starts successfully, check its status,
 
.. code-block:: console

   $ sudo docker logs dtnagent
  
The information about bdeagent within the container will show up like
 
.. code-block:: console

       ___ ___  ___     _                _    
   | _ )   \| __|   /_\  __ _ ___ _ _| |_  
   | _ \ |) | _|   / _ \/ _` / -_) ' \  _| 
   |___/___/|___| /_/ \_\__, \___|_||_\__| 
                        |___/              
       https://bigdataexpress.fnal.gov     
   
   -- Running commit b784e7d, branch mqtt, built on Sep 16 2019 at 17:39:36.
   -- Starting BDE Agent as a foreground process.
   -- Reading configuration from /usr/local/etc/bdeagent.json.
   -- Logging to console.
   
   [2019-10-10 20:49:33] [Main] Starting BDE agent 1.0.0.0...
   [2019-10-10 20:49:33] [Main] Running commit b784e7d: update
   [2019-10-10 20:49:33] [AgentManager] Bootstrapping DTN module.
   [2019-10-10 20:49:33] [DTN Agent] Will use management_interface (enp4s0f0) IPv4 address for mdtmftp+ control channel.
   [2019-10-10 20:49:33] [DTN Agent] Translated "ip-of(&management_interface)" to 131.225.2.29.
   [2019-10-10 20:49:33] [DTN Agent] Will use data_interface[0] (ens6.2) IPv4 address for mdtmftp+ data channel.
   [2019-10-10 20:49:33] [DTN Agent] Translated "ip-of(&data_interfaces[0])" to 192.2.2.1.
   [2019-10-10 20:49:33] [DTN Agent] Setting use_hostname_for_control=true
   [2019-10-10 20:49:33] [DTN Agent] Checksum will use up to 256 threads for files larger than 100000000 bytes.
   [2019-10-10 20:49:33] [DTN Agent] Bootstrapping with ID: 0c:c4:7a:ab:63:7e, name: bde1.fnal.gov
   [2019-10-10 20:49:33] [DTN Agent] Created iozone work directory for /data1 at /data1/bde-iozone/work-NmJ32L.
   [2019-10-10 20:49:33] [DTN Agent] Bringing up LocalStorageAgent for device /dev/nvme0n1

