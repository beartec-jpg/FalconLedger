# Ceremony roles (fill before T0)

| Role | Person | Backup | Contact |
|------|--------|--------|---------|
| Genesis node operator | | | |
| UNL / validators list publisher | | | |
| Portal / faucet operator | | | |
| Neon / DB owner | | | |
| Public announcer (T0 blog/X) | | | |
| DEV key holder A | | | |
| DEV key holder B | | | |
| DEV key holder C | | | |
| Incident commander | | | |

## Decision rights

| Decision | Who |
|----------|-----|
| Abort T0 | Incident commander + genesis operator |
| Publish network id / RPC | Announcer after ops green |
| Execute genesis split | Genesis operator (with second person watching dry-run output) |
| Flip `MAINNET_LIVE=true` | Portal operator after split balances verified |
| Start airdrop cron | Portal operator after `genesis_at` set |
