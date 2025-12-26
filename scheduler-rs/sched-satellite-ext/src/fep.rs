use scheduler_core::extensions::api::StaticResourceDescriptor;
use serde::{Deserialize, Serialize};
use serde_json::Value;

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct Fep {
    pub id: String,
    pub bands: Vec<String>,
    #[serde(default)]
    pub region: Option<String>,
}

impl Fep {
    pub fn from_resource(resource: &StaticResourceDescriptor) -> Result<Self, String> {
        if resource.kind.as_str() != "fep" {
            return Err("not a fep resource".to_string());
        }
        let mut attributes: Value = serde_json::from_str(resource.attributes_json.as_str())
            .map_err(|err| err.to_string())?;
        if let Value::Object(ref mut object) = attributes {
            object.insert(
                "id".to_string(),
                Value::String(resource.id.as_str().to_string()),
            );
        } else {
            return Err("fep attributes must be an object".to_string());
        }
        serde_json::from_value(attributes).map_err(|err| err.to_string())
    }
}

#[cfg(test)]
mod tests {
    use super::Fep;
    use abi_stable::std_types::RString;
    use scheduler_core::extensions::api::StaticResourceDescriptor;
    use serde_json::json;

    #[test]
    fn fep_from_resource_parses_attributes() {
        let resource = StaticResourceDescriptor {
            id: RString::from("fep-1"),
            kind: RString::from("fep"),
            capacity: 1,
            attributes_json: RString::from(json!({
                "bands": ["S", "X"],
                "region": "west"
            }).to_string()),
        };

        let fep = Fep::from_resource(&resource).expect("parse fep");
        assert_eq!(fep.id, "fep-1");
        assert_eq!(fep.bands, vec!["S".to_string(), "X".to_string()]);
        assert_eq!(fep.region.as_deref(), Some("west"));
    }
}
