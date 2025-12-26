use anyhow::{Context, Result};
use scheduler_core::conf::config::{read_config, ExtensionConfig};
use scheduler_core::db::yaml::reader::read_resources;
use scheduler_core::extensions::loader::{load_extensions, ExtensionEntry};
use scheduler_core::model::ConstraintContext;
use scheduler_rest::{router, AppState};
use std::env;
use std::path::PathBuf;
use std::sync::Arc;
use tokio::net::TcpListener;

#[tokio::main]
async fn main() -> Result<()> {
    let config_path = env::args()
        .nth(1)
        .map(PathBuf::from)
        .unwrap_or_else(|| PathBuf::from("config/config.yaml"));
    let config = read_config(&config_path)
        .with_context(|| format!("failed to load config from {}", config_path.display()))?;

    let resources_path = PathBuf::from(config.resources_path());
    let resources = read_resources(&resources_path)
        .with_context(|| format!("failed to load resources from {}", resources_path.display()))?;

    let extension_entries = config
        .extensions()
        .map(map_extension_entries)
        .unwrap_or_default();
    let extensions = load_extensions(&extension_entries)
        .context("failed to load extensions from config")?;
    extensions
        .validate_all(&resources.resources)
        .context("extension validation failed")?;

    let constraint_context = ConstraintContext::new(
        Vec::new(),
        resources.resource_types,
        resources.resources,
    );

    let app_state = AppState {
        context: Arc::new(constraint_context),
        extensions: Arc::new(extensions),
    };
    let app = router(app_state);
    let rest_port = config.rest_port();
    let listener = TcpListener::bind(("0.0.0.0", rest_port)).await?;
    println!("scheduler rest listening on {}", rest_port);
    axum::serve(listener, app).await?;
    Ok(())
}

fn map_extension_entries(entries: &[ExtensionConfig]) -> Vec<ExtensionEntry> {
    entries
        .iter()
        .map(|entry| ExtensionEntry {
            id: entry.id.clone(),
            path: entry.path.clone().into(),
            enabled: entry.enabled,
        })
        .collect()
}
