===============
Troubleshooting
===============

.. note::

   Please report all errors, failures, and other general difficulties
   to us.  We will keep updating this section as more feedbacks are gathered.


Check BDE logs
==============

When things do not work as expected, please view BDE logs for status messages, warnings, and errors:

* On head node:

   * BDE server logs: ``/var/lib/bde/log/bdeagent.log`` on head node.
   * Web portal logs: run ``journalctl -xef -u bdeportal.service``.
   * BDE launcher agent logs: ``/tmp/bdelauncher.log``.

* On DTNs:

  * mdtmftp+ server logs: ``/tmp/mdtmftp.log``.
  * BDE Agent logs: ``/tmp/bdeagent.log``.


Check BDE services
==================

Since BDE software is typically deployed as systemd services, please check whether BDE services are running normally.  

On head node::

  $ sudo systemctl status bdeserver bdelauncher bdeportal

On DTN::

  $ sudo systemctl status bdeagent

If any of these services failed, do one of the following:

  * See ``journalctl`` output to see what went wrong.
  * See the service's log to see what went wrong.
  * Try launching the programs from command line, preferably as a
    foreground process, and look for error messages.


Check basic services
====================

Check whether BDE dependent services are running normally:

  $ sudo systemctl status mosquitto mongodb influxdb


Host certificate errors
=======================

You may come across the following errors when using gsissh client to connect to
a server.

"globus_gsi_gssapi: SSL handshake problems"
-------------------------------------------

If ``gsissh`` client prints this::

  debug1: Calling gss_init_sec_context
  debug1: Delegating credentials
  debug1: Received GSSAPI_CONTINUE
  debug1: Calling gss_init_sec_context
  debug1: Delegating credentials
  debug1: GSS Major Status: Authentication Failed

  GSS Minor Status Error Chain:
  globus_gsi_gssapi: SSL handshake problems
  OpenSSL Error: ../crypto/asn1/a_verify.c:171: in library: asn1 encoding routines, function ASN1_item_verify: EVP lib
  OpenSSL Error: ../crypto/rsa/rsa_ossl.c:586: in library: rsa routines, function rsa_ossl_public_decrypt: padding check failed
  OpenSSL Error: ../crypto/rsa/rsa_pk1.c:67: in library: rsa routines, function RSA_padding_check_PKCS1_type_1: invalid padding

  gss_init_context failed

... and ``gsisshd`` server prints this::

  debug1: Wait SSH2_MSG_GSSAPI_INIT [preauth]
  debug1: Got no client credentials
  debug1: Sending GSSAPI_CONTINUE [preauth]
  debug1: Wait SSH2_MSG_GSSAPI_INIT [preauth]
  ssh_packet_read: Connection closed [preauth]
  debug1: do_cleanup [preauth]
  debug1: monitor_read_log: child log fd closed
  debug1: do_cleanup
  debug1: Killing privsep child 15531

Please change ``/etc/gsissh/sshd_config`` in the DTN::

  PasswordAuthentication no
  ChallengeResponseAuthentication no
  KerberosAuthentication no

  GSSAPIAuthentication yes
  GSSAPIDelegateCredentials yes
  GSSAPICleanupCredentials yes
  GSSAPIStrictAcceptorCheck yes
  GSSAPIKeyExchange yes

  UsePAM yes


"POSSIBLE BREAK-IN ATTEMPT!"
----------------------------

If ``gsissh`` client crashes with a message like::

  debug1: Authenticating to dtn.example.net:2202 as 'bde'
  reverse mapping checking getaddrinfo for dtn.example.net [1.2.3.4] failed - POSSIBLE BREAK-IN ATTEMPT!
  gsissh: globus_object.c:107: globus_object_assert_valid: Assertion `object->type->parent_type == object->parent_object->type' failed.
  Aborted

... and ``gsisshd`` server prints::

  debug1: SSH2_MSG_KEXINIT sent [preauth]
  Connection reset by 162.244.229.106 port 39742 [preauth]
  debug1: do_cleanup [preauth]
  debug1: monitor_read_log: child log fd closed
  debug1: do_cleanup
  debug1: Killing privsep child 16216

... you may want to correct the DNS name of the DTN, or edit ``/etc/hosts``
to enforce the DTN's hostname.


"Can't get X509 name entry from subject"
----------------------------------------

This problem was encountered in Ubuntu. When running ``grid-proxy-init``, it
may print out error messages like::

  $ grid-proxy-init -debug

  User Cert File: /home/fermilab/.globus/usercert.pem
  User Key File: /home/fermilab/.globus/userkey.pem

  Trusted CA Cert Dir: (null)

  Output File: /tmp/x509up_u1002
  Your identity: /DC=org/DC=cilogon/C=US/O=Fermi National Accelerator Laboratory/OU=People/CN=Sajith Sasidharan/CN=UID:sajith
  Enter GRID pass phrase for this identity:
  Creating proxy ...............+++
  ...............................................+++
  Error: Couldn't create proxy certificate.
         grid_proxy_init.c:908: globus_gsi_proxy: Error with the proxy handle
  globus_credential: Error with credential's certificate
  globus_cert_utils: Error getting name entry of subject: Can't get X509 name entry from subject

The problem was caused by an incompatible mix of installed libraries: 
some packages were built for Ubuntu Bionic, some were for Ubuntu Xenial.

If you are on Ubuntu Bionic: uninstall all Xenial packages, remove (or
comment out) everything related to Xenial and Globus/GSI from
``/etc/apt/sources.list.d/``, and install Bionic packages.

If you are on Ubuntu Xenial, do the inverse: remove all Bionic packages,
and only install Xenial packages.
