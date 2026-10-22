# tremor-matchmaker

The matchmaking service: one program and one SQLite file, meant to be
self-hosted next to a few game servers.

    cargo build --release
    ./target/release/tremor-matchmaker --listen 0.0.0.0:27960 --db matchmaker.db

## How a match happens

1. A player queues for one or more modes (`POST /queue`).
2. Once a second the matcher looks for a group per mode: the longest-waiting
   player first, ratings within a window of ±100 that widens by 50 every
   30 s up to ±400, anyone after 3 minutes. Team modes are split by total
   rating.
3. Everyone has 15 s to accept (`POST /ready`). A decline or no answer blocks
   that player for a minute; the others go back to the front of the queue.
4. A free registered server that has the mode is reserved, a map is picked
   from its pool, and each player gets a one-time token. `POST /status` gives
   the player the address and token.
5. The server reports the winner (`POST /servers/result`). Each player's
   Glicko-2 rating is updated against the other side's average; leaving
   early counts as a loss. Ratings show after 10 placement matches.

## Identity and signatures

Players and servers are ed25519 key pairs made on first start. Every request
carries `X-Key` (public key, hex), `X-Time` (unix seconds, within 60 s) and
`X-Signature` (hex) over `METHOD\npath\ntime\nbody`.

## Endpoints

| Who    | Request                     | Body                                       |
|--------|-----------------------------|--------------------------------------------|
| player | `POST /queue`               | `{"modes": ["duel", "tdm"]}`               |
| player | `DELETE /queue`             |                                            |
| player | `POST /status`              |                                            |
| player | `POST /ready`               | `{"accept": true}`                         |
| player | `POST /ratings`             |                                            |
| server | `POST /servers/register`    | `{"address", "name", "maps": {mode: [..]}}`|
| server | `POST /servers/reservation` |                                            |
| server | `POST /servers/result`      | `{"match", "winner", "left_early": [..]}`  |

Modes: duel, roundduel (1v1), tdm2v2, ca2v2 (2v2), tdm, ctf, ca, sacrifice (4v4).

`scripts/mmtest.py URL` runs one duel through a live service.
