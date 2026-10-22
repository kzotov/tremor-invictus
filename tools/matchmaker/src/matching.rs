// Tremor Invictus matchmaker
// Copyright (C) 2026 the Tremor Invictus contributors
// Licensed under the GNU General Public License, version 2 or later.
//
// Who plays whom: queues per mode, a skill window that widens with waiting,
// and teams balanced by total rating.

/// Players a match of each mode needs, two teams of half each
pub fn mode_size(mode: &str) -> Option<usize> {
    match mode {
        "duel" | "roundduel" => Some(2),
        "tdm2v2" | "ca2v2" => Some(4),
        "tdm" | "ctf" | "ca" | "sacrifice" => Some(8),
        _ => None,
    }
}

pub const MODES: [&str; 8] = ["duel", "tdm", "tdm2v2", "ctf", "ca", "ca2v2", "sacrifice", "roundduel"];

/// Rating difference allowed after waiting `secs`: +/-100, widening by 50
/// every 30 s up to +/-400; after 3 minutes anyone
pub fn skill_window(secs: u64) -> f64 {
    if secs >= 180 {
        return f64::INFINITY;
    }
    (100.0 + 50.0 * (secs / 30) as f64).min(400.0)
}

/// One entry in a queue: a player alone, or a party (rated as
/// `party_rating`) that must stay together
#[derive(Clone, Debug)]
pub struct Waiting {
    pub player: String, // the player, or the party's leader
    pub rating: f64,
    pub since: u64, // unix seconds the entry joined the queue
    pub size: usize, // players in the entry
}

/// A party's rating: the highest member's, less 10% of the gap to the average
pub fn party_rating(ratings: &[f64]) -> f64 {
    if ratings.is_empty() {
        return 0.0;
    }
    let high = ratings.iter().cloned().fold(f64::MIN, f64::max);
    let average = ratings.iter().sum::<f64>() / ratings.len() as f64;
    high - 0.1 * (high - average)
}

/// Entries for one match from the ones waiting for a mode: the longest
/// waiting entry and the nearest ratings that every member's window allows,
/// adding up to exactly `size` players
pub fn find_group(waiting: &[Waiting], size: usize, now: u64) -> Option<Vec<usize>> {
    if waiting.iter().map(|w| w.size).sum::<usize>() < size {
        return None;
    }
    // the longest wait first: their window is the widest
    let mut order: Vec<usize> = (0..waiting.len()).collect();
    order.sort_by_key(|&i| waiting[i].since);
    for &anchor in &order {
        if waiting[anchor].size > size / 2 {
            continue;
        }
        let mut near: Vec<usize> = (0..waiting.len()).filter(|&i| i != anchor).collect();
        near.sort_by(|&a, &b| {
            let da = (waiting[a].rating - waiting[anchor].rating).abs();
            let db = (waiting[b].rating - waiting[anchor].rating).abs();
            da.partial_cmp(&db).unwrap()
        });
        let mut group = vec![anchor];
        let mut players = waiting[anchor].size;
        for i in near {
            if players == size {
                break;
            }
            if players + waiting[i].size > size || waiting[i].size > size / 2 {
                continue;
            }
            // everyone in the group must accept everyone else
            let fits = group.iter().all(|&j| {
                let diff = (waiting[i].rating - waiting[j].rating).abs();
                diff <= skill_window(now.saturating_sub(waiting[i].since))
                    && diff <= skill_window(now.saturating_sub(waiting[j].since))
            });
            if fits {
                group.push(i);
                players += waiting[i].size;
            }
        }
        if players == size && balance(&group.iter().map(|&i| (waiting[i].rating, waiting[i].size)).collect::<Vec<_>>()).is_some() {
            return Some(group);
        }
    }
    None
}

/// Two teams of equal player count from entries (rating, players), with
/// total ratings as close as possible; entries never split. None when the
/// sizes can't be split evenly
pub fn balance(entries: &[(f64, usize)]) -> Option<(Vec<usize>, Vec<usize>)> {
    let n = entries.len();
    let total: usize = entries.iter().map(|e| e.1).sum();
    if total % 2 != 0 || n > 16 {
        return None;
    }
    let mut best: Option<(f64, u32)> = None;
    // the first entry stays red, so each split is tried once
    for mask in 0u32..(1 << n) {
        if mask & 1 == 0 {
            continue;
        }
        let (mut red_size, mut red_sum, mut blue_sum) = (0, 0.0, 0.0);
        for (i, e) in entries.iter().enumerate() {
            if mask & (1 << i) != 0 {
                red_size += e.1;
                red_sum += e.0 * e.1 as f64;
            } else {
                blue_sum += e.0 * e.1 as f64;
            }
        }
        if red_size * 2 != total {
            continue;
        }
        let diff = (red_sum - blue_sum).abs();
        if best.map_or(true, |(d, _)| diff < d) {
            best = Some((diff, mask));
        }
    }
    best.map(|(_, mask)| {
        let red = (0..n).filter(|i| mask & (1 << i) != 0).collect();
        let blue = (0..n).filter(|i| mask & (1 << i) == 0).collect();
        (red, blue)
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    fn w(p: &str, rating: f64, since: u64) -> Waiting {
        Waiting { player: p.into(), rating, since, size: 1 }
    }

    #[test]
    fn window_widens() {
        assert_eq!(skill_window(0), 100.0);
        assert_eq!(skill_window(30), 150.0);
        assert_eq!(skill_window(170), 350.0);
        assert!(skill_window(200).is_infinite());
    }

    #[test]
    fn duel_needs_close_ratings_at_first() {
        let q = [w("a", 1500.0, 100), w("b", 1800.0, 100)];
        assert!(find_group(&q, 2, 100).is_none());
        // after a long wait anyone will do
        assert!(find_group(&q, 2, 300).is_some());
    }

    #[test]
    fn picks_nearest() {
        let q = [w("a", 1500.0, 0), w("b", 1900.0, 0), w("c", 1550.0, 0)];
        let g = find_group(&q, 2, 0).unwrap();
        assert!(g.contains(&0) && g.contains(&2));
    }

    #[test]
    fn teams_balanced() {
        let e = [(1800.0, 1), (1700.0, 1), (1600.0, 1), (1500.0, 1)];
        let (red, blue) = balance(&e).unwrap();
        assert_eq!(red.len(), 2);
        assert_eq!(blue.len(), 2);
        let sum = |t: &Vec<usize>| t.iter().map(|&i| e[i].0).sum::<f64>();
        assert_eq!(sum(&red), sum(&blue));
    }

    #[test]
    fn parties_stay_together() {
        // a party of three, a party of two, three alone: 4v4
        let e = [(1600.0, 3), (1500.0, 2), (1550.0, 1), (1500.0, 1), (1450.0, 1)];
        let (red, blue) = balance(&e).unwrap();
        let size = |t: &Vec<usize>| t.iter().map(|&i| e[i].1).sum::<usize>();
        assert_eq!(size(&red), 4);
        assert_eq!(size(&blue), 4);
        // two parties of three and a pair can't make 4v4
        assert!(balance(&[(1500.0, 3), (1500.0, 3), (1500.0, 2)]).is_none());
    }

    #[test]
    fn group_counts_players() {
        let mut q = vec![w("a", 1500.0, 0), w("b", 1500.0, 0), w("c", 1500.0, 0)];
        q[0].size = 2;
        let g = find_group(&q, 4, 0).unwrap();
        assert_eq!(g.len(), 3);
        // a party bigger than a team never fits
        q[0].size = 3;
        assert!(find_group(&q, 4, 0).is_none());
    }

    #[test]
    fn party_rating_leans_to_the_best() {
        assert_eq!(party_rating(&[1800.0, 1400.0]), 1800.0 - 0.1 * 200.0);
    }
}
