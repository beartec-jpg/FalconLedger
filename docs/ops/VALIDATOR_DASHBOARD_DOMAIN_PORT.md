# Validator dashboard: custom domain and port (e.g. 6080)

Bootstrap defaults:

| Service | Default public port | Purpose |
|---------|---------------------|---------|
| Peer (xrpld) | **51235** | Consensus / P2P — keep this open |
| Dashboard | **8080** | HTTP metrics UI (`qxrp-dashboard`) |

You **can** put the dashboard on another port (e.g. **6080**) and/or a **domain**.  
The portal used to assume only `:8080`; it now accepts `host:port` and domains.

---

## A. Change publish port to 6080 (no domain)

On the validator host, edit compose (often `/var/lib/qxrp-validator/docker-compose.yml` or falcon-validator dir):

```yaml
  dashboard:
    ports:
      - "6080:8080"   # was "8080:8080"
```

Then:

```bash
cd /var/lib/qxrp-validator   # or your install dir
docker compose up -d
```

Cloud firewall / security group: **allow TCP 6080** (and keep **51235**).

Portal “Link node”: enter `YOUR.PUBLIC.IP:6080` or `node.example.com:6080`.

Browser: `http://YOUR.PUBLIC.IP:6080`

---

## B. Custom domain (recommended: reverse proxy + TLS)

1. **DNS** — A/AAAA record: `node.example.com` → VPS public IP.  
2. Keep dashboard listening on host **127.0.0.1:8080** or published **6080**.  
3. **Caddy / nginx / Traefik** reverse proxy:

### Caddy example (auto HTTPS)

```caddy
node.example.com {
  reverse_proxy 127.0.0.1:8080
}
```

If you published 6080 instead:

```caddy
node.example.com {
  reverse_proxy 127.0.0.1:6080
}
```

Portal “Link node”: enter `node.example.com` (HTTPS on 443 uses default portal fetch over **http** for stats — for portal proxy you still need a public HTTP port **or** keep host:8080/6080 HTTP for the portal proxy).

**Note:** The portal’s `/api/node-dashboard` currently fetches `http://host:port/api/stats` (HTTP). Options:

- Leave **HTTP on 6080** public for metrics (simplest for portal link), domain for humans via HTTPS reverse proxy on 443, **or**
- Enter `node.example.com:6080` if you only publish HTTP on 6080.

Private/LAN IPs are **rejected** by the portal (SSRF protection). Domain must resolve to a **public** address.

---

## C. Why “not approved” might happen

| Message / behaviour | Cause |
|---------------------|--------|
| Host not allowed | Domain/IP is private, localhost, or DNS → private |
| Host not on allow-list | Vercel env `ALLOWED_DASHBOARD_HOSTS` set and your host not listed |
| Invalid host | Typo, unsupported format |
| Dashboard unreachable | Firewall closed, wrong port, container down |
| Portal still opens :8080 | Old portal build; re-link with `host:6080` after deploy |

Peering does **not** use 8080/6080 — only **51235**. Changing dashboard port does **not** affect UNL or consensus.

---

## D. Checklist for 5th node operator

- [ ] Dashboard container healthy (`docker ps`, curl local health)  
- [ ] Compose maps desired port (`6080:8080` or reverse proxy)  
- [ ] Firewall allows peer **51235** + dashboard port (or 443)  
- [ ] Domain DNS points at public IP (if using domain)  
- [ ] Portal link field: `domain:6080` or `ip:6080`  
- [ ] Image is `mainnet-v2`; AccountNames supported  

---

## E. Peer vs dashboard (don’t mix)

| Port | Change freely? | Effect if closed |
|------|----------------|------------------|
| 51235 | Prefer keep | Peers cannot connect |
| 8080 / 6080 / 443 | Yes | Only dashboard / portal metrics |
