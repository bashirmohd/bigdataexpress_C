Setting up head node services
=============================

On **head node**, it is recommended to install and run:

- BDE web portal
- BDE Server
- BDE Launching Agent
- mdtmftp+ client

These components rely on several third-party software, which should
be installed first:

- MongoDB server
- Mosquitto MQTT broker
- InfluxDB time series database

In addition, BDE uses X.509 certificates to identify users and hosts. 
It is necessary to install the certificates of all the involved Certificate 
Authorities (CAs) on **head node** to establish trust path.

It is recommended to create a user account ``bde`` to run BDE software on **head node**.

Following sections will discuss setting up the head node software.

.. toctree::

   set-up-mongodb.rst
   set-up-mqtt.rst
   set-up-influxdb.rst
   set-up-bde-server.rst
   set-up-bde-launcher.rst
   set-up-bde-portal.rst
   set-up-head-node-trust.rst
