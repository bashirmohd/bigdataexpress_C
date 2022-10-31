====================
Setting up head node
====================

On **BDE head node**, it is recommended to install and run:

- BDE web portal docker image
- BDE Server docker image
- BDE Launching Agent docker image

These components require several third-party software, which should
be installed first:

- MongoDB server docker image
- Mosquitto MQTT broker docker image
- InfluxDB time series database docker image

In addition, BDE uses X.509 certificates to identify users and hosts.
It is necessary to install the certificates of all the involved Certificate
Authorities (CAs) on **head node** to establish trust path.

See the following sections for details.

.. toctree::
   :maxdepth: 2

   set-up-head-node-trust.rst
   set-up-head-node-services.rst
   set-up-head-node-services-single.rst
