.. _set-up-portal:

=========================
Setting up BDE web portal
=========================

BDE web portal is a `Node.js`_ application.  In order to run the web
portal software in **head node**, the following tools are required:

* `Node.js`_ and `npm`_.
* Optionally, a proxy server, such as `Nginx`_.
* Optionally, an SSL certificate for the web server.

In the following steps, it is assumed that the head node has the
domain name ``example.net``.  Please replace all instances of
``example.net`` with your head node's domain name.

.. _Node.js: https://nodejs.org/en/
.. _npm: https://www.npmjs.com/
.. _Nginx: https://nginx.org/


Install Node.js and npm
=======================

On RHEL and derivative distributions, install ``node`` and ``npm``
from the ``epel`` repository:

.. code-block:: console

  $ sudo yum install epel-release
  $ sudo yum install node npm

You will also need some build tools and OpenSSL library:

.. code-block:: console

  $ sudo yum install make gcc gcc-c++ openssl-devel

If the ``epel`` repository is not available, you can alternatively
install ``node`` and ``npm`` from binary or source downloads at
`Node.JS download site`_.

.. _Node.JS download site: https://nodejs.org/en/download/

On Ubuntu 18.04 and newer, install ``node``, ``npm``, and build tools
from Ubuntu's official repositories.

.. code-block:: console

  $ sudo apt install nodejs npm build-essential libssl-dev

On Ubuntu 16.04, you may have to use `nodesource`_ binaries for a
currently maintained version of Node.js:

.. _nodesource: https://github.com/nodesource/distributions#deb

.. code-block:: console

   $ curl -sL https://deb.nodesource.com/setup_8.x | sudo -E bash -
   $ sudo apt install nodejs npm
   $ sudo apt install build-essential libssl-dev


Get the web portal sources code and set it up
=============================================

Assuming that the web portal will be set up under
``/srv/www/bigdata-express-portal``, do this:

.. code-block:: console

  $ sudo mkdir /srv/www
  $ sudo chown $USER: /srv/www
  $ cd /srv/www
  $ git clone ssh://p-bigdata-express@cdcvs.fnal.gov/cvs/projects/bigdata-express-portal
  $ cd bigdata-express-portal
  $ npm install

The ``npm install`` step will print some warnings on the console, but
that should be OK, except when they are about failures to build
``bcrypt`` and ``x509``.  Those npm packages require a C++ compiler
and OpenSSL development package in the system.  If ``npm install``
above fails because it can't find a compiler or OpenSSL, you may have
to install them (see above), and then re-install ``bcrypt`` and
``x509``:

.. code-block:: console

  $ npm install bcrypt x509

Now, create a configuration for your portal, under the top-level of
portal source code, at ``config/local.js``.  You can create this file
from the example configuration file:

.. code-block:: console

   $ cp config/local_example_js config/local.js

Find the configuration block named ``site`` in ``config/local.js``,
and update it.  The block would look like:

.. code-block:: javascript

  site: {
    // new site's name
    local: 'NewBdeSite',

    // message queue server options
    mq_host: 'head-node',
    mq_port: 8883,
    mq_ca: "/path/to/cacert.pem",  // use unencrypted mq server if 'mq_ca' is not set

    // control interface
    server_iface: 'eth0',

    // cilogon:
    clientid: '<client-id>',
    secret: '<client-secret>',
    callback: 'https://hostname:port/auth/callback'
  }

You will need to edit the file to provide:

* A name for the local site.

* MQTT broker's host and port.

  * A CA certificate file for MQTT, if MQTT broker is configured to
    use TLS/SSL.  If no TLS/SSL is configured, omit the ``mq_ca``
    line.

* Network interface's name for the control channel.

* A client ID, secret, and callback URL for CILogon.  See the next
  section (:ref:`register-with-cilogon`) for details.

After updating ``config/local.js``, test if you can launch the portal
successfully:

.. code-block:: console

  $ node app.js --port 5000

You should see a startup message like:

.. code-block:: console

    info:
    info:                .-..-.
    info:
    info:    Sails              <|    .-..-.
    info:    v0.11.5             |\
    info:                       /|.\
    info:                      / || \
    info:                    ,'  |'  \
    info:                 .-'.-==|/_--'
    info:                 `--'-------'
    info:    __---___--___---___--___---___--___
    info:  ____---___--___---___--___---___--___-__
    info:
    info: Server lifted in `/srv/www/bigdata-express-portal`
    info: To see your app, visit https://localhost:5000
    info: To shut down Sails, press <CTRL> + C at any time.


A note on SSL/TLS setup
=======================

By default, BDE portal ships with a self-signed TLS/SSL certificate. If you plan to set up BDE portal without a reverse proxy in the front, we recommend configuring an SSL certificate, issued by a well-trusted CA, for the portal. Otherwise, web browser will give warnings about untrusted website or certificate when visiting the portal.  

1. Request an SSL certificate from a well-trusted CA.

2. Find the configuration block named ``ssl`` in ``config/local.js``:

.. code-block:: javascript

  ssl: {
    key: require('fs').readFileSync(__dirname + '/../ssl/server.key'),
    cert: require('fs').readFileSync(__dirname + '/../ssl/server.crt')
  },

3. Replace ``key`` and ``cert`` with the new cert/key pair:

.. code-block:: javascript

  ssl: {
    key: require('fs').readFileSync('/path/to/ssl/server.key'),
    cert: require('fs').readFileSync('/path/to/ssl/server.crt')
  },


If you plan to set up a reverse proxy in front of BDE portal, no extra SSL/TLS setup is required for BDE portal. See :ref:`portal-nginx-and-certbot` for details on how to set up a reverse proxy with HTTPS.

.. _register-with-cilogon:

Register with CILogon
=====================

Go to https://cilogon.org/oauth2/register, and register a new client:

* Client Name: Fermilab BigData Express
* Contact Email: your email
* Home URL: the portal's URL, such as https://example.net
* Callback URLs: https://example.net/auth/callback
* Scope: check everything, especially ``edu.uiuc.ncsa.myproxy.getcert``.
* Refresh Token Lifetime: leave blank

Once the registration is approved, you will receive a confirmation
email from CILogon.  A **`client ID`** with an associated **`secret`**
will be assigned to the newly registered site, which are required to
set ``clientid`` and ``secret`` in
``/srv/www/bigdata-express-portal/config/local.js``.



Add a new site in ``config/bootstrap.js``
=========================================

Open ``/srv/www/bigdata-express-portal/config/bootstrap.js``, find
function call ``Site.findOrCreate([..])``.  The function's argument is
an array.  Add the new site's details to the end of the array:

.. code-block:: javascript

  {
    abbrev: 'NewBdeSite',
    name: 'Name of the new site',
    url: 'URL of the new site',
    idp: 'Identity provider for the new site'
  }


Add a new site logo
===================

A new site logo can be added in the web portal.

Assuming that ``local`` in ``config/local.js`` is set to **`NewBdeSite`**,

.. code-block:: javascript

  site: {
    // local site
    local: 'NewBdeSite',

    // .. other settings
  }


Add a logo under ``/srv/www/bigdata-express-portal/assets/images``,
with the name ``NewBdeSite-logo.png``. Note that the logo image must
be a PNG file.


Run BDE portal as a systemd service
===================================

First, add a systemd unit file ``/lib/systemd/system/bdeportal.service``:

.. code-block:: ini

  [Unit]
  Description=Bigdata Express Web Portal
  After=network.target

  [Service]
  Type=simple
  ExecStart=/usr/bin/node /srv/www/bigdata-express-portal/app.js --port 5000
  User=bde
  TimeoutStopSec=10
  KillMode=mixed
  Restart=always

  [Install]
  WantedBy=multi-user.target

Then, enable/start the service:

.. code-block:: console

  $ sudo systemctl daemon-reload
  $ sudo systemctl enable bdeportal
  $ sudo systemctl start bdeportal

If firewall allows inbound traffic on port 5000, you should be able to
visit the site at https://example.net:5000.

In case you need to run the web portal on HTTPS port, use port
number 443, and comment out or remove the ``User=bde`` line from the
systemd unit file.


Log messages from the portal
----------------------------

Once you start the portal as a systemd service, you can view log
messages from it by running:

.. code-block:: console

   $ sudo journalctl -xef -u bdeportal


.. _portal-nginx-and-certbot:

Optional: nginx reverse proxy and LetsEncrypt certificates
==========================================================

It is recommended to deploy a nginx reverse proxy in front of the web
portal, which will bring several benefits:

* Nginx can automatically route HTTP traffic to HTTPS.

* Nginx can be easily set up with a security certificate issued by
  `Let's Encrypt <https://certbot.eff.org>`_ project.

* Nginx can report useful error messages when problems occur.

* Nginx can perform load-balancing across multiple web portal
  instances if necessary.

On RHEL and deratives, install nginx from the ``epel`` repository:

.. code-block:: console

  $ sudo yum install epel-release
  $ sudo yum install nginx

On Ubuntu systems, install nginx from official repositories:

.. code-block:: console

  $ sudo apt install nginx

On RHEL7 and deratives, install certbot (available in the ``epel`` repository):

.. code-block:: console

  $ sudo yum install certbot-nginx

Or it may be:

.. code-block:: console

  $ sudo yum install certbot python2-certbot-ngix

On Ubuntu systems, install certbot:

.. code-block:: console

  $ sudo apt install certbot

The exact package names, which are depend on certbot release, may
change.  Please consult `Certbot website`_ for up-to-date
instructions.

.. _Certbot website: https://certbot.eff.org/instructions

Next, run the below command to get a certificate and have Certbot create an Nginx configuration
automatically to serve it:

.. code-block:: console

  $ sudo certbot --nginx


Renewing certbot-issued certificates
------------------------------------

Certbot-issued certificates have a validity of three months. Keeping
them renewed and up-to-date is straightforward.

With newer versions of certbot, enable the systemd timer:

.. code-block:: console

  $ sudo systemctl enable --now certbot-renew.timer

Also edit the POST_HOOK line in ``/etc/sysconfig/certbot``, so that
nginx uses the new certificate after renewal:

.. code-block:: console

  POST_HOOK="--post-hook 'systemctl reload nginx'"

If systemd timer is not available, you will need to use cron. Run
``sudo crontab -e`` and add the following line:

.. code-block:: console

  0 0,12 * * * python -c 'import random; import time; time.sleep(random.random() * 3600)' && certbot renew && systemctl reload nginx


Route nginx traffic to BDE web portal
-------------------------------------

You will need to route traffic between nginx and BDE web portal.

In order to do that, find the "``location / {...}``" configuration
block in ``/etc/nginx/nginx.conf``, and update it with the following:

.. code-block:: nginx

  location / {
      proxy_pass https://localhost:5000;
      proxy_pass_header Server;
      proxy_set_header Host $host;
      proxy_set_header X-Real-IP $remote_addr;
      proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
      proxy_set_header X-Forwarded-Proto $scheme;
      proxy_set_header Upgrade $http_upgrade;     # WebSocket related.
      proxy_set_header Connection "upgrade";      # WebSocket related.
      proxy_read_timeout 3600;                    # Optional.
   }

Setting a bigger ``proxy_read_timeout`` is optional.  Larger timeouts
maybe needed when BDE is computing checksums of very large files, so
that nginx does not give up and report an error before BDE is done
with checksum calculation.

After updating nginx configuration, reload the service:

.. code-block:: console

  $ sudo systemctl reload nginx

Assuming the above steps worked, and assuming that the portal is
running on a host with the domain name ``example.net``, the portal
should be available at https://example.net.

In case you need to debug this setup, check logs:

* certbot logs are at ``/var/log/letsencrypt/letsencrypt.log``.
* nginx error log is at ``/var/log/nginx/error.log``.
* nginx access log is at ``/var/log/nginx/access.log``.


"(13: Permission denied) while connecting to upstream"
------------------------------------------------------

On CentOS7, SELinux would prevent nginx from
routing traffic to BDE web portal.  you will see
such errors in ``/var/log/nginx/error.log``:

.. code-block:: console

  2019/06/11 17:20:34 [crit] 22132#0: *11 connect() to [::1]:5000
    failed (13: Permission denied) while connecting to upstream,
    client: [..], server: [..], request: "GET / HTTP/1.1",
    upstream: "https://[::1]:5000/", host: "[..]"
  [...]
  2019/06/11 17:20:40 [error] 22132#0: *11 no live upstreams
    while connecting to upstream, client: [..], server: [..],
    request: "GET / HTTP/1.1", upstream: "https://localhost/", host: "[..]"

You can solve the problem with:

.. code-block:: console

  $ sudo setsebool -P httpd_can_network_connect 1
