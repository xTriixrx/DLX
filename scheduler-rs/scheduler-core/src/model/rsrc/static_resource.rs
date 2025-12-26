use serde::{Deserialize, Serialize};
use serde_json::Value;

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct StaticResource {
    pub id: String,
    pub kind: String,
    pub capacity: u32,
    #[serde(default)]
    pub attributes: Value,
}
