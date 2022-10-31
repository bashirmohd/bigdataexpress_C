.. _set-up-firewall:

Firewall considerations
=======================

Here's a list of ports that need to be opened for BDE:

+-----------+---------------------------+-----------+-----------------------------+
| Ports     | What                      | Where     | Visibility                  |
+===========+===========================+===========+=============================+
| 80,443    | BDE web portal            | Head node | Public                      |
+-----------+---------------------------+-----------+-----------------------------+
| 1883/8883 | Mosquitto                 | Head node | Internal                    |
+-----------+---------------------------+-----------+-----------------------------+
| 27017     | MongoDB                   | Head node | Internal                    |
+-----------+---------------------------+-----------+-----------------------------+
| 8086      | InfluxDB                  | Head node | Internal                    |
+-----------+---------------------------+-----------+-----------------------------+
| 5001 (*)  | mdtmFTP+ server (control) | DTN       | Reachable from head node(s) |
+-----------+---------------------------+-----------+-----------------------------+
| Many (*)  | mdtmFTP+ server (data)    | DTN       | Between DTNs                |
+-----------+---------------------------+-----------+-----------------------------+

1. The web portal port should be opened to the public.  This can be
   whichever port that you chose to use, and/or ports 80 and 443 if
   nginx reverse proxy is set up.

2. DTNs (and the network control node, if deployed) should be able to
   reach the followoing ports of the head node:

   * MQTT: port 1883 (or 8883 if SSL is enabled)
   * MongoDB: port 20217
   * InfluxDB: port 8086

3. Head node(s) in local and remote sits should be able to reach
   mdtmftp+ server's control port, which is typically run on port
   5001, but it may change depending on how BDE Agent is configured.

4. Between DTNs, a range of TCP ports should be opened to allow data
   transfers between DTNs in local and remote sites.


.. _set-up-headnode-firewall:

Firewall rules on head node
---------------------------

..  Below is based on Qiming's notes: see
   bigdata-express/deploy/firewall

ON RHEL and derivatives, you can use `firewalld
<https://firewalld.org/>`_ to manage firewall rules.

Unmask and enable firewalld service at boot:

.. code-block:: console

   $ sudo systemctl unmask firewalld
   $ sudo systemctl enable firewalld
   $ sudo systemctl start firwalld

Optionally set a default zone:

.. code-block:: console

   $ sudo firewall-cmd --set-default-zone=public

Allow traffic to web portal:

.. code-block:: console

   $ sudo firewall-cmd --zone=public --permanent --add-port=5000/tcp

Open MongoDB port (27017) only to a DTNs with IP 131.225.2.29 and
131.225.2.30:

.. code-block:: console

   $ sudo firewall-cmd --zone=public --permanent \
       --add-rich-rule='rule family="ipv4" \
       source address="131.225.2.29/32" port protocol="tcp" port="27017" accept'

   $ sudo firewall-cmd --zone=public --permanent \
       --add-rich-rule='rule family="ipv4" \
       source address="131.225.2.30/32" port protocol="tcp" port="27017" accept'

Open MQTT port 1883/1884/8883, depending on what's enabled:

.. code-block:: console

   $ sudo firewall-cmd --zone=public --permanent \
       --add-rich-rule='rule family="ipv4" \
       source address="131.225.2.30/32" port protocol="tcp" port="8883" accept'

Open InfluxDB port:

.. code-block:: console

   $ sudo firewall-cmd --zone=public --permanent \
       --add-rich-rule='rule family="ipv4" \
       source address="131.225.2.30/32" port protocol="tcp" port="8088" accept'

You can also manually edit firewalld configuration file present at
``/etc/firewalld/zones/public.xml``.

Now restart service:

.. code-block:: console

   $ sudo systemctl restart firewalld

Verify the rules:

.. code-block:: console

   $ sudo firewall-cmd --zone=public --list-all

Output should be like:

.. code-block:: console

    public (default, active)
      interfaces: enp11s0f0
      sources:
      services: dhcpv6-client ssh
      ports: 5000/tcp
      masquerade: no
      forward-ports:
      icmp-blocks:
      rich rules:
            rule family="ipv4" source address="131.225.2.29/32" port port="27017" protocol="tcp" accept
            rule family="ipv4" source address="131.225.2.30/32" port port="27017" protocol="tcp" accept

References
----------

* `How To Set Up a Firewall Using FirewallD on CentOS 7 <https://www.digitalocean.com/community/tutorials/how-to-set-up-a-firewall-using-firewalld-on-centos-7>`_
* `How to open port for a specific IP address with firewall-cmd on CentOS? <http://serverfault.com/questions/684602/how-to-open-port-for-a-specific-ip-address-with-firewall-cmd-on-centos>`_
* `Fedora wiki: firewalld Rich Language <https://fedoraproject.org/wiki/Features/FirewalldRichLanguage>`_
