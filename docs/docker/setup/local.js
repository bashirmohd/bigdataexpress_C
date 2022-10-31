module.exports = {

  ssl: {
    key: require('fs').readFileSync(__dirname + '/srv/www/ssl/server.key'),
    cert: require('fs').readFileSync(__dirname + '/srv/www/ssl/server.crt')
  },

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
  },
};
