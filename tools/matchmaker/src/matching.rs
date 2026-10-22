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

#[derive(Clone, Debug)]
pub struct Waiting {
    pub player: String,
    pub rating: f64,
    pub since: u64, // unix seconds the player joined the queue
}

/// A group for one match from the players waiting for a mode: the longest
/// waiting player and the nearest ratings that every member's window allows
pub fn find_group(waiting: &[Waiting], size: usize, now: u64) -> Option<Vec<usize>> {
    if waiting.len() < size {
        return None;
    }
    // the longest wait first: their window is the widest
    let mut order: Vec<usize> = (0..waiting.len()).collect();
    order.sort_by_key(|&i| waiting[i].since);
    for &anchor in &order {
        let mut near: Vec<usize> = (0..waiting.len()).filter(|&i| i != anchor).collect();
        near.sort_by(|&a, &b| {
            let da = (waiting[a].rating - waiting[anchor].rating).abs();
            let db = (waiting[b].rating - waiting[anchor].rating).abs();
            da.partial_cmp(&db).unwrap()
        });
        let mut group = vec![anchor];
        for i in near {
            if group.len() == size {
                break;
            }
            // everyone in the group must accept everyone else
            let fits = group.iter().all(|&j| {
                let diff = (waiting[i].rating - waiting[j].rating).abs();
                diff <= skill_window(now.saturating_sub(waiting[i].since))
                    && diff <= skill_window(now.saturating_sub(waiting[j].since))
            });
            if fits {
                group.push(i);
            }
        }
        if group.len() == size {
            return Some(group);
        }
    }
    None
}

/// Two teams of equal size with total ratings as close as the greedy split
/// gets: strongest first, each to the team with the lower total
pub fn balance(ratings: &[f64]) -> (Vec<usize>, Vec<usize>) {
    let mut order: Vec<usize> = (0..ratings.len()).collect();
    order.sort_by(|&a, &b| ratings[b].partial_cmp(&ratings[a]).unwrap());
    let half = ratings.len() / 2;
    let (mut red, mut blue) = (Vec::new(), Vec::new());
    let (mut red_sum, mut blue_sum) = (0.0, 0.0);
    for i in order {
        if (red_sum <= blue_sum && red.len() < half) || blue.len() >= half {
            red_sum += ratings[i];
            red.push(i);
        } else {
            blue_sum += ratings[i];
            blue.push(i);
        }
    }
    (red, blue)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn w(p: &str, rating: f64, since: u64) -> Waiting {
        Waiting { player: p.into(), rating, since }
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
        let (red, blue) = balance(&[1800.0, 1700.0, 1600.0, 1500.0]);
        assert_eq!(red.len(), 2);
        assert_eq!(blue.len(), 2);
        let sum = |t: &Vec<usize>, r: &[f64]| t.iter().map(|&i| r[i]).sum::<f64>();
        let r = [1800.0, 1700.0, 1600.0, 1500.0];
        assert!((sum(&red, &r) - sum(&blue, &r)).abs() <= 200.0);
    }
}
