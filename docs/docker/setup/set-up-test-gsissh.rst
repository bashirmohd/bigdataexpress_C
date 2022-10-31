.. _set-up-test-with-gsissh:

==================================
Verifying trust paths using gsissh
==================================

In :ref:`head-node-trust-path` and in :ref:`dtn-trust-path`, we
discussed installing certificates in your site's nodes. You can verify
if you have installed all the necessary certificates correctly by
using `GSI-enabled OpenSSH`_, also known as ``gsissh``.

.. _GSI-enabled OpenSSH: http://grid.ncsa.illinois.edu/ssh/

You will install gsissh server instances on each of the DTNs, and a
client on the head node, and then make sure you can connect to DTNs
from the head node without errors.

Follow the below steps:

1. Install gsissh server on the DTN:

   .. code-block:: console

      $ sudo yum install gsi-openssh-server

2. Change the default gsissh port

   With the default configuration in ``/etc/gsisshd/sshd_config``,
   gsissh will attempt to run on port 22, but it will likely conflict
   with any "regular" ssh services that we have on that port. Pleae
   change the default gsissh port:

   .. code-block:: console

      $ sudo vim /etc/gsisshd/sshd_config # Change port to 2202

3. On the DTN, start gsissh server in debug mode:

   .. code-block:: console

      $ sudo systemctl stop gsisshd
      $ sudo /usr/sbin/gsisshd -d

4. Install client on the head node:

   .. code-block:: console

      $ sudo yum install gsi-openssh-clients

5. Get a user certificate from `CILogon <https://cilogon.org/>`_, and
   install it in ``~/.globus/usercred.p12``.

   CILogon-issued certificates will have a subject like
   :cn:`/DC=org/DC=cilogon/C=US/O=Fermi National Accelerator
   Laboratory/OU=People/CN=BDE User/CN=UID:bdeuser`.  Note down the
   subject from CILogon website when you get the certficate. You will
   need that in step 6.

   Use ``chmod`` to protect it such that only you can read it:

   .. code-block:: console

      $ chmod 0600 ~/.globus/usercred.p12

   (For details, see CILogon FAQ entry: "`Can I download a CILogon
   certificate to my computer?
   <https://www.cilogon.org/faq#h.p_ID_106>`_".)

6. From ``~/.globus/usercred.p12``, you will have to create a
   password-free ``userkey.pem`` and ``usercert.pem``:

   .. code-block:: console

      $ cd ~/.globus
      $ openssl pkcs12 -in usercred.p12 -nokeys -out usercert.pem
      $ openssl pkcs12 -in usercred.p12 -nocerts -out userkey.pem
      $ chmod 0600 userkey.pem

   Enter password for ``~/.globus/usercred.p12`` when prompted.  The
   resulting key and certificate will not have a password.

7. On the DTN, edit ``/etc/grid-security/grid-mapfile``, so that there
   is a mapping between certificate's Common Name (CN) part and the
   local user.  You will basically need to add a line like below:

   .. code-block:: shell

      "/DC=org/DC=cilogon/C=US/O=Fermi National Accelerator Laboratory/OU=People/CN=BDE User/CN=UID:bdeuser" bde

   You can find the subject on your ``usercert.pem`` using an
   ``openssl`` command:

   .. code-block:: console

      $ openssl x509 -in ~/.globus/usercert.pem -noout -subject
      subject=DC = org, DC = cilogon, C = US, O = Fermi National Accelerator Laboratory, OU = People, CN = BDE User, CN = UID:bdeuser


8. From head node, try connecting to the server running on DTN:

   .. code-block:: console

      $ gsissh -p 2202 $USER@$DTN \
          -o PasswordAuthentication=no \
          -o PubkeyAuthentication=no \
          -o GSSAPIAuthentication=yes

Replace ``$USER`` and ``$DTN`` as appropriate.

If the above steps worked well, you had better get a shell on the DTN.
Otherwise, use the output of both the client and the server to figure
out what went wrong.

If things went well, and if you need gsissh again, you can enable and
start the service:

.. code-block:: console

  $ sudo systemctl enable gsisshd
  $ sudo systemctl start gsisshd

