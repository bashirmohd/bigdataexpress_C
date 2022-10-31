.. _build-bde-mdtmftp:

==========================================
Building BDE and mdtmftp+ from source code
==========================================

Please obtain the source code either from Fermilab Git repository, or
from a snapshot tarball.


.. _build-bde-prereq:

Install prerequisites
=====================

To build BDE server, the following tools are required:

* A C++ compiler that supports C++11.  GCC is recommended.
* CMake build tool (version 3.5 or better).
* Development packages for zlib, mosquitto, and OpenSSL.

To build mdtm library and mdtmftp+, the following tools are required:

* C++ and C compilers.
* GNU Autotools
* Development packages for OpenSSL, numactl, hwloc, lvm2 mosquitto,
  json-c, krb5, ncurses.

To install build tools and dependencies on RHEL and derivatives,
please run:

.. code-block:: console

  $ sudo yum install \
      gcc gcc-c++ make autoconf automake libtool m4 pkgconfig \
      numactl-devel hwloc-devel lvm2-devel libblkid-devel \
      openssl-devel mosquitto-devel json-c-devel \
      krb5-devel libtool-ltdl-devel ncurses-devel \
      libuuid-devel rrdtool-devel

You will also need `CMake <https://cmake.org/>`_ version 3.5 or 3.5+.
CMake on RHEL 7 is an older release.  Please follow instructions on
CMake website to install a newer version.

On Ubuntu, please run:

.. code-block:: console

   $ sudo apt update
   $ sudo apt install \
       gcc g++ make autoconf automake libtool m4 pkg-config cmake \
       libnuma-dev libhwloc-dev liblvm2-dev libblkid-dev \
       libssl-dev libmosquitto-dev libjson-c-dev \
       libkrb5-dev libltdl-dev libncurses5-dev \
       uuid-dev librrd-dev


.. _build-bde:

Building BDE source code
========================

If you check out source code from Git repository, please switch to the
``mqtt`` branch:

.. code-block:: console

  $ git clone ssh://p-bigdata-express@cdcvs.fnal.gov/cvs/projects/bigdata-express-server
  $ cd bigdata-express-server
  $ git checkout mqtt

Other dependencies are downloaded and built locally, by running the
shell script ``bootstrap_libraries.sh`` in source tree's top-level
directory:

.. code-block:: console

  $ cd bigdata-express-server
  $ ./bootstrap_libraries.sh

Then, build BDE server and agents:

.. code-block:: console

   $ mkdir build; cd build
   $ cmake -DCMAKE_INSTALL_PREFIX=/usr/local ..
   $ make

Note that we have omitted the usual ``make install`` step here.  The
``make`` command above will generate BDE server and Agent binaries,
``server/bdeserver`` and ``agent/bdeagent``, under the ``build``
directory:

* ``build/server/bdeserver`` is the BDE Server binary that will be
  copied and installed on the head node.

* ``build/agent/bdeagent`` is the BDE Agent binary that will be copied
  and installed on both **head node** and **DTNs**, with different
  configurations.  On **head node**, ``bdeagent`` will run as ``launch
  agent`` with a `launch agent configuration`.  On DTNs, ``bdeagent``
  will run as ``DTN agent`` with a `dtn agent configuration`.

The generated binaries are statically linked against the locally
installed libraries, and should run on GNU/Linux distributions where
other ABIs (glibc, zlib, OpenSSL, etc.) are compatible.  You should be
able to simply ``scp`` the binaries from the build host to the
partiuclar hosts.


Optional: building BDE test suite
---------------------------------

If you are developing BDE software, you may want to build the test
suite.  If you would like to enable building the test suite, please
pass the ``-DBDE_ENABLE_TESTING=ON`` parameter to CMake, and then run
``ctest`` or ``make test``:

.. code-block:: console

   $ cd build
   $ cmake -DCMAKE_INSTALL_PREFIX=/usr/local -DBDE_ENABLE_TESTING=ON ..
   $ ctest


Optional: building this manual
------------------------------

This manual itself belongs to ``bigdata-express-server`` source tree!
If you need to generate the manual, you will need `Sphinx`_ and
`LaTeX`_ software suites.  To enable generating this manual as part of
the build process, you can pass the flag ``-DBDE_ENABLE_MANUAL=ON``
parameter to CMake:

.. code-block:: console

   $ cd build
   $ cmake -DCMAKE_INSTALL_PREFIX=/usr/local -DBDE_ENABLE_MANUAL=ON ..
   $ make

Pass the flag ``-DBDE_ENABLE_PDF_MANUAL=ON`` to CMake to generate PDF
version of the manual.  You will need to have `LaTeX`_ installed for
this to work.

You can also build the manual without having to run CMake, like so:

.. code-block:: console

   $ cd bigdata-express-server/docs/manual
   $ make html

This would be useful if you are writing the manual in a macOS system.
You will just need to set up `Sphinx` in your machine.


Optional: faster builds with Ninja
----------------------------------

For faster builds, you can use `Ninja`_ instead of Make.  If you have
Ninja build system installed, just use the appropriate generator with
CMake:

.. code-block:: console

  $ mkdir build; cd build
  $ cmake -G Ninja ..
  $ ninja

.. _Sphinx: https://www.sphinx-doc.org/
.. _LaTeX: https://www.latex-project.org/
.. _Ninja: https://ninja-build.org/


.. _build-mdtmftp:

Building mdtm middleware and mdtmftp+
=====================================

First, build and install mdtm library:

.. code-block:: console

  $ git checkout ssh://p-mdtm4fnal@cdcvs.fnal.gov/cvs/projects/mdtm4fnal
  $ cd mdtm4fnal
  $ ./configure --prefix=/usr/local/mdtmftp+/current
  $ make
  $ make install

Then, build mdtmftp+ applications:

.. code-block:: console

  $ git checkout ssh://p-mdtm-app-gt@cdcvs.fnal.gov/cvs/projects/mdtm-app-gt
  $ cd mdtm-app-gt
  $ git checkout mdtmftp+
  $ ./configure --prefix=/usr/local/mdtmftp+/current \
        PKG_CONFIG_PATH=/usr/local/mdtmftp+/current/lib/pkgconfig
  $ make
  $ make install

After ``make install`` is completed, mdtmftp+ programs and supporting
libraries should have been installed under
``/usr/local/mdtmftp+/current``.

* ``/usr/local/mdtmftp+/current/bin/mdtm-ftp-client`` - mdtmftp+
  client that should be insatlled and run on head node.

* ``/usr/local/mdtmftp+/current/sbin/mdtm-ftp-server`` - mdtmftp+
  server that should be installed and run on each DTN.
