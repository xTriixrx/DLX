use super::api::{
    EncodedConstraint,
    ExtensionMetadata,
    SchedulerExtension_TO,
    StaticResourceDescriptor,
};
use super::root::ExtensionRootModuleRef;
use crate::model::ConstraintContext;
use crate::model::rsrc::static_resource::StaticResource;
use abi_stable::library::RootModule;
use abi_stable::std_types::{RBox, RResult, RString, RSlice};
use anyhow::{Context, Result};
use std::path::Path;

pub struct ExtensionEntry {
    pub id: String,
    pub path: std::path::PathBuf,
    pub enabled: bool,
}

struct ExtensionHandle {
    extension: SchedulerExtension_TO<'static, RBox<()>>,
    _module: ExtensionRootModuleRef,
}

pub struct ExtensionRegistry {
    extensions: Vec<ExtensionHandle>,
}

impl ExtensionRegistry {
    pub fn new() -> Self {
        Self { extensions: Vec::new() }
    }

    pub fn metadata(&self) -> Vec<ExtensionMetadata> {
        self.extensions
            .iter()
            .map(|entry| entry.extension.metadata())
            .collect()
    }

    pub fn validate_all(&self, resources: &[StaticResource]) -> Result<()> {
        for entry in &self.extensions {
            let kinds = entry.extension.metadata().resource_kinds;
            let scoped = resources
                .iter()
                .filter(|resource| {
                    kinds.iter().any(|kind| kind.as_str() == resource.kind.as_str())
                })
                .map(to_descriptor)
                .collect::<Vec<_>>();
            let result = entry
                .extension
                .validate_resources(RSlice::from_slice(&scoped));
            if let RResult::RErr(err) = result {
                return Err(anyhow::anyhow!(err.to_string()));
            }
        }
        Ok(())
    }

    pub fn encode_all(&self, context: &ConstraintContext) -> Vec<EncodedConstraint> {
        let context_json = serialize_context(context);
        let mut encoded = Vec::new();
        for entry in &self.extensions {
            let constraints = entry
                .extension
                .encode_domain_constraints(RString::from(context_json.clone()));
            encoded.extend(constraints.into_iter());
        }
        encoded
    }
}

impl Default for ExtensionRegistry {
    fn default() -> Self {
        Self::new()
    }
}

pub fn load_extensions(entries: &[ExtensionEntry]) -> Result<ExtensionRegistry> {
    let mut registry = ExtensionRegistry::new();
    for entry in entries {
        if !entry.enabled {
            continue;
        }
        let handle = load_dynamic_extension(&entry.path)
            .with_context(|| format!("failed to load extension {}", entry.id))?;
        registry.extensions.push(handle);
    }
    Ok(registry)
}

fn load_dynamic_extension(path: &Path) -> Result<ExtensionHandle> {
    let module = ExtensionRootModuleRef::load_from_file(path)
        .with_context(|| format!("failed to load extension {}", path.display()))?;
    let constructor = module.get_extension();
    let extension = constructor();
    Ok(ExtensionHandle {
        extension,
        _module: module,
    })
}

fn to_descriptor(resource: &StaticResource) -> StaticResourceDescriptor {
    let attributes = serde_json::to_string(&resource.attributes).unwrap_or_else(|_| "{}".to_string());
    StaticResourceDescriptor {
        id: RString::from(resource.id.clone()),
        kind: RString::from(resource.kind.clone()),
        capacity: resource.capacity,
        attributes_json: RString::from(attributes),
    }
}

fn serialize_context(context: &ConstraintContext) -> String {
    serde_json::to_string(context).unwrap_or_else(|_| "{}".to_string())
}
