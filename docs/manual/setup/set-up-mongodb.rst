.. _set-up-mongodb:

Setting up MongoDB
==================

MongoDB is typically available from the package repositories
maintained by MongoDB, Inc. Please refer to `MongoDB documentation
<https://docs.mongodb.com/manual/administration/install-on-linux/>`_ on how to install MongoDB on the distribution of your choice.

Install MongoDB
---------------

On RHEL and derivatives, first create
``/etc/yum.repos.d/mongodb-org-4.0.repo`` with the following lines:

.. code-block:: ini

  [mongodb-org-4.0]
  name=MongoDB Repository
  baseurl=https://repo.mongodb.org/yum/redhat/$releasever/mongodb-org/4.0/x86_64/
  gpgcheck=1
  enabled=1
  gpgkey=https://www.mongodb.org/static/pgp/server-4.0.asc

Then, install mongodb:

.. code-block:: console

  $ sudo yum install -y mongodb-org

On Ubuntu, install MongoDB from official repositories. Otherwise, please follow `MongoDB documentation for Ubuntu
<https://docs.mongodb.com/manual/tutorial/install-mongodb-on-ubuntu/>`_.

Finally, enable/start the ``mongod`` service:

.. code-block:: console

  $ sudo systemctl enable mongod
  $ sudo systemctl start mongod


Create BDE Users in MongoDB
---------------------------

Use ``mongo`` shell to create MongoDB ``admin`` and ``bde`` users:

.. code-block:: console

  $ mongo
  > use admin
  > db.createUser({user: 'admin', pwd: 'admin', roles:[{role: 'userAdminAnyDatabase', db: 'admin'}]})
  > use bde
  > db.createUser({user: 'bde', pwd: 'bde', roles:[{role: 'readWrite', db: 'bde'}]})

Restart MongoDB service:

.. code-block:: console

  $ sudo systemctl restart mongod

After restarting, database access is restricted to ``admin`` and
``bde`` users.

.. code-block:: console

  $ mongo admin -u admin -p admin
  $ mongo bde -u bde -p bde


Configure MongoDB
-----------------

Edit ``/etc/mongod.conf`` so that MongoDB server is accessible from DTNs.


Listen on a non-local address
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

By default, MongoDB server listens on port 27017, and only on the local interface. Only clients running on the same host can connect to it. 

Please **comment out** the line that begins with ``bindIP``:

.. code-block:: ini

   net:
     port: 27017
     # Uncomment to listen only on local interface.
     # bindIp: 127.0.0.1

Add DTN IPs to ``bindIP``:

.. code-block:: ini

   net:
     port: 27017
     bindIp: 127.0.0.1,DTN IP1, DTN IP2,...

Note that the above examples use a YAML-based configuration format.
MongoDB introduced support for YAML configuration since version 2.6.
Older configuration format is still supported, and it could be the
case that your installation has shipped with a configuration file in
the older format.  In that case, find the ``bind_ip`` line, and change
it.

.. code-block:: ini

   # Comma separated list of ip addresses to listen on (all local ips by default)
   bind_ip = 127.0.0.1,DTN IP1, DTN IP2,...

   # Specify port number (27017 by default)
   # port = 27017

See MongoDB v2.4 documentation on `Configuration File Options`_ for
more information about the older file format.

.. _Configuration File Options: https://docs.mongodb.com/v2.4/reference/configuration-options/

Test your MongoDB setup
~~~~~~~~~~~~~~~~~~~~~~~

Restart MongoDB service:

.. code-block:: console

  $ sudo systemctl restart mongod


Test if you can connect to the MongoDB instance on the local head node

you can run ``mongo`` shell without any command-line options to connect to a MongoDB instance runing on your localhost.

.. code-block:: console

  $ mongo

Test if you can connect to the MongoDB instace @head node from a DTN

At a DTN, you can sepcify the username, authentication database, and optionally the password in the connection string. 

.. code-block:: console

  $ mongo --username bde --password bde --authenticationDatabase bde --host head.node --port 27017

