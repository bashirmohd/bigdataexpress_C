.. Big Data Express documentation master file, created by
   sphinx-quickstart on Thu Jul 11 10:51:15 2019.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

====================================
BigData Express administrator manual
====================================

Welcome to BigData Express project administrator manual.

**Big Data Express** (BDE) is a data transfer tool developed at `Fermi
National Accelerator Laboratory <https://www.fnal.gov/>`_. BDE seeks
to provide Schedulable, Predictable, and High-Performance data
transfer service in big data and high-performance computing
environments. Please visit `BDE project website
<https://bigdataexpress.fnal.gov/>`_ for more details.

**mdtmFTP** is BDE's default data transfer engine. Detailed
information on mdtmFTP is available at `MDTM website
<https://mdtm.fnal.gov/>`_.

BDE makes use of `CILogon single-point sign-on service
<https://cilogon.org/>`_ to obtain X.509 certificates for secure acces
to data transfer cyberinfrastructure.

If you have questions about the project, or need assistance, please
contact the team at Fermilab:

* Wenji Wu <wenji@fnal.gov>
* Phil DeMar <demar@fnal.gov>
* Qiming Lu <qlu@fnal.gov>
* Liang Zhang <liangz@fnal.gov>
* Sajith Sasidharan <sajith@fnal.gov>


Intended audience
=================

This document is intended to help users to configure, deploy, 
and operate BigData Express.

BigData Express software needs some manual installation and
configuration.  You are expected to have basic Linux/Unix system
administration skills and command line expertise.


.. toctree::
   :maxdepth: 2
   :numbered:
   :caption: Contents:

   introduction.rst
   setup/index.rst
   setup/use-case.rst
   setup/troubleshooting.rst
   setup/set-up-ca.rst
   setup/bde-config-files.rst

..
   Indices and tables
   ==================

   * :ref:`genindex`
   * :ref:`search`
