# Server source

Game/DB server source for this client.

## Layout
| dir | what |
|---|---|
| `Server/game` | game server |
| `Server/db` | database server |
| `Server/common` | headers shared with the client — `length.h`, `tables.h`, `CommonDefines.h`, `item_length.h` |
| `Server/libthecore`, `libgame`, `libsql`, `libpoly`, `liblua` | engine libraries |

## Dependencies (NOT in this repo)
`Extern/` was excluded — it was 194 MB of third-party headers, ~183 MB of it Boost alone.
To build, restore it next to `Server/` as `Extern/include/<lib>`:

- boost · cryptopp · mysql · rapidjson · fmt · IL · msl

`Extern/cryptopp` (the Crypto++ sources) is also required.

## Build
FreeBSD, **32-bit** (`-m32`, see `Server/game/src/Makefile`), gcc12, C++20.

> ⚠ The server must stay 32-bit. `TPacketGCTime` uses `time_t`, which the client reads as
> `uint32_t`. On a 64-bit build `time_t` becomes 8 bytes and the packet stream desynchronises.

## Client compatibility
Audited 2026-08-04 against this client — packet IDs, struct sizes and defines all match.
See `PACKET_COMPATIBILITY_REPORT.md` and the tools in `tools/` (`pktdiff.py`, `sizediff.py`,
`constdiff.py`, `defdiff.py`).

⭐ `ENABLE_DICE_SYSTEM` must stay in sync between `common/CommonDefines.h` and the client's
`Locale_inc.h` — it inserts a value into `EChatType`, which travels on the wire.
