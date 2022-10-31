.. _set-up-mqtt:

Setting up MQTT
===============

BDE uses `Mosquitto`_, an implementation of the `MQTT`_ messaging
protocol, to communicate among different components.

.. _Mosquitto: https://mosquitto.org/
.. _MQTT: https://en.wikipedia.org/wiki/MQTT


Install Mosquitto
-----------------

On RHEL derivatives, install mosquitto from the ``epel`` repository:

.. code-block:: console

  $ sudo yum install epel-release
  $ sudo yum install mosquitto

On Ubuntu, mosquitto should be available from the official
repositories:

.. code-block:: console

  $ sudo apt install mosquitto

Enable and start the ``mosquitto`` service:

.. code-block:: console

  $ sudo systemctl enable mosquitto
  $ sudo systemctl start mosquitto

By default, Mosquitto will listen on port 1833,
and the messages will not be encrypted.  To encrypt the message bus,
please configure SSL/TLS support for Mosquitto.


Configure SSL/TLS for Mosquitto (optional)
------------------------------------------

To enable SSL/TLS connections with Mosquitto, a key and certificate
must be available, and the certificate should be signed by a CA.

Update Mosquitto configuration at ``/etc/mosquitto/mosquitto.conf``
with paths to the CA certificate, server certificate, and server key:

.. code-block:: ini

   # cafile defines a file containing the CA certificates.
   cafile /etc/ssl/mqtt-ca.crt

   # Path to the PEM encoded server certificate.
   certfile /etc/ssl/mqtt-cert.pem

   # Path to the PEM encoded keyfile.
   keyfile /etc/ssl/mqtt-key.pem

Restart Mosquitto service:

.. code-block:: console

  $ sudo systemctl restart mosquitto

Test the setup by starting a MQTT subscriber on a ``test``
topic, using the ``mosquitto_sub`` command:

.. code-block:: console

  $ mosquitto_sub -h host -p 8883 \
      --cafile /etc/ssl/mqtt-ca.crt -t test

You should be able to publish a message on ``test`` topic with
``mosquitto_pub``:

.. code-block:: console

  $ mosquitto_pub -h host -p 8883 \
      --cafile /etc/ssl/mqtt-ca.crt -t test -m "Hello world!"

The subscriber should be able to read the message you published, and
print it on its console.
