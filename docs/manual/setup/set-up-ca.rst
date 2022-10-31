.. _appendix-set-up-own-ca:

================================
Appendix: Setting up your own CA
================================

When setting up head node services with TLS/SSL enabled, we assume
that you have the necessary keys and certificates.  However, getting
signed certificate from a third-party vendor may not be a possibility
for you.

In those cases, and for testing purposes, you may want to set up a
Certificate Authority who can sign certificate requests.


.. _appendix-set-up-ca:

Set up a Certificate Authority
==============================

The below command will generate a certificate authority certificate
and key:

.. code-block:: console

   $ openssl req -new -x509 -days 3650 \
       -extensions v3_ca -keyout ca.key -out ca.crt

.. _appendix-gen-csr:

Generate keys and certificate signing requests
==============================================

Now, for each service, you will need to generate a key and a
certificate signing request.

To generate a server key:

..
   .. code-block:: console

      $ openssl genrsa -des3 -out server.key 2048

   Generate a server key without encryption:

.. code-block:: console

   $ openssl genrsa -out server.key 2048

Now you can generate a certificate signing request to send to the CA:

.. code-block:: console

   $ openssl req -out server.csr -key server.key -new


.. _appendix-sign-csr:

Sign the certificate signing requests
=====================================

Send the CSR to the CA, or sign it with your CA key:

.. code-block:: console

   $ openssl x509 -req -in server.csr \
       -CA ca.crt -CAkey ca.key -CAcreateserial \
       -out server.crt -days 365

Now you can deploy ``server.crt`` and ``server.key`` with MQTT,
MongoDB, and InfluxDB servers, and you can configure clients with the
CA certificate in ``ca.crt``.

