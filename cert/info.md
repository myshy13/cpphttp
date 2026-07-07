# Certificates

## required files

> - **server.key**: this is the private key used to authenticate the server and decrypt the client's messages
> - **Server.crt**: this is the public key that gets sent to the client

## Create a cert

```bash
openssl req -x509 -newkey rsa:2048 -nodes \
  -keyout server.key \
  -out server.crt \
  -days 365 \
  -subj "/CN=localhost"
```
