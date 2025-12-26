use crate::io::yaml::read_yaml_file;
use anyhow::{Context, Result};
use serde::Deserialize;
use std::path::Path;

#[derive(Debug, Deserialize)]
pub struct SchedulerConfig {
  pub slot_interval_minutes: Option<u32>,
  pub rest_port: Option<u16>,
  pub resources_path: Option<String>,
  pub extensions: Option<Vec<ExtensionConfig>>,
}

impl SchedulerConfig {
  pub fn slot_interval_minutes(&self) -> u32 {
    self.slot_interval_minutes.unwrap_or(5)
  }

  pub fn rest_port(&self) -> u16 {
    self.rest_port.unwrap_or(8080)
  }

  pub fn resources_path(&self) -> String {
    self.resources_path
      .clone()
      .unwrap_or_else(|| "config/resources.yaml".to_string())
  }

  pub fn extensions(&self) -> Option<&[ExtensionConfig]> {
    self.extensions.as_deref()
  }
}

#[derive(Debug, Deserialize, Clone)]
pub struct ExtensionConfig {
  pub id: String,
  pub path: String,
  #[serde(default)]
  pub enabled: bool,
}

pub fn read_config(path: &Path) -> Result<SchedulerConfig> {
  read_yaml_file(path)
    .with_context(|| format!("failed to load scheduler config at {}", path.display()))
}
