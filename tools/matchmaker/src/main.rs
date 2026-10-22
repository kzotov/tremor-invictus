// Tremor Invictus matchmaker
// Copyright (C) 2026 the Tremor Invictus contributors
// Licensed under the GNU General Public License, version 2 or later.
//
// One self-hostable service: players queue for modes, the matcher groups
// them by rating, everyone accepts a ready check, a registered game server
// is reserved with one-time tokens, and the server reports the result,
// which updates Glicko-2 ratings.
//
// Identity: a player is the ed25519 public key their game made on first
// start (hex). Every request is signed:
//   X-Key: public key (hex), X-Time: unix seconds, X-Signature: hex of the
//   signature over "<METHOD>\n<path>\n<time>\n<body>".
// Game servers sign the same way with their own key.
//
// Usage: tremor-matchmaker [--listen 0.0.0.0:27960] [--db matchmaker.db]

mod glicko;
mod matching;
mod store;

use axum::{
    body::Bytes,
    extract::State,
    http::{HeaderMap, Method, StatusCode, Uri},
    response::{IntoResponse, Response},
    routing::{get, post},
    Json, Router,
};
use ed25519_dalek::{Signature, Verifier, VerifyingKey};
use rand::Rng;
use serde::Deserialize;
use serde_json::{json, Value};
use std::collections::HashMap;
use std::sync::{Arc, Mutex};
use std::time::{Duration, SystemTime, UNIX_EPOCH};

const READY_SECS: u64 = 15; // to accept a match
const DECLINE_BLOCK_SECS: u64 = 60; // out of the queues after declining
const SERVER_TIMEOUT_SECS: u64 = 60; // a server is gone without a heartbeat
const CONNECT_SECS: u64 = 60; // to join the reserved server
const MATCH_MAX_SECS: u64 = 2 * 3600; // a reserved match that never reports is dropped
const SIGNATURE_WINDOW_SECS: u64 = 60;

fn now() -> u64 {
    SystemTime::now().duration_since(UNIX_EPOCH).map(|d| d.as_secs()).unwrap_or(0)
}

fn token() -> String {
    let bytes: [u8; 16] = rand::thread_rng().gen();
    hex::encode(bytes)
}

struct Queued {
    modes: Vec<String>,
    since: u64,
    members: Vec<String>, // the queued player, or the whole party
}

struct Party {
    leader: String,
    members: Vec<String>,
}

const PARTY_MAX: usize = 4; // the largest team

struct GameServer {
    addr: String,
    name: String,
    modes: Vec<String>,
    maps: HashMap<String, Vec<String>>, // mode -> pool
    seen: u64,
    busy: Option<String>,               // the match it hosts
}

#[derive(Clone, PartialEq)]
enum MatchState {
    ReadyCheck,
    Reserved,
    Finished,
}

struct Match {
    id: String,
    mode: String,
    map: String,
    red: Vec<String>,
    blue: Vec<String>,
    units: Vec<(String, Vec<String>)>, // queue key and players, to queue again
    accepted: Vec<String>,
    deadline: u64,
    state: MatchState,
    server: Option<String>, // server key
    tokens: HashMap<String, String>,
}

impl Match {
    fn players(&self) -> impl Iterator<Item = &String> {
        self.red.iter().chain(self.blue.iter())
    }
}

#[derive(Default)]
struct Inner {
    queue: HashMap<String, Queued>, // by player, or by party leader
    parties: HashMap<String, Party>, // by invite code
    player_party: HashMap<String, String>,
    blocked: HashMap<String, u64>,       // player -> until
    servers: HashMap<String, GameServer>, // server key -> server
    matches: HashMap<String, Match>,
    player_match: HashMap<String, String>,
    profiles: HashMap<String, Profile>, // what others see on the match-found screen
}

#[derive(Default, Clone, Deserialize)]
struct Profile {
    #[serde(default)]
    name: String,
    #[serde(default)]
    champion: String,
}

struct App {
    inner: Mutex<Inner>,
    store: Mutex<store::Store>,
}

type Shared = Arc<App>;

fn error(status: StatusCode, msg: &str) -> Response {
    (status, Json(json!({ "error": msg }))).into_response()
}

/// The signed caller's key, or why the request is refused
fn verify(method: &Method, uri: &Uri, headers: &HeaderMap, body: &[u8]) -> Result<String, Response> {
    let get = |name: &str| headers.get(name).and_then(|v| v.to_str().ok()).map(|s| s.to_string());
    let (key_hex, time, sig_hex) = match (get("x-key"), get("x-time"), get("x-signature")) {
        (Some(k), Some(t), Some(s)) => (k, t, s),
        _ => return Err(error(StatusCode::UNAUTHORIZED, "unsigned request")),
    };
    let t: u64 = time.parse().map_err(|_| error(StatusCode::UNAUTHORIZED, "bad time"))?;
    if now().abs_diff(t) > SIGNATURE_WINDOW_SECS {
        return Err(error(StatusCode::UNAUTHORIZED, "stale request"));
    }
    let key_bytes: [u8; 32] = hex::decode(&key_hex)
        .ok()
        .and_then(|b| b.try_into().ok())
        .ok_or_else(|| error(StatusCode::UNAUTHORIZED, "bad key"))?;
    let sig_bytes: [u8; 64] = hex::decode(&sig_hex)
        .ok()
        .and_then(|b| b.try_into().ok())
        .ok_or_else(|| error(StatusCode::UNAUTHORIZED, "bad signature"))?;
    let key = VerifyingKey::from_bytes(&key_bytes).map_err(|_| error(StatusCode::UNAUTHORIZED, "bad key"))?;
    let mut message = format!("{}\n{}\n{}\n", method, uri.path(), time).into_bytes();
    message.extend_from_slice(body);
    key.verify(&message, &Signature::from_bytes(&sig_bytes))
        .map_err(|_| error(StatusCode::UNAUTHORIZED, "signature does not match"))?;
    Ok(key_hex.to_lowercase())
}

fn parse<T: for<'de> Deserialize<'de>>(body: &[u8]) -> Result<T, Response> {
    if body.is_empty() {
        return serde_json::from_slice(b"{}").map_err(|_| error(StatusCode::BAD_REQUEST, "bad body"));
    }
    serde_json::from_slice(body).map_err(|e| error(StatusCode::BAD_REQUEST, &e.to_string()))
}

// ------------------------------------------------------------------ players

#[derive(Deserialize)]
struct QueueBody {
    modes: Vec<String>,
}

async fn queue_join(State(app): State<Shared>, method: Method, uri: Uri, headers: HeaderMap, body: Bytes) -> Response {
    let player = match verify(&method, &uri, &headers, &body) {
        Ok(p) => p,
        Err(r) => return r,
    };
    let req: QueueBody = match parse(&body) {
        Ok(r) => r,
        Err(r) => return r,
    };
    let mut inner = app.inner.lock().unwrap();
    let members = match inner.player_party.get(&player).and_then(|c| inner.parties.get(c)) {
        Some(party) if party.leader != player => return error(StatusCode::FORBIDDEN, "the party leader queues"),
        Some(party) => party.members.clone(),
        None => vec![player.clone()],
    };
    // only modes whose teams fit the party
    let modes: Vec<String> = req
        .modes
        .into_iter()
        .filter(|m| matching::mode_size(m).map_or(false, |size| members.len() <= size / 2))
        .collect();
    if modes.is_empty() {
        return error(StatusCode::BAD_REQUEST, "no mode fits");
    }
    for p in &members {
        if let Some(&until) = inner.blocked.get(p) {
            if until > now() {
                return error(StatusCode::FORBIDDEN, &format!("blocked for {} s", until - now()));
            }
        }
        if inner.player_match.contains_key(p) {
            return error(StatusCode::CONFLICT, "already in a match");
        }
    }
    for p in &members {
        inner.queue.remove(p);
    }
    inner.queue.insert(player, Queued { modes, since: now(), members });
    Json(json!({ "state": "searching" })).into_response()
}

async fn queue_leave(State(app): State<Shared>, method: Method, uri: Uri, headers: HeaderMap, body: Bytes) -> Response {
    let player = match verify(&method, &uri, &headers, &body) {
        Ok(p) => p,
        Err(r) => return r,
    };
    let mut inner = app.inner.lock().unwrap();
    let key = queue_key(&inner, &player);
    inner.queue.remove(&key);
    Json(json!({ "state": "idle" })).into_response()
}

async fn status(State(app): State<Shared>, method: Method, uri: Uri, headers: HeaderMap, body: Bytes) -> Response {
    let player = match verify(&method, &uri, &headers, &body) {
        Ok(p) => p,
        Err(r) => return r,
    };
    let mut inner = app.inner.lock().unwrap();
    // the body may carry the player's name and champion
    if !body.is_empty() {
        if let Ok(p) = serde_json::from_slice::<Profile>(&body) {
            let name: String = p.name.chars().filter(|c| !c.is_control()).take(32).collect();
            let champion: String = p.champion.chars().filter(|c| c.is_ascii_alphanumeric()).take(32).collect();
            inner.profiles.insert(player.clone(), Profile { name, champion });
        }
    }
    let inner = inner;
    let party = party_json(&inner, &player);
    if let Some(q) = inner.queue.get(&queue_key(&inner, &player)) {
        return Json(json!({ "state": "searching", "modes": q.modes, "waiting": now() - q.since, "party": party }))
            .into_response();
    }
    if let Some(m) = inner.player_match.get(&player).and_then(|id| inner.matches.get(id)) {
        let team = if m.red.contains(&player) { "red" } else { "blue" };
        let store = app.store.lock().unwrap();
        let players: Vec<Value> = m
            .players()
            .map(|p| {
                let profile = inner.profiles.get(p).cloned().unwrap_or_default();
                let (rating, games) = store.rating(p, &m.mode);
                json!({
                    "key": p, "name": profile.name, "champion": profile.champion,
                    "team": if m.red.contains(p) { "red" } else { "blue" },
                    "accepted": m.accepted.contains(p),
                    "rating": if games >= 10 { json!(rating.rating.round()) } else { json!(null) },
                })
            })
            .collect();
        drop(store);
        let mut out = json!({
            "match": m.id, "mode": m.mode, "map": m.map, "team": team,
            "red": m.red, "blue": m.blue, "party": party, "players": players,
        });
        match m.state {
            MatchState::ReadyCheck => {
                out["state"] = json!("ready_check");
                out["accepted"] = json!(m.accepted.contains(&player));
                out["seconds"] = json!(m.deadline.saturating_sub(now()));
            }
            MatchState::Reserved => {
                let server = m.server.as_ref().and_then(|k| inner.servers.get(k));
                out["state"] = json!("connect");
                out["address"] = json!(server.map(|s| s.addr.clone()));
                out["token"] = json!(m.tokens.get(&player));
            }
            MatchState::Finished => out["state"] = json!("finished"),
        }
        return Json(out).into_response();
    }
    Json(json!({ "state": "idle", "party": party })).into_response()
}

/// Where a player's search is kept: under their party's leader, or their own key
fn queue_key(inner: &Inner, player: &str) -> String {
    inner
        .player_party
        .get(player)
        .and_then(|c| inner.parties.get(c))
        .map(|p| p.leader.clone())
        .unwrap_or_else(|| player.to_string())
}

fn party_json(inner: &Inner, player: &str) -> Value {
    match inner.player_party.get(player).and_then(|c| inner.parties.get(c).map(|p| (c, p))) {
        Some((code, p)) => json!({ "code": code, "leader": p.leader, "members": p.members }),
        None => Value::Null,
    }
}

// ------------------------------------------------------------------ parties

fn party_code() -> String {
    // no 0/O or 1/I to mix up when read out
    const LETTERS: &[u8] = b"ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    let mut rng = rand::thread_rng();
    (0..6).map(|_| LETTERS[rng.gen_range(0..LETTERS.len())] as char).collect()
}

/// Out of a party; the party's search stops, and the next member leads
fn leave_party(inner: &mut Inner, player: &str) {
    let code = match inner.player_party.remove(player) {
        Some(c) => c,
        None => return,
    };
    let empty = match inner.parties.get_mut(&code) {
        Some(party) => {
            inner.queue.remove(&party.leader);
            party.members.retain(|m| m != player);
            if party.leader == player {
                if let Some(next) = party.members.first() {
                    party.leader = next.clone();
                }
            }
            party.members.is_empty()
        }
        None => false,
    };
    if empty {
        inner.parties.remove(&code);
    }
}

async fn party_create(State(app): State<Shared>, method: Method, uri: Uri, headers: HeaderMap, body: Bytes) -> Response {
    let player = match verify(&method, &uri, &headers, &body) {
        Ok(p) => p,
        Err(r) => return r,
    };
    let mut inner = app.inner.lock().unwrap();
    leave_party(&mut inner, &player);
    inner.queue.remove(&player);
    let mut code = party_code();
    while inner.parties.contains_key(&code) {
        code = party_code();
    }
    inner.parties.insert(code.clone(), Party { leader: player.clone(), members: vec![player.clone()] });
    inner.player_party.insert(player, code.clone());
    Json(json!({ "code": code })).into_response()
}

#[derive(Deserialize)]
struct JoinBody {
    code: String,
}

async fn party_join(State(app): State<Shared>, method: Method, uri: Uri, headers: HeaderMap, body: Bytes) -> Response {
    let player = match verify(&method, &uri, &headers, &body) {
        Ok(p) => p,
        Err(r) => return r,
    };
    let req: JoinBody = match parse(&body) {
        Ok(r) => r,
        Err(r) => return r,
    };
    let code = req.code.trim().to_uppercase();
    let mut inner = app.inner.lock().unwrap();
    match inner.parties.get(&code) {
        None => return error(StatusCode::NOT_FOUND, "no party with that code"),
        Some(p) if p.members.len() >= PARTY_MAX => return error(StatusCode::CONFLICT, "the party is full"),
        Some(p) if p.members.contains(&player) => return Json(json!({ "code": code })).into_response(),
        Some(_) => {}
    }
    leave_party(&mut inner, &player);
    inner.queue.remove(&player);
    let leader = inner.parties[&code].leader.clone();
    inner.queue.remove(&leader); // the party changed: search again
    inner.parties.get_mut(&code).unwrap().members.push(player.clone());
    inner.player_party.insert(player, code.clone());
    Json(json!({ "code": code })).into_response()
}

async fn party_leave(State(app): State<Shared>, method: Method, uri: Uri, headers: HeaderMap, body: Bytes) -> Response {
    let player = match verify(&method, &uri, &headers, &body) {
        Ok(p) => p,
        Err(r) => return r,
    };
    leave_party(&mut app.inner.lock().unwrap(), &player);
    Json(json!({ "ok": true })).into_response()
}

#[derive(Deserialize)]
struct ReadyBody {
    accept: bool,
}

async fn ready(State(app): State<Shared>, method: Method, uri: Uri, headers: HeaderMap, body: Bytes) -> Response {
    let player = match verify(&method, &uri, &headers, &body) {
        Ok(p) => p,
        Err(r) => return r,
    };
    let req: ReadyBody = match parse(&body) {
        Ok(r) => r,
        Err(r) => return r,
    };
    let mut inner = app.inner.lock().unwrap();
    let id = match inner.player_match.get(&player) {
        Some(id) => id.clone(),
        None => return error(StatusCode::NOT_FOUND, "no match"),
    };
    if req.accept {
        if let Some(m) = inner.matches.get_mut(&id) {
            if m.state == MatchState::ReadyCheck && !m.accepted.contains(&player) {
                m.accepted.push(player);
            }
        }
    } else {
        cancel_ready_check(&mut inner, &id, &[player]);
    }
    Json(json!({ "ok": true })).into_response()
}

async fn ratings(State(app): State<Shared>, method: Method, uri: Uri, headers: HeaderMap, body: Bytes) -> Response {
    let player = match verify(&method, &uri, &headers, &body) {
        Ok(p) => p,
        Err(r) => return r,
    };
    let store = app.store.lock().unwrap();
    let list: Vec<Value> = store
        .ratings(&player)
        .into_iter()
        .map(|(mode, rating, games)| {
            // shown only after 10 placement matches
            if games >= 10 {
                json!({ "mode": mode, "rating": rating.round(), "games": games })
            } else {
                json!({ "mode": mode, "placement": games })
            }
        })
        .collect();
    Json(json!({ "player": player, "ratings": list })).into_response()
}

/// Everyone in a ready check that failed: the ones who said no (or did not
/// answer) are blocked for a minute, the rest go back to the front of the queue
fn cancel_ready_check(inner: &mut Inner, id: &str, decliners: &[String]) {
    let m = match inner.matches.remove(id) {
        Some(m) => m,
        None => return,
    };
    let t = now();
    for p in m.players() {
        inner.player_match.remove(p);
        if decliners.contains(p) || !m.accepted.contains(p) {
            inner.blocked.insert(p.clone(), t + DECLINE_BLOCK_SECS);
        }
    }
    // whole entries where everyone accepted go to the front of the queue
    for (key, members) in &m.units {
        if members.iter().all(|p| m.accepted.contains(p) && !decliners.contains(p)) {
            inner.queue.insert(
                key.clone(),
                Queued { modes: vec![m.mode.clone()], since: t.saturating_sub(180), members: members.clone() },
            );
        }
    }
}

// ------------------------------------------------------------------ servers

#[derive(Deserialize)]
struct RegisterBody {
    address: String,
    name: String,
    maps: HashMap<String, Vec<String>>, // mode -> the maps it has for that mode
}

async fn server_register(State(app): State<Shared>, method: Method, uri: Uri, headers: HeaderMap, body: Bytes) -> Response {
    let key = match verify(&method, &uri, &headers, &body) {
        Ok(k) => k,
        Err(r) => return r,
    };
    let req: RegisterBody = match parse(&body) {
        Ok(r) => r,
        Err(r) => return r,
    };
    let mut inner = app.inner.lock().unwrap();
    let busy = inner.servers.get(&key).and_then(|s| s.busy.clone());
    let modes = req.maps.keys().filter(|m| matching::mode_size(m).is_some()).cloned().collect();
    inner.servers.insert(
        key.clone(),
        GameServer { addr: req.address, name: req.name, modes, maps: req.maps, seen: now(), busy },
    );
    Json(json!({ "ok": true })).into_response()
}

/// The server's current reservation, polled by the server
async fn server_reservation(State(app): State<Shared>, method: Method, uri: Uri, headers: HeaderMap, body: Bytes) -> Response {
    let key = match verify(&method, &uri, &headers, &body) {
        Ok(k) => k,
        Err(r) => return r,
    };
    let mut inner = app.inner.lock().unwrap();
    let busy = match inner.servers.get_mut(&key) {
        Some(s) => {
            s.seen = now();
            s.busy.clone()
        }
        None => return error(StatusCode::NOT_FOUND, "not registered"),
    };
    match busy.and_then(|id| inner.matches.get(&id)) {
        Some(m) if m.state == MatchState::Reserved => Json(json!({
            "match": m.id, "mode": m.mode, "map": m.map, "red": m.red, "blue": m.blue,
            "tokens": m.tokens, "connect_seconds": CONNECT_SECS,
        }))
        .into_response(),
        _ => Json(json!({ "match": null })).into_response(),
    }
}

#[derive(Deserialize)]
struct ResultBody {
    #[serde(rename = "match")]
    match_id: String,
    // team modes: "red", "blue" or "draw"; duels: the winner's key, or "draw"
    winner: String,
    #[serde(default)]
    left_early: Vec<String>,
}

async fn server_result(State(app): State<Shared>, method: Method, uri: Uri, headers: HeaderMap, body: Bytes) -> Response {
    let key = match verify(&method, &uri, &headers, &body) {
        Ok(k) => k,
        Err(r) => return r,
    };
    let req: ResultBody = match parse(&body) {
        Ok(r) => r,
        Err(r) => return r,
    };
    let mut inner = app.inner.lock().unwrap();
    let m = match inner.matches.get_mut(&req.match_id) {
        Some(m) if m.server.as_deref() == Some(&key) && m.state == MatchState::Reserved => m,
        _ => return error(StatusCode::FORBIDDEN, "not this server's match"),
    };
    m.state = MatchState::Finished;
    let (mode, red, blue) = (m.mode.clone(), m.red.clone(), m.blue.clone());
    let players: Vec<String> = m.players().cloned().collect();
    if let Some(s) = inner.servers.get_mut(&key) {
        s.busy = None;
    }
    for p in &players {
        inner.player_match.remove(p);
    }
    drop(inner);

    // each player against the other side's average, one game per match;
    // leaving early counts as a loss
    let store = app.store.lock().unwrap();
    let side_score = |p: &String, mine_red: bool| -> f64 {
        if req.left_early.contains(p) {
            return 0.0;
        }
        match req.winner.as_str() {
            "draw" => 0.5,
            "red" => if mine_red { 1.0 } else { 0.0 },
            "blue" => if mine_red { 0.0 } else { 1.0 },
            w => if w == p.as_str() { 1.0 } else { 0.0 },
        }
    };
    let average = |team: &[String]| -> glicko::Rating {
        let rs: Vec<glicko::Rating> = team.iter().map(|p| store.rating(p, &mode).0).collect();
        let n = rs.len().max(1) as f64;
        glicko::Rating {
            rating: rs.iter().map(|r| r.rating).sum::<f64>() / n,
            deviation: rs.iter().map(|r| r.deviation).sum::<f64>() / n,
            volatility: glicko::START_VOLATILITY,
        }
    };
    let (red_avg, blue_avg) = (average(&red), average(&blue));
    let mut updated = Vec::new();
    for p in &players {
        let mine_red = red.contains(p);
        let (r, games) = store.rating(p, &mode);
        let opponent = if mine_red { blue_avg } else { red_avg };
        let new = glicko::update(r, &[(opponent, side_score(p, mine_red))]);
        updated.push((p.clone(), new, games + 1));
    }
    for (p, r, games) in updated {
        store.set_rating(&p, &mode, r, games);
    }
    store.finish_match(&req.match_id, &String::from_utf8_lossy(&body), now());
    Json(json!({ "ok": true })).into_response()
}

// ------------------------------------------------------------------ the matcher

/// Once a second: ready checks that ran out, servers that went quiet,
/// accepted matches to reserve, and new matches from the queues
fn tick(app: &App) {
    let t = now();
    let mut inner = app.inner.lock().unwrap();

    inner.blocked.retain(|_, until| *until > t);
    inner.servers.retain(|_, s| t - s.seen < SERVER_TIMEOUT_SECS);

    // a match whose server went quiet, or that never reported, is dropped
    // without ratings, and its players are free again
    let lost: Vec<String> = inner
        .matches
        .values()
        .filter(|m| {
            m.state == MatchState::Reserved
                && (m.server.as_ref().map_or(true, |k| !inner.servers.contains_key(k)) || t > m.deadline)
        })
        .map(|m| m.id.clone())
        .collect();
    for id in lost {
        if let Some(m) = inner.matches.remove(&id) {
            for p in m.players() {
                inner.player_match.remove(p);
            }
            if let Some(s) = m.server.as_ref().and_then(|k| inner.servers.get_mut(k)) {
                s.busy = None;
            }
        }
    }

    // ready checks: everyone in, or time out
    let ids: Vec<String> = inner.matches.keys().cloned().collect();
    for id in ids {
        let (state, all_in, deadline) = {
            let m = &inner.matches[&id];
            (m.state.clone(), m.players().all(|p| m.accepted.contains(p)), m.deadline)
        };
        if state == MatchState::ReadyCheck && all_in {
            reserve(app, &mut inner, &id);
        } else if state == MatchState::ReadyCheck && t > deadline {
            cancel_ready_check(&mut inner, &id, &[]);
        } else if state == MatchState::Finished {
            inner.matches.remove(&id);
        }
    }

    // new matches, mode by mode; a player queued for several modes goes to
    // the first that has a match
    for mode in matching::MODES {
        let size = matching::mode_size(mode).unwrap();
        let store = app.store.lock().unwrap();
        let waiting: Vec<matching::Waiting> = inner
            .queue
            .iter()
            .filter(|(_, q)| q.modes.iter().any(|m| m == mode) && q.members.len() <= size / 2)
            .map(|(p, q)| {
                let ratings: Vec<f64> = q.members.iter().map(|m| store.rating(m, mode).0.rating).collect();
                matching::Waiting {
                    player: p.clone(),
                    rating: matching::party_rating(&ratings),
                    since: q.since,
                    size: q.members.len(),
                }
            })
            .collect();
        drop(store);
        if let Some(group) = matching::find_group(&waiting, size, t) {
            let entries: Vec<(f64, usize)> = group.iter().map(|&i| (waiting[i].rating, waiting[i].size)).collect();
            let (red_units, blue_units) = match matching::balance(&entries) {
                Some(split) => split,
                None => continue,
            };
            let members = |key: &String| inner.queue.get(key).map(|q| q.members.clone()).unwrap_or_default();
            let units: Vec<(String, Vec<String>)> =
                group.iter().map(|&i| (waiting[i].player.clone(), members(&waiting[i].player))).collect();
            let red: Vec<String> = red_units.iter().flat_map(|&u| units[u].1.clone()).collect();
            let blue: Vec<String> = blue_units.iter().flat_map(|&u| units[u].1.clone()).collect();
            let id = token();
            for (key, _) in &units {
                inner.queue.remove(key);
            }
            for p in red.iter().chain(blue.iter()) {
                inner.player_match.insert(p.clone(), id.clone());
            }
            inner.matches.insert(
                id.clone(),
                Match {
                    id,
                    mode: mode.to_string(),
                    map: String::new(),
                    red,
                    blue,
                    units,
                    accepted: Vec::new(),
                    deadline: t + READY_SECS,
                    state: MatchState::ReadyCheck,
                    server: None,
                    tokens: HashMap::new(),
                },
            );
        }
    }
}

/// A free server that hosts the mode, a map from its pool, and a token for
/// each player; without a free server the match waits
fn reserve(app: &App, inner: &mut Inner, id: &str) {
    let mode = inner.matches[id].mode.clone();
    let key = inner
        .servers
        .iter()
        .find(|(_, s)| s.busy.is_none() && s.modes.contains(&mode))
        .map(|(k, _)| k.clone());
    let key = match key {
        Some(k) => k,
        None => return, // try again next tick
    };
    let server = inner.servers.get_mut(&key).unwrap();
    server.busy = Some(id.to_string());
    let pool = server.maps.get(&mode).cloned().unwrap_or_default();
    let (addr, name) = (server.addr.clone(), server.name.clone());
    let m = inner.matches.get_mut(id).unwrap();
    m.map = if pool.is_empty() { String::new() } else { pool[rand::thread_rng().gen_range(0..pool.len())].clone() };
    m.server = Some(key);
    m.state = MatchState::Reserved;
    m.deadline = now() + MATCH_MAX_SECS;
    m.tokens = m.players().map(|p| (p.clone(), token())).collect();
    let players: Vec<String> = m.players().cloned().collect();
    app.store.lock().unwrap().add_match(id, &mode, &m.map, &format!("{} {}", addr, name), &players.join(" "), now());
}

async fn health() -> &'static str {
    "tremor-matchmaker\n"
}

#[tokio::main]
async fn main() {
    let mut listen = "0.0.0.0:27960".to_string();
    let mut db = "matchmaker.db".to_string();
    let args: Vec<String> = std::env::args().collect();
    let mut i = 1;
    while i < args.len() {
        match args[i].as_str() {
            "--listen" if i + 1 < args.len() => {
                listen = args[i + 1].clone();
                i += 1;
            }
            "--db" if i + 1 < args.len() => {
                db = args[i + 1].clone();
                i += 1;
            }
            _ => {
                eprintln!("usage: {} [--listen addr:port] [--db file]", args[0]);
                std::process::exit(1);
            }
        }
        i += 1;
    }

    let store = store::Store::open(&db).expect("open the database");
    let app: Shared = Arc::new(App { inner: Mutex::new(Inner::default()), store: Mutex::new(store) });

    let ticker = app.clone();
    tokio::spawn(async move {
        let mut interval = tokio::time::interval(Duration::from_secs(1));
        loop {
            interval.tick().await;
            tick(&ticker);
        }
    });

    let router = Router::new()
        .route("/", get(health))
        .route("/queue", post(queue_join).delete(queue_leave))
        .route("/status", post(status))
        .route("/ready", post(ready))
        .route("/ratings", post(ratings))
        .route("/party/create", post(party_create))
        .route("/party/join", post(party_join))
        .route("/party/leave", post(party_leave))
        .route("/servers/register", post(server_register))
        .route("/servers/reservation", post(server_reservation))
        .route("/servers/result", post(server_result))
        .with_state(app);

    let listener = tokio::net::TcpListener::bind(&listen).await.expect("bind");
    println!("tremor-matchmaker listening on {}", listen);
    axum::serve(listener, router).await.expect("serve");
}
