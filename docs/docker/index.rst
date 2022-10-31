BigData Express Docker Manual
=============================

This manual discusses deployment, configuration, and
operation of BigData Express in Docker containers.  If you are operating
BigData Express in a non-container environment, you should follow
`BigData Express Administrator Manual`_.

.. _BigData Express Administrator Manual: https://bigdataexpress.fnal.gov/admin_manual/index.html

**Big Data Express** (BDE) is a data transfer tool developed at `Fermi
National Accelerator Laboratory`_.  BDE seeks to provide schedulable,
predictable, and high-performance data transfer service in big data
and high-performance computing environments. Please visit `BDE project
website`_ for more details.

.. _Fermi National Accelerator Laboratory: https://www.fnal.gov/
.. _BDE project website: https://bigdataexpress.fnal.gov/
.. _MDTM website: https://mdtm.fnal.gov/
.. _CILogon single-point sign-on service: https://cilogon.org/

**mdtmFTP** is BDE's default data transfer engine. Detailed
information on mdtmFTP is available at `MDTM website`_.

BDE makes use of `CILogon single-point sign-on service`_ to obtain
X.509 certificates for secure acces to data transfer
cyberinfrastructure.

If you have questions about the project, or need assistance, please
contact the team at Fermilab:

* Wenji Wu <wenji@fnal.gov>
* Phil DeMar <demar@fnal.gov>
* Qiming Lu <qlu@fnal.gov>
* Liang Zhang <liangz@fnal.gov>
* Sajith Sasidharan <sajith@fnal.gov>


Intended audience
=================

This document is intended for engineers and system administrators 
who need to operate, configure, and deploy BigData Express.

You are expected to have basic Linux/Unix system administration skills, 
command line expertise, and familiarity with Docker.


.. toctree::
   :maxdepth: 2
   :numbered:
   :caption: Contents:

   introduction.rst
   setup.rst

..
   Indices and tables
   ==================

   * :ref:`genindex`
   * :ref:`search`
