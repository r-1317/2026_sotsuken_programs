use std::cmp::Ordering;
use std::collections::{BinaryHeap, VecDeque};
use std::env;
use std::fmt::Display;
use std::process;
use std::time::Instant;

#[derive(Clone)]
pub struct TreeNode {
    pub parent: isize,
    pub children: Vec<usize>,
    pub turn: usize,
    pub action_id: isize,
}

#[derive(Clone)]
pub struct State {
    pub turn: usize,
    pub f_sum: f64,
    pub g_sum_known: f64,
    pub g_sum_true: f64,
    pub pending_g: VecDeque<f64>,
    pub tree_index: isize,
    pub state_id: u64,
    pub padding: Vec<u8>,
}

impl State {
    pub fn new(padding_size: usize) -> Self {
        Self {
            turn: 0,
            f_sum: 0.0,
            g_sum_known: 0.0,
            g_sum_true: 0.0,
            pending_g: VecDeque::new(),
            tree_index: -1,
            state_id: 0,
            padding: vec![0; padding_size],
        }
    }

    pub fn eval(&self) -> f64 {
        self.f_sum + self.g_sum_known
    }

    pub fn true_score(&self) -> f64 {
        self.f_sum + self.g_sum_true
    }
}

pub struct Args {
    pub loops: usize,
    pub branch: usize,
    pub max_turn: usize,
    pub delay: usize,
    pub seed: u64,
    pub padding_size: usize,
}

enum ParseSignal {
    Help,
    Error(String),
}

fn usage(prog: &str) {
    eprintln!(
        "Usage: {} [--loops N] [--branch N] [--turns N] [--delay L] [--seed N] [--padding-size N]",
        prog
    );
    eprintln!(
        "Defaults: loops=1000 branch=30 turns=100 delay=10 seed=1 padding-size=0"
    );
}

fn parse_next<T>(argv: &[String], index: &mut usize, name: &str) -> Result<T, ParseSignal>
where
    T: std::str::FromStr,
    T::Err: Display,
{
    if *index + 1 >= argv.len() {
        return Err(ParseSignal::Error(format!("Missing value for {}", name)));
    }

    *index += 1;
    argv[*index]
        .parse::<T>()
        .map_err(|err| ParseSignal::Error(format!("Invalid value for {}: {}", name, err)))
}

fn parse_args(argv: &[String]) -> Result<Args, ParseSignal> {
    let mut loops = 1000usize;
    let mut branch = 30usize;
    let mut max_turn = 100usize;
    let mut delay = 10usize;
    let mut seed = 1u64;
    let mut padding_size = 0usize;

    let mut index = 1usize;
    while index < argv.len() {
        let arg = &argv[index];

        match arg.as_str() {
            "--loops" => {
                loops = parse_next(argv, &mut index, arg)?;
            }
            "--branch" => {
                branch = parse_next(argv, &mut index, arg)?;
            }
            "--turns" => {
                max_turn = parse_next(argv, &mut index, arg)?;
            }
            "--delay" => {
                delay = parse_next(argv, &mut index, arg)?;
            }
            "--seed" => {
                seed = parse_next(argv, &mut index, arg)?;
            }
            "--padding-size" => {
                padding_size = parse_next(argv, &mut index, arg)?;
            }
            "-h" | "--help" => return Err(ParseSignal::Help),
            _ => return Err(ParseSignal::Error(format!("Unknown argument: {}", arg))),
        }

        index += 1;
    }

    if loops == 0 || branch == 0 || max_turn == 0 {
        return Err(ParseSignal::Error("Invalid argument.".to_string()));
    }

    Ok(Args {
        loops,
        branch,
        max_turn,
        delay,
        seed,
        padding_size,
    })
}

struct Mt19937_64 {
    mt: [u64; 312],
    index: usize,
}

impl Mt19937_64 {
    fn new(seed: u64) -> Self {
        let mut mt = [0u64; 312];
        mt[0] = seed;
        for i in 1..312 {
            mt[i] = 6364136223846793005u64
                .wrapping_mul(mt[i - 1] ^ (mt[i - 1] >> 62))
                .wrapping_add(i as u64);
        }

        Self { mt, index: 312 }
    }

    fn next_u64(&mut self) -> u64 {
        const NN: usize = 312;
        const MM: usize = 156;
        const MATRIX_A: u64 = 0xB5026F5AA96619E9;
        const UM: u64 = 0xFFFFFFFF80000000;
        const LM: u64 = 0x7FFFFFFF;

        if self.index >= NN {
            for i in 0..(NN - MM) {
                let x = (self.mt[i] & UM) | (self.mt[i + 1] & LM);
                self.mt[i] = self.mt[i + MM] ^ (x >> 1) ^ if x & 1 != 0 { MATRIX_A } else { 0 };
            }
            for i in (NN - MM)..(NN - 1) {
                let x = (self.mt[i] & UM) | (self.mt[i + 1] & LM);
                self.mt[i] = self.mt[i + MM - NN]
                    ^ (x >> 1)
                    ^ if x & 1 != 0 { MATRIX_A } else { 0 };
            }
            let x = (self.mt[NN - 1] & UM) | (self.mt[0] & LM);
            self.mt[NN - 1] = self.mt[MM - 1] ^ (x >> 1) ^ if x & 1 != 0 { MATRIX_A } else { 0 };
            self.index = 0;
        }

        let mut x = self.mt[self.index];
        self.index += 1;

        x ^= (x >> 29) & 0x5555555555555555;
        x ^= (x << 17) & 0x71D67FFFEDA60000;
        x ^= (x << 37) & 0xFFF7EEE000000000;
        x ^= x >> 43;
        x
    }

    fn next_f64(&mut self) -> f64 {
        self.next_u64() as f64 / (u64::MAX as f64 + 1.0)
    }
}

fn build_path(tree: &[TreeNode], mut node_index: isize) -> Vec<usize> {
    let mut path_nodes = Vec::new();
    while node_index != -1 {
        path_nodes.push(node_index as usize);
        node_index = tree[node_index as usize].parent;
    }
    path_nodes.reverse();
    path_nodes
}

#[derive(Clone)]
struct HeapItem(State);

impl PartialEq for HeapItem {
    fn eq(&self, other: &Self) -> bool {
        self.0.eval().total_cmp(&other.0.eval()) == Ordering::Equal
    }
}

impl Eq for HeapItem {}

impl PartialOrd for HeapItem {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl Ord for HeapItem {
    fn cmp(&self, other: &Self) -> Ordering {
        self.0.eval().total_cmp(&other.0.eval())
    }
}

fn run_existing_impl(args: Args) {
    let start_time = Instant::now();

    let mut rng = Mt19937_64::new(args.seed);
    let mut pq: Vec<BinaryHeap<HeapItem>> = (0..=args.max_turn).map(|_| BinaryHeap::new()).collect();
    let mut tree = Vec::new();
    let mut next_state_id = 0u64;

    let mut root = State::new(args.padding_size);
    root.turn = 0;
    root.tree_index = 0;
    root.state_id = next_state_id;
    next_state_id += 1;

    tree.push(TreeNode {
        parent: -1,
        children: Vec::new(),
        turn: 0,
        action_id: -1,
    });

    pq[0].push(HeapItem(root));

    for _ in 0..args.loops {
        for t in 0..args.max_turn {
            if pq[t].is_empty() {
                continue;
            }

            let cur = pq[t].pop().expect("frontier was checked for emptiness").0;

            for k in 0..args.branch {
                let f = rng.next_f64();
                let g = rng.next_f64();

                let mut nxt = cur.clone();
                nxt.turn = cur.turn + 1;
                nxt.f_sum += f;
                nxt.g_sum_true += g;
                nxt.pending_g.push_back(g);
                nxt.state_id = next_state_id;
                next_state_id += 1;

                if nxt.pending_g.len() > args.delay {
                    if let Some(front) = nxt.pending_g.pop_front() {
                        nxt.g_sum_known += front;
                    }
                }

                let node_index = tree.len();
                tree.push(TreeNode {
                    parent: cur.tree_index,
                    children: Vec::new(),
                    turn: nxt.turn,
                    action_id: k as isize,
                });

                tree[cur.tree_index as usize].children.push(node_index);
                nxt.tree_index = node_index as isize;

                pq[t + 1].push(HeapItem(nxt));
            }
        }
    }

    let mut best: Option<State> = None;

    if let Some(item) = pq[args.max_turn].peek() {
        best = Some(item.0.clone());
    } else {
        for t in (0..args.max_turn).rev() {
            if let Some(item) = pq[t].peek() {
                best = Some(item.0.clone());
                break;
            }
        }
    }

    let Some(best) = best else {
        eprintln!("No state found.");
        process::exit(1);
    };

    let _path_nodes = build_path(&tree, best.tree_index);

    let elapsed_time = start_time.elapsed().as_secs_f64();
    eprintln!("elapsed_time = {} seconds", elapsed_time);
}

struct AVLSet {
    root: Option<Box<Node>>,
    size: usize,
}

struct Node {
    value: State,
    left: Option<Box<Node>>,
    right: Option<Box<Node>>,
    height: i32,
}

impl Node {
    fn new(value: State) -> Self {
        Self {
            value,
            left: None,
            right: None,
            height: 1,
        }
    }
}

impl AVLSet {
    fn new() -> Self {
        Self { root: None, size: 0 }
    }

    fn empty(&self) -> bool {
        self.root.is_none()
    }

    fn size(&self) -> usize {
        self.size
    }

    fn height(node: &Option<Box<Node>>) -> i32 {
        node.as_ref().map_or(0, |node| node.height)
    }

    fn compare(a: &State, b: &State) -> Ordering {
        match a.eval().total_cmp(&b.eval()) {
            Ordering::Equal => a.state_id.cmp(&b.state_id),
            other => other,
        }
    }

    fn update(node: &mut Box<Node>) {
        node.height = 1 + Self::height(&node.left).max(Self::height(&node.right));
    }

    fn balance_factor(node: &Node) -> i32 {
        Self::height(&node.left) - Self::height(&node.right)
    }

    fn rotate_right(mut node: Box<Node>) -> Box<Node> {
        let mut new_root = node.left.take().expect("rotate_right requires a left child");
        node.left = new_root.right.take();
        Self::update(&mut node);
        new_root.right = Some(node);
        Self::update(&mut new_root);
        new_root
    }

    fn rotate_left(mut node: Box<Node>) -> Box<Node> {
        let mut new_root = node.right.take().expect("rotate_left requires a right child");
        node.right = new_root.left.take();
        Self::update(&mut node);
        new_root.left = Some(node);
        Self::update(&mut new_root);
        new_root
    }

    fn rebalance(mut node: Box<Node>) -> Box<Node> {
        Self::update(&mut node);
        let bf = Self::balance_factor(&node);

        if bf > 1 {
            if Self::balance_factor(node.left.as_ref().expect("left child exists")) < 0 {
                let left = node.left.take().expect("left child exists");
                node.left = Some(Self::rotate_left(left));
            }
            return Self::rotate_right(node);
        }

        if bf < -1 {
            if Self::balance_factor(node.right.as_ref().expect("right child exists")) > 0 {
                let right = node.right.take().expect("right child exists");
                node.right = Some(Self::rotate_right(right));
            }
            return Self::rotate_left(node);
        }

        node
    }

    fn insert_node(node: Option<Box<Node>>, value: State) -> Box<Node> {
        match node {
            None => Box::new(Node::new(value)),
            Some(mut node) => {
                if Self::compare(&value, &node.value) == Ordering::Less {
                    node.left = Some(Self::insert_node(node.left.take(), value));
                } else {
                    node.right = Some(Self::insert_node(node.right.take(), value));
                }
                Self::rebalance(node)
            }
        }
    }

    fn erase_min_node(node: Option<Box<Node>>) -> Option<Box<Node>> {
        match node {
            None => None,
            Some(mut node) => {
                if node.left.is_none() {
                    return node.right;
                }
                node.left = Self::erase_min_node(node.left.take());
                Some(Self::rebalance(node))
            }
        }
    }

    fn erase_max_node(node: Option<Box<Node>>) -> Option<Box<Node>> {
        match node {
            None => None,
            Some(mut node) => {
                if node.right.is_none() {
                    return node.left;
                }
                node.right = Self::erase_max_node(node.right.take());
                Some(Self::rebalance(node))
            }
        }
    }

    fn max_node(mut node: &Node) -> &Node {
        while let Some(ref right) = node.right {
            node = right;
        }
        node
    }

    fn insert(&mut self, value: State) {
        self.root = Some(Self::insert_node(self.root.take(), value));
        self.size += 1;
    }

    fn erase_smallest(&mut self) {
        if self.root.is_none() {
            return;
        }
        self.root = Self::erase_min_node(self.root.take());
        self.size -= 1;
    }

    fn erase_largest(&mut self) {
        if self.root.is_none() {
            return;
        }
        self.root = Self::erase_max_node(self.root.take());
        self.size -= 1;
    }

    fn max(&self) -> &State {
        &Self::max_node(self.root.as_ref().expect("set is not empty")).value
    }
}

fn run_new_avl_impl(args: Args) {
    let start_time = Instant::now();

    let mut rng = Mt19937_64::new(args.seed);
    let mut pq: Vec<AVLSet> = (0..=args.max_turn).map(|_| AVLSet::new()).collect();
    let mut tree = Vec::new();
    let mut next_state_id = 0u64;

    let mut root = State::new(args.padding_size);
    root.turn = 0;
    root.tree_index = 0;
    root.state_id = next_state_id;
    next_state_id += 1;

    tree.push(TreeNode {
        parent: -1,
        children: Vec::new(),
        turn: 0,
        action_id: -1,
    });

    pq[0].insert(root);

    for _ in 0..args.loops {
        for t in 0..args.max_turn {
            if pq[t].empty() {
                continue;
            }

            let cur = pq[t].max().clone();
            pq[t].erase_largest();

            for k in 0..args.branch {
                let f = rng.next_f64();
                let g = rng.next_f64();

                let mut nxt = cur.clone();
                nxt.turn = cur.turn + 1;
                nxt.f_sum += f;
                nxt.g_sum_true += g;
                nxt.pending_g.push_back(g);
                nxt.state_id = next_state_id;
                next_state_id += 1;

                if nxt.pending_g.len() > args.delay {
                    if let Some(front) = nxt.pending_g.pop_front() {
                        nxt.g_sum_known += front;
                    }
                }

                let node_index = tree.len();
                tree.push(TreeNode {
                    parent: cur.tree_index,
                    children: Vec::new(),
                    turn: nxt.turn,
                    action_id: k as isize,
                });

                tree[cur.tree_index as usize].children.push(node_index);
                nxt.tree_index = node_index as isize;

                pq[t + 1].insert(nxt);
                if pq[t + 1].size() > args.loops {
                    pq[t + 1].erase_smallest();
                }
            }
        }
    }

    let mut best: Option<State> = None;

    if !pq[args.max_turn].empty() {
        best = Some(pq[args.max_turn].max().clone());
    } else {
        for t in (0..args.max_turn).rev() {
            if !pq[t].empty() {
                best = Some(pq[t].max().clone());
                break;
            }
        }
    }

    let Some(best) = best else {
        eprintln!("No state found.");
        process::exit(1);
    };

    let _path_nodes = build_path(&tree, best.tree_index);

    let elapsed_time = start_time.elapsed().as_secs_f64();
    eprintln!("elapsed_time = {} seconds", elapsed_time);
}

pub fn run_existing() {
    let argv: Vec<String> = env::args().collect();
    let prog = argv.first().map(String::as_str).unwrap_or("simple_problem");

    match parse_args(&argv) {
        Ok(args) => run_existing_impl(args),
        Err(ParseSignal::Help) => usage(prog),
        Err(ParseSignal::Error(message)) => {
            eprintln!("{}", message);
            usage(prog);
            process::exit(1);
        }
    }
}

pub fn run_new_avl() {
    let argv: Vec<String> = env::args().collect();
    let prog = argv.first().map(String::as_str).unwrap_or("simple_problem");

    match parse_args(&argv) {
        Ok(args) => run_new_avl_impl(args),
        Err(ParseSignal::Help) => usage(prog),
        Err(ParseSignal::Error(message)) => {
            eprintln!("{}", message);
            usage(prog);
            process::exit(1);
        }
    }
}