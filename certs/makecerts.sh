#!/usr/bin/bash

if [ "${PWD##*/}" != "certs" ]; then
    echo Run this in the certs folder
    exit 1
fi
if [ -z "$MQTT_SERVER" ]; then
    echo Define MQTT_SERVER
    exit 1
fi
SERVER_NAME=$MQTT_SERVER

if [ -d "$SERVER_NAME" ]; then
    echo Run \"rm -fr $SERVER_NAME\" to regenerate these keys
    exit 1
fi
mkdir $SERVER_NAME
echo Generating keys in $PWD/$SERVER_NAME

# Gerar CA
openssl genrsa -out $SERVER_NAME/ca.key 2048
openssl req -new -x509 -days 99999 -key $SERVER_NAME/ca.key -out $SERVER_NAME/ca.crt -subj "/C=BR/ST=RN/L=Natal/O=Stanley Net Br/OU=Software/CN=rpiroot"

# Gerar certificado do broker
openssl genrsa -out $SERVER_NAME/server.key 2048
openssl req -new -out $SERVER_NAME/server.csr -key $SERVER_NAME/server.key -subj "/C=BR/ST=RN/L=Natal/O=Stanley Net Br/OU=Software/CN=$SERVER_NAME"
openssl x509 -req -in $SERVER_NAME/server.csr -CA $SERVER_NAME/ca.crt -CAkey $SERVER_NAME/ca.key -CAcreateserial -out $SERVER_NAME/server.crt -days 9999

# Gerar certificado do cliente
openssl genrsa -out $SERVER_NAME/client.key 2048
openssl req -new -out $SERVER_NAME/client.csr -key $SERVER_NAME/client.key -subj "/C=BR/ST=RN/L=Natal/O=Stanley Net Br/OU=Software/CN=$SERVER_NAME"
openssl x509 -req -in $SERVER_NAME/client.csr -CA $SERVER_NAME/ca.crt -CAkey $SERVER_NAME/ca.key -CAcreateserial -out $SERVER_NAME/client.crt -days 999

# Gerar mqtt_client.inc para TLS_ROOT_CERT (CA)
echo -n \#define TLS_ROOT_CERT \" > $SERVER_NAME/mqtt_client.inc
cat $SERVER_NAME/ca.crt | awk '{printf "%s\\n\\\n", $0}' >> $SERVER_NAME/mqtt_client.inc
echo "\"" >> $SERVER_NAME/mqtt_client.inc
echo >> $SERVER_NAME/mqtt_client.inc

# Gerar mqtt_client.inc para TLS_CLIENT_KEY
echo -n \#define TLS_CLIENT_KEY \" >> $SERVER_NAME/mqtt_client.inc
cat $SERVER_NAME/client.key | awk '{printf "%s\\n\\\n", $0}' >> $SERVER_NAME/mqtt_client.inc
echo "\"" >> $SERVER_NAME/mqtt_client.inc
echo >> $SERVER_NAME/mqtt_client.inc

# Gerar mqtt_client.inc para TLS_CLIENT_CERT
echo -n \#define TLS_CLIENT_CERT \" >> $SERVER_NAME/mqtt_client.inc
cat $SERVER_NAME/client.crt | awk '{printf "%s\\n\\\n", $0}' >> $SERVER_NAME/mqtt_client.inc
echo "\"" >> $SERVER_NAME/mqtt_client.inc
