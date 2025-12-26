use anyhow::{Context, Result};
use serde::de::DeserializeOwned;
use serde_saphyr as saphyr;
use std::fs;
use std::path::Path;

pub fn read_yaml_file<T: DeserializeOwned>(path: &Path) -> Result<T> {
  let contents = fs::read_to_string(path)
    .with_context(|| format!("failed to read yaml file at {}", path.display()))?;
  saphyr::from_str(&contents).context("failed to parse yaml")
}
