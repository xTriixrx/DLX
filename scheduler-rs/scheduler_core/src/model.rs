//! Shared scheduler domain objects.

use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct Resource {
    pub id: String,
    pub capacity: u32,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct Task {
    pub id: String,
    pub required: Vec<String>,
    pub duration_slots: u32,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct SchedulerProblem {
    pub resources: Vec<Resource>,
    pub tasks: Vec<Task>,
}
