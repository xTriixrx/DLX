mod antenna;
mod fep;

use antenna::Antenna;
use fep::Fep;
use abi_stable::sabi_trait::prelude::TD_Opaque;
use abi_stable::std_types::{RResult, RString, RVec, RSlice};
use scheduler_core::extensions::api::{
    EncodedConstraint,
    ExtensionMetadata,
    SchedulerExtension,
    SchedulerExtension_TO,
    StaticResourceDescriptor,
};
use scheduler_core::extensions::root::{ExtensionRootModule, ExtensionRootModuleRef};
use abi_stable::prefix_type::PrefixTypeTrait;

pub struct SatelliteExtension;

impl SatelliteExtension {
    pub fn new() -> Self {
        Self
    }
}

impl SchedulerExtension for SatelliteExtension {
    fn metadata(&self) -> ExtensionMetadata {
        ExtensionMetadata {
            id: RString::from("satellite"),
            version: RString::from("0.1.0"),
            api_version: RString::from("v1"),
            resource_kinds: RVec::from(vec![
                RString::from("antenna"),
                RString::from("fep"),
            ]),
        }
    }

    fn validate_resources(
        &self,
        resources: RSlice<'_, StaticResourceDescriptor>,
    ) -> RResult<(), RString> {
        for resource in resources.iter() {
            match resource.kind.as_str() {
                "antenna" => {
                    if let Err(err) = Antenna::from_resource(resource) {
                        return RResult::RErr(RString::from(err));
                    }
                }
                "fep" => {
                    if let Err(err) = Fep::from_resource(resource) {
                        return RResult::RErr(RString::from(err));
                    }
                }
                _ => {}
            }
        }
        RResult::ROk(())
    }

    fn decode_resources(
        &self,
        resources: RSlice<'_, StaticResourceDescriptor>,
    ) -> RResult<RVec<RString>, RString> {
        let mut decoded: RVec<RString> = RVec::new();
        for resource in resources.iter() {
            match resource.kind.as_str() {
                "antenna" => {
                    let antenna = match Antenna::from_resource(resource) {
                        Ok(value) => value,
                        Err(err) => return RResult::RErr(RString::from(err)),
                    };
                    let json = match serde_json::to_string(&antenna) {
                        Ok(value) => value,
                        Err(err) => return RResult::RErr(RString::from(err.to_string())),
                    };
                    decoded.push(RString::from(json));
                }
                "fep" => {
                    let fep = match Fep::from_resource(resource) {
                        Ok(value) => value,
                        Err(err) => return RResult::RErr(RString::from(err)),
                    };
                    let json = match serde_json::to_string(&fep) {
                        Ok(value) => value,
                        Err(err) => return RResult::RErr(RString::from(err.to_string())),
                    };
                    decoded.push(RString::from(json));
                }
                _ => {}
            }
        }
        RResult::ROk(decoded)
    }

    fn encode_domain_constraints(&self, _context_json: RString) -> RVec<EncodedConstraint> {
        RVec::new()
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn get_extension() -> SchedulerExtension_TO<'static, abi_stable::std_types::RBox<()>> {
    SchedulerExtension_TO::from_value(SatelliteExtension::new(), TD_Opaque)
}

#[abi_stable::export_root_module]
pub fn get_root_module() -> ExtensionRootModuleRef {
    ExtensionRootModule { get_extension }.leak_into_prefix()
}
