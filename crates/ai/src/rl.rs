//! Reinforcement-learning agent loop API.
//!
//! Gives AI agents a standard observation → action → reward interface so
//! games built on Litt can expose trainable environments, and agents can
//! be trained or evaluated headlessly (the engine ethos).
//!
//! Components:
//! - [`Environment`] trait: reset / step returning observations + reward
//! - [`Agent`] trait: act / learn
//! - [`RandomAgent`] — uniform baseline over discrete actions
//! - [`TabularQAgent`] — real tabular Q-learning (epsilon-greedy)
//! - [`run_episode`] / [`train_episodes`] — evaluation + training loops
//!
//! Zero dependencies; deterministic RNG so runs are reproducible.

// =============================================================================
// Spaces
// =============================================================================

/// Action space definition.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ActionSpace {
    /// N discrete actions (indices 0..N)
    Discrete(u32),
    /// Continuous action vector of dimension D (clamped to [-1, 1])
    Continuous(u32),
}

impl ActionSpace {
    pub fn size(&self) -> u32 {
        match self {
            Self::Discrete(n) | Self::Continuous(n) => *n,
        }
    }
}

/// Flat float observation vector.
pub type Observation = Vec<f32>;

/// Reward for one step.
pub type Reward = f32;

/// Result of one environment step.
#[derive(Clone, Debug)]
pub struct StepOutput {
    pub observation: Observation,
    pub reward: Reward,
    pub done: bool,
    /// Optional human/agent-readable info (score events, truncation reason…)
    pub info: Vec<String>,
}

/// An action chosen by an agent.
#[derive(Clone, Debug)]
pub enum Action {
    Discrete(u32),
    Continuous(Vec<f32>),
}

// =============================================================================
// Traits
// =============================================================================

/// A trainable/evaluable environment (game level, sim, …).
pub trait Environment {
    /// Reset to the initial state; returns the first observation.
    fn reset(&mut self) -> Observation;

    /// Apply an action; returns the next observation, reward, and done flag.
    fn step(&mut self, action: &Action) -> StepOutput;

    fn observation_size(&self) -> usize;
    fn action_space(&self) -> ActionSpace;
}

/// An agent that observes and acts, optionally learning.
pub trait Agent {
    fn name(&self) -> &str;

    /// Pick an action for the current observation.
    fn act(&mut self, observation: &Observation) -> Action;

    /// Learn from one transition (SARSA/Q-learning style).
    /// Default: no-op (for non-learning agents).
    fn observe(
        &mut self,
        _prev_observation: &Observation,
        _action: &Action,
        _reward: Reward,
        _next_observation: &Observation,
        _done: bool,
    ) {
    }
}

// =============================================================================
// Deterministic RNG (xorshift64*)
// =============================================================================

/// Small deterministic RNG so episodes are reproducible across machines.
#[derive(Clone, Debug)]
pub struct Rng(u64);

impl Rng {
    pub fn new(seed: u64) -> Self {
        Self(seed.max(1))
    }

    pub fn next_u64(&mut self) -> u64 {
        let mut x = self.0;
        x ^= x >> 12;
        x ^= x << 25;
        x ^= x >> 27;
        self.0 = x;
        x.wrapping_mul(0x2545F4914F6CDD1D)
    }

    /// Uniform f32 in [0, 1)
    pub fn next_f32(&mut self) -> f32 {
        (self.next_u64() >> 40) as f32 / (1u64 << 24) as f32
    }

    /// Uniform integer in [0, n)
    pub fn below(&mut self, n: u32) -> u32 {
        (self.next_u64() % n.max(1) as u64) as u32
    }
}

impl Default for Rng {
    fn default() -> Self { Self::new(0x9E3779B97F4A7C15) }
}

// =============================================================================
// Baseline agent
// =============================================================================

/// Picks uniformly random actions — baseline for sanity-checking envs.
pub struct RandomAgent {
    space: ActionSpace,
    rng: Rng,
}

impl RandomAgent {
    pub fn new(space: ActionSpace, seed: u64) -> Self {
        Self { space, rng: Rng::new(seed) }
    }
}

impl Agent for RandomAgent {
    fn name(&self) -> &str { "random" }

    fn act(&mut self, _observation: &Observation) -> Action {
        match self.space {
            ActionSpace::Discrete(n) => Action::Discrete(self.rng.below(n)),
            ActionSpace::Continuous(n) => {
                Action::Continuous((0..n).map(|_| self.rng.next_f32() * 2.0 - 1.0).collect())
            }
        }
    }
}

// =============================================================================
// Tabular Q-learning agent
// =============================================================================

/// Tabular Q-learning with epsilon-greedy exploration over discretized states.
///
/// Observations are bucketed to `buckets_per_dim` levels per dimension, so
/// any continuous environment becomes table-addressable. Real learning,
/// zero dependencies.
pub struct TabularQAgent {
    action_space: ActionSpace,
    obs_buckets: u32,
    obs_range: (f32, f32),
    pub learning_rate: f32,
    pub discount: f32,
    pub epsilon: f32,
    epsilon_decay: f32,
    epsilon_min: f32,
    q: std::collections::HashMap<u64, Vec<f32>>,
    rng: Rng,
    last_state: Option<u64>,
    last_action: Option<u32>,
    pub updates: u64,
}

impl TabularQAgent {
    /// Create a Q-learning agent.
    ///
    /// * `obs_buckets` — discretization levels per observation dim
    /// * `obs_range`   — expected min/max of each observation dim
    pub fn new(action_space: ActionSpace, obs_buckets: u32, obs_range: (f32, f32)) -> Self {
        Self {
            action_space,
            obs_buckets,
            obs_range,
            learning_rate: 0.1,
            discount: 0.99,
            epsilon: 0.2,
            epsilon_decay: 0.999,
            epsilon_min: 0.01,
            q: std::collections::HashMap::new(),
            rng: Rng::new(0xDEADBEEF),
            last_state: None,
            last_action: None,
            updates: 0,
        }
    }

    fn discretize(&self, observation: &Observation) -> u64 {
        // FNV-style fold of per-dim bucket ids into one state key
        let (lo, hi) = self.obs_range;
        let span = (hi - lo).max(1e-6);
        let mut hash: u64 = 0xCBF29CE484222325;
        for &v in observation {
            let norm = ((v - lo) / span).clamp(0.0, 0.999_999);
            let bucket = (norm * self.obs_buckets as f32) as u32;
            hash = (hash ^ bucket as u64).wrapping_mul(0x100000001B3);
        }
        hash
    }

    fn q_row(&mut self, state: u64) -> &mut Vec<f32> {
        let n = self.action_space.size() as usize;
        self.q.entry(state).or_insert_with(|| vec![0.0; n])
    }

    fn greedy_action(&mut self, state: u64) -> u32 {
        let row = self.q_row(state).clone();
        let mut best = 0u32;
        let mut best_v = f32::NEG_INFINITY;
        for (i, &v) in row.iter().enumerate() {
            if v > best_v {
                best_v = v;
                best = i as u32;
            }
        }
        best
    }
}

impl Agent for TabularQAgent {
    fn name(&self) -> &str { "tabular-q" }

    fn act(&mut self, observation: &Observation) -> Action {
        let state = self.discretize(observation);
        let n = self.action_space.size();

        let action = if self.rng.next_f32() < self.epsilon {
            self.rng.below(n)
        } else {
            self.greedy_action(state)
        };

        self.last_state = Some(state);
        self.last_action = Some(action);
        Action::Discrete(action)
    }

    fn observe(
        &mut self,
        _prev_observation: &Observation,
        _action: &Action,
        reward: Reward,
        next_observation: &Observation,
        done: bool,
    ) {
        let (Some(s), Some(a)) = (self.last_state, self.last_action) else { return };
        let next_state = self.discretize(next_observation);
        let (lr, discount) = (self.learning_rate, self.discount);

        let next_max = if done {
            0.0
        } else {
            let row = self.q_row(next_state).clone();
            row.iter().copied().fold(f32::NEG_INFINITY, f32::max).max(0.0)
        };

        let row = self.q_row(s);
        let target = reward + discount * next_max;
        row[a as usize] += lr * (target - row[a as usize]);

        // Decay exploration
        self.epsilon = (self.epsilon * self.epsilon_decay).max(self.epsilon_min);
        self.updates += 1;
    }
}

// =============================================================================
// Episode loops
// =============================================================================

/// Statistics from one episode.
#[derive(Clone, Debug)]
pub struct EpisodeStats {
    pub steps: u32,
    pub total_reward: Reward,
    pub final_info: Vec<String>,
}

/// Run one evaluation episode (no learning beyond agent.observe calls).
pub fn run_episode<E: Environment, A: Agent>(
    env: &mut E,
    agent: &mut A,
    max_steps: u32,
) -> EpisodeStats {
    let mut obs = env.reset();
    let mut total = 0.0;
    let mut steps = 0u32;
    let mut info = Vec::new();

    for _ in 0..max_steps {
        steps += 1;
        let action = agent.act(&obs);
        let out = env.step(&action);
        total += out.reward;
        agent.observe(&obs, &action, out.reward, &out.observation, out.done);
        obs = out.observation;
        if !out.info.is_empty() {
            info.extend(out.info);
        }
        if out.done {
            break;
        }
    }

    EpisodeStats { steps, total_reward: total, final_info: info }
}

/// Train an agent for `episodes`, returns per-episode rewards.
pub fn train_episodes<E: Environment, A: Agent>(
    env: &mut E,
    agent: &mut A,
    episodes: u32,
    max_steps: u32,
) -> Vec<Reward> {
    let mut rewards = Vec::with_capacity(episodes as usize);
    for _ in 0..episodes {
        let stats = run_episode(env, agent, max_steps);
        rewards.push(stats.total_reward);
    }
    rewards
}

// =============================================================================
// Demo environment (for tests / agent smoke-testing)
// =============================================================================

/// Grid-world: reach the goal at position `goal`; +1 reward, -0.01 per step.
/// Observation = normalized (agent_x, agent_y). Discrete 4-action space.
pub struct GridWorld {
    size: u32,
    goal: (u32, u32),
    pos: (i32, i32),
    steps: u32,
    max_steps: u32,
}

impl GridWorld {
    pub fn new(size: u32, max_steps: u32) -> Self {
        let edge = size as i32 - 1;
        Self {
            size,
            goal: (edge.max(0) as u32, edge.max(0) as u32),
            pos: (0, 0),
            steps: 0,
            max_steps,
        }
    }

    fn obs(&self) -> Observation {
        vec![
            self.pos.0 as f32 / self.size as f32,
            self.pos.1 as f32 / self.size as f32,
        ]
    }
}

impl Default for GridWorld {
    fn default() -> Self { Self::new(8, 128) }
}

impl Environment for GridWorld {
    fn reset(&mut self) -> Observation {
        self.pos = (0, 0);
        self.steps = 0;
        self.obs()
    }

    fn step(&mut self, action: &Action) -> StepOutput {
        let a = match action {
            Action::Discrete(a) => *a % 4,
            Action::Continuous(v) => {
                let idx = v.iter().enumerate().max_by(|(_, x), (_, y)| x.partial_cmp(y).unwrap_or(std::cmp::Ordering::Equal));
                idx.map(|(i, _)| i as u32 % 4).unwrap_or(0)
            }
        };
        match a {
            0 => self.pos.0 = (self.pos.0 + 1).min(self.size as i32 - 1),
            1 => self.pos.0 = (self.pos.0 - 1).max(0),
            2 => self.pos.1 = (self.pos.1 + 1).min(self.size as i32 - 1),
            _ => self.pos.1 = (self.pos.1 - 1).max(0),
        }

        self.steps += 1;
        let reached = (self.pos.0 as u32, self.pos.1 as u32) == self.goal;
        let timeout = self.steps >= self.max_steps;
        let reward = if reached { 1.0 } else { -0.01 };

        StepOutput {
            observation: self.obs(),
            reward,
            done: reached || timeout,
            info: if reached { vec!["goal".to_string()] } else { Vec::new() },
        }
    }

    fn observation_size(&self) -> usize { 2 }

    fn action_space(&self) -> ActionSpace { ActionSpace::Discrete(4) }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn episode_step_count_is_real_steps() {
        // Goal is at the far corner of an 8x8 grid: with max_steps=5 the
        // episode times out after exactly 5 steps.
        let mut env = GridWorld::new(8, 5);
        let mut agent = RandomAgent::new(ActionSpace::Discrete(4), 42);
        let stats = run_episode(&mut env, &mut agent, 5);
        assert_eq!(stats.steps, 5);
    }

    #[test]
    fn random_agent_runs_episode() {
        let mut env = GridWorld::new(8, 200);
        let mut agent = RandomAgent::new(ActionSpace::Discrete(4), 42);
        let stats = run_episode(&mut env, &mut agent, 200);
        assert!(stats.total_reward > -3.0); // bounded penalty world
    }

    #[test]
    fn q_agent_learns_gridworld() {
        let mut env = GridWorld::new(8, 256);
        let mut agent = TabularQAgent::new(ActionSpace::Discrete(4), 16, (0.0, 1.0));
        let rewards = train_episodes(&mut env, &mut agent, 400, 256);

        // Early episodes should be worse than late ones (learning signal)
        let early: f32 = rewards[..50].iter().sum::<f32>() / 50.0;
        let late: f32 = rewards[350..].iter().sum::<f32>() / 50.0;
        assert!(late > early, "expected learning: early={early:.3} late={late:.3}");
        assert!(agent.updates > 100);
    }

    #[test]
    fn rng_is_deterministic() {
        let mut a = Rng::new(7);
        let mut b = Rng::new(7);
        for _ in 0..100 {
            assert_eq!(a.next_u64(), b.next_u64());
        }
    }
}
