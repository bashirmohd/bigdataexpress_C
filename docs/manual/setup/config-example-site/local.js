module.exports = {

    // SSL certificate and key
    ssl: {
        key: require('fs').readFileSync('/path/to/ssl/privkey.pem'),
        cert: require('fs').readFileSync('/path/to/ssl/fullchain.pem')
    },

    site: {
        // Local site name
        local: "Site-A",

        // MQTT parameters
        mq_host: "localhost",
        mq_port: 1883,

        // Control interface.
        server_iface: "ens160",

        // Cilogon parameters.
        clientid: '[elided]',
        secret: '[elided]',
        callback: 'https://head.site-a.net/auth/callback'
    },
};
