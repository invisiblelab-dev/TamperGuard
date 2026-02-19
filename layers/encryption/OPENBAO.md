# OpenBao Setup: Store and Read an Encryption Key via API

This guide shows how to deploy OpenBao and configure it to provide encryption keys to the encryption layer.

> **Note**: The encryption layer's Vault integration has been tested and verified to work with **OpenBao**. OpenBao is a fork of HashiCorp Vault that maintains API compatibility.

## Overview

This guide covers:

- **Running OpenBao locally (dev mode)**
- **Creating and storing an encryption key in KV v2**
- **Creating a read‑only policy for that key**
- **Creating a token that can read the key**

All commands assume you are in the OpenBao repository root.

---

## 1. Run OpenBao

### 1.1 Dev mode (local testing)

Build and start the dev server (quick and insecure, for local use only):

```bash
make dev
bin/bao server -dev
```

The server prints a **Root Token**, for example:

```text
Root Token: s.sWvZjwpCL9ZevPW4un3eEjay
```

In a new terminal, set your environment:

```bash
export BAO_ADDR='http://127.0.0.1:8200'
export BAO_TOKEN='s.sWvZjwpCL9ZevPW4un3eEjay'  # paste your Root Token
```

> **Note**: `server -dev` is for local development only (insecure for production).

### 1.2 Production-style server (config file)

For a more realistic deployment, run OpenBao with a configuration file instead of `-dev`.

1. **Create a config file**, for example `config.hcl`:

   ```hcl
   storage "file" {
     path = "/opt/bao/data"
   }

   listener "tcp" {
     address       = "0.0.0.0:8200"
     tls_disable   = 0
     tls_cert_file = "/opt/bao/certs/server.crt"
     tls_key_file  = "/opt/bao/certs/server.key"
   }

   disable_mlock = true
   api_addr      = "https://your-hostname:8200"
   ```

   - **storage**: points to a persistent directory on disk (create it and set permissions).
   - **listener**: enables HTTPS on `0.0.0.0:8200` using your TLS cert and key.
   - Adjust paths and hostname for your environment.

2. **Start the server**:

   ```bash
   bin/bao server -config=config.hcl
   ```

3. **Initialize and unseal (first-time only)**:

   In another terminal:

   ```bash
   export BAO_ADDR='https://your-hostname:8200'

   # Initialize
   bin/bao operator init -key-shares=1 -key-threshold=1
   ```

   This prints:

   - One **unseal key**
   - One **initial root token**

   Unseal the server:

   ```bash
   bin/bao operator unseal <UNSEAL_KEY>
   ```

   Then set the root token in your shell (similar to dev mode):

   ```bash
   export BAO_TOKEN='<ROOT_TOKEN_FROM_INIT>'
   ```

From this point on, the remaining steps in this guide (KV secret, policy, token, and API access)
are the same whether you started OpenBao in dev mode or with a production config.

---

## 2. Create and store an encryption key in KV v2

### 2.1 Enable KV v2 (if not already)

```bash
bin/bao secrets enable -path=secret kv-v2
```

If it is already enabled, you can ignore the error about the mount existing.

### 2.2 Generate and store a 256‑bit key

```bash
ENC_KEY="$(openssl rand -hex 32)"   # 32 bytes → 64 hex chars
bin/bao kv put secret/myapp/encryption key="$ENC_KEY"
```

Verify that it was stored:

```bash
bin/bao kv get secret/myapp/encryption
```

You should see the secret value under `Data.key`.

---

## 3. Create a read‑only policy for that key

Create a file named `encryptor.hcl` in the repo root with:

```hcl
path "secret/data/myapp/*" {
  capabilities = ["read"]
}

path "secret/metadata/myapp/*" {
  capabilities = ["read"]
}
```

Upload the policy (using the root token):

```bash
bin/bao policy write encryptor encryptor.hcl
bin/bao policy read encryptor   # sanity check
```

This policy **only allows `read`** of that KV path (no write, delete, or list).

---

## 4. Create a token that can read the key

With `BAO_TOKEN` still set to the root token:

```bash
bin/bao token create -policy=encryptor -format=json
```

The output contains an `auth.client_token`. Example:

```json
{
  "auth": {
    "client_token": "s.QHdlEExO1IVdlNcy6ZXWeB5t",
    "policies": ["default", "encryptor"],
    "token_policies": ["default", "encryptor"]
  }
}
```

Export this as your **application token**:

```bash
export APP_TOKEN='s.QHdlEExO1IVdlNcy6ZXWeB5t'
```

### 4.1 Verify capabilities for the token

```bash
bin/bao token capabilities "$APP_TOKEN" secret/data/myapp/encryption
```

Expected output:

```text
read
```

### 4.2 Read the key via CLI using the app token

```bash
BAO_TOKEN="$APP_TOKEN" bin/bao kv get secret/myapp/encryption
```

You should see the secret value under `Data.key`.

### 4.3 Read the key via HTTP API

Use the application token as an HTTP header.  
OpenBao (like Vault) uses the `X-Vault-Token` header name:

```bash
curl \
  -H "X-Vault-Token: $APP_TOKEN" \
  "$BAO_ADDR/v1/secret/data/myapp/encryption"
```

You should get a JSON response similar to:

```json
{
  "data": {
    "data": {
      "key": "7da46e98f9643f34e8a4c68079816ec1ca9bbf4c68a3e50f842808848df50119"
    },
    "metadata": {
      "created_time": "2025-12-02T13:07:55.72449Z",
      "deletion_time": "",
      "destroyed": false,
      "version": 1
    }
  }
}
```

The actual encryption key is at JSON path `data.data.key`.

---

## 5. Configure the Encryption Layer

Once your OpenBao server is running and you have created a token, configure the encryption layer in your `config.toml`:

```toml
[encryption_layer]
type = "encryption"
next = "next_layer_name"
block_size = 4096
api_key = "s.QHdlEExO1IVdlNcy6ZXWeB5t"  # Your application token
vault_addr = "http://127.0.0.1:8200"
secret_path = "v1/secret/data/myapp/encryption"
```

When the layer initializes, it will:

1. Make an HTTP request to `http://127.0.0.1:8200/v1/secret/data/myapp/encryption`
2. Include the header `X-Vault-Token: s.QHdlEExO1IVdlNcy6ZXWeB5t`
3. Parse the JSON response and extract the key from `data.data.key`
4. Use that key for all encryption/decryption operations

---

## 6. Summary

- **Dev server**: `bin/bao server -dev` with `BAO_ADDR` and `BAO_TOKEN` set.
- **Secret**: stored in KV v2 at `secret/myapp/encryption`.
- **Policy**: `encryptor` policy grants **read‑only** access to `secret/data/myapp/*`.
- **Token**: created with `bin/bao token create -policy=encryptor`, used via:
  - CLI: `BAO_TOKEN="$APP_TOKEN" bin/bao kv get secret/myapp/encryption`
  - HTTP: `curl -H "X-Vault-Token: $APP_TOKEN" "$BAO_ADDR/v1/secret/data/myapp/encryption"`

## Additional Resources

- [OpenBao Documentation](https://openbao.org/docs/)
- [OpenBao GitHub Repository](https://github.com/openbao/openbao)

