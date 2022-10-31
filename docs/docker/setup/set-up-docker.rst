.. _running-docker:

==============
Running Docker
==============

This section provides some quick notes on getting started with Docker.

Please refer to official `Docker documentation`_ for complete,
accurate, and most up-to-date documentation on getting and running
Docker.

.. _Docker documentation: https://docs.docker.com/


Install Docker
==============

On Ubuntu:

.. code-block:: console

   $ sudo apt update
   $ sudo apt install docker.io

On Red Hat Enterprise Linux and derivatives:

.. code-block:: console

   $ sudo yum install docker


Run Docker
==========

Check your Docker installation works by running ``hello-world``, which
will pull the ``hello-world:latest`` image from Docker hub and run it:

.. code-block:: console

   $ docker run hello-world

You may get an error message like this:

.. code-block:: console

   Got permission denied while trying to connect to the Docker daemon
   socket at unix:///var/run/docker.sock [..]: dial unix
   /var/run/docker.sock: connect: permission denied

In this case, assuming that you have the username ``bde`` and that you
are using a filesystem that supports ACLs, you can give yourself
read/write access to ``/var/run/docker.sock`` like so:

.. code-block:: console

   $ sudo setfacl -m user:bde:rw /var/run/docker.sock

Check that the user ``bde`` has read/write access to ``/var/run/docker.sock``:

.. code-block:: console

   $ getfacl /var/run/docker.sock
   getfacl: Removing leading '/' from absolute path names
   # file: var/run/docker.sock
   # owner: root
   # group: docker
   user::rw-
   user:bde:rw-
   group::rw-
   mask::rw-
   other::---

In the event you are unable to use ``setfacl``, you can add yourself
to the group ``docker``:

.. code-block:: console

   $ sudo usermod -a -G docker bde

You will need to log out, and log back in for this to take effect.

Now ``docker run hello-wolrd`` should work:

.. code-block:: console

   $ docker run hello-world
   Unable to find image 'hello-world:latest' locally
   Trying to pull repository docker.io/library/hello-world ...
   latest: Pulling from docker.io/library/hello-world
   1b930d010525: Pull complete
   Digest: sha256:b8ba256769a0ac28dd126d584e0a2011cd2877f3f76e093a7ae560f2a5301c00
   Status: Downloaded newer image for docker.io/hello-world:latest

   Hello from Docker!
   This message shows that your installation appears to be working correctly.

   To generate this message, Docker took the following steps:
    1. The Docker client contacted the Docker daemon.
    2. The Docker daemon pulled the "hello-world" image from the Docker Hub.
       (amd64)
    3. The Docker daemon created a new container from that image which runs the
       executable that produces the output you are currently reading.
    4. The Docker daemon streamed that output to the Docker client, which sent it
       to your terminal.

   To try something more ambitious, you can run an Ubuntu container with:
    $ docker run -it ubuntu bash

   Share images, automate workflows, and more with a free Docker ID:
    https://hub.docker.com/

   For more examples and ideas, visit:
    https://docs.docker.com/get-started/
