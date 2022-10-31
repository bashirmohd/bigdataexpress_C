=================================
BigData Express Docker repository
=================================

BDE project's Docker images are hosted at a registry maintained by
Fermilab

 ``publicregistry.fnal.gov`` 

First, configure Docker to trust Fermilab's certificate:

.. code-block:: console

   $ sudo mkdir -p /etc/docker/certs.d/publicregistry.fnal.gov
   $ wget https://bigdataexpress.fnal.gov/download/ca.crt -O ca.crt
   $ sudo mv ca.crt /etc/docker/certs.d/publicregistry.fnal.gov

Next, pull images from Fermilab's Docker registry:

.. code-block:: console

   $ docker pull publicregistry.fnal.gov/bigdata_express/bdeagent:1.5-xenial
   1.5-xenial: Pulling from bigdata_express/bdeagent
   96341445c139: Pull complete
   [...]
   721b5e1127f6: Pull complete
   Digest: sha256:e1cba1fd88030d96a400810c94984b9571cbd54b68045e9082df25303e17a32e
   Status: Downloaded newer image for publicregistry.fnal.gov/bigdata_express/bdeagent:1.5-xenial

If you have not installed the CA certificate, you will get an error
like this:

.. code-block:: console

   Error response from daemon: Get https://publicregistry.fnal.gov/v2/:
   x509: certificate signed by unknown authority
