.. _dtn-trust-path:

=================================
Setting up X.509 security on DTN
=================================

.. _dtn-trust-path-cilogon:

Installing CA certificates
==========================

Setting up trust paths for user certificates
--------------------------------------------

Get CILogon CA budle from the `download
site`_:

.. _download site: https://ca.cilogon.org/downloads

.. code-block:: console

  $ cd ~
  $ wget https://cilogon.org/cilogon-ca-certificates.tar.gz

Create the directory ``/etc/grid-security/certificates`` if necessary:

.. code-block:: console

  $ sudo mkdir -p /etc/grid-security/certificates

Install CILogon CA bundle in ``/etc/grid-security/certificates``:

.. code-block:: console

  $ cd /etc/grid-security/certificates
  $ sudo tar zxvf ~/cilogon-ca-certificates.tar.gz --strip-components 2


.. _dtn-trust-keypair:

Setting up trust paths for host certificates
--------------------------------------------

Each DTN requries a host certificate. Admins at local and remote sites may request host certificates for their DTNs from different CAs. Please install all CA certificates under the path ``/etc/grid-security/certificates``.

Requesting and installing host certificate
==========================================

To acquire a host certificate for a DTN, you must submit a request to a Certificate Authority (CA).

We recommend requesting host certificates from one of the following CAs:


* Use `InCommon`_.  See :ref:`dtn-trust-incommon` for details.
* Use `LetsEncrypt`_.  See :ref:`dtn-trust-letsencrypt` for details.
* Use `Globus SimpleCA`_.  See :ref:`dtn-trust-simpleca` for details.

.. _LetsEncrypt: https://letsencrypt.org/
.. _Globus SimpleCA: https://gridcf.org/gct-docs/6.0/admin/install/appendix.html#gtadmin-simpleca

.. _dtn-trust-incommon:

Using InCommon
--------------

For a fixed annual fee, `InCommon`_ will issue unlimited host
certificates to its member institutions.  If your institution is a
`InCommon subscriber`_, you can use a host certificate issued by InCommon.
`Open Science Grid`_ has created tools for working with InCommon CA.

You can use `OSG PKI tools`_ to generate the certificate signing
request, and then submit it to InCommon for signing.  For more
information, see `OSG documentation on InCommon`_.

.. _InCommon: https://www.incommon.org/
.. _Open Science Grid: https://opensciencegrid.org/
.. _InCommon subscriber: https://www.incommon.org/federation/incommon-federation-participants/
.. _OSG PKI tools: https://github.com/opensciencegrid/osg-pki-tools
.. _OSG documentation on InCommon: https://opensciencegrid.org/docs/security/host-certs/#requesting-incommon-igtf-host-certificates

To generate a Certificate Signing Request (CSR) and a host key, you
will need the ``osg-cert-request`` tool from ``osg-pki-tools``
package, available in ``osg`` repository.  This CSR will be signed by
InCommon CA.  Follow the steps below:

1. Use instructions in `OSG documentation`_ for enabling the ``osg``
repository for RHEL and derivatives, and then install the package:

.. _OSG documentation: https://opensciencegrid.org/docs/common/yum/

.. code-block:: console

   $ sudo yum install osg-pki-tools

2. Generate a CSR and private key using the ``osg-cert-request`` tool:

.. code-block:: console

   $ osg-cert-request --hostname $HOST \
             --country $COUNTRY \
             --state $STATE \
             --locality $LOCALITY \
             --organization $ORGANIZATION

If successful, the CSR will be named ``$HOSTNAME.req`` and the private
key will be named ``$HOSTNAME-key.pem``.

3. Find your institution's InCommon contact , and submit the CSR that
you generated above to them.  Request a 1-year ``IGTF Server
Certificate`` for ``OTHER`` server software.

4. Download the host certificate only (not the full chain) signed by
your institution, and copy it to the DTN, along with key you generated
above.

5. Verify that the issuer ``CN`` field is ``InCommon IGTF Server CA``:

.. code-block:: console

   $ openssl x509 -in <PATH TO CERTIFICATE> -noout -issuer
   issuer= /C=US/O=Internet2/OU=InCommon/CN=InCommon IGTF Server CA

6. Install the host certificate and key, with the right permissions
and ownership.  Assuming that you copied ``hostcert.pem`` and
``hostkey.pem`` to your home directory:

.. code-block:: console

  $ sudo cp ~/hostcert.pem /etc/grid-security/hostcert.pem
  $ sudo cp ~/hostkey.pem /etc/grid-security/hostkey.pem
  $ sudo chown root:root /etc/grid-security/hostcert.pem /etc/grid-security/hostkey.pem
  $ sudo chmod 444 /etc/grid-security/hostcert.pem
  $ sudo chmod 400 /etc/grid-security/hostkey.pem


.. _dtn-trust-letsencrypt:

Using LetsEncrypt
-----------------

`LetsEncrypt <https://letsencrypt.org/>`_ is a Certificate Authority
that offers free and automated certificates.  Certificates issued by
LetsEncrypt are trusted by most browsers and operating system.

1. Install certbot package.

On RHEL derivatives, install cerbot from the ``epel`` repository

.. code-block:: console

  $ sudo yum install epel-release
  $ sudo yum install certbot

On Ubuntu systems, install certbot with:

.. code-block:: console

  $ sudo apt install certbot

2. Run the following command to obtain a host certificate with Let's
Encrypt:

.. code-block:: console

   $ sudo certbot certonly --standalone --email $ADMIN_EMAIL -d $HOST

Once the above command is finished, a key and LetsEncrypt-issued
certificate will be installed in the following paths:

* ``/etc/letsencrypt/live/$HOST/privkey.pem``
* ``/etc/letsencrypt/live/$HOST/cert.pem``

3. Make symbolic links to the below paths,
respectively:

* ``/etc/grid-security/hostkey.pem``
* ``/etc/grid-security/hostcert.pem``

Run the below commands to make the symbolic links:

.. code-block:: console

  $ sudo ln -s /etc/letsencrypt/live/$HOST/cert.pem /etc/grid-security/hostcert.pem
  $ sudo ln -s /etc/letsencrypt/live/$HOST/privkey.pem /etc/grid-security/hostkey.pem
  $ sudo chmod 0600 /etc/letsencrypt/archive/$HOST/privkey*.pem

.. _trust_osg_le: https://opensciencegrid.org/docs/security/host-certs/#requesting-host-certificates-using-lets-encrypt


.. _dtn-trust-simpleca:

Using SimpleCA
--------------

Alternatively, you can use SimpleCA to generate and issue your own host certificates by following the instructions:

1. Choose a node, which is called ``CA-Node``, to install and run SimpleCA.

2. Set up a SimpleCA instance on ``CA-Node``:

.. code-block:: console

  $ sudo yum install globus-simple-ca

This package's post-install scripts will generate the necessary setup in ``/var/lib/globus/simple_ca/``.

3. On ``CA-Node``, run the command ``grid-ca-package`` to generate a package for the Simple CA root certificate:

* On RHEL and derivatives

.. code-block:: console

  $ grid-ca-package -r
  The available CA configurations installed on this host are:

  Directory: /etc/grid-security/certificates

  [... elided ...]

  7) 72e85ee6 -  /O=Grid/OU=GlobusTest/OU=simpleCA-certs.fnal.gov/CN=Globus Simple CA

  [... elided ...]

  Enter the index number of the CA to package [q to quit]: 7
  Creating RPM source tarball... done
  	globus_simple_ca_72e85ee6.tar.gz
  Creating RPM binary...

  [... elided ...]

  $ ls globus*
  globus-simple-ca-72e85ee6-1.0-1.el7.noarch.rpm  globus_simple_ca_72e85ee6.tar.gz

* On Ubuntu (or Debian)

.. code-block:: console

  $ grid-ca-package -d


4. Install SimpleCA root certificate package in BDE nodes

Copy the generated package from ``CA-Node`` to your nodes, and
install them locally using ``rpm`` or ``dpkg`` command.

This should install our SimpleCA root certficate in ``/etc/grid-security/certficates``.

If you have nodes running Ubuntu or Debian, use `Alien <https://wiki.debian.org/Alien>`_ to convert the RPM package to a DEB package.

5. Generate a host certificate request

.. code-block:: console

   $ mkdir -p ~/certs/nci/${host}
   $ cd ~/certs/nci/${host}
   $ grid-cert-request -dir `pwd` -host ${host} -ip ${IPv4}

Replace ``${host}`` with the DTN's DNS name, and ``${IPv4}`` with its
IPv4 address.  If the DTN does not have a DNS name, omit ``-host
${host}`` from arguments to ``grid-cert-request``.

Copy ``hostcert_request.pem`` to ``CA-Node``.

6. At ``CA-Node``, sign the host certificate request:

.. code-block:: console

   $ sudo grid-ca-sign -in hostcert_request.pem -out hostsigned.pem

7. Verify the certificate:

.. code-block:: console

   $ openssl x509 -in hostsigned.pem -text | head -n 12

The signed host certificate is named ``hostsigned.pem``.Please rename
it to ``hostcert.pem``:

.. code-block:: console

   $ mv hostsigned.pem hostcert.pem

8. Intall host certificate in DTN

Once ``hostkey.pem`` and ``hostcert.pem`` are ready, copy them to the
folder ``/etc/grid-security`` in the DTN, with appropriate permissions:

.. code-block:: console

   $ cd /etc/grid-security
   $ sudo chown root:root hostcert.pem hostkey.pem
   $ sudo chmod 0600 hostkey.pem
   $ sudo chmod 0644 hostcert.pem


Verifying trust paths
=====================

Once you have set up the necessary X.509 certificate trust paths, you
should test that things work as expected.  For this, you would need to
use `gsissh`_, a special version of ``ssh`` built for grid systems.

.. _gsissh: http://grid.ncsa.illinois.edu/ssh/

Please see the section :ref:`set-up-test-with-gsissh` for details on
how to do this.


What if things did not go well?
===============================

If you need additional support, please get in touch
with BDE team at Fermilab.
