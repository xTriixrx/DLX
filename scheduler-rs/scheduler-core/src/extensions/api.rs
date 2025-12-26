use abi_stable::std_types::{RResult, RSlice, RString, RVec};

#[derive(abi_stable::StableAbi, Clone, Debug, PartialEq, Eq)]
#[repr(C)]
pub struct ExtensionMetadata {
    pub id: RString,
    pub version: RString,
    pub api_version: RString,
    pub resource_kinds: RVec<RString>,
}

#[derive(abi_stable::StableAbi, Clone, Debug, PartialEq, Eq)]
#[repr(C)]
pub struct StaticResourceDescriptor {
    pub id: RString,
    pub kind: RString,
    pub capacity: u32,
    pub attributes_json: RString,
}

#[derive(abi_stable::StableAbi, Clone, Debug, PartialEq, Eq)]
#[repr(C)]
pub struct EncodedConstraint {
    pub id: RString,
    pub description: RString,
    pub payload_json: RString,
}

#[abi_stable::sabi_trait]
pub trait SchedulerExtension: Send + Sync {
    fn metadata(&self) -> ExtensionMetadata;

    fn validate_resources(
        &self,
        resources: RSlice<'_, StaticResourceDescriptor>,
    ) -> RResult<(), RString>;

    fn decode_resources(
        &self,
        resources: RSlice<'_, StaticResourceDescriptor>,
    ) -> RResult<RVec<RString>, RString>;

    fn encode_domain_constraints(&self, context_json: RString) -> RVec<EncodedConstraint>;
}
