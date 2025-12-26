use axum::{
    extract::{Path, State},
    http::StatusCode,
    routing::get,
    Json, Router,
};
use scheduler_core::extensions::loader::ExtensionRegistry;
use scheduler_core::model::ConstraintContext;
use scheduler_core::model::rsrc::static_resource::StaticResource;
use serde::Serialize;
use std::collections::HashMap;
use std::sync::Arc;

#[derive(Clone)]
pub struct AppState {
    pub context: Arc<ConstraintContext>,
    pub extensions: Arc<ExtensionRegistry>,
}

#[derive(Serialize)]
struct ResourcesIndex {
    resources: HashMap<String, Vec<String>>,
}

pub fn router(state: AppState) -> Router {
    Router::new()
        .route("/health", get(|| async { "ok" }))
        .route("/v1/resources", get(list_resources))
        .route("/v1/resources/:kind", get(list_resources_by_kind))
        .route("/v1/resources/:kind/:id", get(get_resource))
        .with_state(state)
}

async fn list_resources(State(state): State<AppState>) -> Json<ResourcesIndex> {
    let mut resources = HashMap::new();
    for resource in state.context.resources() {
        resources
            .entry(resource.kind.clone())
            .or_insert_with(Vec::new)
            .push(resource.id.clone());
    }
    for ids in resources.values_mut() {
        ids.sort();
    }
    Json(ResourcesIndex { resources })
}

async fn list_resources_by_kind(
    State(state): State<AppState>,
    Path(kind): Path<String>,
) -> Json<Vec<String>> {
    let mut ids = state
        .context
        .resources_by_kind(&kind)
        .into_iter()
        .map(|resource| resource.id.clone())
        .collect::<Vec<_>>();
    ids.sort();
    Json(ids)
}

async fn get_resource(
    State(state): State<AppState>,
    Path((kind, id)): Path<(String, String)>,
) -> Result<Json<StaticResource>, StatusCode> {
    match state.context.resource(&id) {
        Some(resource) if resource.kind == kind => Ok(Json(resource.clone())),
        _ => Err(StatusCode::NOT_FOUND),
    }
}
