// Tremor Invictus matchmaker
// Copyright (C) 2026 the Tremor Invictus contributors
// Licensed under the GNU General Public License, version 2 or later.
//
// Glicko-2 ratings, one match per rating period. Ratings are kept on the
// familiar 1500 scale; the update works on the internal scale (mu, phi).

const SCALE: f64 = 173.7178;
const TAU: f64 = 0.5; // how fast volatility may change
const EPSILON: f64 = 0.000001;

pub const START_RATING: f64 = 1500.0;
pub const START_DEVIATION: f64 = 350.0;
pub const START_VOLATILITY: f64 = 0.06;

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Rating {
    pub rating: f64,
    pub deviation: f64,
    pub volatility: f64,
}

impl Default for Rating {
    fn default() -> Self {
        Rating { rating: START_RATING, deviation: START_DEVIATION, volatility: START_VOLATILITY }
    }
}

fn g(phi: f64) -> f64 {
    1.0 / (1.0 + 3.0 * phi * phi / (std::f64::consts::PI * std::f64::consts::PI)).sqrt()
}

fn expected(mu: f64, mu_j: f64, phi_j: f64) -> f64 {
    1.0 / (1.0 + (-g(phi_j) * (mu - mu_j)).exp())
}

/// The new rating after one rating period against these opponents, with a
/// score per game (1 win, 0.5 draw, 0 loss).
pub fn update(player: Rating, games: &[(Rating, f64)]) -> Rating {
    let mu = (player.rating - START_RATING) / SCALE;
    let phi = player.deviation / SCALE;
    let sigma = player.volatility;

    if games.is_empty() {
        // no games: only the deviation grows
        let phi_star = (phi * phi + sigma * sigma).sqrt();
        return Rating { rating: player.rating, deviation: phi_star * SCALE, volatility: sigma };
    }

    // estimated variance and improvement from the games
    let mut v_inv = 0.0;
    let mut delta_sum = 0.0;
    for (opponent, score) in games {
        let mu_j = (opponent.rating - START_RATING) / SCALE;
        let phi_j = opponent.deviation / SCALE;
        let e = expected(mu, mu_j, phi_j);
        let gj = g(phi_j);
        v_inv += gj * gj * e * (1.0 - e);
        delta_sum += gj * (score - e);
    }
    let v = 1.0 / v_inv;
    let delta = v * delta_sum;

    // the new volatility, by the Illinois algorithm
    let a = (sigma * sigma).ln();
    let f = |x: f64| {
        let ex = x.exp();
        ex * (delta * delta - phi * phi - v - ex) / (2.0 * (phi * phi + v + ex).powi(2)) - (x - a) / (TAU * TAU)
    };
    let mut big_a = a;
    let mut big_b = if delta * delta > phi * phi + v {
        (delta * delta - phi * phi - v).ln()
    } else {
        let mut k = 1.0;
        while f(a - k * TAU) < 0.0 {
            k += 1.0;
        }
        a - k * TAU
    };
    let mut f_a = f(big_a);
    let mut f_b = f(big_b);
    while (big_b - big_a).abs() > EPSILON {
        let big_c = big_a + (big_a - big_b) * f_a / (f_b - f_a);
        let f_c = f(big_c);
        if f_c * f_b <= 0.0 {
            big_a = big_b;
            f_a = f_b;
        } else {
            f_a /= 2.0;
        }
        big_b = big_c;
        f_b = f_c;
    }
    let sigma_new = (big_a / 2.0).exp();

    let phi_star = (phi * phi + sigma_new * sigma_new).sqrt();
    let phi_new = 1.0 / (1.0 / (phi_star * phi_star) + 1.0 / v).sqrt();
    let mu_new = mu + phi_new * phi_new * delta_sum;

    Rating { rating: mu_new * SCALE + START_RATING, deviation: phi_new * SCALE, volatility: sigma_new }
}

#[cfg(test)]
mod tests {
    use super::*;

    // The worked example from the Glicko-2 description: a 1500 / 200 player
    // beats a 1400 / 30 player, loses to 1550 / 100 and to 1700 / 300.
    #[test]
    fn worked_example() {
        let player = Rating { rating: 1500.0, deviation: 200.0, volatility: 0.06 };
        let games = [
            (Rating { rating: 1400.0, deviation: 30.0, volatility: 0.06 }, 1.0),
            (Rating { rating: 1550.0, deviation: 100.0, volatility: 0.06 }, 0.0),
            (Rating { rating: 1700.0, deviation: 300.0, volatility: 0.06 }, 0.0),
        ];
        let r = update(player, &games);
        assert!((r.rating - 1464.06).abs() < 0.1, "rating {}", r.rating);
        assert!((r.deviation - 151.52).abs() < 0.1, "deviation {}", r.deviation);
        assert!((r.volatility - 0.05999).abs() < 0.0001, "volatility {}", r.volatility);
    }

    #[test]
    fn winner_goes_up() {
        let a = Rating::default();
        let b = Rating::default();
        assert!(update(a, &[(b, 1.0)]).rating > 1500.0);
        assert!(update(b, &[(a, 0.0)]).rating < 1500.0);
    }
}
