use serde::{Deserialize, Serialize};
use std::collections::HashMap;

use super::rsrc::rsrc_type::ResourceType;
use super::rsrc::static_resource::StaticResource;

pub type SlotId = String;

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct TimeSlot {
    pub id: SlotId,
    pub start_epoch_seconds: u64,
    pub end_epoch_seconds: u64,
    pub horizon_index: u32,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct ConstraintContext {
    slots: Vec<TimeSlot>,
    resource_types: Vec<ResourceType>,
    resources: Vec<StaticResource>,
    #[serde(skip)]
    slot_index: HashMap<SlotId, usize>,
    #[serde(skip)]
    resource_type_index: HashMap<String, usize>,
    #[serde(skip)]
    resource_index: HashMap<String, usize>,
    #[serde(skip)]
    resource_kind_index: HashMap<String, Vec<usize>>,
}

impl ConstraintContext {
    pub fn new(
        slots: Vec<TimeSlot>,
        resource_types: Vec<ResourceType>,
        resources: Vec<StaticResource>,
    ) -> Self {
        let mut context = Self {
            slots,
            resource_types,
            resources,
            slot_index: HashMap::new(),
            resource_type_index: HashMap::new(),
            resource_index: HashMap::new(),
            resource_kind_index: HashMap::new(),
        };
        context.rebuild_indexes();
        context
    }

    pub fn empty() -> Self {
        Self::new(Vec::new(), Vec::new(), Vec::new())
    }

    pub fn add_slot(&mut self, slot: TimeSlot) {
        let id = slot.id.clone();
        self.slots.push(slot);
        self.slot_index.insert(id, self.slots.len() - 1);
    }

    pub fn add_resource(&mut self, resource: StaticResource) {
        let id = resource.id.clone();
        let kind = resource.kind.clone();
        self.resources.push(resource);
        let index = self.resources.len() - 1;
        self.resource_index.insert(id, index);
        self.resource_kind_index.entry(kind).or_default().push(index);
    }

    pub fn add_resource_type(&mut self, resource_type: ResourceType) {
        let id = resource_type.id.clone();
        self.resource_types.push(resource_type);
        self.resource_type_index
            .insert(id, self.resource_types.len() - 1);
    }

    pub fn remove_resource(&mut self, resource_id: &str) -> Option<StaticResource> {
        let index = self.resource_index.get(resource_id).copied()?;
        let removed = self.resources.swap_remove(index);
        self.rebuild_indexes();
        Some(removed)
    }

    pub fn remove_resource_type(&mut self, resource_type_id: &str) -> Option<ResourceType> {
        let index = self.resource_type_index.get(resource_type_id).copied()?;
        let removed = self.resource_types.swap_remove(index);
        self.resources
            .retain(|resource| resource.kind != resource_type_id);
        self.rebuild_indexes();
        Some(removed)
    }

    pub fn slots(&self) -> &[TimeSlot] {
        &self.slots
    }

    pub fn resource_types(&self) -> &[ResourceType] {
        &self.resource_types
    }

    pub fn resources(&self) -> &[StaticResource] {
        &self.resources
    }

    pub fn slot(&self, slot_id: &str) -> Option<&TimeSlot> {
        self.slot_index
            .get(slot_id)
            .and_then(|index| self.slots.get(*index))
    }

    pub fn resource(&self, resource_id: &str) -> Option<&StaticResource> {
        self.resource_index
            .get(resource_id)
            .and_then(|index| self.resources.get(*index))
    }

    pub fn resource_type(&self, resource_type_id: &str) -> Option<&ResourceType> {
        self.resource_type_index
            .get(resource_type_id)
            .and_then(|index| self.resource_types.get(*index))
    }

    pub fn resources_by_type(&self, resource_type_id: &str) -> Vec<&StaticResource> {
        self.resources_by_kind(resource_type_id)
    }

    pub fn resources_by_kind(&self, kind: &str) -> Vec<&StaticResource> {
        match self.resource_kind_index.get(kind) {
            Some(indices) => indices
                .iter()
                .filter_map(|index| self.resources.get(*index))
                .collect(),
            None => Vec::new(),
        }
    }

    fn rebuild_indexes(&mut self) {
        self.slot_index.clear();
        for (index, slot) in self.slots.iter().enumerate() {
            self.slot_index.insert(slot.id.clone(), index);
        }

        self.resource_type_index.clear();
        for (index, resource_type) in self.resource_types.iter().enumerate() {
            self.resource_type_index
                .insert(resource_type.id.clone(), index);
        }

        self.resource_index.clear();
        self.resource_kind_index.clear();
        for (index, resource) in self.resources.iter().enumerate() {
            self.resource_index.insert(resource.id.clone(), index);
            self.resource_kind_index
                .entry(resource.kind.clone())
                .or_default()
                .push(index);
        }
    }
}

impl Default for ConstraintContext {
    fn default() -> Self {
        Self::empty()
    }
}
