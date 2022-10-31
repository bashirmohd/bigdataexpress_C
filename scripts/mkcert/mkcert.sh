#! /bin/sh

set -eu pipefail

# You get a certificate! I get a certificate!  Everybody gets a
# certificate!
#
# This script will quickly generate a key and self-signed certificate,
# which obviously beats searching the web for fifteen minutes every
# time you need a key and certificate.
#
# "OpenSSL Command-Line HOWTO" <https://www.madboa.com/geek/openssl/>
# has nearly everything anyone would need to know about openssl
# commandline.
#
# To examine the certificate, do: `openssl x509 -text -in cert.pem`

HOST=$(hostname -f || hostname)

echo "Generating key and certificate for ${HOST}"

openssl req -x509 -newkey rsa:4096 -keyout key.pem -out cert.pem -nodes \
        -subj "/C=US/ST=Illinois/L=Batavia/O=Fermilab/OU=CCD/CN=${HOST}"
