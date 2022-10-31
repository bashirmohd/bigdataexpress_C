.. BigData Express Design documentation master file, created by
   sphinx-quickstart on Mon Jul 29 11:06:09 2019.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

BigData Express project design
==============================

This document discusses design aspects of BigData Express project.

**Big Data Express** (BDE) is a data transfer tool developed at `Fermi
National Accelerator Laboratory <https://www.fnal.gov/>`_. BDE seeks
to provide schedulable, predictable, and high-performance data
transfer service in big data and high-performance computing
environments.  Please visit `BDE project website
<https://bigdataexpress.fnal.gov/>`_ for more details.

**mdtmFTP** is BDE's default data transfer engine. Detailed
information on mdtmFTP is available at `MDTM website
<https://mdtm.fnal.gov/>`_.

BDE makes use of `CILogon <https://cilogon.org/>`_ single-point
sign-on service to obtain X.509 certificates for secure acces to data
transfer cyberinfrastructure.

If you have questions about the project, or need assistance, please
contact the team at Fermilab:

* Wenji Wu <wenji@fnal.gov>
* Phil DeMar <demar@fnal.gov>
* Qiming Lu <qlu@fnal.gov>
* Liang Zhang <liangz@fnal.gov>
* Sajith Sasidharan <sajith@fnal.gov>

.. toctree::
   :maxdepth: 2
   :caption: Contents:

.. toctree::
   :maxdepth: 4
   :numbered:
   :caption: Table of Contents

   architecture.rst


Indices and tables
==================

* :ref:`genindex`
* :ref:`search`
