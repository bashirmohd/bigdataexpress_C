.. _head-node-trust-path:

Setting up X.509 security on head node
======================================

BDE uses X.509 certificates to identify users and DTNs. The
certificate authorities (CAs) provide the trust roots for the public
key infrastructure BDE uses to maintian integrity of its services.

At **head node**, it is necessary to install all CA certificates under
the path ``/etc/grid-security/certificates``, which include:

* CILogon CA bundle. BDE uses CILogon to issue user certificates.

* The certificates of the CAs that issue host certificates for DTNs at
  local and remote sites.


Setting up trust paths for user certificates
--------------------------------------------

To set up trust paths for user certificates, install CILogon CA
bundle.

First, obtain CILogon CA budle from the `download site`_:

.. _download site: https://ca.cilogon.org/downloads

.. code-block:: console

  $ cd ~
  $ wget https://cilogon.org/cilogon-ca-certificates.tar.gz

Then, create the directory ``/etc/grid-security/certificates`` if necessary:

.. code-block:: console

  $ sudo mkdir -p /etc/grid-security/certificates

Finally install CILogon CA bundle in ``/etc/grid-security/certificates``:

.. code-block:: console

  $ cd /etc/grid-security/certificates
  $ sudo tar zxvf ~/cilogon-ca-certificates.tar.gz --strip-components 2


Setting up trust paths for host certificates
--------------------------------------------

Each DTN requries a host certificates. Admins at local and remote
sites may request host certificates for their DTNs from different
CAs. Please install all required CA certificates under the path
``/etc/grid-security/certificates``.
