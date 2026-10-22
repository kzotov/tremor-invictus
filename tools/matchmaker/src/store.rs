// Tremor Invictus matchmaker
// Copyright (C) 2026 the Tremor Invictus contributors
// Licensed under the GNU General Public License, version 2 or later.
//
// Ratings and match history in one SQLite file.

use crate::glicko::Rating;
use rusqlite::{params, Connection, OptionalExtension};

pub struct Store {
    db: Connection,
}

impl Store {
    pub fn open(path: &str) -> rusqlite::Result<Store> {
        let db = Connection::open(path)?;
        db.execute_batch(
            "CREATE TABLE IF NOT EXISTS ratings (
                player TEXT NOT NULL,
                mode TEXT NOT NULL,
                rating REAL NOT NULL,
                deviation REAL NOT NULL,
                volatility REAL NOT NULL,
                games INTEGER NOT NULL DEFAULT 0,
                PRIMARY KEY (player, mode)
            );
            CREATE TABLE IF NOT EXISTS matches (
                id TEXT PRIMARY KEY,
                mode TEXT NOT NULL,
                map TEXT NOT NULL,
                server TEXT NOT NULL,
                players TEXT NOT NULL,
                result TEXT,
                created INTEGER NOT NULL,
                finished INTEGER
            );",
        )?;
        Ok(Store { db })
    }

    /// A player's rating in a mode, the starting one if they have none
    pub fn rating(&self, player: &str, mode: &str) -> (Rating, u32) {
        self.db
            .query_row(
                "SELECT rating, deviation, volatility, games FROM ratings WHERE player = ?1 AND mode = ?2",
                params![player, mode],
                |r| Ok((Rating { rating: r.get(0)?, deviation: r.get(1)?, volatility: r.get(2)? }, r.get(3)?)),
            )
            .optional()
            .ok()
            .flatten()
            .unwrap_or((Rating::default(), 0))
    }

    pub fn set_rating(&self, player: &str, mode: &str, rating: Rating, games: u32) {
        let _ = self.db.execute(
            "INSERT INTO ratings (player, mode, rating, deviation, volatility, games) VALUES (?1, ?2, ?3, ?4, ?5, ?6)
             ON CONFLICT (player, mode) DO UPDATE SET rating = ?3, deviation = ?4, volatility = ?5, games = ?6",
            params![player, mode, rating.rating, rating.deviation, rating.volatility, games],
        );
    }

    pub fn add_match(&self, id: &str, mode: &str, map: &str, server: &str, players: &str, now: u64) {
        let _ = self.db.execute(
            "INSERT INTO matches (id, mode, map, server, players, created) VALUES (?1, ?2, ?3, ?4, ?5, ?6)",
            params![id, mode, map, server, players, now as i64],
        );
    }

    pub fn finish_match(&self, id: &str, result: &str, now: u64) {
        let _ = self.db.execute(
            "UPDATE matches SET result = ?2, finished = ?3 WHERE id = ?1",
            params![id, result, now as i64],
        );
    }

    /// Every mode's rating for a player: (mode, rating, games)
    pub fn ratings(&self, player: &str) -> Vec<(String, f64, u32)> {
        let mut out = Vec::new();
        if let Ok(mut st) = self.db.prepare("SELECT mode, rating, games FROM ratings WHERE player = ?1 ORDER BY mode") {
            if let Ok(rows) = st.query_map(params![player], |r| Ok((r.get(0)?, r.get(1)?, r.get(2)?))) {
                out.extend(rows.flatten());
            }
        }
        out
    }
}
