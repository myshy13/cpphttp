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

## Trusting the cert

### MacOS

1. Create the cert with the command above
2. Open keychain access (you may need to enter your password)
3. Select login or system, to make it for your user only, or for the system, respectively.
4. Click certificates in the top bar
5. Drag and drop the server.crt file into Keychain Access (you may need to enter your password)
6. Double click the new item labeled "localhost"
7. In the trust section of the info, change "when using this certificate" to "always trust"

## Linux

There are a few ways to trust the cert on Linux depending on distro and browser.

- System (Debian/Ubuntu):

```bash
sudo cp server.crt /usr/local/share/ca-certificates/server.crt
sudo update-ca-certificates
```

After that, restart browsers.

- System (Fedora/RHEL):

```bash
sudo cp server.crt /etc/pki/ca-trust/source/anchors/
sudo update-ca-trust extract
```

- For Firefox (uses its own NSS DB):

Install certutil (from nss-tools or libnss3-tools), then:

```bash
certutil -d sql:$HOME/.pki/nssdb -A -t "C,," -n "localhost" -i server.crt
```

Replace "localhost" with the cert name if different. Restart Firefox.

- For Chromium/Chrome on Linux the system store is usually used (see Debian/Ubuntu step); if not, import via browser settings.

If you only need the cert trusted for a single command or session, some tools/browsers accept flags to ignore certificate validation (not recommended for production).
